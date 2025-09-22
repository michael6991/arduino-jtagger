#ifndef __MAIN__H__
#define __MAIN__H__

#include "Arduino.h"

/**
 * Error return code definitions
 */
#define OK                       0
#define ERR_GENERAL              1
#define ERR_BAD_CONVERSION       2
#define ERR_OUT_OF_BOUNDS        3
#define ERR_BAD_IDCODE           4
#define ERR_BAD_PREFIX_OR_SUFFIX 5
#define ERR_BAD_TAP_STATE        6
#define ERR_UNVALID_IR_OR_DR_LEN 7
#define ERR_TDO_STUCK_AT_0       8
#define ERR_TDO_STUCK_AT_1       9

/**  
* If you don't wish to see debug info such as TAP state transitions put 0.
* Otherwise assign 1.
*/
#define DEBUGTAP 0

/** 
* If you don't wish to see debug info regarding user input via serial port put 0.
* Otherwise assign 1.
*/
#define DEBUGSERIAL 1

/**
 * If 1 then print each time TAP is reset
 */
#define PRINT_RESET_TAP 0


// Define JTAG pins as you wish
#define TCK 7
#define TMS 8
#define TDI 9
#define TDO 10
#define TRST 11

#define MAX_DR_LEN 512
#ifndef MAX_DR_LEN
#define MAX_DR_LEN 1024 // usually the BSR and might be larger than that
#endif

#define MANY_ONES 100

/*	Choose a half-clock cycle delay	*/
// half clock cycle
// #define HC delay(1);
// or
#define DELAY_US 100 // delay in microseonds for a half-clock cycle (HC) to drive TCK.
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
 
typedef enum TapState
{
    TEST_LOGIC_RESET, RUN_TEST_IDLE,
    SELECT_DR, CAPTURE_DR, SHIFT_DR, EXIT1_DR, PAUSE_DR, EXIT2_DR, UPDATE_DR,
    SELECT_IR, CAPTURE_IR, SHIFT_IR, EXIT1_IR, PAUSE_IR, EXIT2_IR, UPDATE_IR
}tap_state;


/**
 * @brief Detects the the existence of a chain and checks the ir length.
 * @param out An integer that represents the length of the instructions.
 * Most certainly less than 255 bits.
 */
int detect_chain(uint8_t* out);

/**
 * @brief Convert char into a hexadecimal number
 * @param ch Character to convert
 * @return Hexadecimal representation of the char, or error code if out of bounds.
 */
int chr2hex(char ch);

/**
 * @brief Busy waiting with a while loop for a user input.
 * Prior to loop, the function clears the rx serial buffer
 * and notifys the user that we are ready for input.
 */
void notify_input_and_busy_wait_for_serial_input(const char* message);

/**
 * @brief Used for various tasks where an input character needs to be received
 * from the user via the serial port.
 * @param message Message for the user.
 * @return char input from user.
 */
char getCharacter(const char* message);

/**
 * @brief Get string from the user via the serial port.
 * @return String input from user.
 */
String getString(const char* message);

/**
 * @brief Used for various tasks where a number needs to be received
 * from the user via the serial port.
 */
void fetchNumber(const char* message);

/**
 * @brief Used for various tasks where a hexadecimal number needs to be received
 * from the user via the serial port.
 * @param num_bytes The amount of hexadecimal characters to receive.
 * @param message Message for the user.
 * @return uint32_t representation of the hexadecimal number from user.
 */
uint32_t getInteger(int num_bytes, const char* message);

/**
 *
 * @brief Receive a number from the user in different formats: 0x, 0b, or decimal.
 * With an option to return the fetched number in a uint32 format.
 * @param message A message for the user.
 * @param dest Destination array. Will contain user's input value.
 * @param size Size (in bytes) of the destination array.
 * @param out The constructed number.
 */
int parseNumber(uint8_t* dest, uint16_t size, const char* message, uint32_t* out);

/**
 * @brief Convert the content of a String object into an integer number,
 * where every byte (char) represents a single bit. 
 * Together the string represents a binary number.
 * First element of the string is the LSB.
 * Max length of the given string is 32.
 * @param str Pointer to the array of bits.
 * @param out An unsigned integer that represents the the value of the array bits.
 */
int binStringToInt(String str, uint32_t* out);

/**
 * @brief Convert array of bytes into an integer number, where every byte
 * represents a single bit. Together the array represents a binary number.
 * First element of array is the LSB. 
 * Max length of the given array is 32.
 * @param arr Pointer to the array of bits.
 * @param len Integer that represents the length of the binary array.
 * @param out Pointer to result integer that represents the the value of the bits array.
 */
int binArrayToInt(uint8_t* arr, int len, uint32_t* out);

/**
 * @brief Convert a binary string into a bytes array arr that will represent
 * the binary value just as string. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the binary digits. (LSB is the first char of string).
 * @param strSize Length of the string object.
 * @return ok or error code.
 */
int binStrToBinArray(uint8_t* arr, int arrSize, String str ,int strSize);

/**
 * @brief Convert a hexadecimal string into a bytes array arr that will represent
 * the binary value of the hexadecimal array. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the hexadecimal digits.
 * (LSB is the first char of string).
 * @param strSize Length of the string object.
 * @return ok or error code
 */
int hexStrToBinArray(uint8_t* arr, int arrSize, String str, int strSize);

/**
 * @brief Convert base 10 decimal string into a bytes array arr that will represent
 * the binary value of the decimal array. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the decimal digits.
 * (LSB is the first char of string).
 * @param strSize Length of the string object.
 */
int decStrToBinArray(uint8_t* arr, int arrSize, String str, int strSize);

/**
 * @brief Convert an integer number n into a bytes array arr that will represent
 * the binary value of n. each element (byte) in arr represent a bit.
 * First element is the LSB. Largest number is a 32 bit number.
 * @param arr Pointer to the output array with the binary values.
 * @param len Length of the output array in bytes. (max size 32)
 * @param n The integer to convert.
 */
int intToBinArray(uint8_t* arr, uint32_t n, uint16_t len);

/**
 * 
 * @brief Return to TEST LOGIC RESET state of the TAP FSM.
 * Apply 5 TCK cycles accompanied with TMS logic state 1.
 */
void reset_tap();

/**
*	@brief Insert data of length dr_len to DR, and end the interaction
*	in the state end_state which can be one of the following:
*	TLR, RTI.
*	@param dr_in Pointer to the input data array. (bytes array)
*	@param dr_len Length of the register currently connected between tdi and tdo.
*	@param end_state TAP state after dr inseration.
*	@param dr_out Pointer to the output data array. (bytes array)
*/
void insert_dr(uint8_t* dr_in, uint8_t dr_len, uint8_t end_state, uint8_t* dr_out);

/**
*	@brief Insert data of length ir_len to IR, and end the interaction
*	in the state end_state which can be one of the following:
*	TLR, RTI, SelectDR.
*	@param ir_in Pointer to the input data array. Bytes array, where each
*	@param ir_len Length of the register currently connected between tdi and tdo.
*	@param end_state TAP state after dr inseration.
*	@param ir_out Pointer to the output data array. (bytes array)
*/
void insert_ir(uint8_t* ir_in, uint8_t ir_len, uint8_t end_state, uint8_t* ir_out);

/**
 * @brief Fill the register with zeros
 * @param reg Pointer to the register to flush.
 */
void clear_reg(uint8_t* reg, uint16_t len);

/**
 * @brief Clean the IR and DR together
 */
void flush_ir_dr(uint8_t* ir_reg, uint8_t* dr_reg, uint16_t ir_len, uint16_t dr_len);

/**
 * @brief Find out the dr length of a specific instruction.
 * Make sure that current state is TLR prior this calling this function.
 * @param instruction Pointer to the bytes array that contains the instruction.
 * @param ir_len The length of the IR. (Needs to be know prior to function call).
 * @param process_ticks Number of TCK ticks to wait for the inserted instruction to "process in".
 * @return Counter that represents the size of the DR. Or 0 if didn't find
 * a valid size. (DR may not be implemented or some other reason).
 */
uint32_t detect_dr_len(uint8_t* instruction, uint8_t ir_len, uint32_t process_ticks);

/**
 * @brief Similarly to discovery command in urjtag, performs a brute force search
 * of each possible values of the IR register to get its corresponding DR leght in bits.
 * Test Logic Reset (TLR) state is being reached after each instruction.
 * @param first ir value to begin with.
 * @param last Usually 2 to the power of (ir_len) - 1.
 * @param max_dr_len Maximum data register allowed.
 * @param ir_in Pointer to ir_in register.
 * @param ir_out Pointer to ir_out register.
*/
int discovery(uint32_t first, uint32_t last, uint16_t max_dr_len, uint8_t* ir_in, uint8_t* ir_out);

/**
*	@brief Advance the TAP machine 1 state ahead according to the current state 
*	and next state of the IEEE 1149.1 standard.
*	@param next_state The next state to advance to.
*/
int advance_tap_state(uint8_t next_state);

/**
 * @brief Waits for the incoming of a special character to Serial.
 * @return The input char.
*/
char serialEvent(char character);

/**
 * @brief Prints the given array from last element to first.
 * @param arr Pointer to array.
 * @param len Number of cells to print from that array.
*/
void printArray(uint8_t* arr, uint32_t len);

/**
 * @brief Sends the bytes of an array/buffer via the serial port.
 * @param buf Pointer to the buffer of data to be sent.
 * @param chunk_size Number of bytes to send to host.
 */
void sendDataToHost(uint8_t* buf, uint16_t chunk_size);

/**
 * @brief Clear the remaining artifacts from previous operation
 * no matter what it was.
 * Cleaning this buffer is sometimes esssetial for correct operration
 * of the serial interface. (And is generaly a good practice).
 */
void clear_serial_rx_buf();

/**
 * @brief Prints ASCII art
 */
void print_welcome();

/**
 * @brief Display all available commands.
 * Also, you can add you custom commands for a specific target.
 * For example the MAX10 FPGA commands are included.
 */
void print_main_menu();


#endif /* __MAIN_H__ */