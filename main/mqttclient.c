#ifndef WIFIPROVISIONING_H
#endif
#include "WifiProvisioning.h"
#include "mqttclient.h"


#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "bms_state.h"


static const char *TAG = "MQTT";

esp_mqtt_client_handle_t mqtt_client;

mqttConfig mConfig;

bool ok_torun = false;

mqttConfig* getPointerToConfig() {
    return &mConfig;
}

bool validate_mqtt_host_port(const char *mqtt_host, int mqtt_port) {
    // Check if MQTT host is empty or exceeds maximum length
    if (mqtt_host == NULL || strlen(mqtt_host) == 0 || strlen(mqtt_host) >= 64) {
        return false;
    }
    
    // Check if MQTT port is within valid range
    if (mqtt_port <= 0 || mqtt_port > 65535) {
        return false;
    }
    
    return true;
}

char* build_mqtt_uri(const char *mqtt_host, int mqtt_port) {
    // Perform basic sanity check on MQTT host and port
    if (!validate_mqtt_host_port(mqtt_host, mqtt_port)) {
        return NULL;
    }

    // Allocate memory for the MQTT URI (mqtt://<host>:<port>)
    char *mqtt_uri = malloc(strlen("mqtt://") + strlen(mqtt_host) + 6); // 6 for ":<port>" and null terminator
    if (mqtt_uri == NULL) {
        return NULL; // Memory allocation failed
    }

    // Build the MQTT URI
    sprintf(mqtt_uri, "mqtt://%s:%d", mqtt_host, mqtt_port);

    return mqtt_uri;
}

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    // case MQTT_EVENT_CONNECTED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    //     //msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
    //     ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

    //     msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
    //     ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

    //     msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
    //     ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

    //     msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
    //     ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
    //     break;
    // case MQTT_EVENT_DISCONNECTED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    //     break;

    // case MQTT_EVENT_SUBSCRIBED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    //     //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
    //     ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
    //     break;
    // case MQTT_EVENT_UNSUBSCRIBED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    //     break;
    // case MQTT_EVENT_PUBLISHED:
    //     ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    //     break;
    // case MQTT_EVENT_DATA:
    //     ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    //     printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
    //     printf("DATA=%.*s\r\n", event->data_len, event->data);
    //     break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            // log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            // log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            // log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        //ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
// static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
// {
//     switch (event->event_id) {
//         case MQTT_EVENT_CONNECTED:
//             ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
//             break;
//         case MQTT_EVENT_DISCONNECTED:
//             ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
//             break;
//         case MQTT_EVENT_SUBSCRIBED:
//             ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
//             break;
//         case MQTT_EVENT_UNSUBSCRIBED:
//             ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
//             break;
//         case MQTT_EVENT_PUBLISHED:
//             ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
//             break;
//         case MQTT_EVENT_DATA:
//             ESP_LOGI(TAG, "MQTT_EVENT_DATA");
//             printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
//             printf("DATA=%.*s\r\n", event->data_len, event->data);
//             break;
//         case MQTT_EVENT_ERROR:
//             ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
//             break;
//         case MQTT_EVENT_ANY:
//             ESP_LOGI(TAG, "MQTT_EVENT_ANY");
//             break;
//         case MQTT_EVENT_BEFORE_CONNECT:
//             ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
//             break;
//         case MQTT_EVENT_DELETED:
//             ESP_LOGI(TAG, "MQTT_EVENT_DELETED");
//             break;
//         case MQTT_USER_EVENT:
//             ESP_LOGI(TAG, "MQTT_USER_EVENT");
//             break;
//         default:
//             ESP_LOGI(TAG, "MQTT_EVENT_Default");
//             break;
//     }
//     return ESP_OK;
// }

void set_mqtt_ok_to_run(bool ok)
{
    ok_torun  = ok;
}

void set_mqtt_runtime_config(mqttConfig *config) {
    printf("MQTT Host Name: %s, Port: %d\n", config->mqtt_host, config->mqtt_port);
    strncpy(mConfig.mqtt_host, config->mqtt_host, sizeof(mConfig.mqtt_host) - 1);
    mConfig.mqtt_host[sizeof(mConfig.mqtt_host) - 1] = '\0'; // Ensure null-termination

    mConfig.mqtt_port = config->mqtt_port;
    // Further processing with config->mqtt_host and config->mqtt_port
}

void mqtt_app_start()
{
    char *mqtt_uri = build_mqtt_uri(mConfig.mqtt_host, mConfig.mqtt_port);
    esp_mqtt_client_config_t mqtt_cfg = {
         .broker.address.uri = mqtt_uri
        
    };
    ESP_LOGE(TAG, "Mqtt host = %s", mqtt_cfg.broker.address.uri );            
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler_cb, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void mqtt_publish(const char *topic, const char *data, int data_len) {
    // Check if mqtt_client is initialized, handle error if not
    if (mqtt_client == NULL) {
        // Handle error, maybe log an error message or return an error code
        return;
    }

    // Publish message
    esp_mqtt_client_publish(mqtt_client, topic, data, data_len, 1, 0);
}