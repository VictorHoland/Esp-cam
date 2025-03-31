//
// Created by vnrju on 08/15/2022.
//

#ifndef EDGE_SOFTEX_STREAMING_H
#define EDGE_SOFTEX_STREAMING_H

#ifdef CONFIG_SOFTEX_STREAMING_SUPPORT

#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <edge_softex/edge_softex.h>


/**
 * @brief Protocolos de streaming suportados
 */
typedef enum {
    STREAM_PROTOCOL_NONE,
#if SOFTEX_STREAMING_SUPPORT_HTTP
    STREAM_PROTOCOL_HTTP,          //!< Protocolo HTTP
#endif
#if SOFTEX_STREAMING_SUPPORT_RTSP
    STREAM_PROTOCOL_RTSP,          //!< Protocolo RTSP
#endif
#if SOFTEX_STREAMING_SUPPORT_CUSTOM
    STREAM_PROTOCOL_CUSTOM,        //!< Protocolo customizado
#endif
    __stream_protocol_t_AMOUNT,
} stream_protocol_t;

/**
 * @brief Callback para implementação de streaming customizado
 */
typedef int (*custom_streaming_callback_t)();

/**
 * @brief Estrutura para armazenar os dados usados no gerenciamento do streaming durante a execução
 */
typedef struct {
    stream_protocol_t protocol;                                   //!< Protocolo de streaming utilizado
    xQueueHandle input_queue;                                     //!< Fila para receber a imagem
    xQueueHandle output_queue;                                    //!< Fila para devolver a imagem
    xTaskHandle task_handle;                                      //!< Identificador da tarefa responsável pelo streaming
    StackType_t stack_buffer[SOFTEX_STREAMING_STACK_SIZE];        //!< Buffer para a pilha da tarefa de streaming
    StaticTask_t task_buffer;                                     //!< Pilha da tarefa de streaming
    pdTASK_CODE task_code;                                        //!< Ponteiro para função da tarefa associada ao tipo de protocolo escolhido
    custom_streaming_callback_t custom_start;                     //!< Callback para iniciar o protocolo customizado de streaming
    custom_streaming_callback_t custom_stop;                      //!< Callback para interromper o protocolo customizado de streaming
} streaming_manager_t;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief Inicia o streaming utilizando o protocolo @a protocol.
 *
 * @param protocol Protocolo utilizado para fazer o streaming
 * @param input_queue Identificador da fila usada para receber as imagens
 * @param output_queue Identificador da fila para onde cada frame será mandada após a transmissão (opcional)
 * @return
 *      - 0 caso o streaming seja iniciado com sucesso
 *      - -EINVAL caso o tipo de streaming seja maior que @a streaming_type_t__AMMOUNT
 */
int stream_start(stream_protocol_t protocol, xQueueHandle input_queue, xQueueHandle output_queue);
#ifdef __cplusplus
}
#endif

/**
 * @brief Interrompe o streaming
 * @return
 *      - 0 caso o streaming seja interrompido com sucesso
 *      - -EFAULT caso o handle da tarefa seja inválido
 */
int stream_stop();

/**
 * @brief Configura os callbacks para o protocolo de streaming customizado
 */
void setup_custom(custom_streaming_callback_t start_callback, custom_streaming_callback_t stop_callback);

#endif

#endif //EDGE_SOFTEX_STREAMING_H