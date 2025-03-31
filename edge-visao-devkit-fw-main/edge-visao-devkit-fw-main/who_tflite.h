//
// Created by vnrju on 06/14/2022.
//
#ifndef EDGE_VISAO_DEVKIT_FW_TFLITE_CLASSIFIER_H
#define EDGE_VISAO_DEVKIT_FW_TFLITE_CLASSIFIER_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/schema/schema_generated.h>

#include <who_camera.h>


typedef void (*TfLitePreprocess)(TfLiteTensor *input, camera_fb_t *frame);
typedef void (*TfLitePostprocess)(TfLiteTensor *output);

typedef struct who_tflite_config_t {
    int tensorArenaSize;
    uint8_t* tensorArena;
    tflite::MicroOpResolver* resolver;
    TfLitePreprocess preprocess;
    TfLitePostprocess postprocess;
    void* modelBuffer;
} who_tflite_config_t;

void register_tflite_model(who_tflite_config_t* tflite_config, QueueHandle_t frameI, QueueHandle_t frameO, bool returnFb);

#endif //EDGE_VISAO_DEVKIT_FW_TFLITE_CLASSIFIER_H
