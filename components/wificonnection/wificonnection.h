#ifndef WIFICONNECTION_H
#define WIFICONNECTION_H
#include "esp_event.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdbool.h>

#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define TCP_SUCCESS 1 << 0
#define TCP_FAILURE 1 << 1
#define MAX_FAILURES 10

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
esp_err_t connect_wifi();
esp_err_t send_sql_query(const char* server_ip, int port, const char* query, char* response, size_t response_size);




#endif
