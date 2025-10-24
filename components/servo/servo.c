#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "servo.h"

esp_err_t servo_init(void) {
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = SERVO_TIMER,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) return err;

    ledc_channel_config_t channel_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = SERVO_CHANNEL,
        .timer_sel = SERVO_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = SERVO_PIN,
        .duty = 0,
        .hpoint = 0
    };
    return ledc_channel_config(&channel_config);
}
esp_err_t servo_set_angle(float angle) {
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    uint32_t pulse_us = SERVO_MIN_PULSE_US +
        (angle / 180.0) * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US);

    uint32_t duty = (pulse_us * SERVO_MAX_DUTY) / 20000;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty);
    if (err != ESP_OK) return err;

    return ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL);
}

esp_err_t servo_set_pulse_width(uint32_t pulse_us) {
    if (pulse_us < SERVO_MIN_PULSE_US) pulse_us = SERVO_MIN_PULSE_US;
    if (pulse_us > SERVO_MAX_PULSE_US) pulse_us = SERVO_MAX_PULSE_US;

    uint32_t duty = (pulse_us * SERVO_MAX_DUTY) / 20000;

    esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty);
    if (err != ESP_OK) return err;

    return ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL);
}
