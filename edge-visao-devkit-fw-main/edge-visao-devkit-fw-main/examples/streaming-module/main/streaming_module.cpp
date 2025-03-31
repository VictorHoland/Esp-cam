#include <esp_log.h>

#include <edge_softex/camera.h>
#include <edge_softex/streaming.h>
#include <edge_softex/wifi.h>


#define TAG "Example Streaming Module"
#define IMAGE_CLASSIFICATION_WIFI_SSID "adalovelace"
#define IMAGE_CLASSIFICATION_WIFI_PASSWORD "geladotrincando"


uint8_t camera_queue_buffer[SOFTEX_CAMERA_FB_COUNT * sizeof(camera_fb_t *)];
StaticQueue_t camera_queue;
QueueHandle_t camera_queue_handle;


#ifdef __cplusplus
extern "C" {
#endif

void app_main(void) {
    int err;

    camera_queue_handle = xQueueCreateStatic(SOFTEX_CAMERA_FB_COUNT, sizeof(camera_fb_t *), camera_queue_buffer,
                                             &camera_queue);

    err = wifi_setup(IMAGE_CLASSIFICATION_WIFI_SSID, IMAGE_CLASSIFICATION_WIFI_PASSWORD);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize wifi (error %d)\n", err);
        return;
    }

    err = camera_setup(camera_queue_handle);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize the camera (error %d)\n", err);
        return;
    }

    camera_start();
    stream_start(STREAM_PROTOCOL_RTSP, camera_queue_handle, NULL);
}

#ifdef __cplusplus
}
#endif