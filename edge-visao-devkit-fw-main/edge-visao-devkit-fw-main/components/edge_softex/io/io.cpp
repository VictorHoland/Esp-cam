//
// Created by vnrju on 10/10/2022.
//

#include <edge_softex/io.h>
#include <cstdint>
#include <esp_event_base.h>
#include <esp_err.h>
#include <mqtt_client.h>
#include <esp_log.h>
#include <tensorflow/lite/c/common.h>
#include <edge_softex/tflite.h>

#ifndef SOFTEX_IO_TASK_STACK_SIZE
#define SOFTEX_IO_TASK_STACK_SIZE CONFIG_SOFTEX_IO_TASK_STACK_SIZE
#endif
#ifndef SOFTEX_MQTT_TOPIC_NAME_STR_LEN
#define SOFTEX_MQTT_TOPIC_NAME_STR_LEN CONFIG_SOFTEX_MQTT_TOPIC_NAME_STR_LEN
#endif

#define TAG "softex_io"

static struct {
    io_config_t *io_config;
    QueueHandle_t queue;
    esp_mqtt_client_handle_t mqtt_client;
    uint8_t io_task_stack_buffer[SOFTEX_IO_TASK_STACK_SIZE];
    StaticTask_t io_task;
} self = {
        .io_config = NULL,
        .queue = NULL,
        .mqtt_client = NULL,
};

/* MQTT */
/**
 * @brief Callback para eventos do cliente MQTT
 *
 * @param event_handler_arg
 * @param event_base
 * @param event_id
 * @param event_data
 * */
static void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * @brief Configura o cliente MQTT
 *
 * @param config Parâmetros de configuração
 *
 * @return
 *      - 0 caso o cliente tenha sido configurado com sucesso
 *      - -ENOSR caso não haja memória suficiente
 *      - -EINVAL caso algum parâmetro de configuração do cliente esteja incorreto
 *      - -E algum outro erro no início do cliente
 * */
static int mqtt_setup(io_config_t *config);

/* GPIO */
/**
 * @brief Configura a saída via GPIO
 *
 * @param config Parâmetros de configuração
 *
 * @return
 *      - 0 caso o GPIO seja configurado com sucesso
 *      - -EINVAL caso algum parâmetro de configuração do GPIO esteja incorreto
 */
static int gpio_setup(io_config_t *config);

/**
 * @brief Tarefa que recebe um resultado e envia para as saídas configuradas
 *
 * @param arg Não utilizado
 */
static void io_task(void *arg);

int io_setup(io_type_t io_type, io_config_t *config, QueueHandle_t queue) {
    int err;

    SOFTEX_CHECK_ARG_NOT_NULL(config);
    SOFTEX_CHECK_ARG_NOT_NULL(queue);

    switch (io_type) {
        case IO_OUTPUT_GPIO:
            self.io_config = config;
            err = gpio_setup(config);
            if (err < 0) {
                ESP_LOGE(TAG, "Invalid GPIO config");
                return err;
            }
            break;

        case IO_OUTPUT_MQTT:
            if (strlen(config->topic_name) == 0) {
                ESP_LOGE(TAG, "Empty topic name");
                return -ENOTSUP;
            }

            self.io_config = config;
            err = mqtt_setup(config);
            if (err < 0) {
                ESP_LOGE(TAG, "Invalid MQTT config");
                return err;
            }
            break;
    }

    self.queue = queue;

    return 0;
}

int io_start() {
    xTaskCreateStatic(io_task, "io_task", SOFTEX_IO_TASK_STACK_SIZE, NULL, 5, self.io_task_stack_buffer, &self.io_task);

    return 0;
}

static void mqtt_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    (void) event_handler_arg;
    (void) event_base;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;

    switch ((esp_mqtt_event_id_t) event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;
        case MQTT_EVENT_DELETED:
            ESP_LOGI(TAG, "MQTT_EVENT_DELETED");
            break;
        case MQTT_EVENT_ANY:
            break;
    }
}

static int mqtt_setup(io_config_t *config) {
    self.mqtt_client = esp_mqtt_client_init(&config->mqtt);

    esp_err_t err = esp_mqtt_client_register_event(self.mqtt_client, (esp_mqtt_event_id_t) ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler,
                                                   config);
    if (err != ESP_OK) {
        switch (err) {
            case ESP_ERR_NO_MEM:
                return -ENOSR;
            case ESP_ERR_INVALID_ARG:
                return -EINVAL;
        }
    }

    err = esp_mqtt_client_start(self.mqtt_client);
    if (err != ESP_OK) {
        switch (err) {
            case ESP_ERR_INVALID_ARG:
                return -EINVAL;
            case ESP_FAIL:
                return -EINTERNAL;
        }

        return 0;
    }

    return (int) self.mqtt_client;
}

static int gpio_setup(io_config_t *config) {
    int i;
    uint64_t pin_bit_mask = 0;

    for (i = 0; i < config->pin_count; i++) {
        pin_bit_mask |= (1 << config->pins[i]);
    }

    config->gpio.pin_bit_mask = pin_bit_mask;

    esp_err_t err = gpio_config(&config->gpio);
    if (err != ESP_OK) {
        return -EINVAL;
    }

    return 0;
}

static void io_task(void *arg) {
    (void) arg;

    model_output_t *output;

    while (true) {
        if (xQueueReceive(self.queue, &output, portMAX_DELAY)) {
            if (self.io_config->pins != NULL) {
                printf("output: %p\n", output);
                if (output->task_type == TASK_CLASSIFICATION) {
                    int i;
                    for (i = 0; i < self.io_config->pin_count; i++) {
                        int level = (i == output->classification.argmax);

                        printf("setting gpio %d level to %d (argmax = %d)\n", self.io_config->pins[i], level, output->classification.argmax);

                        if (self.io_config->pins[i] == -1) {
                            continue;
                        }

                        gpio_set_level(self.io_config->pins[i], level);
                    }
                }
            }

            if (self.mqtt_client != NULL) {
                if (output->task_type == TASK_CLASSIFICATION) {
#define BUFFER_SIZE 32
                    char buffer[BUFFER_SIZE] = "";
                    const char *label = output->classification.label ? output->classification.label : "";
                    snprintf(buffer, BUFFER_SIZE, "%d %s", output->classification.argmax, label);
                    int message_id = esp_mqtt_client_publish(self.mqtt_client, self.io_config->topic_name, buffer,
                                                             strlen(buffer), self.io_config->qos, false);

                    if (message_id == -1) {
                        ESP_LOGE(TAG, "Failed to publish message");
                    }
#undef BUFFER_SIZE
                }
                if (output->task_type == TASK_REGRESSION) {
                    int message_id = esp_mqtt_client_publish(self.mqtt_client, self.io_config->topic_name,
                                                             (char *) output->regression.data,
                                                             output->regression.len * sizeof(float),
                                                             self.io_config->qos, false);
                    if (message_id == -1) {
                        ESP_LOGE(TAG, "Failed to publish message");
                    }
                }
            }
        }
    }

    vTaskDelete(NULL);
}
