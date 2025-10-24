#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mfrc522.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "servo.h"
#include "wificonnection.h"
#include "main.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char* TAG = "MAIN";


void app_main(void)
{
    

    printf("Initializing RC522 RFID reader...\n");

    mfrc522_start(MFRC522_CS_PIN, MFRC522_RST_PIN);
    esp_err_t rett = servo_init();
    if (rett != ESP_OK) {
        printf("Servo initialization failed: %s\n", esp_err_to_name(rett));
        return;
    }
    while (1) {
        uint8_t uid[10];
        uint8_t uid_size = 0;
        if (mfrc522_get_uid(uid, &uid_size)) {
            if (uid_size > 0) {
                char uid_hex[32];
                uid_to_hex_string(uid, uid_size, uid_hex);

                printf("Card detected! UID: %s\n", uid_hex);

                if (http_get_uid(uid_hex) == ESP_OK) {
                    printf("Access granted!\n");
                    servo_set_angle(180);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    servo_set_angle(0);
                }
                else {
                    printf("Access denied!\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void uid_to_hex_string(uint8_t* uid, uint8_t uid_size, char* hex_string) {
    for (int i = 0; i < uid_size; i++) {
        sprintf(hex_string + (i * 2), "%02X", uid[i]);
    }
    hex_string[uid_size * 2] = '\0';
}
