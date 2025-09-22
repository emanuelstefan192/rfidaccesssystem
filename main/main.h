#ifndef MAIN_H
#define MAIN_H

#define MFRC522_RST_PIN 5
#define MFRC522_CS_PIN  22


void uid_to_hex_string(uint8_t* uid, uint8_t uid_size, char* hex_string);
#endif
