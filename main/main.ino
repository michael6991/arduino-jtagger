/** @file main.ino
 *
 * @brief Basic Jtagger, built for simple purposes such as:
 * detecting existance of a scan chain. read idcode, insert ir and dr,
 * and other simple or complex implementations of custom made operations.
 *
 * Can be simply modified for your requirements.
 *
 * @author Michael Vigdorchik
 */


#include "main.h"
#include "max10_funcs.h"


// Global Variables
uint32_t idcode = 0;
uint8_t dr_out[MAX_DR_LEN] = {0};
uint8_t dr_in[MAX_DR_LEN] = {0};
uint8_t ir_len = 1;
String digits = "";
enum TapState current_state;


/**
 * @brief Detects the the existence of a chain and checks the ir length.
 * @return An integer that represents the length of the instructions.
 */
uint8_t detect_chain()
{
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
	if (id_arr[0] != 1) {
		Serial.println("\n\nBad IDCODE or not implemented, LSB = 0");
		return 0;
	}

	// turn idcode_bits into an unsigned integer
	idcode = binArrayToInt(id_arr, 32);
	Serial.print("\nFound IDCODE: 0x"); Serial.print(idcode, HEX);

	// find ir length.
	Serial.print("\nAttempting to find IR length of target ...");
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
	advance_tap_state(EXIT1_IR);
	advance_tap_state(UPDATE_IR);
	advance_tap_state(RUN_TEST_IDLE);

	Serial.println("\n\nDid not find valid IR length\n");
	return 0;
}


/**
 * @brief Convert char into a hexadecimal number
 * @param ch Character to convert
 * @return Hexadecimal representation of the char, or -1 if out of bounds.
 */
int chr2hex(char ch)
{
	if (ch >= 'a' && ch <= 'f')
		return ch - 0x57;
	
	if (ch >= 'A' && ch <= 'F')
		return ch - 0x37;
	
	if (ch >= '0' && ch <= '9')
		return ch - 0x30;
	
    return -1;
}


/**
 * @brief Convert array of bytes into an integer number, where every byte
 * represents a single bit. Together the array represents a binary number.
 * First element of array is the LSB. 
 * Max length of the given array is 32.
 * @param arr Pointer to the array of bits.
 * @param len Integer that represents the length of the binary array.
 * @return An integer that represents the the value of the array bits.
 */
uint32_t binArrayToInt(uint8_t * arr, int len)
{	
	uint32_t integer = 0;
  	uint32_t mask = 1;

	if (len > 32){
		Serial.print("\nbinArrayToInt function, array size is too large");
		Serial.println("\nBad conversion.");
		Serial.flush();
		return -1;
	}
	
	for (int i = 0; i < len; i++) {
		if (arr[i])
			integer |= mask;
		
		mask = mask << 1;
	}
	return integer;
}


/**
 * @brief Convert the content of a String object into an integer number,
 * where every byte (char) represents a single bit. 
 * Together the string represents a binary number.
 * First element of the string is the LSB.
 * Max length of the given string is 32.
 * @param str Pointer to the array of bits.
 * @return An unsigned integer that represents the the value of the array bits.
 */
uint32_t binStringToInt(String str)
{
	uint32_t integer = 0;
  	uint32_t mask = 1;
	
	if (str.length() > 32){
		Serial.println("\nbinStrToInt function, string length is too large");
		Serial.println("Bad conversion.");
		return -1;
	}
	
	for (int i = str.length() - 1; i >= 0; i--) {	
		if (str[i] == '1')
			integer |= mask;
		
		mask = mask << 1;
	}
	return integer;
}


/**
 * @brief Clear the remaining artifacts from previous operation
 * no matter what it was.
 * Cleaning this buffer is sometimes esssetial for correct operration
 * of the serial interface. (And is generaly a good practice).
 */
void clear_serial_rx_buf() {
	while (Serial.available()) {Serial.read();}
}


/**
 * @brief Used for various tasks where a hexadecimal number needs to be received
 * from the user via the serial port.
 * @param num_bytes The amount of hexadecimal characters to receive.
 * @param message Message for the user.
 * @return uint32_t representation of the hexadecimal number from user.
 */
uint32_t getInteger(int num_bytes, const char * message)
{
    char myData[num_bytes] = {0};

	clear_serial_rx_buf(); // first, clean the input buffer
	Serial.print(message); // notify user to input a value
    while (Serial.available() == 0){} // wait for user input

    byte m = Serial.readBytesUntil('\n', myData, num_bytes);
    myData[m] = '\0';  // insert null charcater

#if DEBUGSERIAL
	Serial.print("myData: ");
	Serial.print(myData) ; // shows: the hexadecimal string from user
#endif
    // convert string to hexadeciaml value
    uint32_t z = strtol(myData, NULL, 16);

#if DEBUGSERIAL
    Serial.print("\nreceived: 0x");
    Serial.println(z, HEX); // shows 12A3
    Serial.flush();
#endif
    return z;
}


/**
 * @brief Convert a binary string into a bytes array arr that will represent
 * the binary value just as string. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the binary digits.
 * (LSB is the first char of string).
 * @param strSize Length of the string object.
 * @return -1 for error
 */
int binStrToBinArray(uint8_t * arr, int arrSize, String str ,int strSize)
{
    int i = 0;
	int j = 0;

	if (strSize > arrSize){
		Serial.print("\nbinStrToBinArray function,length of string is larger than destination array");
		Serial.print("\ndestination array size: "); Serial.print(arrSize);
		Serial.print("\nstring requires: "); Serial.print(strSize);
		Serial.println("\nBad Conversion");
		Serial.flush();
        return -1;
	}
	clear_reg(arr, arrSize);

	// last digit in received string is the least significant
	for (i = strSize - 1; i >= 0; i--)
		arr[strSize - 1 - i] = str[i] - 0x30; // ascii to unsigned int    

	// fill the remaining array elements with zeros
	for (i = strSize; i < arrSize; i++)
        arr[i] = 0;
}


/**
 * @brief Convert a hexadecimal string into a bytes array arr that will represent
 * the binary value of the hexadecimal array. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the hexadecimal digits.
 * (LSB is the first char of string).
 * @param strSize Length of the string object.
 * @return -1 for error
 */
int hexStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize)
{
	int i = 0;
	int j = 0;
	int vacantBits = 0;
	uint8_t n = 0;
	
	if (strSize * 4 > arrSize)
	{
		// check how many bits left on arr that can be populated with bits from the last digit.
		vacantBits = 4 - ((strSize * 4) - arrSize);  // nibble size in bits - (str digit * nibble size) - arr size in bits
		// maybe the last digit can fit in the 1,2, or 3 bits of the last digit
		if (vacantBits <= 0)
		{
			Serial.print("\nhexStrToBinArray function, destination array is not large enough");
			Serial.print("\ndestination array size in bits: "); Serial.print(arrSize);
			Serial.print("\nstring requires size: "); Serial.print(strSize * 4);
			Serial.print("\nVacant bits: "); Serial.print(vacantBits);
			Serial.println("\nBad Conversion");
			Serial.flush();
			return -1;
		}

		if (vacantBits == 3 && chr2hex(str[0]) > 7)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
		if (vacantBits == 2 && chr2hex(str[0]) > 3)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
		if (vacantBits == 1 && chr2hex(str[0]) > 1)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
	}
	clear_reg(arr, arrSize);

	// last digit in received string is the least significant
	for (i = strSize - 1; i >= 0; i--)
	{
		n = chr2hex(str[i]);
		
		if (n == -1){
			Serial.println("\nhexStrToHexArray function, bad digit type");
			Serial.println("Bad Conversion");
			return -1;
		}
		
		// do this if we reached the last digit and arrSize < strSize * 4
		if (i == 0 && vacantBits > 0)
		{
			switch (vacantBits)
			{
			case 3: // only hex digits [0, 7] may be written to last 3 bits of arr
				if (n & 0x04)
					arr[j + 2] = 1;

			case 2: // only hex digits [0, 3] may be written to last 2 bits of arr
				if (n & 0x02)
					arr[j + 1] = 1;
			
			case 1: // only hex digits [0, 1] may be written to last 1 bit of arr
				if (n & 0x01)
					arr[j] = 1;
			default:
				break;  // break out of switch statement
			}
			break;  // break out of for loop
		}

		// copy nibble bits to destination array (LSB first)
		if (n & 0x01)
			arr[j] = 1;
		if (n & 0x02)
			arr[j + 1] = 1;
		if (n & 0x04)
			arr[j + 2] = 1;
		if (n & 0x08)
			arr[j + 3] = 1;
		
		j += 4;  // update destination array index
	}
}


/**
 * @brief Convert base 10 decimal string into a bytes array arr that will represent
 * the binary value of the decimal array. each element (byte) in arr represents a bit.
 * First element is the LSB.
 * @param arr Pointer to the output array with the binary values.
 * @param arrSize Length of the output array in bytes.
 * @param str String that represents the decimal digits.
 * (LSB is the first char of string).
 * @param strSize Length of the string object.
 * @return -1 for error
 */
int decStrToBinArray(uint8_t * arr, int arrSize, String str, int strSize)
{
	int i = 0;
	int j = 0;
	int vacantBits = 0;
	uint8_t n = 0;
	
	if (strSize * 4 > arrSize)
	{
		// check how many bits left on arr that can be populated with bits from the last digit.
		vacantBits = 4 - ((strSize * 4) - arrSize);  // nibble size in bits - (str digit * nibble size) - arr size in bits
		// maybe the last digit can fit in the 1,2, or 3 bits of the last digit
		if (vacantBits <= 0)
		{
			Serial.print("\ndecStrToBinArray function, destination array is not large enough");
			Serial.print("\ndestination array size in bits: "); Serial.print(arrSize);
			Serial.print("\nstring requires size: "); Serial.print(strSize * 4);
			Serial.print("\nVacant bits: "); Serial.print(vacantBits);
			Serial.println("\nBad Conversion");
			Serial.flush();
			return -1;
		}
		if (vacantBits == 3 && chr2hex(str[0]) > 7)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
		if (vacantBits == 2 && chr2hex(str[0]) > 3)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
		if (vacantBits == 1 && chr2hex(str[0]) > 1)
			Serial.print("\nWarning, last digit is to large to fit register. Expect bad conversion.");
	}
	clear_reg(arr, arrSize);

	// last digit in received string is the least significant
	for (i = strSize - 1; i >= 0; i--)
	{
		n = chr2hex(str[i]);
		
		if (n == -1){
			Serial.println("\nhexStrToHexArray function, bad digit type");
			Serial.println("Bad Conversion");
			return -1;
		}

		// do this if we reached the last digit and arrSize < strSize * 4
		if (i == 0 && vacantBits > 0)
		{
			switch (vacantBits)
			{
			case 3: // only hex digits [0, 7] may be written to last 3 bits of arr
				if (n & 0x04)
					arr[j + 2] = 1;

			case 2: // only hex digits [0, 3] may be written to last 2 bits of arr
				if (n & 0x02)
					arr[j + 1] = 1;
			
			case 1: // only hex digits [0, 1] may be written to last 1 bit of arr
				if (n & 0x01)
					arr[j] = 1;
			default:
				break;  // break out of switch statement
			}
			break;  // break out of for loop
		}

		// copy nibble bits to destination array (LSB first)
		if (n & 0x01)
			arr[j] = 1;
		if (n & 0x02)
			arr[j + 1] = 1;
		if (n & 0x04)
			arr[j + 2] = 1;
		if (n & 0x08)
			arr[j + 3] = 1;
		
		j += 4;  // update destination array index
	}
}


/**
 * @brief Convert an integer number n into a bytes array arr that will represent
 * the binary value of n. each element (byte) in arr represent a bit.
 * First element is the LSB. Largest number is a 32 bit number.
 * @param arr Pointer to the output array with the binary values.
 * @param len Length of the output array in bytes. (max size 32)
 * @param n The integer to convert.
 */
int intToBinArray(uint8_t * arr, uint32_t n, uint16_t len)
{
	if (len > 32)
	{
		Serial.print("\nintToBinArray function, array size is larger than 32");
		Serial.println("\nBad Conversion");
		return -1;
	}
	uint32_t mask = 1;
	
	clear_reg(arr, len);
	
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
 * @brief Used for various tasks where an input character needs to be received
 * from the user via the serial port.
 * @param message Message for the user.
 * @return char input from user.
 */
char getCharacter(const char * message)
{
    char inChar[1] = {0};

	clear_serial_rx_buf(); // first, clean the input buffer
	Serial.print(message); // notify user to input a value
	while (Serial.available() == 0) {}  // wait for user input

    Serial.readBytesUntil('\n', inChar, 1);
	char chr = inChar[0];

#if DEBUGSERIAL
    Serial.print("\nchar: "); Serial.print(chr);
    Serial.flush();
#endif
    return chr;
}


/**
 * @brief Get string from the user via the serial port.
 * @return String input from user.
 */
String getString(const char * message)
{
	String str;
	clear_serial_rx_buf(); // first, clean the input buffer
	Serial.print(message); // notify user to input a value
	while (Serial.available() == 0) {} // wait for user input
	
	str = Serial.readStringUntil('\n');

#if DEBUGSERIAL
	Serial.print("\nstring: ");	Serial.print(str);
	Serial.print("\nstring length = "); Serial.print(str.length());
	Serial.flush();
#endif
	return str;
}


/**
 *
 * @brief Receive a number from the user, in different types of format.
 * 0x , 0b, or decimal.
 * with an option to return the fetched number as is in a uint32 format.
 * 
 * @param message A message for the user.
 * @param dest Destination array. Will contain user's input value.
 * @param size Size (in bytes) of the destination array. 
 */
uint32_t parseNumber(uint8_t * dest, uint16_t size, const char * message)
{
	char prefix = '0';

	clear_serial_rx_buf(); // first, clean the input buffer
	Serial.print(message); // notify user to input a value
	fetchNumber(); // fetch the number from the user
	
	// received hex or bin number with prefix or a decimal witout prefix
	if (digits.length() >= 2)
		prefix = digits[1];	
	// else, no prefix, just a decimal number with 1 digit

	// user sent hexadecimal format
	if (prefix == 'x')
	{
		// cut out the digits without the prefix
		digits = digits.substring(2);

		// user wants the function to return the fetched number as is without storing in destination array
		if (dest == NULL)
		{
			// Prepare the character array (the buffer)
			char tmp[digits.length() + 1];  // with 1 extra char for '/0'
			// convert String to char array
			digits.toCharArray(tmp, digits.length() + 1);
			// add null terminator
			tmp[digits.length()] = '\0';
			// convert to unsigned int
			uint32_t z = strtoul(tmp, NULL, 16);
			return z;
		}

		hexStrToBinArray(dest, size, digits, digits.length());
	}
	// user sent binary format
	else if (prefix == 'b')
	{
		// cut out the digits without the prefix
		digits = digits.substring(2);
	
		// convert binary to integer and return
		if (dest == NULL){
			uint32_t z = binStringToInt(digits);
			return z;
		}

		binStrToBinArray(dest, size, digits, digits.length());
	}
	// user sent decimal format
	else
	{
		if (isDigit(prefix) && digits.length() > 0)
		{
			// construct a whole decimal from the string
			char tmp[digits.length() + 1];
			digits.toCharArray(tmp, digits.length() + 1);
			tmp[digits.length()] = '\0';
			uint32_t z = strtoul(tmp, NULL, 10);
			
			if (dest == NULL){
				return z;
			}
			
			intToBinArray(dest, z, size);
		}

		else{
			Serial.println("\nBad prefix. Did not get number.");
		}
	}
	return 0;
}


/**
	@brief Return to TEST LOGIC RESET state of the TAP FSM.
	Apply 5 TCK cycles accompanied with TMS logic state 1.
*/
void reset_tap(void)
{
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
*	@brief Insert data of length dr_len to DR, and end the interaction
*	in the state end_state which can be one of the following:
*	TLR, RTI.
*	@param dr_in Pointer to the input data array. (bytes array)
*	@param dr_len Length of the register currently connected between tdi and tdo.
*	@param end_state TAP state after dr inseration.
*	@param dr_out Pointer to the output data array. (bytes array)
*/
void insert_dr(uint8_t * dr_in, uint8_t dr_len, uint8_t end_state, uint8_t * dr_out)
{
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
*	@brief Insert data of length ir_len to IR, and end the interaction
*	in the state end_state which can be one of the following:
*	TLR, RTI, SelectDR.
*	@param ir_in Pointer to the input data array. Bytes array, where each
*	@param ir_len Length of the register currently connected between tdi and tdo.
*	@param end_state TAP state after dr inseration.
*	@param ir_out Pointer to the output data array. (bytes array)
*/
void insert_ir(uint8_t * ir_in, uint8_t ir_len, uint8_t end_state, uint8_t * ir_out)
{
	// Make sure that current state is TLR
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
void clear_reg(uint8_t * reg, uint16_t len){
	for (uint16_t i = 0; i < len; i++)
		reg[i] = 0;
}


/**
 * @brief Clean the IR and DR together
 */
void flush_ir_dr(uint8_t * ir_reg, uint8_t * dr_reg, uint16_t ir_len, uint16_t dr_len){
	clear_reg(ir_reg, ir_len);
	clear_reg(dr_reg, dr_len);
}


/**
	@brief Find out the dr length of a specific instruction.
	@param instruction Pointer to the bytes array that contains the instruction.
	@param ir_len The length of the IR. (Needs to be know prior to function call).
	@return Counter that represents the size of the DR. Or 0 if did not find
	a valid size. (DR may not be implemented or some other reason).
*/
uint16_t detect_dr_len(uint8_t * instruction, uint8_t ir_len)
{	
	/* Make sure that current state is TLR*/

	uint8_t tmp[ir_len];  // temporary array to strore the shifted out bits of IR
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
	return 0;
}


/**
 * @brief Similarly to discovery command in urjtag, performs a brute force search
 * of each possible values of the IR register to get its corresponding DR leght in bits.
 * Test Logic Reset (TLR) state is being reached after each instruction.
 * @param first ir value to begin with.
 * @param last Usually 2 to the power of (ir_len) - 1.
 * @param ir_in Pointer to ir_in register.
 * @param ir_out Pointer to ir_out register.
 * @param dr_in Pointer to dr_in register.
 * @param dr_out Pointer to dr_out register.
 * @param maxDRLen Maximum data register allowed.
*/
void discovery(uint16_t maxDRLen, uint32_t last, uint32_t first, uint8_t * ir_in, uint8_t * ir_out)
{
	uint32_t instruction = 0;
	int i, counter = 0;

	// discover all dr lengths corresponding to their ir.
	Serial.print("\n\nDiscovery of instructions from 0x"); Serial.print(first, HEX);
	Serial.print(" to 0x"); Serial.println(last, HEX);
	
	for (instruction=first ; instruction <= last; instruction++){
		// reset tap
		reset_tap();
		counter = 0;
		
		// prepare to shift instruction
		intToBinArray(ir_in, instruction, ir_len);

		// Shift instruction
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

		// A couple of clock cycles to process the instruction
		HC; HC; HC; HC; HC; HC; HC; HC;
		
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
	reset_tap();
	Serial.println("\n\n   Done");
}


/**
*	@brief Advance the TAP machine 1 state ahead according to the current state 
*	and next state of the IEEE 1149.1 standard.
*	@param next_state The next state to advance to.
*/
void advance_tap_state(uint8_t next_state)
{
#if DEBUGTAP
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
char serialEvent(char character)
{
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
 * @brief Prints the given array from last element to first.
 * @param arr Pointer to array.
 * @param len Number of cells to print from that array.
*/
void printArray(uint8_t * arr, uint16_t len){
	for (int16_t i = len - 1; i >= 0; i--){
		Serial.print(arr[i], DEC);
	}
	Serial.flush();
}


/**
 * @brief Sends the bytes of an array/buffer via the serial port.
 * @param buf Pointer to the buffer of data to be sent.
 * @param chunk_size Number of bytes to send to host.
 */
void sendDataToHost(uint8_t * buf, uint16_t chunk_size)
{
	for (int i = 0; i < chunk_size; ++i){
		Serial.write(buf[i]);
	}
	Serial.flush();
}


void print_main_menu(){
	Serial.flush();	
	Serial.print("\n\nMain JTAGGER Menu:\n");
	Serial.print("All parameters should be passed in the format {0x || 0b || decimal}\n");
	Serial.print("c - Detect chain\n");
	Serial.print("d - Discovery\n");
	Serial.print("i - Insert IR\n");
	Serial.print("l - Detect DR length\n");
	Serial.print("r - Insert DR\n");
	Serial.print("t - Reset TAP state machine\n");
	Serial.print("m - MAX10 FPGA commands\n");
	Serial.print("z - Exit\n");
	Serial.flush();
}


void setup(){
	/* Initialize mode for jtag pins */
	pinMode(TCK, OUTPUT);
	pinMode(TMS, OUTPUT);
	pinMode(TDI, OUTPUT);
	pinMode(TDO, INPUT_PULLUP);
	pinMode(TRST, OUTPUT);

	/* Initial pins state */
	digitalWrite(TCK, 0);
	digitalWrite(TMS, 1);
	digitalWrite(TDI, 1);
	digitalWrite(TRST, 1);

	/* Initialize serial communication */
	Serial.begin(115200);
	while (!Serial) { }
	Serial.setTimeout(500); // set timeout for various serial R/W funcs
	Serial.println("Ready...");
}


void loop() {
	char command = '0';
	int len = 0;
	int nBits = 0;
	current_state = TEST_LOGIC_RESET;
	
	// to begin session
	getCharacter("Insert 's' to start > ");

	// detect chain and read idcode
	ir_len = detect_chain();
	Serial.print("IR length: "); Serial.print(ir_len, DEC);

	// define ir register according to ir length
	uint8_t ir_in[ir_len];
	uint8_t ir_out[ir_len];

	reset_tap();

	while (1){
		print_main_menu();
		command = getCharacter("\ncmd > ");
		
		switch (command)
		{
		case 'c':
			// detect chain and read idcode
			ir_len = detect_chain();
			Serial.print("IR length: "); Serial.print(ir_len);
			break;

		case 'd':
			// discovery of existing IRs
			discovery(parseNumber(NULL, 20, "Max allowed DR length > "),
					parseNumber(NULL, 20, "Final IR > "),
					parseNumber(NULL, 20, "Initial IR > "),
					ir_in,
					ir_out);
			break;

		case 'i':
			// insert ir
			parseNumber(ir_in, ir_len, "\nShift IR > ");
			insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
			
			Serial.print("\nIR  in: ");
			printArray(ir_in, ir_len);
			if (ir_len <= 32) // print the hex value if lenght is not large enough
				Serial.print(" | 0x"); Serial.print(binArrayToInt(ir_in, ir_len), HEX);

			Serial.print("\nIR out: ");
			printArray(ir_out, ir_len);
			if (ir_len <= 32) // print the hex value if lenght is not large enough
				Serial.print(" | 0x"); Serial.print(binArrayToInt(ir_out, ir_len), HEX);
			break;

		case 'l':
			// detect current dr length
			len = detect_dr_len(ir_in, ir_len);
			if (len == 0){
				Serial.println("\nDid not find the current DR length, TDO stuck at 1");
			}
			else{
				Serial.print("\nDR length: ");
				Serial.print(len);
			}
			break;

		case 'm':
			// entering into MAX10 FPGA command menu
			max10_main(ir_len, ir_in, ir_out, dr_in, dr_out);
			break;

		case 'r':
			// insert dr	
			nBits = parseNumber(NULL, 32, "Enter amount of bits to shift > ");
			if (nBits == 0)
				break;
			
			parseNumber(dr_in, nBits, "\nShift DR > ");
			insert_dr(dr_in, nBits, RUN_TEST_IDLE, dr_out);
			
			Serial.print("\nDR  in: ");
			printArray(dr_in, nBits);
			if (nBits <= 32) // print the hex value if lenght is not large enough
				Serial.print(" | 0x"); Serial.print(binArrayToInt(dr_in, nBits), HEX);

			Serial.print("\nDR out: ");
			printArray(dr_out, nBits);
			if (nBits <= 32) // print the hex value if lenght is not large enough
				Serial.print(" | 0x"); Serial.print(binArrayToInt(dr_out, nBits), HEX);

			break;

		case 't':
			// reset tap
			reset_tap();
			break;
			
		case 'z':
			// quit main loop
			Serial.print("\nExiting menu...");
			break;
		
		default:
			break;
		}
		if (command == 'z')
			break;		
	}

	reset_tap();
	Serial.end();
	while(1);
}

