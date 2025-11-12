#include <gbm.h>
#include <xf86drm.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    int drm_fd;
    struct gbm_device *gbm_device;
    struct gbm_bo *bo;

    // 1. Open the DRM device
    drm_fd = open("/dev/dri/card0", O_RDWR);
    if (drm_fd < 0) {
        perror("Failed to open DRM device");
        return -1;
    }

    // 2. Create a GBM device from the DRM file descriptor
    gbm_device = gbm_create_device(drm_fd);
    if (!gbm_device) {
        perror("Failed to create GBM device");
        close(drm_fd);
        return -1;
    }

    // 3. Create a GBM buffer object (BO)
    bo = gbm_bo_create(
        gbm_device,
        1280,   // width
        720,    // height
        GBM_FORMAT_XRGB8888, // format
        GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING // usage flags
    );
    if (!bo) {
        perror("Failed to create GBM buffer object");
        gbm_device_destroy(gbm_device);
        close(drm_fd);
        return -1;
    }

    // At this point, the GBM buffer object 'bo' is ready to be used.
    // It can be converted into a DRM framebuffer for display (scanout).
    int width =1280;
    int height = 720;
uint32_t stride;
void *map_data;

// Map the buffer for CPU access
char *pixels = gbm_bo_map(bo, 0, 0, width, height,
                           GBM_BO_TRANSFER_WRITE, &stride, &map_data);

if (pixels) {
    // --- Draw with the CPU ---
    // Example: Fill the buffer with a color
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Assuming GBM_FORMAT_XRGB8888
            uint32_t *pixel = (uint32_t *)(pixels + y * stride + x * 4);
            *pixel = 0x00FF0000; // Red color
        }
    }

    // Unmap the buffer when done
    gbm_bo_unmap(bo, map_data);
}




    // --- Cleanup ---
    gbm_bo_destroy(bo);
    gbm_device_destroy(gbm_device);
    close(drm_fd);

    printf("Successfully created and destroyed a GBM buffer.\n");

    return 0;
}
