//
// Created by vnrju on 08/22/2022.
//

#include <edge_softex/camera.h>
#include <cerrno>


static struct {
    QueueHandle_t output_queue;
    StaticTask_t capture_task;
    TaskHandle_t capture_handle;
    StackType_t capture_buffer[SOFTEX_CAMERA_CAPTURE_TASK_STACK_SIZE];
} self;

/**
 * @brief Configuração padrão do driver de câmera
 * */
static const camera_config_t config = {
        .pin_pwdn = SOFTEX_CAMERA_PIN_PWDN,
        .pin_reset = SOFTEX_CAMERA_PIN_RESET,
        .pin_xclk = SOFTEX_CAMERA_PIN_XCLK,
        .pin_sscb_sda = SOFTEX_CAMERA_PIN_SDA,
        .pin_sscb_scl = SOFTEX_CAMERA_PIN_SCL,
        .pin_d7 = SOFTEX_CAMERA_PIN_D7,
        .pin_d6 = SOFTEX_CAMERA_PIN_D6,
        .pin_d5 = SOFTEX_CAMERA_PIN_D5,
        .pin_d4 = SOFTEX_CAMERA_PIN_D4,
        .pin_d3 = SOFTEX_CAMERA_PIN_D3,
        .pin_d2 = SOFTEX_CAMERA_PIN_D2,
        .pin_d1 = SOFTEX_CAMERA_PIN_D1,
        .pin_d0 = SOFTEX_CAMERA_PIN_D0,
        .pin_vsync = SOFTEX_CAMERA_PIN_VSYNC,
        .pin_href = SOFTEX_CAMERA_PIN_HREF,
        .pin_pclk = SOFTEX_CAMERA_PIN_PCLK,
        .xclk_freq_hz = SOFTEX_CAMERA_XCLK_FREQ_HZ,
        .ledc_timer = SOFTEX_CAMERA_LEDC_TIMER,
        .ledc_channel = SOFTEX_CAMERA_LEDC_CHANNEL,
        .pixel_format = SOFTEX_CAMERA_PIXFORMAT,
        .frame_size = SOFTEX_CAMERA_FRAMESIZE,
        .jpeg_quality = SOFTEX_CAMERA_JPEG_QUALITY,
        .fb_count = SOFTEX_CAMERA_FB_COUNT,
        .fb_location = SOFTEX_CAMERA_FB_LOCATION,
        .grab_mode = SOFTEX_CAMERA_GRAB_MODE,
};

/**
 * @brief Tarefa básica para capturar as imagens da câmera
 *
 * @param arg Dado de usuário
 */
static void capture_task(void* arg);

int camera_setup(QueueHandle_t output_queue) {
    self.output_queue = output_queue;

    esp_camera_init(&config);

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor == NULL) {
        return -ENXIO;
    }

    return 0;
}

int camera_start() {
    self.capture_handle = xTaskCreateStatic(capture_task, "capture", SOFTEX_CAMERA_CAPTURE_TASK_STACK_SIZE, NULL,
                                            SOFTEX_CAMERA_CAPTURE_TASK_STACK_SIZE, self.capture_buffer,
                                            &self.capture_task);

    return 0;
}

void capture_task(void *arg) {
    (void) arg;

    while (true) {
        camera_fb_t* frame = esp_camera_fb_get();

        if(frame != NULL) {
            if (self.output_queue != NULL) {
                xQueueSend(self.output_queue, &frame, portMAX_DELAY);
            } else {
                esp_camera_fb_return(frame);
            }
        }

        vTaskDelay(1);
    }

    vTaskDelete(NULL);
}