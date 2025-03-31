//
// Created by vnrju on 10/10/2022.
//

#ifndef SOFTEX_WIFI_H
#define SOFTEX_WIFI_H

#ifndef SOFTEX_WIFI_SSID
#define SOFTEX_WIFI_SSID CONFIG_SOFTEX_WIFI_SSID
#endif
#ifndef SOFTEX_WIFI_PASSWORD
#define SOFTEX_WIFI_PASSWORD CONFIG_SOFTEX_WIFI_PASSWORD
#endif
#ifndef SOFTEX_WIFI_MAXIMUM_RETRY
#define SOFTEX_WIFI_MAXIMUM_RETRY CONFIG_SOFTEX_WIFI_MAXIMUM_RETRY
#endif

/**
 * @brief Configura o Wi-fi e conecta à rede @a ssid
 *
 * @param ssid Nome da rede que se deseja conectar
 * @param password Senha da rede
 *
 * @return
 *      - 0 caso o WiFi seja configurado corretamente
 *      -
 */
int wifi_setup(const char* ssid, const char* password);


#endif //SOFTEX_WIFI_H
