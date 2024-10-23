/* --------------------------------------------------------------------------------------- */
/* ------------------- Custom functions for MAX10 FPGA project ----------------------------*/
/* --------------------------------------------------------------------------------------- */

#include "jtagger.h"
#include "max10_ir.h"

/**
 * @brief Read user defined 32 bit code of MAX10 FPGA.
 * @param ir_in ir_in
 * @param ir_out ir_out
 * @param dr_in dr_in
 * @param dr_out dr_out
 * @return 32 bit integer that represents the user code.
 */
uint32_t max10_read_user_code(const uint8_t ir_len, uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out)
{
	clear_reg(dr_out, MAX_DR_LEN);
	intToBinArray(ir_in, USERCODE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
	insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);
	
	return binArrayToInt(dr_out, 32);
}


/**
 * @brief Perform read flash operation on the MAX10 FPGA, by getting an address range and 
 * incrementing the given address in each iteration with ISC_ADDRESS_SHIFT, before invoking ISC_READ.
 * @param ir_in Pointer to the input data array. Bytes array, where each
 * @param ir_out Pointer to the output data array. (bytes array)
 * @param dr_in Pointer to the input data array. (bytes array)
 * @param dr_out Pointer to the output data array. (bytes array)
 * @param start Address from which to start the flash reading.
 * @param num Amount of 32 bit words to read, starting from the start address.
*/
void max10_read_ufm_range(const uint8_t ir_len, uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out, const uint32_t start, const uint32_t num)
{
	if (num < 0){
		Serial.println("\nNumber of words to read must be positive. Exiting...");
		return;
	}
	Serial.println("\nReading flash in address iteration fashion");
	
	intToBinArray(ir_in, ISC_ENABLE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	delay(15); // delay between ISC_Enable and read attenpt.(may be shortened)
	
	for (uint32_t j=start ; j < (start + num); j += 4){
		// shift address instruction
		intToBinArray(ir_in, ISC_ADDRESS_SHIFT, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
		
		// shift address value
		clear_reg(dr_in, 32);
		intToBinArray(dr_in, j, 23);
		insert_dr(dr_in, 23, RUN_TEST_IDLE, dr_out);
		
		// shift read instruction
		intToBinArray(ir_in, ISC_READ, ir_len);
		insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

		// read data
		clear_reg(dr_in, 32);
		insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);

		// print address and corresponding data
		Serial.print("\n0x"); Serial.print(j, HEX);
		Serial.print(": 0x"); Serial.print(binArrayToInt(dr_out, 32), HEX);
		Serial.flush();
	}
}


/**
 * @brief Perform read flash operation on the MAX10 FPGA, by getting an address range and 
 * incrementing the given address in each iteration with ISC_ADDRESS_SHIFT, before invoking ISC_READ.
 * @param ir_in Pointer to the input data array.  (bytes array)
 * @param ir_out Pointer to the output data array. (bytes array)
 * @param dr_in Pointer to the input data array. (bytes array)
 * @param dr_out Pointer to the output data array. (bytes array)
 * @param start Address from which to start the flash reading.
 * @param num Amount of 32 bit words to read, starting from the start address.
*/
void max10_read_ufm_range_burst(const uint8_t ir_len, uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out, const uint32_t start, const uint32_t num)
{
	if (num < 0){
		Serial.println("\nNumber of words to read must be positive. Exiting...");
		return;
	}
	Serial.println("\nReading flash in burst fashion");
	
	
	intToBinArray(ir_in, ISC_ENABLE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	delay(15); // delay between ISC_Enable and read attenpt.(may be shortened)

	// shift address instruction
	intToBinArray(ir_in, ISC_ADDRESS_SHIFT, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	// shift address value
	clear_reg(dr_in, 32);
	intToBinArray(dr_in, start, 23);
	insert_dr(dr_in, 23, RUN_TEST_IDLE, dr_out);

	// shift read instruction
	intToBinArray(ir_in, ISC_READ, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	clear_reg(dr_in, 32);

	for (uint32_t j=start ; j < (start + num); j += 4){
		// read data in burst fashion
		insert_dr(dr_in, 32, RUN_TEST_IDLE, dr_out);

		// print address and corresponding data
		Serial.print("\n0x"); Serial.print(j, HEX);
		Serial.print(": 0x"); Serial.print(binArrayToInt(dr_out, 32), HEX);
		Serial.flush();
	}
}


/**
 * @brief User interface with the various flash reading functions.
 * @param ir_in  Pointer to the input data array.  (bytes array)
 * @param ir_out Pointer to the output data array. (bytes array)
 * @param dr_in Pointer to the input data array. (bytes array)
 * @param dr_out Pointer to the output data array. (bytes array)
*/
void max10_readFlashSession(const uint8_t ir_len, uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out)
{
	uint32_t startAddr = 0;
	uint32_t numToRead = 0;

	Serial.print("\nReading flash address range");
	
	while (1){
		clear_reg(dr_in, MAX_DR_LEN);
		clear_reg(dr_out, MAX_DR_LEN);
		
		reset_tap();
		
		startAddr = parseNumber(NULL, 16, "\nInsert start addr > ");
		numToRead = parseNumber(NULL, 16, "\nInsert amount of words to read > ");
		max10_read_ufm_range_burst(ir_len, ir_in, ir_out, dr_in, dr_out, startAddr, numToRead);
			
		if (getCharacter("\nInput 'q' to quit loop, else to continue > ") == 'q'){
			Serial.println("Exiting...");
			break;
		}
	}
}


/**
 * According to MAX10 BSDL
 * 
 *   "FLOW_ERASE " &
    "INITIALIZE " &
        "(ISC_ADDRESS_SHIFT 23:000000 WAIT TCK 1)" &
      "(DSM_CLEAR                   WAIT 350.0e-3)," &
 *
 * @brief Erase the entire flash
 * @param ir_in Pointer to ir_in register.
 * @param ir_out Pointer to ir_out register.
 */
void max10_erase_device(const uint8_t ir_len, uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out)
{
	Serial.println("\nErasing device ...");

	clear_reg(ir_in, ir_len);
	clear_reg(dr_in, 32);

	intToBinArray(ir_in, ISC_ENABLE, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	delay(1);

	intToBinArray(ir_in, ISC_ADDRESS_SHIFT, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);
	
	intToBinArray(dr_in, 0x00, 23);
	insert_dr(dr_in, 23, RUN_TEST_IDLE, dr_out);

	delay(1);

	intToBinArray(ir_in, DSM_CLEAR, ir_len);
	insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

	delay(400);

	Serial.println("\nDone");
}


void max10_print_menu()
{
	Serial.flush();	
	Serial.print("\n\nMAX10 FPGA Menu:\n");
	Serial.print("a - Read flash\n");
	Serial.print("b - Read user code\n");
	Serial.print("c - Erase flash\n");
	Serial.print("z - Exit\n");
    Serial.flush();
}


/**
 * @brief Prompts the user to choose what to execute
 * from the available menu of max10 commands.
 */
void max10_main(const uint8_t ir_len,
                uint8_t * ir_in,
                uint8_t * ir_out,
                uint8_t * dr_in,
                uint8_t * dr_out)
{
    max10_print_menu();
    char command = getCharacter("\nmax10 > ");

    switch (command)
    {
    case 'a':
	    // attempt to read address range from ufm
	    max10_readFlashSession(ir_len, ir_in, ir_out, dr_in, dr_out);
        break;

	case 'b':
        // read user code
		Serial.print("\nUser Code: 0x"); 
		Serial.print(max10_read_user_code(ir_len, ir_in, ir_out, dr_in, dr_out), HEX);
		flush_ir_dr(ir_in, dr_out, ir_len, MAX_DR_LEN);
		break;

    case 'c':
    	// erase the whole flash
		max10_erase_device(ir_len, ir_in, ir_out, dr_in, dr_out);
        break;

    case 'z':
        // quit max10 commands menu
        Serial.print("\nGoing back to main menu...");
        break;

    default:
        break;
    }
}
