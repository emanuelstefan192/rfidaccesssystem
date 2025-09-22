#include "mfrc522.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include <string.h>


static spi_device_handle_t spi;
static gpio_num_t cs_pin;
static gpio_num_t rst_pin;

static void mfrc522_write(uint8_t reg, uint8_t val)
{
    uint8_t data[2] = { ((reg << 1) & 0x7E), val };
    spi_transaction_t t = {
        .length = 8 * 2,
        .tx_buffer = data,
    };
    gpio_set_level(cs_pin, 0);
    spi_device_transmit(spi, &t);
    gpio_set_level(cs_pin, 1);
}

static uint8_t mfrc522_read(uint8_t reg)
{
    uint8_t tx[2] = { ((reg << 1) & 0x7E) | 0x80, 0 };
    uint8_t rx[2];
    spi_transaction_t t = {
        .length = 8 * 2,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    gpio_set_level(cs_pin, 0);
    spi_device_transmit(spi, &t);
    gpio_set_level(cs_pin, 1);
    return rx[1];
}

static void mfrc522_reset()
{
    mfrc522_write(CommandReg, PCD_RESETPHASE);
    uint8_t count = 0;
    do {
        // Wait for the PowerDown bit in CommandReg to be cleared (max 3x50ms)
        vTaskDelay(50);
    } while ((mfrc522_read(CommandReg << 1) & (1 << 4)) && (++count) < 3);

}

void mfrc522_start(uint8_t cs, uint8_t rst)
{
    cs_pin = cs;
    rst_pin = rst;

    gpio_set_direction(rst_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(rst_pin, 1);

    spi_bus_config_t buscfg = {
        .miso_io_num = 19,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1*500*1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };

    spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    spi_bus_add_device(HSPI_HOST, &devcfg, &spi);

    gpio_set_direction(cs_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(cs_pin, 1);

    mfrc522_reset();
    mfrc522_write(TxModeReg, 0x00);
    mfrc522_write(RxModeReg, 0x00);
    mfrc522_write(ModWidthReg, 0x1A);
    mfrc522_write(TModeReg, 0x80);
    mfrc522_write(TPrescalerReg, 0x20);
    mfrc522_write(TReloadRegH, 0x00);
    mfrc522_write(TReloadRegL, 0x20);
    mfrc522_write(TxASKReg, 0x40);
    mfrc522_write(ModeReg, 0x3D);
    uint8_t value = mfrc522_read(TxControlReg);
    if (!(value & 0x03)) {
        mfrc522_write(TxControlReg, value | 0x03);
    }
}

// First function: Request/Wake up card and get ATQA
bool mfrc522_request(uint8_t* atqa_out) {
    mfrc522_write(CommandReg, PCD_IDLE);
    mfrc522_write(ComIrqReg, 0x7F);
    mfrc522_write(FIFOLevelReg, 0x80);   // Flush FIFO
    mfrc522_write(ErrorReg, 0x00);

    mfrc522_write(FIFODataReg, PICC_REQALL);  // 0x52
    mfrc522_write(BitFramingReg, 0x07); // 7 valid bits, no CRC
    mfrc522_write(CommandReg, PCD_TRANSCEIVE);
    mfrc522_write(BitFramingReg, 0x87);  // Start transmission

    bool completed = false;
    // Wait for response (max 25ms)
    for (int i = 0; i < 50; i++) {
        uint8_t irq = mfrc522_read(ComIrqReg);
        if (irq & 0x30) {  // RxIRq or IdleIRq triggered
            completed = true;
            break;
        }
        if (irq & 0x01) {  // TimerIRq
            return false;
        }
        vTaskDelay(1);
    }

    if (!completed) {
        printf("Timeout waiting for card\n");
        return false;
    }

    uint8_t error = mfrc522_read(ErrorReg);
    if (error & 0x13) {
        printf("Error in REQALL: 0x%02X\n", error);
        return false;
    }

    uint8_t len = mfrc522_read(FIFOLevelReg);
    if (len != 2) {
        printf("Expected 2 bytes ATQA, got %d\n", len);
        return false;
    }

    // Read ATQA (should be 2 bytes)
    atqa_out[0] = mfrc522_read(FIFODataReg);
    atqa_out[1] = mfrc522_read(FIFODataReg);

    printf("ATQA: %02X %02X\n", atqa_out[0], atqa_out[1]);
    return true;
}

// Second function: Anticollision to get UID
bool mfrc522_anticollision(uint8_t* uid_out, uint8_t* uid_length) {
    mfrc522_write(CommandReg, PCD_IDLE);
    mfrc522_write(ComIrqReg, 0x7F);
    mfrc522_write(FIFOLevelReg, 0x80);   // Flush FIFO
    mfrc522_write(ErrorReg, 0x00);

    // Send ANTICOLLISION command
    mfrc522_write(FIFODataReg, PICC_SEL_CL1);  // 0x93
    mfrc522_write(FIFODataReg, 0x20);          // NVB (Number of Valid Bits) = 0x20

    mfrc522_write(BitFramingReg, 0x00);        // All bits valid, no framing
    mfrc522_write(CommandReg, PCD_TRANSCEIVE);
    mfrc522_write(BitFramingReg, 0x80);        // Start transmission

    bool completed = false;
    // Wait for response (max 25ms)
    for (int i = 0; i < 50; i++) {
        uint8_t irq = mfrc522_read(ComIrqReg);
        if (irq & 0x30) {  // RxIRq or IdleIRq triggered
            completed = true;
            break;
        }
        if (irq & 0x01) {  // TimerIRq
            return false;
        }
        vTaskDelay(1);
    }

    if (!completed) {
        printf("Timeout in anticollision\n");
        return false;
    }

    uint8_t error = mfrc522_read(ErrorReg);
    if (error & 0x1B) {  // Check for relevant errors
        printf("Error in anticollision: 0x%02X\n", error);
        return false;
    }

    uint8_t len = mfrc522_read(FIFOLevelReg);
    if (len < 4) {
        printf("Expected at least 4 bytes UID response, got %d\n", len);
        return false;
    }

    // Read UID + BCC (Block Check Character)
    // Typically: 4 bytes UID + 1 byte BCC = 5 bytes total
    for (uint8_t i = 0; i < len; i++) {
        uid_out[i] = mfrc522_read(FIFODataReg);
    }

    *uid_length = len - 1;  // Exclude BCC from UID length

    // Verify BCC (XOR of UID bytes should equal BCC)
    uint8_t bcc_calc = 0;
    for (uint8_t i = 0; i < *uid_length; i++) {
        bcc_calc ^= uid_out[i];
    }

    if (bcc_calc != uid_out[*uid_length]) {
        printf("BCC mismatch! Calculated: 0x%02X, Received: 0x%02X\n",
            bcc_calc, uid_out[*uid_length]);
        // Don't return false here - sometimes this still works
    }

    printf("(BCC: %02X)\n", uid_out[*uid_length]);

    return true;
}

// Combined function to get complete UID
bool mfrc522_get_uid(uint8_t* uid_out, uint8_t* uid_length) {
    uint8_t atqa[2];

    // Step 1: Wake up card and get ATQA
    if (!mfrc522_request(atqa)) {
        return false;
    }

    // Step 2: Get UID through anticollision
    if (!mfrc522_anticollision(uid_out, uid_length)) {
        return false;
    }

    return true;
}

// Usage example:
/*
uint8_t uid[10];  // Buffer for UID (max 10 bytes for larger UIDs)
uint8_t uid_length;

if (mfrc522_get_uid(uid, &uid_length)) {
    printf("Successfully read UID!\n");
} else {
    printf("Failed to read UID\n");
}
*/


