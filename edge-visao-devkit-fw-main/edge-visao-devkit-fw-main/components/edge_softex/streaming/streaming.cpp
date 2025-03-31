
#include <cerrno>
#include <esp_log.h>
#include <app_httpd.hpp>
#include <esp_http_server.h>
#include <app_mdns.h>

#include "edge_softex/edge_softex.h"
#include "edge_softex/streaming.h"
#include "rtsp_protocol_task.h"

#ifdef CONFIG_SOFTEX_STREAMING_SUPPORT

extern httpd_handle_t stream_httpd;

/**
 * @brief Singleton com os dados de gerenciamento do streaming
 */
static streaming_manager_t self = {
        .protocol = STREAM_PROTOCOL_NONE,
        .input_queue = NULL,
        .output_queue = NULL,
        .task_handle = NULL,
        .task_code = NULL,
        .custom_start = NULL,
        .custom_stop = NULL,
};

int stream_start(stream_protocol_t protocol, xQueueHandle input_queue, xQueueHandle output_queue) {
    SOFTEX_CHECK_ENUM_RANGE(stream_protocol_t, protocol);
    SOFTEX_CHECK_ARG_NOT_NULL(input_queue);

    self.protocol = protocol;
    self.input_queue = input_queue;
    self.output_queue = output_queue;

    switch (protocol) {
        case STREAM_PROTOCOL_NONE:
            return 0;
#if SOFTEX_STREAMING_SUPPORT_HTTP
            case STREAM_PROTOCOL_HTTP:
//            app_wifi_main();
            app_mdns_main();
            register_httpd(input_queue, output_queue, output_queue == NULL);
            self.task_handle = stream_httpd;
            break;
#endif
#if SOFTEX_STREAMING_SUPPORT_RTSP
        case STREAM_PROTOCOL_RTSP:
            self.task_handle = xTaskCreateStatic(rtsp_protocol_task, "streaming-rtsp", CONFIG_SOFTEX_STREAMING_RTSP_STACK_SIZE,
                                                 &self, tskIDLE_PRIORITY, self.stack_buffer,
                                                 &self.task_buffer);
            break;
#endif
#if SOFTEX_STREAMING_SUPPORT_CUSTOM
        case STREAM_PROTOCOL_CUSTOM:
            if (self.custom_start != NULL) {
                self.custom_start();
            }
            break;
#endif
        default:
            break;
    }

    return 0;
}

int stream_stop() {
    if (self.task_handle == NULL)
        return -EFAULT;

    switch (self.protocol) {
        case STREAM_PROTOCOL_NONE:
            break;
#ifdef CONFIG_SOFTEX_STREAMING_SUPPORT_HTTP
        case STREAM_PROTOCOL_HTTP:
            httpd_stop(stream_httpd);
            break;
#endif
#if SOFTEX_STREAMING_SUPPORT_RTSP
        case STREAM_PROTOCOL_RTSP:
            rtsp_stop();
            vTaskDelete(self.task_handle);
            break;
#endif
#if SOFTEX_STREAMING_SUPPORT_CUSTOM
        case STREAM_PROTOCOL_CUSTOM:
            if (self.custom_stop != NULL) {
                self.custom_stop();
            }
            break;
#endif
        default:
            break;
    }
    return 0;
}

void setup_custom(custom_streaming_callback_t start_callback, custom_streaming_callback_t stop_callback) {
    self.custom_start = start_callback;
    self.custom_stop = stop_callback;
}

#endif