//
// Created by vnrju on 08/25/2022.
//

#ifndef EDGE_SOFTEX_H
#define EDGE_SOFTEX_H

#include <cstdint>
#include <cerrno>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Errors
#define EINVST __ELASTERROR + 1 /**< Invalid state */
#define EINTERNAL __ELASTERROR + 2 /**< Internal error */

// Validation macros
#define SOFTEX_CHECK_ENUM_RANGE(ENUM, VALUE) if(VALUE < 0 || VALUE >= __##ENUM##_AMOUNT) return -EINVAL
#define SOFTEX_CHECK_ARG_NOT_NULL(VALUE) if(VALUE == NULL) {ESP_LOGE("", #VALUE " is null"); return -EINVAL;}
#define SOFTEX_ERROR_CHECK(error) if (error) {\
    printf("[SOFTEX_ERROR] %s", strerror(error));\
}

#define SOFTEX_STREAMING_SUPPORT CONFIG_SOFTEX_STREAMING_SUPPORT
#define SOFTEX_STREAMING_STACK_SIZE CONFIG_SOFTEX_STREAMING_STACK_SIZE
#define SOFTEX_STREAMING_SUPPORT_HTTP CONFIG_SOFTEX_STREAMING_SUPPORT_HTTP
#define SOFTEX_STREAMING_SUPPORT_RTSP CONFIG_SOFTEX_STREAMING_SUPPORT_RTSP
#define SOFTEX_STREAMING_SUPPORT_CUSTOM CONFIG_SOFTEX_STREAMING_SUPPORT_CUSTOM

#endif //EDGE_SOFTEX_H
