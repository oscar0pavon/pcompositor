#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

// Struct to hold all the necessary KMS/DRM state
struct kms_dev {
    int fd;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeCrtc *crtc;
    struct gbm_device *gbm_dev;
    struct gbm_surface *gbm_surface;
    struct gbm_bo *gbm_bo;
    uint32_t fb_id;
    void *map;
};

static struct kms_dev dev;

// Helper function to find a suitable connector and CRTC
static int find_suitable_modeset_config(void) {
    int i, j;
    int ret = -1;

    // Iterate through all connectors
    for (i = 0; i < dev.resources->count_connectors; ++i) {
        dev.connector = drmModeGetConnector(dev.fd, dev.resources->connectors[i]);
        if (!dev.connector) {
            fprintf(stderr, "Could not get connector %d: %m\n", i);
            continue;
        }

        // Check if the connector is currently connected
        if (dev.connector->connection == DRM_MODE_CONNECTED && dev.connector->count_modes > 0) {
            // Find a valid encoder for this connector
            for (j = 0; j < dev.connector->count_encoders; ++j) {
                dev.encoder = drmModeGetEncoder(dev.fd, dev.connector->encoders[j]);
                if (!dev.encoder) {
                    fprintf(stderr, "Could not get encoder %d: %m\n", j);
                    continue;
                }

                // A CRTC (Cathode Ray Tube Controller) must be available
                if (dev.encoder->crtc_id) {
                    dev.crtc = drmModeGetCrtc(dev.fd, dev.encoder->crtc_id);
                    if (!dev.crtc) {
                        fprintf(stderr, "Could not get CRTC %d: %m\n", dev.encoder->crtc_id);
                        drmModeFreeEncoder(dev.encoder);
                        continue;
                    }

                    // A valid setup found!
                    ret = 0;
                    goto end;
                }
                drmModeFreeEncoder(dev.encoder);
            }
        }
        drmModeFreeConnector(dev.connector);
    }

end:
    if (ret) {
        fprintf(stderr, "No suitable DRM connector/encoder/CRTC found.\n");
    }
    return ret;
}

// Set up GBM and allocate a buffer
static int setup_gbm_buffer(void) {
    uint32_t width = dev.crtc->mode.hdisplay;
    uint32_t height = dev.crtc->mode.vdisplay;
    uint32_t stride;
    int ret;

    // Create a GBM device from the DRM file descriptor
    dev.gbm_dev = gbm_create_device(dev.fd);
    if (!dev.gbm_dev) {
        fprintf(stderr, "Failed to create GBM device: %m\n");
        return -1;
    }

    // Create a GBM surface to render into
    dev.gbm_surface = gbm_surface_create(dev.gbm_dev, width, height,
                                         GBM_FORMAT_XRGB8888,
                                         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!dev.gbm_surface) {
        fprintf(stderr, "Failed to create GBM surface\n");
        return -1;
    }

    // Allocate the buffer object
    dev.gbm_bo = gbm_surface_lock_front_buffer(dev.gbm_surface);
    if (!dev.gbm_bo) {
        fprintf(stderr, "Failed to lock front buffer\n");
        return -1;
    }

    // Add a DRM framebuffer from the GBM buffer
    ret = drmModeAddFB(dev.fd,
                       width,
                       height,
                       24, // depth
                       32, // bpp
                       gbm_bo_get_stride(dev.gbm_bo),
                       gbm_bo_get_handle(dev.gbm_bo).u32,
                       &dev.fb_id);
    if (ret) {
        fprintf(stderr, "Failed to add FB: %m\n");
        return ret;
    }

    // Map the buffer for CPU access to draw
    dev.map = mmap(NULL, gbm_bo_get_stride(dev.gbm_bo) * height, PROT_READ | PROT_WRITE,
                   MAP_SHARED, dev.fd, gbm_bo_get_offset(dev.gbm_bo, 0));
    if (dev.map == MAP_FAILED) {
        fprintf(stderr, "Failed to map buffer: %m\n");
        return -1;
    }

    return 0;
}

static void draw_gradient(void) {
    uint32_t width = dev.crtc->mode.hdisplay;
    uint32_t height = dev.crtc->mode.vdisplay;
    uint32_t stride = gbm_bo_get_stride(dev.gbm_bo);
    uint32_t *pixels = (uint32_t *)dev.map;
    int x, y;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            uint8_t red = (uint8_t)((float)x / width * 255);
            uint8_t blue = (uint8_t)((float)y / height * 255);
            pixels[(y * (stride / 4)) + x] = (red << 16) | blue;
        }
    }
}

int main(void) {
    // 1. Open the DRM device
    dev.fd = open("/dev/dri/card0", O_RDWR);
    if (dev.fd < 0) {
        fprintf(stderr, "Failed to open DRM device: %m\n");
        return EXIT_FAILURE;
    }

    // 2. Become the DRM master
    // This is required for modesetting.
    if (drmSetMaster(dev.fd) < 0) {
        fprintf(stderr, "Failed to become DRM master: %m\n");
        close(dev.fd);
        return EXIT_FAILURE;
    }

    // 3. Find a suitable display configuration
    dev.resources = drmModeGetResources(dev.fd);
    if (!dev.resources) {
        fprintf(stderr, "Failed to get DRM resources: %m\n");
        goto cleanup_fd;
    }

    if (find_suitable_modeset_config() != 0) {
        goto cleanup_resources;
    }

    // 4. Set up GBM and buffers
    if (setup_gbm_buffer() != 0) {
        goto cleanup_modeset;
    }

    // 5. Draw to the buffer

      draw_gradient();

    // 6. Perform modesetting to display the buffer
    int ret = drmModeSetCrtc(dev.fd,
                             dev.encoder->crtc_id,
                             dev.fb_id,
                             0, 0, // x, y
                             &dev.connector->connector_id,
                             1,
                             &dev.crtc->mode);
    if (ret != 0) {
        fprintf(stderr, "Failed to set CRTC: %m\n");
        goto cleanup_all;
    }

    printf("Displaying a color gradient. Press Enter to exit.\n");
    getchar();

cleanup_all:
    // Restore the CRTC to its original state (or just turn it off)
    drmModeSetCrtc(dev.fd, dev.crtc->crtc_id, 0, 0, 0, NULL, 0, NULL);
    munmap(dev.map, gbm_bo_get_stride(dev.gbm_bo) * dev.crtc->mode.vdisplay);
    gbm_surface_release_buffer(dev.gbm_surface, dev.gbm_bo);
    drmModeRmFB(dev.fd, dev.fb_id);
    gbm_surface_destroy(dev.gbm_surface);
    gbm_device_destroy(dev.gbm_dev);

cleanup_modeset:
    drmModeFreeCrtc(dev.crtc);
    drmModeFreeEncoder(dev.encoder);
    drmModeFreeConnector(dev.connector);

cleanup_resources:
    drmModeFreeResources(dev.resources);
    drmDropMaster(dev.fd);

cleanup_fd:
    close(dev.fd);

    return EXIT_SUCCESS;
}
