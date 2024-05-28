#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H
#include "WifiProvisioning.h"

#include <stdbool.h>
#include <stdio.h>
//extern ok_torun = false;

// config.h


// Contents of mqttclient.h go here

// Define the configuration struct
typedef struct {
    char mqtt_host[64];
    int mqtt_port;
} mqttConfig;

// Function prototypes (if any)


void initialize_config(mqttConfig *config);
void set_mqtt_runtime_config(mqttConfig *config);
void set_mqtt_ok_to_run(bool ok);
void mqtt_publish(const char *topic, const char *data, int data_len);
mqttConfig* getPointerToConfig();
void mqtt_app_start();


#endif  // CONFIG_H