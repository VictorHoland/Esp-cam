//
// Created by vnrju on 08/16/2022.
//

#ifndef EDGE_SOFTEX_STREAMING_RTSP_PROTOCOL_CB_H
#define EDGE_SOFTEX_STREAMING_RTSP_PROTOCOL_CB_H

#include <esp_camera.h>

#include <edge_softex/edge_softex.h>

#if SOFTEX_STREAMING_SUPPORT_RTSP

/**
 * @brief Tarefa de gerenciamento do streaming. Aguarda o primeiro frame para obter informação sobre o tamanho da imagem. Em seguida, aguarda a conexão com o cliente para transmitir as imagens.
 *
 * @param arg Dado de usuário. Ponteiro para o gerenciador de streaming
 */
void rtsp_protocol_task(void* arg);

/**
 * @brief Interrompe o streaming RTSP.
 *
 * @return
 */
int rtsp_stop();
#endif

#endif //EDGE_SOFTEX_STREAMING_RTSP_PROTOCOL_CB_H
