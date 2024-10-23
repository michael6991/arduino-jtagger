#ifndef __MAIN__H__
#define __MAIN__H__

#include "Arduino.h"

/**  
* If you don't wish to see debug info such as TAP state transitions put 0.
* Otherwise assign 1.
*/
#define DEBUGTAP (0)
/** 
* If you don't wish to see debug info regarding user input via serial port put 0.
* Otherwise assign 1.
*/
#define DEBUGSERIAL (0)


// Define JTAG pins as you wish
#define TCK 7
#define TMS 8
#define TDI 9
#define TDO 10
#define TRST 11


#define MAX_DR_LEN 32

#ifndef MAX_DR_LEN
#define MAX_DR_LEN 256    // usually BSR
#endif


/*	Choose a half-clock cycle delay	*/
// half clock cycle
// #define HC delay(1);
// or
#define DELAY_US (100) // delay in microseonds for a half-clock cycle (HC) to drive TCK.
#define HC delayMicroseconds(DELAY_US);

// A more precise way to delay a half clock cycle:
/*
#define HC \
{ \
    __asm__ __volatile__("nop \n\t \
                          nop \n\t \
                          nop \n\t \
                          nop \n\t \
                          " : :);  \
}
Notice, that you also need to override the digitalWrite and digitalRead
functions with an appropriate assembly in order to reach the desired JTAG speeds.
*/
 
enum TapState
{
    TEST_LOGIC_RESET, RUN_TEST_IDLE,
    SELECT_DR, CAPTURE_DR, SHIFT_DR, EXIT1_DR, PAUSE_DR, EXIT2_DR, UPDATE_DR,
    SELECT_IR, CAPTURE_IR, SHIFT_IR, EXIT1_IR, PAUSE_IR, EXIT2_IR, UPDATE_IR
};


uint8_t detect_chain();
int chr2hex(char ch);
String getString(const char * message);
char getCharacter(const char * message);
void fetchNumber(const char * message);
uint32_t getInteger(int num_bytes, const char * message);
uint32_t parseNumber(uint8_t * dest, uint16_t size, const char * message);
uint32_t binStringToInt(String str);
uint32_t binArrayToInt(uint8_t * arr, int len);
int binStrToBinArray(uint8_t * arr, int arrSize, String str ,int strSize);
int hexStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize);
int decStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize);
int intToBinArray(uint8_t * arr, uint32_t n, uint16_t len);
uint16_t detect_dr_len(uint8_t * instruction, uint8_t ir_len);
void discovery(uint16_t maxDRLen, uint32_t last, uint32_t first, uint8_t * ir_in, uint8_t * ir_out);
void reset_tap(void);
void insert_dr(uint8_t * dr_in, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out);
void insert_ir(uint8_t * ir_in, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out);
void clear_reg(uint8_t * reg, uint16_t len);
void flush_ir_dr(uint8_t * ir_reg, uint8_t * dr_reg, uint16_t ir_len, uint16_t dr_len);
void advance_tap_state(uint8_t next_state);
char serialEvent(char character);
void printArray(uint8_t * arr, uint16_t len);
void sendDataToHost(uint8_t * buf, uint16_t chunk_size);
void clear_serial_rx_buf();
void print_main_menu();
void print_welcome();

#endif /* __MAIN_H__ */