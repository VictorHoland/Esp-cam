//
// Created by vnrju on 08/30/2022.
//

#include "edge_softex/tflite.h"

#include <esp_camera.h>
#include <cerrno>

#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <esp_log.h>

/** @brief Tamanho da pilha utilizada pela tarefa de processamento */
#define SOFTEX_TFLITE_PROCESS_TASK_STACK_SIZE CONFIG_SOFTEX_TFLITE_PROCESS_TASK_STACK_SIZE
/** @brief Id do núcleo onde a tarefa de processamento será criada */
#define SOFTEX_TFLITE_PROCESS_TASK_CORE CONFIG_SOFTEX_TFLITE_PROCESS_TASK_CORE
/** @brief Largura da imagem de entrada da rede */
#define SOFTEX_TARGET_INPUT_WIDTH CONFIG_SOFTEX_TARGET_INPUT_WIDTH
/** @brief Altura da imagem de entrada da rede */
#define SOFTEX_TARGET_INPUT_HEIGHT CONFIG_SOFTEX_TARGET_INPUT_HEIGHT
/** @brief Tamanho total da imagem de entrada. Equivalente a @ref SOFTEX_TARGET_INPUT_WIDTH * @ref SOFTEX_TARGET_INPUT_HEIGHT */
#define SOFTEX_TARGET_INPUT_SIZE SOFTEX_TARGET_INPUT_WIDTH * SOFTEX_TARGET_INPUT_HEIGHT

#if CONFIG_SOFTEX_CAMERA_PIXFORMAT_GRAYSCALE
#define GET_PIXEL(frame, i, j) frame->buf[i * frame->width + j]
#elif CONFIG_SOFTEX_CAMERA_PIXFORMAT_RGB565
#define GET_PIXEL(frame, i, j) ((uint16_t*) frame->buf)[i * frame->width + j]
#else
#define GET_PIXEL(frame, i, j) 0
#endif

#define TAG "softex-tflite"

/**
 * @brief
 */
static struct {
    tflite_config_t *config;                                                         //!<
    tflite::Model *model;                                                            //!<
    tflite::MicroErrorReporter *error_reporter;                                      //!<
    tflite::MicroInterpreter *interpreter;                                           //!<
    TfLiteTensor *input_tensor;                                                      //!<
    TfLiteTensor *output_tensor;                                                     //!<
    uint8_t process_task_stack_buffer[SOFTEX_TFLITE_PROCESS_TASK_STACK_SIZE];        //!<
    model_output_t model_output;                                                     //!<
    QueueHandle_t image_input_queue;                                                 //!< Fila de onde virá a imagem de entrada
    QueueHandle_t model_output_queue;                                                //!< Fila por onde será enviado o resultado da inferência
    QueueHandle_t image_output_queue;                                                //!< Fila que receberá a imagem após o processamento
} self = {
        .config = NULL,
        .model = NULL,
        .error_reporter = NULL,
        .interpreter = NULL,
        .input_tensor = NULL,
        .output_tensor = NULL
};

/**
  * @brief Processa a imagem capturada, redimensionando para o tamanho @ref SOFTEX_TARGET_INPUT_WIDTH X @ref SOFTEX_TARGET_INPUT_HEIGHT, e preenche o tensor de entrada do modelo com a imagem resultante
  */
static void process_input(camera_fb_t *frame);

/**
  * @brief Processa o resultado da inferência do modelo e envia-o pela fila
  */
static void process_inference();

/**
  * @brief Integra as etapas de processamento (@ref process_input e @ref process_inference).
  *
  * @param arg Dado de usuário passado para a tarefa (não utilizado)
  */
static void task_process_handler(void *arg);


int tflite_setup(tflite_config_t *config, QueueHandle_t image_input_queue, QueueHandle_t model_output_queue,
                 QueueHandle_t image_output_queue) {
    SOFTEX_CHECK_ARG_NOT_NULL(image_input_queue);
    SOFTEX_CHECK_ARG_NOT_NULL(model_output_queue);
    SOFTEX_CHECK_ARG_NOT_NULL(config->tensor_arena);
    SOFTEX_CHECK_ARG_NOT_NULL(config->model);

    self.config = config;

    static tflite::MicroErrorReporter static_error_reporter;
    self.error_reporter = &static_error_reporter;

    self.model = (tflite::Model *) tflite::GetModel(config->model);
    if (self.model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGI(TAG, "Model version %d differs from TfLite schema version %d", self.model->version(),
                 TFLITE_SCHEMA_VERSION);

        return -ENOTSUP;
    }

    static tflite::MicroInterpreter static_interpreter(self.model, *config->op_resolver, config->tensor_arena,
                                                       config->tensor_arena_size,
                                                       self.error_reporter);

    self.interpreter = &static_interpreter;

    TfLiteStatus allocate_status = self.interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        TF_LITE_REPORT_ERROR(self.error_reporter, "AllocateTensors() failed");
        return -ENOSR;
    }

    self.input_tensor = self.interpreter->input(0);
    self.output_tensor = self.interpreter->output(0);

    if (SOFTEX_TARGET_INPUT_WIDTH * SOFTEX_TARGET_INPUT_HEIGHT > self.input_tensor->bytes) {
        ESP_LOGE(TAG, "Input target size is larger than tensor size %d > %d", SOFTEX_TARGET_INPUT_SIZE,
                 self.input_tensor->bytes);

        return -ERANGE;
    }

    // Initialize model output structure
    self.model_output.task_type = self.config->task;
    if (self.config->task == TASK_CLASSIFICATION) {
        self.model_output.classification.label = NULL;
        self.model_output.classification.argmax = -1;
        self.model_output.classification.probability = 0.0f;
    } else if (self.config->task == TASK_REGRESSION) {
        self.model_output.regression.len = self.config->output_len;
        self.model_output.regression.data = (float *) malloc(self.config->output_len * sizeof(float));
        if (self.model_output.regression.data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory to store regression data");
            return -ENOSR;
        }
    }

    self.image_input_queue = image_input_queue;
    self.model_output_queue = model_output_queue;
    self.image_output_queue = image_output_queue;

    return 0;
}

int tflite_start() {
    static StaticTask_t task;
    TaskHandle_t task_handle = xTaskCreateStaticPinnedToCore(task_process_handler, "inference", SOFTEX_TFLITE_PROCESS_TASK_STACK_SIZE, NULL, 5,
                                  self.process_task_stack_buffer, &task, SOFTEX_TFLITE_PROCESS_TASK_CORE);

    if (task_handle == NULL) {
        ESP_LOGE(TAG, "Failed to create inference task");
        return -EFAULT;
    }

    return 0;
}

static void process_input(camera_fb_t *frame) {
    int x_ratio = (int) ((frame->width << 16) / SOFTEX_TARGET_INPUT_WIDTH) + 1;
    int y_ratio = (int) ((frame->height << 16) / SOFTEX_TARGET_INPUT_HEIGHT) + 1;
    int x;
    int y;

    for (int i = 0; i < SOFTEX_TARGET_INPUT_HEIGHT; i++) {
        for (int j = 0; j < SOFTEX_TARGET_INPUT_WIDTH; j++) {
            x = ((j * x_ratio) >> 16);
            y = ((i * y_ratio) >> 16);
            int pixel = GET_PIXEL(frame, x, y);
#if defined(CONFIG_SOFTEX_CAMERA_PIXFORMAT_GRAYSCALE) && defined(CONFIG_INVERT_IMAGE)
            pixel = 255 - pixel;
#endif
            self.input_tensor->data.f[i * SOFTEX_TARGET_INPUT_WIDTH + j] = (float) pixel / 255.0f;
        }
    }
}

static void process_inference() {
    if (self.config->task == TASK_CLASSIFICATION) {
        int argmax = 0;
        float max = self.output_tensor->data.f[0];
        for (int i = 1; i < self.config->output_len; i++) {
            if (self.output_tensor->data.f[i] > max) {
                max = self.output_tensor->data.f[i];
                argmax = i;
            }
        }

        self.model_output.classification.argmax = argmax;
        if (self.config->labels != NULL) {
            self.model_output.classification.label = (char*) self.config->labels[argmax];
        }
    } else if (self.config->task == TASK_REGRESSION) {
        for (int i = 0; i < self.config->output_len; i++) {
            self.model_output.regression.data[i] = self.output_tensor->data.f[i];
        }
    }
}

static void task_process_handler(void *arg) {
    (void) arg;

    camera_fb_t *frame = NULL;

    while (true) {
        if (xQueueReceive(self.image_input_queue, &frame, portMAX_DELAY)) {
            if (frame == NULL) {
                continue;
            }

            process_input(frame);

            TfLiteStatus invoke_status = self.interpreter->Invoke();
            if (invoke_status != kTfLiteOk) {
                TF_LITE_REPORT_ERROR(self.error_reporter, "Invoke failed");
                continue;
            }

            process_inference();

            model_output_t *item = &self.model_output;
            xQueueSend(self.model_output_queue, &item, portMAX_DELAY);

            if (self.image_output_queue != NULL) {
                xQueueSend(self.image_output_queue, &frame, portMAX_DELAY);
            } else if (frame != NULL) {
                esp_camera_fb_return(frame);
            }
        }

        vTaskDelay(1);
    }
}
