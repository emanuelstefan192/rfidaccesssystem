#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

/** DEFINES **/


/** GLOBALS **/

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static int s_retry_num = 0;

// task tag
static const char* TAG = "WIFI";
/** FUNCTIONS **/

//event handler for wifi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAX_FAILURES)
        {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
            s_retry_num++;
        }
        else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
        }
    }
}

//event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }

}

// connect to wifi and return the result
esp_err_t connect_wifi()
{
    int status = WIFI_FAILURE;

    /** INITIALIZE ALL THE THINGS **/
    //initialize the esp network interface
    ESP_ERROR_CHECK(esp_netif_init());

    //initialize default esp event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //create wifi station in the wifi driver
    esp_netif_create_default_wifi_sta();

    //setup wifi station with the default wifi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /** EVENT LOOP CRAZINESS **/
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &wifi_handler_event_instance));

    esp_event_handler_instance_t got_ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &ip_event_handler,
        NULL,
        &got_ip_event_instance));

    /** START THE WIFI DRIVER **/
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "DIGI-mJz9",
            .password = "uHrCuzah3F",
         .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA initialization complete");

    /** NOW WE WAIT **/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
        WIFI_SUCCESS | WIFI_FAILURE,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_SUCCESS) {
        ESP_LOGI(TAG, "Connected to ap");
        status = WIFI_SUCCESS;
    }
    else if (bits & WIFI_FAILURE) {
        ESP_LOGI(TAG, "Failed to connect to ap");
        status = WIFI_FAILURE;
    }
    else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}

esp_err_t send_sql_query(const char* server_ip, int port, const char* query, int* response, size_t response_size)
{
    struct sockaddr_in serverInfo = { 0 };
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_addr.s_addr = inet_addr(server_ip);
    serverInfo.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }

    if (connect(sock, (struct sockaddr*)&serverInfo, sizeof(serverInfo)) != 0) {
        ESP_LOGE(TAG, "Failed to connect to server");
        close(sock);
        return ESP_FAIL;
    }

    // Create JSON message with query
    cJSON* json = cJSON_CreateObject();
    cJSON* json_query = cJSON_CreateString(query);
    cJSON_AddItemToObject(json, "query", json_query);

    char* json_string = cJSON_Print(json);

    // Send query length first, then query
    uint32_t query_len = strlen(json_string);
    send(sock, &query_len, sizeof(query_len), 0);
    send(sock, json_string, query_len, 0);

    // Receive response length
    uint32_t response_len;
    if (recv(sock, &response_len, sizeof(response_len), 0) <= 0) {
        ESP_LOGE(TAG, "Failed to receive response length");
        close(sock);
        cJSON_Delete(json);
        free(json_string);
        return ESP_FAIL;
    }

    // Receive response data
    if (response_len < response_size) {
        int received = recv(sock, response, response_len, 0);
        response[received] = '\0';
        ESP_LOGI(TAG, "Query response: %s", response);
    }

    close(sock);
    cJSON_Delete(json);
    free(json_string);
    return ESP_OK;
}
