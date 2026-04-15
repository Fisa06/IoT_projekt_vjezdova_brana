//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H

/**
 * @brief Start WiFi provisioning process
 *
 * This function initializes WiFi provisioning using SoftAP scheme.
 * If device is not provisioned, it starts provisioning mode.
 * If device is already provisioned, it connects to saved WiFi network.
 *
 * @note This function blocks until WiFi connection is established
 */
void wifi_provisioning_start(void);

/**
 * @brief Check if device is already provisioned
 *
 * @return true if device has saved WiFi credentials, false otherwise
 */
bool wifi_is_provisioned(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
