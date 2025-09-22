#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mfrc522.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "servo.h"
#include "wificonnection.h"
#include "main.h"


void app_main(void)
{
    esp_err_t status = WIFI_FAILURE;

    //initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // connect to wireless AP
    status = connect_wifi();
    if (WIFI_SUCCESS != status)
    {
        ESP_LOGI(TAG, "Failed to associate to AP, dying...");
        return;
    }
    
    uint8_t uidright[10] = { 0xA2, 0xFC, 0x4F, 0x05}; 
    printf("Initializing RC522 RFID reader...\n");

    mfrc522_start(MFRC522_CS_PIN, MFRC522_RST_PIN);
    esp_err_t ret = servo_init();
    if (ret != ESP_OK) {
        printf("Servo initialization failed: %s\n", esp_err_to_name(ret));
        return;
    }
    while (1) {
        uint8_t uid[10];
        uint8_t uid_size = 0;
        bool access_denied = false;
        if (mfrc522_get_uid(uid, &uid_size)) {
            if (uid_size > 0) {
                char uid_hex[32];
                uid_to_hex_string(uid, uid_size, uid_hex);

                printf("Card detected! UID: %s\n", uid_hex);
                static char query[256];
                sprintf(query, "SELECT ID FROM UID_CARD WHERE UID = '%s';", uid_hex);
                const char* server_ip = '192.168.0.1';
                int port = 8080;
                char response[1024];
                size_t response_size = sizeof(response);
                send_sql_query(server_ip, port, query, response, response_size);

                if (strlen(response) > 0) {
                    printf("Access granted!\n");
                    servo_set_angle(180);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    servo_set_angle(0);
                }
                else {
                    printf("Access denied!\n");
                    // Optional: Add denied access logging
                    // log_access_attempt(uid_hex, false);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void uid_to_hex_string(uint8_t* uid, uint8_t uid_size, char* hex_string) {
    for (int i = 0; i < uid_size; i++) {
        sprintf(hex_string + (i * 3), "%02X", uid[i]);
        if (i < uid_size - 1) {
            hex_string[i * 3 + 2] = ':';
        }
    }
    hex_string[uid_size * 3 - 1] = '\0'; // Remove last colon
}
