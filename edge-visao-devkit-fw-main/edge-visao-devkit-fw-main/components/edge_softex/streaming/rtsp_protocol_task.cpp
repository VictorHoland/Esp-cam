//
// Created by vnrju on 08/16/2022.
//

#include "rtsp_protocol_task.h"

#include <errno.h>

#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <string.h>
#include <edge_softex/edge_softex.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_timer.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "CRtspSession.h"
#include "OV2640Streamer.h"

#include "edge_softex/streaming.h"
#include "EdgeSoftexStreamer.h"

#if SOFTEX_STREAMING_SUPPORT_RTSP

#define TAG "rtsp-protocol-task"

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID            //!< SSID da rede configurada através do menuconfig
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD        //!< Senha da rede configurada através do menuconfig
#define ENABLE_DEBUG

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t instance_any_id;
static esp_event_handler_instance_t instance_got_ip;

static OV2640 cam;
static CRtspSession *session;
static EdgeSoftexStreamer *streamer;
static QueueHandle_t input_queue = NULL;
static QueueHandle_t output_queue = NULL;

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

/* The event group allows multiple bits for each event, but we only care about one event:
 * - we are connected to the AP with an IP */
#define WIFI_CONNECTED_BIT BIT0
/**
 * @brief
 *
 * @param arg
 * @param event_base
 * @param event_id
 * @param event_data
 */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

/**
 * @brief
 */
static void wifi_init_sta(void);

/**
 * @brief
 *
 * @return
 */
//lifted from Arduino framework
static unsigned long millis();

/**
 * @brief
 *
 * @param ms
 */
static void delay(uint32_t ms);

/**
 * @brief
 */
static void client_worker();

/**
 * @brief
 *
 * @param frame
 */
static void rtsp_server(camera_fb_t *frame);

void rtsp_protocol_task(void *arg) {
    streaming_manager_t *streaming_manager = (streaming_manager_t *) arg;
    input_queue = streaming_manager->input_queue;
    output_queue = streaming_manager->output_queue;

    camera_fb_t *frame;
    while (true) {
        if (xQueueReceive(input_queue, &frame, portMAX_DELAY)) {
            if (frame) {
                rtsp_server(frame);
            }
        }
    }

    vTaskDelete(NULL);
}

int rtsp_stop() {
    if(session != NULL) {
        session->m_stopped = true;

        return 0;
    }

    return -EINVST;
}

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Attempting to connect...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Attempting to reconnect...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};

    strcpy((char *) wifi_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID);
    strcpy((char *) wifi_config.sta.password, EXAMPLE_ESP_WIFI_PASS);
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    /* Setting a password implies station will connect to all security modes including WEP/WPA.
     * However these modes are deprecated and not advisable to be used. Incase your Access point
     * doesn't support WPA2, these mode can be enabled by commenting below line */
    wifi_config.sta.threshold = {
            .authmode = WIFI_AUTH_WPA2_PSK
    };
    wifi_config.sta.pmf_cfg = {
            .capable = true,
            .required = false
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) */
    /* The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP SSID: %s password: %s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

unsigned long millis() {
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void client_worker() {
    camera_fb_t *frame;

    while (!session->m_stopped) {
        session->handleRequests(0);
        unsigned long now = millis();
        if (xQueueReceive(input_queue, &frame, portMAX_DELAY)) {
            session->broadcastCurrentFrame(now, frame);

            if (output_queue == NULL) {
                esp_camera_fb_return(frame);
            }
            else {
                xQueueSend(output_queue, &frame, portMAX_DELAY);
            }
        } else {
        }
        //let the system do something else for a bit
        vTaskDelay(1);
    }

    //shut ourselves down
    delete streamer;
    delete session;

    vTaskDelete(NULL);
}

void rtsp_server(camera_fb_t *frame) {
    SOCKET server_socket;
    SOCKET client_socket;
    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    /*
    camera_config_t config = esp32cam_aithinker_config;
    config.frame_size = CAM_FRAMESIZE;
    config.jpeg_quality = CAM_QUALITY;
    config.xclk_freq_hz = 16500000; //seems to increase stability compared to the full 20000000
    cam.init(config);

    // setup other camera options
    sensor_t *s = esp_camera_sensor_get();
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 1);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 2); // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
    s->set_whitebal(s, 0);       // 0 = disable , 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable
    s->set_wb_mode(s, 2);        // 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
    s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable
    s->set_aec2(s, 0);           // 0 = disable , 1 = enable
    s->set_ae_level(s, 0);       // -2 to 2
    s->set_aec_value(s, 300);    // 0 to 1200
    s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t) 0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable , 1 = enable
    s->set_wpc(s, 1);            // 0 = disable , 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable
    s->set_lenc(s, 1);           // 0 = disable , 1 = enable
    s->set_hmirror(s, CONFIG_CAM_HORIZONTAL_MIRROR); // 0 = disable , 1 = enable
    s->set_vflip(s, CONFIG_CAM_VERTICAL_FLIP);       // 0 = disable , 1 = enable
    s->set_dcw(s, 1);            // 0 = disable , 1 = enable
    s->set_colorbar(s, 0);       // 0 = disable , 1 = enable
    */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8554); // listen on RTSP port 8554
    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    int enable = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed! errno=%d\n", errno);
    }

    // bind our server socket to the RTSP port and listen for a client connection
    if (bind(server_socket, (sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
        printf("bind failed! errno=%d\n", errno);
    }

    if (listen(server_socket, 5) != 0) {
        printf("listen failed! errno=%d\n", errno);
    }

    printf("\n\nrtsp://%s.localdomain:8554/mjpeg/1\n\n", CONFIG_LWIP_LOCAL_HOSTNAME);

    // loop forever to accept client connections
    while (true) {
        printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
        client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_addr_len);
        printf("Client connected from: %s\n", inet_ntoa(client_addr.sin_addr));
        streamer = new EdgeSoftexStreamer((SOCKET) client_socket, frame);
        session = new CRtspSession((SOCKET) client_socket, streamer);

        esp_camera_fb_return(frame);

        client_worker();
        vTaskDelay(1);
    }

    close(server_socket);
}

#endif