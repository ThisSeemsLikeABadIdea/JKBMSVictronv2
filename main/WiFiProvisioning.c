#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include <esp_wifi.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include <esp_event.h>
#include "esp_log.h"

#include "esp_event_loop.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_netif.h"

#include <netdb.h>
#include <sys/param.h>

#include "htmlcomponents.h"

#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"

#include "mqttclient.h"
#include "esp_wifi_types.h"
#include "bms_state.h"
#include "WifiProvisioning.h"

#define TAG "WIFI_PROV"

// Define event bits for Wi-Fi

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD

#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
#define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
#define ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
#define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif
#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif
#define MAX_AP_COUNT 20
#define MAX_WIFI_FAIL_COUNT 3
#define TAG "WIFI_PROV_HANDLER"

// Define the stack size and priority for the task
#define WIFI_SCAN_TASK_STACK_SIZE 4096
#define WIFI_SCAN_TASK_PRIORITY   5

// To be removed and replced with wifi max retries
int s_retry_num = 0;
int retry_num = 10; // max retries

// global print debugs
bool debugsEnabled = true;

bool wifi_configured = false;
bool wifi_connected = false;
bool wifi_scan_running = false;
static EventGroupHandle_t wifi_event_group;
bool mqtt_config_loaded = false;
bool mqtt_configured = false;
mqttConfig mconfig;

// keep a list of SSID's here
// Global variable to store WiFi SSIDs
bool wifi_scan_task_created = false;

wifi_ap_record_t wifi_ap_records[MAX_AP_COUNT];
uint16_t ap_count;

// Max association attempts
static int wifi_fail_count = 0;

bool get_mqtt_config_loaded() {
    return mqtt_config_loaded;
}

bool get_mqtt_configured() {
    return mqtt_configured;
}


bool get_wifiStatus()
{
    return wifi_connected;
}

// Function prototype for the task
void wifi_scan_task(void *pvParameters);

// Create a task to handle Wi-Fi scanning
void create_wifi_scan_task() {
    xTaskCreate(&wifi_scan_task, "wifi_scan_task", WIFI_SCAN_TASK_STACK_SIZE, NULL, WIFI_SCAN_TASK_PRIORITY, NULL);
}

// Task function to initiate Wi-Fi scanning when connected
void wifi_scan_task(void *pvParameters) {
    while (1) {
        uint16_t number = 20;        
        // Wait until Wi-Fi is connected
        while (!wifi_connected) {
            vTaskDelay(pdMS_TO_TICKS(1000));  // Delay for 1 second before checking again
        }
         wifi_scan_config_t scan_config = {
        .ssid = NULL,        // Scan for all SSIDs
        .bssid = NULL,       // Scan for all BSSIDs
        .channel = 0,        // Scan on all channels
        .show_hidden = true, // Show hidden APs
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 0,
        };
        memset(wifi_ap_records, 0, sizeof(wifi_ap_records));
        //ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
        //ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(MAX_AP_COUNT, wifi_ap_records));
        // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        // ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
        
        // for (int i = 0; i < number; i++) {
        // ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        // ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);               
        // ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
        // }
        // Perform Wi-Fi scanning
        // Add your Wi-Fi scanning code here
        
        // After scanning, you can reset the flag if necessary
        //wifi_connected = false;

        // Delay before checking Wi-Fi connection status again
        vTaskDelay(pdMS_TO_TICKS(5000));  // Delay for 5 seconds before scanning again
    }
}

// NVS namespace for WiFi configuration
#define NVS_NAMESPACE "wifi_config"
wifi_config_t loaded_wifi_config;

void wifi_scan() {
    printf("&&&&&&&&&&&&&&&&&& wifi scan is starting");
   if(!wifi_scan_task_created)
   {
    wifi_scan_running = true;
    wifi_scan_task_created = true;
    create_wifi_scan_task();
   }
}

// Handler for serving the WiFi provisioning page
esp_err_t wifi_provisioning_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Handling WiFi provisioning request...");
    const char* html =  get_wifi_provisioning_page();
    //ESP_LOGI(TAG, "WiFi provisioning page content: %s", html);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html , HTTPD_RESP_USE_STRLEN);
    
    ESP_LOGI(TAG, "WiFi provisioning request handled successfully.");
    
    return ESP_OK;
}

void printDebug(const char *message)
{
    if (debugsEnabled)
    {
        printf("[DEBUG] %s\n", message);
    }
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
  
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if ( s_retry_num < retry_num  ) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connected = true;
    }else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < retry_num) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
    ESP_LOGI(TAG, "connect to the AP fail");
    }
}


// Save WiFi credentials to flash memory
void save_wifi_credentials(const char *wifi_name, const char *wifi_password)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        printf("Error opening NVS handle\n");
        return;
    }
    err = nvs_set_str(nvs_handle, "wifi_name", wifi_name);
    if (err != ESP_OK)
    {
        printf("Error setting WiFi name\n");
    }
    err = nvs_set_str(nvs_handle, "wifi_password", wifi_password);
    if (err != ESP_OK)
    {
        printf("Error setting WiFi password\n");
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

esp_err_t wifi_configuration_post_handler(httpd_req_t *req)
{

    httpd_uri_t *uri = req->uri;
    const char *url = uri->uri;

    ESP_LOGI(TAG, "WiFi provisioning configuration post called");
    // Buffer to store the data received in the POST request
    char content[200];

    // Get the length of the data received in the POST request    
    int content_length = req->content_len +1;

    int ret = httpd_req_recv(req, content, content_length);
    
    // Get the data received in the POST request
    httpd_req_get_url_query_str(req, content, content_length);

    // Log the received data
    content[content_length - 1] = '\0';
     
        // Log the received data
    ESP_LOG_BUFFER_HEXDUMP(TAG, content, content_length, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Received data size: %i", content_length);

    ESP_LOG_BUFFER_HEX(TAG, content, content_length);
    ESP_LOGI(TAG, "Received data size: %i", content_length);
    ESP_LOGI(TAG, "Received data: %s", content);
    // Buffer to store WiFi name and password
    char wifi_name[32], wifi_password[64];

    // Parse the received data to extract WiFi name and password
    if (httpd_query_key_value(content, "wifi_name", wifi_name, sizeof(wifi_name)) != ESP_OK)
    {
        // If parsing fails, respond with a 400 error (Bad Request)
        httpd_resp_send(req, "Invalid parameters", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Received wifi name data: %s", wifi_name);

    if(httpd_query_key_value(content, "wifi_password", wifi_password, sizeof(wifi_password)) != ESP_OK)
    {
        // If parsing fails, respond with a 400 error (Bad Request)
        httpd_resp_send(req, "Invalid parameters", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }    
    ESP_LOGI(TAG, "Received wifi password data: %s", wifi_password);
    // Save WiFi credentials to flash memory or use them as needed
    // Example: 
    save_wifi_credentials(wifi_name, wifi_password);

    // Respond with a success message
    httpd_resp_send(req, "WiFi configured successfully", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "WiFi configured successfully. Redirecting...", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;

    wifi_configured = true;
    return ESP_OK;
}

// Load WiFi credentials from flash memory
void load_wifi_credentials()
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err != ESP_OK) {
        printf("NVS flash initialization failed\n");
        return;
    }

    // Open NVS namespace
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        // If NVS namespace does not exist, create it
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
            printf("Error opening NVS handle\n");
            return;
        }
    }

    char wifi_name[32], wifi_password[64];
    size_t size = sizeof(wifi_name);
    //loaded_wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    // Read WiFi name from NVS
    err = nvs_get_str(nvs_handle, "wifi_name", wifi_name, &size);
    if (err == ESP_OK) {
        size = sizeof(wifi_password);
        // Read WiFi password from NVS
        err = nvs_get_str(nvs_handle, "wifi_password", wifi_password, &size);
        if (err == ESP_OK) {
            printf("WiFi Name: %s, WiFi Password: %s\n", wifi_name, wifi_password);
            wifi_configured = true;
             // Copy loaded credentials to global variable
            strcpy((char*)loaded_wifi_config.sta.ssid, wifi_name);
            strcpy((char*)loaded_wifi_config.sta.password, wifi_password);
        }
    }
    
    // Close NVS handle
    nvs_close(nvs_handle);
}
void load_mqtt_credentials()
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Initialize NVS
    err = nvs_flash_init();
    if (err != ESP_OK) {
        printf("NVS flash initialization failed\n");
        return;
    }

    // Open NVS namespace
    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        // If NVS namespace does not exist, create it
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err != ESP_OK) {
            printf("Error opening NVS handle\n");
            return;
        }
    }

    char mqtt_host_name[64];
    int mqtt_port;
    size_t size = sizeof(mqtt_host_name);
    //loaded_wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    // Read WiFi name from NVS
    err = nvs_get_str(nvs_handle, "mqtt_host_name", mqtt_host_name, &size);

    if (err == ESP_OK) {
        
        // Read WiFi password from NVS
        char portraw[6];
        memset(portraw, 0, sizeof(portraw));
        size = sizeof(portraw) - 1;
        err = nvs_get_str(nvs_handle, "mqtt_port", portraw, &size);
        if (err == ESP_OK) {
            // Parse the port string into an integer
            mqtt_port = atoi(portraw);
            printf("MQTT Host Name: %s, Port: %d\n", mqtt_host_name, mqtt_port);
            mqtt_configured = true;
            // implement set mqtt config here 
            
            strcpy(mconfig.mqtt_host, mqtt_host_name);
            mconfig.mqtt_port = mqtt_port;
            set_mqtt_runtime_config(&mconfig);
            set_mqtt_ok_to_run(true);
            mqtt_config_loaded = true;
        }
    }
    
    // Close NVS handle
    nvs_close(nvs_handle);
}

// Initialize NVS flash memory
void init_nvs()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

// Handler for processing the WiFi configuration
esp_err_t wifi_configuration_handler(httpd_req_t *req)
{
    char content[100];

    int content_length = httpd_req_get_url_query_len(req) + 1;
    if (content_length > sizeof(content))
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_req_get_url_query_str(req, content, content_length);

    char wifi_name[32], wifi_password[64];
    if (httpd_query_key_value(content, "wifi_name", wifi_name, sizeof(wifi_name)) != ESP_OK ||
        httpd_query_key_value(content, "wifi_password", wifi_password, sizeof(wifi_password)) != ESP_OK)
    {
        // Send custom response for a 400 error (Bad Request)
        httpd_resp_send(req, "Invalid parameters", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }
    // Save WiFi credentials to flash memory
    save_wifi_credentials(wifi_name, wifi_password);
    wifi_configured = true;
    //httpd_resp_send(req, "WiFi configured successfully", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "301 Moved Permanently");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Save WiFi credentials to flash memory
void save_mqtt_details(const char *mqtt_host_name, const char *mqtt_Port)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        printf("Error opening NVS handle\n");
        return;
    }
    err = nvs_set_str(nvs_handle, "mqtt_host_name",mqtt_host_name);
    if (err != ESP_OK)
    {
        printf("Error setting mqtt hostname\n");
    }    
    err = nvs_set_str(nvs_handle, "mqtt_port", mqtt_Port);
    if (err != ESP_OK)
    {
        printf("Error setting mqtt port\n");
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

// Handler for processing the WiFi configuration
esp_err_t mqtt_configuration_handler(httpd_req_t *req)
{
    httpd_uri_t *uri = req->uri;
    const char *url = uri->uri;
    ESP_LOGV(TAG, "MQTT Config Handler Called");    
    mqtt_configured = false;
    mqtt_config_loaded = false;
    bool failed = false;
    
    char content[100];
    
    // Get the length of the data received in the POST request    
    int content_length = req->content_len +1;
    int ret = httpd_req_recv(req, content, content_length);
    
    // Get the data received in the POST request
    httpd_req_get_url_query_str(req, content, content_length);        
    // content[content_length - 1] = '\0';
     
    char mqtt_host_name[64], mqtt_Port[12];
    ESP_LOGV(TAG, "Getting Post Key Value Pairs"); 
    if (httpd_query_key_value(content, "host_Name", mqtt_host_name, sizeof(mqtt_host_name)) != ESP_OK)        
    {
        // Send custom response for a 400 error (Bad Request)
        httpd_resp_send(req, "Invalid parameters in Host Name", HTTPD_RESP_USE_STRLEN);
        failed = true;
    }
    if (httpd_query_key_value(content, "mqtt_Port", mqtt_Port, sizeof(mqtt_Port)) != ESP_OK)        
    {
        // Send custom response for a 400 error (Bad Request)
        httpd_resp_send(req, "Invalid parameters in Port Number", HTTPD_RESP_USE_STRLEN);
        failed = true;
    }
    if(failed){
        return ESP_FAIL;
    }
    ESP_LOGV(TAG, "Saving MQTT settings"); 
    // // Save WiFi credentials to flash memory
    save_mqtt_details(mqtt_host_name, mqtt_Port);
    mqtt_configured = true;
    //httpd_resp_send(req, "Mqtt configured successfully", HTTPD_RESP_USE_STRLEN);
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "MQTT configured successfully. Redirecting...", HTTPD_RESP_USE_STRLEN);
    
    return ESP_OK;
    
}

esp_err_t api_command_handler(httpd_req_t *req) {
    char content[200];

    httpd_uri_t *uri = req->uri;
    const char *url = uri->uri;

    char command_argument[32]; // Adjust the size according to your requirements
    
    int content_length = req->content_len +1;

    int ret = httpd_req_recv(req, content, content_length);

    httpd_req_get_url_query_str(req, content, content_length);

    content[content_length - 1] = '\0';
       // Log the received payload
    ESP_LOGW(TAG, "Payload received: %s", content);
    ESP_LOGW(TAG, "Payload Argument %s", command_argument);

    // Get the value associated with the key "command"
    if (httpd_query_key_value(content, "command", command_argument, sizeof(command_argument)) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing command parameter");
        return ESP_FAIL;
    }
    
    
    // Compare the received command and execute the corresponding action
    if (strcmp(command_argument, "restart") == 0) {
        ESP_LOGI(TAG, "Restart Called.");
        
        httpd_resp_set_status(req, "303 See Other");
        httpd_resp_set_hdr(req, "Location", "/");
        httpd_resp_send(req, "Restart Called", HTTPD_RESP_USE_STRLEN);

        esp_restart();
        // Execute restart command
        // You can call your restart function here
    } else if (strcmp(command_argument, "startWifiClient") == 0) {
        // Execute startWifiClient command
        // You can call your start Wifi client function here
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Unknown command");
        return ESP_FAIL;
    }

    // Send a response indicating success
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

// esp_err_t mqtt_configuration_handler_uri(httpd_req_t *req)
// {
//     if(!mqtt_configured)
//     {
//         load_mqtt_credentials();
//     }
//     if(debugsEnabled){
//     ESP_LOGV(TAG, "MQTT2 Config Handler Called");
//     }
//     // Check if MQTT is configured
//     if (!mqtt_configured) {
//         // If MQTT is not configured, return an error response
//         httpd_resp_send_404(req);
//         return ESP_FAIL;
//     }

//     // Create a JSON object to hold MQTT configuration
//     cJSON *root = cJSON_CreateObject();
//     cJSON_AddStringToObject(root, "mqtt_host", mconfig.mqtt_host);
//     cJSON_AddNumberToObject(root, "mqtt_port", mconfig.mqtt_port);
    
//     // Convert JSON object to string
//     char *json_string = cJSON_Print(root);
//     cJSON_Delete(root); // Clean up cJSON object
    
//     // Send JSON string as response
//     httpd_resp_set_type(req, "application/json");
//     httpd_resp_send(req, json_string, strlen(json_string));
    
//     // Free dynamically allocated memory
//     free(json_string);
    
//     return ESP_OK;
// }

//mqtt_configuration_handler
// Handler for processing the WiFi configuration
esp_err_t wifi_status_post_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateArray();

    // Add scanned WiFi networks to the JSON array
    for (int i = 0; i < ap_count; i++) {
        cJSON *network_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(network_obj, "ssid", (const char *)wifi_ap_records[i].ssid);
        cJSON_AddNumberToObject(network_obj, "rssi", wifi_ap_records[i].rssi);
        cJSON_AddItemToArray(root, network_obj);
    }

    // Convert JSON array to string
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root); // Clean up cJSON object

    // Send JSON string as response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));

    // Free dynamically allocated memory
    free(json_string);
    if(!wifi_scan_running){
        wifi_scan();
    }
    
    return ESP_OK;
}
//MQTT Status
esp_err_t mqtt_status_get_handler(httpd_req_t *req)
{
    if(!mqtt_config_loaded){
        load_mqtt_credentials();
        
    }
    // Sample MQTT configuration data
    const char *mqtt_host = mconfig.mqtt_host;
    int mqtt_port = mconfig.mqtt_port;

    // Create a JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mqtt_host", mqtt_host);
    cJSON_AddNumberToObject(root, "mqtt_port", mqtt_port);
    
    // Convert JSON object to string
    char *json_string = cJSON_Print(root);
    cJSON_Delete(root); // Clean up cJSON object
    
    // Send JSON string as response
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    // Free dynamically allocated memory
    free(json_string);
    
    return ESP_OK;
}

// Start the Access Point (AP) for WiFi configuration
void start_wifi_ap()
{
      // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ipInfo;
    
    IP4_ADDR(&ipInfo.ip, 192,168,4,1);
    IP4_ADDR(&ipInfo.gw, 192,168,4,1); // do not advertise as a gateway router
    IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
    esp_netif_dhcps_stop(wifiAP);
    esp_netif_set_ip_info(wifiAP, &ipInfo);
    esp_netif_dhcps_start(wifiAP);

    // Initialize Wi-Fi stack
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32_Configuration",
            .ssid_len = 0,
            .password = "esp32config",
            .channel = 0,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100,
            
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_connected = true;
}

void start_http_server(){
  httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t wifi_provisioning_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = wifi_provisioning_handler,
        };        

        httpd_uri_t wifi_configuration_uri = {
            .uri = "/configure",
            .method = HTTP_POST,
            .handler = wifi_configuration_post_handler,
            .user_ctx = NULL
        };
        httpd_uri_t mqtt_configuration_handler_uri = {
            .uri = "/mqttconfigure",
            .method = HTTP_POST,
            .handler = mqtt_configuration_handler,
            .user_ctx = NULL
        };
        

        httpd_uri_t mqtt_status_handler_uri = {
            .uri = "/mqtt_status",
            .method = HTTP_GET,
            .handler = mqtt_status_get_handler,
            .user_ctx = NULL
        };
        httpd_uri_t wifi_status_uri = {
            .uri = "/wifi_networks",
            .method = HTTP_GET,
            .handler = wifi_status_post_handler, // mqtt_configuration_handler
            .user_ctx = NULL
        };

        httpd_uri_t command_api_uri = {
            .uri = "/api/command",
            .method = HTTP_POST,
            .handler = api_command_handler, // mqtt_configuration_handler
            .user_ctx = NULL
        };
        //api_command_handler

        httpd_register_uri_handler(server, &wifi_configuration_uri);
        httpd_register_uri_handler(server, &wifi_provisioning_uri);
        httpd_register_uri_handler(server, &mqtt_configuration_handler_uri);
        httpd_register_uri_handler(server, &mqtt_status_handler_uri);
        httpd_register_uri_handler(server, &wifi_status_uri);
        httpd_register_uri_handler(server, &command_api_uri);
        
    }
}


void start_wifi_client()
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Initialize Wi-Fi stack
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &loaded_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
   
    //EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    if (bits & WIFI_CONNECTED_BIT)
    {
          printf("Connected to Wi-Fi\n");

        // Retrieve and print IP address
        esp_netif_ip_info_t ip_info;  
               
        // ESP_ERROR_CHECK(esp_netif_get_ip_info(IP_EVENT_STA_GOT_IP, &ip_info));
        // printf("IP Address: %s\n", ip4addr_ntoa(&ip_info.ip));

        // Retrieve and print MAC address
        uint8_t mac_addr[6];
        ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac_addr));
        printf("MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac_addr[0], mac_addr[1], mac_addr[2],
               mac_addr[3], mac_addr[4], mac_addr[5]);


    }
    else
    {
        printf("Failed to connect to Wi-Fi\n");

        // Troubleshooting steps
        wifi_sta_info_t wifi_info;
        ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&wifi_info));
        
        printf("Wi-Fi RSSI: %d\n", wifi_info.rssi);

        // Check if Wi-Fi is in a low-power state
        wifi_ps_type_t ps_type;
        ESP_ERROR_CHECK(esp_wifi_get_ps(&ps_type));
        printf("Wi-Fi power save mode: %d\n", ps_type);

        // Check if Wi-Fi is in a disconnected state
        // wifi_conn_status_t conn_status;
        // ESP_ERROR_CHECK(esp_wifi_get_connection_status(&conn_status));
        //printf("Wi-Fi connection status: %d\n", conn_status);      
    }
}

// Start the WiFi provisioning server
void start_wifi_provisioning_server()
{
  // Initialize NVS
    init_nvs();

    // Load WiFi credentials from flash memory
    load_wifi_credentials();

    // // Start WiFi provisioning server
     if (!wifi_configured)
     {
        printDebug("Debug message: Wifi Not Configured, Start AP");
    //     // Start AP for WiFi configuration
         start_wifi_ap();
         start_http_server();
     }
     else
     {
        printDebug("Debug message: Wifi Configured, Start wifi");
        // Start HTTP server for provisioning page
         start_wifi_client();

         printDebug("Debug message: Wifi Configured, Start web server");
         start_http_server();
     }

    // Perform WiFi scanning
    if(wifi_connected){
        if(!wifi_scan_running)
        {
            
            wifi_scan();
        }
        
    }
    
}
