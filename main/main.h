#ifndef __MAIN__H__
#define __MAIN__H__
#include "Arduino.h"

uint8_t detect_chain(void);
int chr2hex(char ch);
uint32_t binArrayToInt(uint8_t * arr, int len);
uint32_t binStringToInt(String str);
void clear_serial_rx_buf();
uint32_t getInteger(int num_bytes, const char * message);
int binStrToBinArray(uint8_t * arr, int arrSize, String str ,int strSize);
int hexStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize);
int decStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize);
int intToBinArray(uint8_t * arr, uint32_t n, uint16_t len);
void fetchNumber();
uint32_t parseNumber(uint8_t * dest, uint16_t size, const char * message);
void reset_tap(void);
void insert_dr(uint8_t * dr_in, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out);
void insert_ir(uint8_t * ir_in, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out);
void clear_reg(uint8_t * reg, uint16_t len);
uint16_t detect_dr_len(uint8_t * instruction, uint8_t ir_len);
void discovery(uint16_t maxDRLen, uint32_t last, uint32_t first, uint8_t * ir_in, uint8_t * ir_out);
void advance_tap_state(uint8_t next_state);
char serialEvent(char character);
void printArray(uint8_t * arr, uint16_t len);
void sendDataToHost(uint8_t * buf, uint16_t chunk_size);
void printMenu();
#endif /* __MAIN_H__ */