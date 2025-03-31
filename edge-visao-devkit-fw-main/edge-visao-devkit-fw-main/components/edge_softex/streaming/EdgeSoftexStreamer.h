//
// Created by vnrju on 08/25/2022.
//

#ifndef STREAMING_MODULE_EDGESOFTEXSTREAMER_H
#define STREAMING_MODULE_EDGESOFTEXSTREAMER_H

#include <edge_softex/edge_softex.h>

#if SOFTEX_STREAMING_SUPPORT_RTSP

#include "CStreamer.h"

/**
 * @brief Implementação de streamer para integrar a biblioteca MicroRTSP com o driver da câmera
 */
class EdgeSoftexStreamer : public CStreamer {
public:
    /**
     * @brief Inicializa o streamer com o tamanho da imagem
     *
     * @param aClient
     * @param frame
     */
    explicit EdgeSoftexStreamer(SOCKET aClient, camera_fb_t *frame);

    /**
     * @brief Transmite a imagem
     *
     * @param curMsec Tempo atual em milissegundos
     * @param frame Imagem a ser transmitida
     */
    void streamImage(uint32_t curMsec, camera_fb_t *frame) override;
};

#endif

#endif //STREAMING_MODULE_EDGESOFTEXSTREAMER_H

