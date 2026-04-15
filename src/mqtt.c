//
// Created by karel on 15.04.2026.
//

#include "mqtt.h"
#include "mqtt_client.h"
#include "esp_crt_bundle.h"


esp_err_t mqtt_init() {

    const esp_mqtt_client_config_t mqtt_client_config = {
        .broker = {
            .address = {.uri = "mqtts://mqtt.xdlabs.cloud",
            .port = 8883},
                .verification = {
                    .crt_bundle_attach = esp_crt_bundle_attach,
            },
        },
       .session = {
           .keepalive = 120,
           .last_will = {
               .topic = "xdd/status",
               .msg = "offline",
               .msg_len = 7,
               .qos = 1,
               .retain = 1
           }

       },
       .credentials = {
           .username = "123",
           .client_id = "123",
           .authentication = {
               .password = "123"
           }
       }
    };

    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_client_config);
    esp_err_t err = esp_mqtt_client_start(mqtt_client);
    return err;
}
