#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

esp_err_t servo_init(void);
esp_err_t servo_set_angle(float angle);
esp_err_t servo_set_pulse_width(uint32_t pulse_us);

// Servo configuration
#define SERVO_PIN           21     // GPIO pin connected to servo signal wire
#define SERVO_TIMER         LEDC_TIMER_0   // Timer for PWM
#define SERVO_CHANNEL       LEDC_CHANNEL_0 // PWM channel
#define SERVO_FREQ_HZ       50             // 50Hz frequency for servo control

// Pulse width constants for SG90 servo (in microseconds)
#define SERVO_MIN_PULSE_US  500    // 0 degrees (0.5ms pulse)
#define SERVO_MAX_PULSE_US  2500   // 180 degrees (2.5ms pulse)
#define SERVO_MID_PULSE_US  1500   // 90 degrees (1.5ms pulse)

// PWM resolution
#define SERVO_RESOLUTION    LEDC_TIMER_16_BIT
#define SERVO_MAX_DUTY      ((1 << 16) - 1)  // 2^16 - 1 for 16-bit resolution

#endif
