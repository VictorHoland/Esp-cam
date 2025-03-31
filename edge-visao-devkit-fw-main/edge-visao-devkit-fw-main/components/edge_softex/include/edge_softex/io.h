//
// Created by vnrju on 10/10/2022.
//

#ifndef SOFTEX_IO_H
#define SOFTEX_IO_H

#include <mqtt_client.h>
#include <driver/gpio.h>

#define MESSAGE_LEN 10

typedef enum {
    IO_OUTPUT_GPIO,
    IO_OUTPUT_MQTT
} io_type_t;

typedef struct {
    esp_mqtt_client_config_t mqtt;
    const char* topic_name;
    int qos;
    int message_len;
    gpio_config_t gpio;
    const gpio_num_t* pins;
    int pin_count;
} io_config_t;

/**
 * Configura a saída do modelo.
 *
 * @details O valor de @p config não é copiado. Certifique-se que o valor original permanece válido após a chamada da função
 *
 * @param io_type Tipo de saída.
 *
 * @param config Parâmetros de configuração
 * @param queue Fila utilizada para receber as inferências
 * @return
 *      - 0 caso a configuração tenha sido feita com sucesso.
 *      - -ENOTSUP caso o tipo seja @a io_type_t e o nome do tópico esteja vazio
 *      - -EINVAL caso algum parâmetro de configuração do GPIO ou MQTT esteja incorreto
 */
int io_setup(io_type_t io_type, io_config_t *config, QueueHandle_t queue);

/**
 * @brief Inicia a tarefa de tratamento da saída do modelo.
 *
 * @return
 *      - 0 caso a tarefa seja criada com sucesso
 */
int io_start();

#endif // SOFTEX_IO_H
