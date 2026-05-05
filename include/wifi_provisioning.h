//
// Created by karel on 15.04.2026.
//

#ifndef IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
#define IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H

/* Starts BLE Wi-Fi provisioning or connects to already saved Wi-Fi. */
void wifi_provisioning_start(void);

/* Clears saved Wi-Fi credentials. Used by the boot reset button. */
void wifi_provisioning_reset(void);

#endif //IOT_PROJEKT_VJEZDOVA_BRANA_WIFI_PROVISIONING_H
