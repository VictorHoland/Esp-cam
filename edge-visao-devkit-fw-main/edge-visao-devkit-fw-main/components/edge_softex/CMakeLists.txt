FILE(GLOB_RECURSE SRC *.cpp)

idf_component_register(
        SRC_DIRS camera streaming tflite wifi io
        INCLUDE_DIRS include
        REQUIRES esp32-camera esp_http_server nvs_flash Micro-RTSP modules tflite-lib mqtt
)