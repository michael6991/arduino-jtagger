/** @file main.ino
 *
 * @brief basic jtagger, built for simple purposes such as:
 *		detecting existance of a scan chain. read idcode, insert ir and dr,
 *		and easy implementation of custom made operations.
 *
 * @author Michael Vigdorchik, October 2019, Retro
 */


#include "Arduino.h"
#include "ir.h"

/* 
comment out this line if you don't wish to see debug info such as
TAP state transitions.
*/
// #define DEBUGTAP

/* 
comment out this line if you don't wish to see debug info regarding
user input via serial port.
*/
#define DEBUGSERIAL




// set the jtag pins
#define TCK 7
#define TMS 8
#define TDI 9
#define TDO 10
#define TRST 11


#ifndef MAX_DR_LEN
#define MAX_DR_LEN 256    // usually BSR
#endif


/*	Choose a half-clock cycle delay	*/
// half clock cycle
// #define HC delay(1);
// or
#define HC delayMicroseconds(500);
// A more precise way to delay a half clock cycle
// #define HC __asm__ __volatile__(
//     		 "nop\t\n
//     		  nop\t\n
//     		  nop\t\n
//     		  nop\t\n" : : );



// Global Variables
uint32_t idcode = 0;
uint8_t dr_out[MAX_DR_LEN] = {0};
uint8_t dr_in[MAX_DR_LEN] = {0};
uint8_t ir_len = 0;



enum TapStates
{
	TEST_LOGIC_RESET, RUN_TEST_IDLE,
	SELECT_DR, CAPTURE_DR, SHIFT_DR, EXIT1_DR, PAUSE_DR, EXIT2_DR, UPDATE_DR,
	SELECT_IR, CAPTURE_IR, SHIFT_IR, EXIT1_IR, PAUSE_IR, EXIT2_IR, UPDATE_IR
};

enum TapStates current_state;




/**
 * @brief Detects the the existence of a chain and checks the ir length.
 * @return An integer that represents the length of the instructions.
 */
uint8_t detect_chain(void){

	uint8_t id_arr[32] = { 0 };
	uint32_t idcode = 0;
	uint32_t i = 0;
	uint8_t counter = 0;


	reset_tap();

	// try to read IDCODE first and then detect the IR length
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);
	
	// shift out the id code from the id code register
	for (i = 0; i < 31; i++)
	{
		advance_tap_state(SHIFT_DR);
		id_arr[i] = digitalRead(TDO);  // LSB first
	}
	id_arr[i] = digitalRead(TDO);
	advance_tap_state(EXIT1_DR);

	// LSB of IDCODe must be 1.
	if (id_arr[0] != 1)
	{
		Serial.println("\n\nBad IDCODE or not implemented, LSB = 0");
	}

	// turn idcode_bits into an unsigned integer
	idcode = arrayToInt(id_arr, 32);
	Serial.print("\nFound IDCODE: 0x"); Serial.print(idcode, HEX);


	// find ir length.
	Serial.println("\n\nAttempting to find IR length of part ...\n\n");
	reset_tap();
	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(SELECT_IR);
	advance_tap_state(CAPTURE_IR);
	advance_tap_state(SHIFT_IR);
	
	// shift in about 100 ones into TDI to clear the register
	// from its previos content. then shift a single zero followed by
	// a bunch of ones and cout the amount of clock cycles from inserting zero
	// till we read it in TDO.
	
	digitalWrite(TDI, 1);
	for (i = 0; i < 100; ++i)
	{
		advance_tap_state(SHIFT_IR);
	}

	digitalWrite(TDI, 0);
	advance_tap_state(SHIFT_IR);

	digitalWrite(TDI, 1);
	for (i = 0; i < 100; ++i)
	{
		advance_tap_state(SHIFT_IR);

		if (digitalRead(TDO) == 0){
			counter++;
			return counter;
		}
		counter++;
	}

	Serial.println("\n\nDid not find valid IR length\n");
	return 0;
}


/**
 * @brief Convert array of bytes into an integer number, where every byte
 * represents a single bit. Together the array represents a binary number.
 * First element of array is the LSB.
 * @param arr Pointer to the array of bits.
 * @param len Integer that represents the length of the binary array.
 * @return An integer that represents the the value of the array bits.
 */
uint32_t arrayToInt(uint8_t * arr, int len){
	
	uint32_t integer = 0;
  	uint32_t mask = 1;
	
	for (int i = 0; i < len; i++) {
		if (arr[i])
			integer |= mask;
		
		mask = mask << 1;
	}
	return integer;
}


/**
 * @brief Convert an integer number n into a bytes array arr that will represent
 * the binary value of n. each element (byte) in arr represent a bit.
 * First element is the LSB. Largest number is a 32 bit number.
 * @param arr Pointer to the output array with the binary values.
 * @param len Length of the output array in bytes. (max size 32)
 * @param n The integer to convert.
 */
void intToBinArray(uint8_t * arr, uint32_t n, uint16_t len){
	if (len > 32)
	{
		Serial.println("inToArray function, len is larger than 32 bits");
		Serial.println("Bad Conversion");
	}
	uint32_t mask = 1;
	
	flush_reg(arr, len);
	
	for (int i = 0; i < len; i++) {
        if (n & mask){
            arr[i] = 1;
        }
        else{
            arr[i] = 0;
        }
        mask <<= 1;
	}
}

/**
	@brief Return to TEST LOGIC RESET state of the TAP FSM.
	Invoke 5 TCK cycles accompanied with TMS logic state 1.
*/
void reset_tap(void){
	Serial.println("\nresetting tap");
	for (uint8_t i = 0; i < 5; ++i)
	{
		digitalWrite(TMS, 1);
		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;
	}
	current_state = TEST_LOGIC_RESET;
}


/**
	@brief Insert data of length dr_len to DR, and end the interaction
	in the state end_state which can be one of the following:
	TLR, RTI.
	@param dr_in Pointer to the input data array. (bytes array)
	@param dr_len Length of the register currently connected between tdi and tdo.
	@param end_state TAP state after dr inseration.
	@param dr_out Pointer to the output data array. (bytes array)
*/
void insert_dr(uint8_t * dr_in, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out){
	/* Make sure that current state is TLR*/
	uint16_t i = 0;

	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	// shift data bits into the DR. make sure that first bit is LSB
	for (i = 0; i < dr_len - 1; i++)
	{
		digitalWrite(TDI, dr_in[i]);
		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;
		dr_out[i] = digitalRead(TDO);  // read the shifted out bits . LSB first
	}

	// read and write the last DR bit and continue to the end state
	digitalWrite(TDI, dr_in[i]);
	advance_tap_state(EXIT1_DR);
	dr_out[i] = digitalRead(TDO); 

	advance_tap_state(UPDATE_DR);

	if (end_state == RUN_TEST_IDLE){	
		advance_tap_state(RUN_TEST_IDLE);
	}
	else if (end_state == SELECT_DR){
		advance_tap_state(SELECT_DR);
	}
	else if (end_state == TEST_LOGIC_RESET){
		reset_tap();
	}
}



/**
	@brief Insert data of length ir_len to IR, and end the interaction
	in the state end_state which can be one of the following:
	TLR, RTI, SelectDR.
	@param ir_in Pointer to the input data array. Bytes array, where each
	@param ir_len Length of the register currently connected between tdi and tdo.
	@param end_state TAP state after dr inseration.
	@param ir_out Pointer to the output data array. (bytes array)
*/
void insert_ir(uint8_t * ir_in, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out){

	/* Make sure that current state is TLR*/
	uint8_t i = 0;

	advance_tap_state(RUN_TEST_IDLE);
	advance_tap_state(SELECT_DR);
	advance_tap_state(SELECT_IR);
	advance_tap_state(CAPTURE_IR);
	advance_tap_state(SHIFT_IR);

	// shift data bits into the IR. make sure that first bit is LSB
	for (i = 0; i < ir_len - 1; i++)
	{
		digitalWrite(TDI, ir_in[i]);
		digitalWrite(TCK, 0); HC;
		digitalWrite(TCK, 1); HC;
		ir_out[i] = digitalRead(TDO);  // read the shifted out bits . LSB first
	}

	// read and write the last IR bit and continue to the end state
	digitalWrite(TDI, ir_in[i]);
	advance_tap_state(EXIT1_IR);	
	ir_out[i] = digitalRead(TDO);
	
	advance_tap_state(UPDATE_IR);

	if (end_state == RUN_TEST_IDLE){	
		advance_tap_state(RUN_TEST_IDLE);
	}
	else if (end_state == SELECT_IR){
		advance_tap_state(SELECT_IR);
	}
	else if (end_state == TEST_LOGIC_RESET){
		reset_tap();
	}
}


/**
 * @brief Fill the register with zeros
 * @param reg Pointer to the register to flush.
 */
void flush_reg(uint8_t * reg, uint16_t len){
	for (uint16_t i = 0; i < len; i++)
		reg[i] = 0;
}


/**
 * @brief Clean the IR and DR by calling flush_reg function.
 */
void flush_ir_dr(uint8_t * ir_reg, uint8_t * dr_reg, uint16_t ir_len, uint16_t dr_len){
	flush_reg(ir_reg, ir_len);
	flush_reg(dr_reg, dr_len);
}



/**
	@brief Find out the dr length of a specific instruction.
	@param instruction Pointer to the bytes array that contains the instruction.
	@param ir_len The length of the IR. (Needs to be know prior to function call).
	@return Counter that represents the size of the DR. Or 0 if did not find
	a valid size. (DR may not be implemented or some other reason).
*/
uint16_t detect_dr_len(uint8_t * instruction, uint8_t ir_len){
	
	/* Make sure that current state is TLR*/

	uint8_t tmp[ir_len] = {0};  // temporary array to strore the shifted out bits of IR
	uint16_t i = 0;
	uint16_t counter = 0;

	// insert the instruction we wish to check
	insert_ir(instruction, ir_len, RUN_TEST_IDLE, tmp);

	/* 
		check the length of the DR register between TDI and TDO
		by inserting many ones (MAX_DR_LEN) to clean the register.
		afterwards, insert a single zero and start counting the amount
		of TCK clock cycles till the appearence of that zero in TDO.
	*/
	advance_tap_state(SELECT_DR);
	advance_tap_state(CAPTURE_DR);
	advance_tap_state(SHIFT_DR);

	digitalWrite(TDI, 1);
	for (i = 0; i < MAX_DR_LEN; ++i)
	{
		advance_tap_state(SHIFT_DR);
	}

	digitalWrite(TDI, 0);
	advance_tap_state(SHIFT_DR);

	digitalWrite(TDI, 1);
	for (i = 0; i < MAX_DR_LEN; ++i)
	{
		advance_tap_state(SHIFT_DR);

		if (digitalRead(TDO) == 0){
			++counter;
			return counter;
		}
		counter++;
	}
	Serial.println("\n\nDid not find the current DR length, TDO stuck at: ");
	Serial.print(digitalRead(TDO) ,DEC);
	return 0;
}



/**
	@brief Advance the TAP machine 1 state ahead according to the current state 
	and next state of the IEEE 1149 standard.
*/
void advance_tap_state(uint8_t next_state){

#ifdef DEBUGTAP
	Serial.print("\ntap state: ");
	Serial.print(current_state, HEX);
#endif

	switch ( current_state ){

		case TEST_LOGIC_RESET:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == TEST_LOGIC_RESET){
				// stay in test logic reset
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			break;


		case RUN_TEST_IDLE:
			if (next_state == SELECT_DR){
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			else if (next_state == RUN_TEST_IDLE){ 
				// stay in run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			break;


		case SELECT_DR:
			if (next_state == CAPTURE_DR){
				// go to capture dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = CAPTURE_DR;
			}
			else if (next_state == SELECT_IR){ 
				// go to select ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_IR;
			}
			break;


		case CAPTURE_DR:
			if (next_state == SHIFT_DR){
				// go to shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_DR;
			}
			else if (next_state == EXIT1_DR){ 
				// go to exit1 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_DR;
			}
			break;

		case SHIFT_DR:
			if (next_state == SHIFT_DR){
				// stay in shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT1_DR){ 
				// go to exit1 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_DR;
			}
			break;


		case EXIT1_DR:
			if (next_state == PAUSE_DR){
				// go to pause dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = PAUSE_DR;
			}
			else if (next_state == UPDATE_DR){ 
				// go to update dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_DR;
			}
			break;


		case PAUSE_DR:
			if (next_state == PAUSE_DR){
				// stay in pause dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT2_DR){ 
				// go to exit2 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT2_DR;
			}
			break;


		case EXIT2_DR:
			if (next_state == SHIFT_DR){
				// go to shift dr
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_DR;
			}
			else if (next_state == UPDATE_DR){ 
				// go to update dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_DR;
			}
			break;


		case UPDATE_DR:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == SELECT_DR){ 
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			break;


		case SELECT_IR:
			if (next_state == CAPTURE_IR){
				// go to capture ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = CAPTURE_IR;
			}
			else if (next_state == TEST_LOGIC_RESET){ 
				// go to test logic reset
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = TEST_LOGIC_RESET;
			}
			break;


		case CAPTURE_IR:
			if (next_state == SHIFT_IR){
				// go to shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_IR;
			}
			else if (next_state == EXIT1_IR){ 
				// go to exit1 ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_IR;
			}
			break;


		case SHIFT_IR:
			if (next_state == SHIFT_IR){
				// stay in shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT1_IR){ 
				// go to exit1 ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT1_IR;
			}
			break;


		case EXIT1_IR:
			if (next_state == PAUSE_IR){
				// go to pause ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = PAUSE_IR;
			}
			else if (next_state == UPDATE_IR){ 
				// go to update ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_IR;
			}
			break;

		
		case PAUSE_IR:
			if (next_state == PAUSE_IR){
				// stay in pause ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
			}
			else if (next_state == EXIT2_IR){ 
				// go to exit2 dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = EXIT2_IR;
			}
			break;


		case EXIT2_IR:
			if (next_state == SHIFT_IR){
				// go to shift ir
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SHIFT_IR;
			}
			else if (next_state == UPDATE_IR){ 
				// go to update ir
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = UPDATE_IR;
			}
			break;


		case UPDATE_IR:
			if (next_state == RUN_TEST_IDLE){
				// go to run test idle
				digitalWrite(TMS, 0);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = RUN_TEST_IDLE;
			}
			else if (next_state == SELECT_DR){ 
				// go to select dr
				digitalWrite(TMS, 1);
				digitalWrite(TCK, 0); HC;
				digitalWrite(TCK, 1); HC;
				current_state = SELECT_DR;
			}
			break;


		default:
			Serial.println("Error: incorrent TAP state !");
			break;
	}

}



/**
 * @brief Waits for the incoming of a special character to Serial.
 * @return The input char.
*/
char serialEvent(char character) {
  char inChar;
  while (Serial.available() == 0) {
    // get the new byte:
    inChar = (char)Serial.read();
    // if the incoming character is a newline, 
	// break from while and proceed to main loop
    // do something about it:
    if (inChar == character) {
      break;
    }
  }
  Serial.flush();
  return inChar;
}



/**
 * @brief Used for various tasks where a hexadecimal number needs to be received
 * from the user via the serial port.
 * @param num_bytes The amount of hexadecimal characters to receive.
 * @param message Message for the user.
 * @return uint32_t representation of the hexadecimal number from user.
 */
uint32_t getNumber(int num_bytes, const char * message){
    char myData[num_bytes];

    // first, clean the input buffer
	while (Serial.available())
		Serial.read();
	
	// notify user to input a value
	Serial.print(message);

    while (Serial.available() == 0)
    {
		// wait for user input
	}

    byte m = Serial.readBytesUntil('\n', myData, num_bytes);

    myData[m] = '\0';  //insert null charcater

// #ifdef DEBUGSERIAL
	// Serial.print("myData: ");
	// Serial.print(myData) ;///shows: the hexadecimal string from user
// #endif

    //------------ convert string to hexadeciaml value
    uint32_t z = strtol(myData, NULL, 16);

#ifdef DEBUGSERIAL
    Serial.print("\nreceived: 0x");
    Serial.println(z, HEX);    //shows 12A3
    Serial.flush();
#endif
    return z;
}


/**
 * @brief Prints the given array.
*/
void printArray(uint8_t * arr, uint16_t len){
	for (int16_t i = len-1; i >= 0; i--){
		Serial.print(arr[i], DEC);
	}
	Serial.flush();
}


/**
 * @brief Sends the bytes of an array/buffer via the serial port.
 * @param buf Pointer to the buffer of data to be sent.
 * @param chunk_size Number of bytes to send to host.
 */
void send_data_to_host(uint8_t * buf, uint16_t chunk_size)
{
	for (int i = 0; i < chunk_size; ++i)
	{
		Serial.write(buf[i]);
	}
	Serial.flush();
}



/* --------------------------------------------------------------------------------------- */
/* ------------------- My custom functions for MAX10 FPGA project -------------------------*/
/* --------------------------------------------------------------------------------------- */

/**
 * @brief Read user defined 32 bit code of MAX10 FPGA.
 * @param ir_in ir_in
 * @param ir_out ir_out
 * @param dr_in dr_in
 * @param dr_out dr_out
 * @return 32 bit integer that represents the user code.
 */
uint32_t read_user_code(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out){
	uint32_t usercode = 0;

	flush_reg(dr_out, MAX_DR_LEN);

	intToBinArray(ir_in, USERCODE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
	insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);
	
	usercode = arrayToInt(dr_out, 32);

	Serial.print("\nUSERCODE: 0x"); Serial.print(usercode ,HEX);
	return usercode;
}


/**
 * @brief 
 * @param ir_in
 * @param ir_out
 * @param dr_in
 * @param dr_out
 * @param start
 * @param stop
*/
void read_ufm_range(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out, uint32_t start, uint32_t stop){
	if (start >= stop){
		Serial.println("\nStart address is bigger or equal to stop address. Exiting...");
		return;
	}
	
	intToBinArray(ir_in, ISC_ENABLE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	delay(15); // delay bw=etweeb ISC_Enable and read attenpt.(may be shortened)
	
	for (uint32_t j=start ; j < stop; j += 4){
		// shift address instruction
		intToBinArray(ir_in, ISC_ADDRESS_SHIFT, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
		
		// shift address value
		flush_reg(dr_in, 32);
		intToBinArray(dr_in, j, 23);
		insert_dr(dr_in, 23, RUN_TEST_IDLE, dr_out);
		
		// shift read instruction
		intToBinArray(ir_in, ISC_READ, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

		// read data
		flush_reg(dr_in, 32);
		insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);

		// print address and corresponding data
		Serial.print("\n0x"); Serial.print(j, HEX);
		Serial.print(": 0x"); Serial.print(arrayToInt(dr_out, 32), HEX);
		Serial.flush();
	}
}


/**
 * @brief 
 * @param ir_in
 * @param ir_out
 * @param dr_in
 * @param dr_out
*/
void readFlashSession(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out){

	uint32_t startAddr = 0;
	uint32_t stopAddr = 0;
	Serial.print("\nReading flash address range");
	
	while (1){
		flush_reg(dr_in, MAX_DR_LEN);
		flush_reg(dr_out, MAX_DR_LEN);
		
		reset_tap();

		startAddr = getNumber(16, "\nInsert start addr: ");
		stopAddr = getNumber(16, "\nInsert stop addr: ");
		read_ufm_range(ir_in, ir_out, dr_in, dr_out, startAddr, stopAddr);
		
		Serial.println("\nInput 'q' to quit loop, else to continue");
		serialEvent('q');
		return;
	}
}



/**
 * @brief 
 * @param first
 * @param last Usually 2^ir_len - 1
 * @param ir_in
 * @param ir_out
 * @param dr_in
 * @param dr_out
 * @param maxDRLen
*/
void discovery(uint32_t first, uint32_t last, uint8_t * ir_in, uint8_t * ir_out, uint16_t maxDRLen){
	uint32_t instruction = 0;
	int counter;
	int i;
	

	// discover all dr lengths corresponding to their ir.
	Serial.print("\n\nDiscovery of instructions from 0x"); Serial.print(first, HEX);
	Serial.print(" to 0x"); Serial.println(last, HEX);
	
	
	for (instruction=first ; instruction <= last; instruction++){
		// reset tap
		reset_tap();
		counter = 0;

		
		intToBinArray(ir_in, ISC_ENABLE, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
		HC; HC;
		
		
		// prepare to shift instruction
		intToBinArray(ir_in, instruction, ir_len);

		// shift instruction
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

		/* some delay to process the instruction */
		HC; HC; HC; HC;  // a couple of clock cycles
		
		advance_tap_state(SELECT_DR);
		advance_tap_state(CAPTURE_DR);

		digitalWrite(TDI, 1);
		
		for (i = 0; i < maxDRLen; i++){
			advance_tap_state(SHIFT_DR);
		}

		digitalWrite(TDI, 0);
		advance_tap_state(SHIFT_DR);
		digitalWrite(TDI, 1);

		for (i = 0; i < maxDRLen; i++){
			counter++;
			advance_tap_state(SHIFT_DR);

			if (digitalRead(TDO) == 0)
				break;
		}
		
		if (counter == maxDRLen){
			counter = -1; // tdo stuck at 1
		}
		Serial.print("\nDetecting DR length for IR 0x");
		Serial.print(instruction, HEX); Serial.print(" ... ");
		Serial.print(counter);
	}
	Serial.println("\n\n   Done");
}



void setup() {	

	/* initialize mode for jtag pins */
	pinMode(TCK, OUTPUT);
	pinMode(TMS, OUTPUT);
	pinMode(TDI, OUTPUT);
	pinMode(TDO, INPUT);
	pinMode(TRST, OUTPUT);

	/* initial pins state */
	digitalWrite(TCK, 0);
	digitalWrite(TMS, 1);
	digitalWrite(TDI, 1);
	digitalWrite(TDO, HIGH); // to use PullUp
	digitalWrite(TRST, 1);

	/* Initialize serial communication */
	Serial.begin(115200);
	while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
	}
	Serial.println("Ready...");
}



void loop() {
	current_state = TEST_LOGIC_RESET;
	
	Serial.println("Insert 's' to start");
	serialEvent('s');


	// detect chain and read idcode
	ir_len = detect_chain();
	Serial.println("\nIR length: "); Serial.print(ir_len, DEC);

	uint8_t ir_in[ir_len] = {0};
	uint8_t ir_out[ir_len] = {0};

	reset_tap();
	
	// read user code
	read_user_code(ir_in, ir_out, dr_in, dr_out);
	flush_ir_dr(ir_in, dr_out, ir_len, MAX_DR_LEN);


	
	// attempt to read address range from ufm
	readFlashSession(ir_in, ir_out, dr_in, dr_out);
	// disable ISC
	intToBinArray(ir_in, ISC_DISABLE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
		

	// discovery(0,1024,ir_in,ir_out,760);

	reset_tap();
	while(1);
}

