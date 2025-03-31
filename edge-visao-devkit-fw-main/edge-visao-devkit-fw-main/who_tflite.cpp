//
// Created by vnrju on 06/14/2022.
//

#include "who_tflite.h"

#include <esp_log.h>

#if CONFIG_PIXFORMAT_GRAYSCALE
#define GET_PIXEL(frame, i, j) frame->buf[i * frame->width + j]
#elif CONFIG_PIXFORMAT_RGB565
#define GET_PIXEL(frame, i, j) ((uint16_t*) frame->buf)[i * frame->width + j]
#endif

static const char *TAG = "edge_vision_sdk";
static QueueHandle_t xQueueFrameI = nullptr;
static QueueHandle_t xQueueFrameO = nullptr;
static bool gReturnFb = true;
static who_tflite_config_t *gConfig = nullptr;
static tflite::ErrorReporter *errorReporter = nullptr;
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input = nullptr;
static TfLiteTensor *output = nullptr;

static void task_process_handler(void *arg) {
    camera_fb_t *frame = nullptr;

    while (true) {
        if (xQueueFrameI != nullptr)
            xQueueReceive(xQueueFrameI, &frame, portMAX_DELAY);

        TickType_t startTime, totalInferenceStartTime;
        TickType_t preprocessTime, inferenceTime, postprocessTime, totalInferenceTime;

        // Preprocess data
        totalInferenceStartTime = xTaskGetTickCount();
        startTime = totalInferenceStartTime;
        //gConfig->preprocess(input, frame);

        int xRatio = (int) ((frame->width << 16) / CONFIG_TARGET_INPUT_WIDTH) + 1;
        int yRatio = (int) ((frame->height << 16) / CONFIG_TARGET_INPUT_HEIGHT) + 1;
        int x, y;

        for (int i = 0; i < CONFIG_TARGET_INPUT_HEIGHT; i++) {
            for (int j = 0; j < CONFIG_TARGET_INPUT_WIDTH; j++) {
                x = ((j * xRatio) >> 16);
                y = ((i * yRatio) >> 16);
                int pixel = GET_PIXEL(frame, x, y);
#if CONFIG_PIXFORMAT_GRAYSCALE && CONFIG_INVERT_IMAGE
                pixel = 255 - pixel;
#endif
                input->data.f[i * CONFIG_TARGET_INPUT_WIDTH + j] = (float) pixel / 255.0f;
            }
        }

        preprocessTime = xTaskGetTickCount() - startTime;

        // Make inference
        startTime = xTaskGetTickCount();
        TfLiteStatus invokeStatus = interpreter->Invoke();
        if (invokeStatus != kTfLiteOk) {
            TF_LITE_REPORT_ERROR(errorReporter, "Invoke failed");
            continue;
        }
        inferenceTime = xTaskGetTickCount() - startTime;

        // Postprocess
        startTime = xTaskGetTickCount();
        gConfig->postprocess(output);
        postprocessTime = xTaskGetTickCount() - startTime;
        totalInferenceTime = xTaskGetTickCount() - totalInferenceStartTime;

#if 1// Todo: change to CONFIG_EDGE_VISION_SDK_DEBUG
        ESP_LOGI(TAG, "Total Inference Time: % 3dms (Preprocess: % 3dms\tInference: % 3dms\tPostprocess: % 3dms)",
                 totalInferenceTime * portTICK_PERIOD_MS, preprocessTime * portTICK_PERIOD_MS,
                 inferenceTime * portTICK_PERIOD_MS, postprocessTime * portTICK_PERIOD_MS);
#endif

        if (xQueueFrameO != nullptr) {
            xQueueSend(xQueueFrameO, &frame, portMAX_DELAY);
        } else {
            if (frame != nullptr) {
                if (gReturnFb) {
                    esp_camera_fb_return(frame);
                } else {
                    free(frame);
                }
            }
        }
    }
}

void register_tflite_model(who_tflite_config_t *config, QueueHandle_t frameI, QueueHandle_t frameO, bool returnFb) {
    xQueueFrameI = frameI;
    xQueueFrameO = frameO;
    gReturnFb = returnFb;
    gConfig = config;

    static tflite::MicroErrorReporter staticErrorReporter;
    errorReporter = &staticErrorReporter;

    if (!config->modelBuffer) {
        // ESP_LOGI(TAG, "Model buffer is null");
        return;
    }

    model = tflite::GetModel(config->modelBuffer);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        // ESP_LOGI(TAG, "Model version %d differs from TfLite schema version %d", model->version(),
        //        TFLITE_SCHEMA_VERSION);
        return;
    }

    if (gConfig->tensorArena == NULL) {
        // ESP_LOGI(TAG, "Allocating tensor arena");
        gConfig->tensorArena = (uint8_t *) malloc(gConfig->tensorArenaSize);
    }

    if (gConfig->tensorArena == NULL) {
        // printf("Couldn't allocate memory of %d bytes\n", gConfig->tensorArenaSize);
        return;
    }

    static tflite::MicroInterpreter staticInterpreter(model, *gConfig->resolver, gConfig->tensorArena,
                                                      gConfig->tensorArenaSize,
                                                      errorReporter);
    interpreter = &staticInterpreter;

    TfLiteStatus allocateStatus = interpreter->AllocateTensors();
    if (allocateStatus != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(errorReporter, "AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    xTaskCreatePinnedToCore(task_process_handler, TAG, 2 * 1024, nullptr, 5, nullptr, 0);

    // ESP_LOGI(TAG, "TfLite model registered");
}

//endregion
