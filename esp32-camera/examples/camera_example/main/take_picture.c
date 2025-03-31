#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include <stdio.h>  // Para manipulação de arquivos

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS portTICK_PERIOD_MS
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "driver/uart.h"

#define UART_NUM UART_NUM_1
#define TX_PIN 17  // Pino TX do ESP32
#define RX_PIN 16  // Pino RX do ESP32
#define UART_BAUDRATE 115200

#define BOARD_WROVER_KIT 1

// Definição dos pinos da câmera (mantenha os mesmos do seu código original)
#ifdef BOARD_WROVER_KIT
#define CAM_PIN_PWDN 21
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK 2
#define CAM_PIN_SIOD 4
#define CAM_PIN_SIOC 5
#define CAM_PIN_D7 36
#define CAM_PIN_D6 37
#define CAM_PIN_D5 14
#define CAM_PIN_D4 13
#define CAM_PIN_D3 12
#define CAM_PIN_D2 11
#define CAM_PIN_D1 10
#define CAM_PIN_D0 9
#define CAM_PIN_VSYNC 19
#define CAM_PIN_HREF 6
#define CAM_PIN_PCLK 18
#endif

static const char *TAG = "example:take_picture";

// Inicializa a UART
void init_uart()
{
    ESP_LOGI(TAG, "Inicializando UART...");
    uart_config_t uart_config = {
        .baud_rate = UART_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, TX_PIN, RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM, 1024 * 2, 0, 0, NULL, 0);
    ESP_LOGI(TAG, "UART inicializada com sucesso!");
}

// Função para enviar a imagem pela UART
void send_image_uart(camera_fb_t *pic)
{
    ESP_LOGI(TAG, "Enviando imagem pela UART...");
    char header[20];
    snprintf(header, sizeof(header), "IMG_START:%d\n", pic->len);
    uart_write_bytes(UART_NUM, header, strlen(header));

    // Enviar os dados da imagem
    uart_write_bytes(UART_NUM, (const char *)pic->buf, pic->len);

    // Enviar sinal de finalização
    char footer[] = "\nIMG_END\n";
    uart_write_bytes(UART_NUM, footer, strlen(footer));

    ESP_LOGI(TAG, "Imagem enviada via UART!");
}

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 10000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

static esp_err_t init_camera(void)
{
    ESP_LOGI(TAG, "Inicializando câmera...");
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao iniciar a câmera");
        return err;
    }
    ESP_LOGI(TAG, "Câmera inicializada com sucesso!");
    return ESP_OK;
}

void app_main(void)
{
    
    ESP_LOGI(TAG, "Antes de inicializar a câmera...");
    esp_err_t err = init_camera();
    ESP_LOGI(TAG, "Depois de inicializar a câmera...");

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Falha ao iniciar a câmera: %s", esp_err_to_name(err));
        return;  // Corrigido: sem retornar um valor
    }

    while (1)
    {
        ESP_LOGI(TAG, "Capturando imagem...");
        camera_fb_t *pic = esp_camera_fb_get();

        if (pic)
        {
            ESP_LOGI(TAG, "Imagem capturada! Tamanho: %zu bytes", pic->len);
            send_image_uart(pic); // Envia a imagem pela UART
            esp_camera_fb_return(pic);
        }
        else
        {
            ESP_LOGE(TAG, "Falha ao capturar imagem");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Adiciona um delay de 2 segundos para evitar reset pelo WDT
    }

}