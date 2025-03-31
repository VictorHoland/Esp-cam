//
// Created by vnrju on 08/25/2022.
//

#include <esp_log.h>
#include "EdgeSoftexStreamer.h"

#if SOFTEX_STREAMING_SUPPORT_RTSP

EdgeSoftexStreamer::EdgeSoftexStreamer(SOCKET aClient, camera_fb_t *frame)
        : CStreamer(aClient, frame->width, frame->height) {
    ESP_LOGI("EdgeSoftexStreamer", "Initializing streamer\t(width: %d height: %d)", frame->width, frame->height);
}

void EdgeSoftexStreamer::streamImage(uint32_t curMsec, camera_fb_t *frame) {
    static uint8_t* out;
    static size_t out_len;
    frame2jpg(frame, 12, &out, &out_len);

    streamFrame(frame->buf, frame->len, curMsec);
}

#endif