//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H

/**
 * @brief Start Wi-Fi provisioning process
 *
 * This function initializes Wi-Fi provisioning using the BLE scheme.
 * If device is not provisioned, it starts provisioning mode.
 * If device is already provisioned, it connects to the saved Wi-Fi network.
 *
 * @note This function blocks until Wi-Fi connection is established
 */
void wifi_provisioning_start(void);

/**
 * @brief Clear saved Wi-Fi provisioning credentials
 *
 * After calling this function, the next call to wifi_provisioning_start()
 * will enter provisioning mode until new credentials are stored.
 */
void wifi_provisioning_reset(void);

/**
 * @brief Check if device is already provisioned
 *
 * @return true if device has saved Wi-Fi credentials, false otherwise
 */
bool wifi_is_provisioned(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
