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

#include "art.h"
#include "jtagger.h"
#include "max10_funcs.h"


// Global Variables
uint32_t idcode = 0;
uint8_t dr_out[MAX_DR_LEN] = {0};  // TODO put these variables inside tap_t
uint8_t dr_in[MAX_DR_LEN] = {0};
uint8_t ir_len = 1;
String digits = "";
tap_state current_state;
tap_t taps[MAX_ALLOWED_TAPS];

int detect_chain(uint8_t* out)
{
    uint8_t id_arr[32] = {0};
    uint32_t idcode = 0;
    uint32_t i = 0;
    uint8_t counter = 0;
    int rc = OK;

    reset_tap();

    // try to read IDCODE first and then detect the IR length
    advance_tap_state(RUN_TEST_IDLE);
    advance_tap_state(SELECT_DR);
    advance_tap_state(CAPTURE_DR);
    advance_tap_state(SHIFT_DR);
    
    // shift out the IDCODE from the id code register
    // assumed that the IDCODE IR is the default IR after power up.
    // first bit to read out is the LSB of the IDCODE.
    for (i = 0; i < 32; i++)
    {
        advance_tap_state(SHIFT_DR);
        id_arr[i] = digitalRead(TDO);
    }
    advance_tap_state(EXIT1_DR);

    // LSB of IDCODE must be 1.
    if (id_arr[0] != 1)
    {
        Serial.println("\n\nBad IDCODE or not implemented, LSB = 0");
        return -ERR_BAD_IDCODE;
    }
    // turn IDCODE bits into unsigned integer
    rc = binArrayToInt(id_arr, 32, &idcode);
    if (rc != OK) {
        return rc;
    }

    Serial.print("\nFound IDCODE: ");
    printArray(id_arr, 32);
    Serial.print(" (0x");
    Serial.print(idcode, HEX);
    Serial.print(")");

    // find ir length.
    Serial.print("\nAttempting to find IR length of target ...\n");
    reset_tap();
    advance_tap_state(RUN_TEST_IDLE);
    advance_tap_state(SELECT_DR);
    advance_tap_state(SELECT_IR);
    advance_tap_state(CAPTURE_IR);
    advance_tap_state(SHIFT_IR);
    
    // shift in about MANY_ONES amount of ones into TDI to clear the register
    // from its previos content. then shift a single zero followed by
    // a bunch of ones and cout the amount of clock cycles from inserting zero
    // till we read it in TDO.

    digitalWrite(TDI, 1);
    for (i = 0; i < MANY_ONES; ++i) 
    {
        advance_tap_state(SHIFT_IR);
    }

    digitalWrite(TDI, 0);
    advance_tap_state(SHIFT_IR);

    digitalWrite(TDI, 1);
    for (i = 0; i < MANY_ONES; ++i)
    {
        advance_tap_state(SHIFT_IR);

        if (digitalRead(TDO) == 0)
        {
            counter++;
            *out = counter;
            return OK;
        }
        counter++;
    }

    advance_tap_state(EXIT1_IR);
    advance_tap_state(UPDATE_IR);
    advance_tap_state(RUN_TEST_IDLE);

    Serial.println("\nDidn't find valid IR length");
    return -ERR_UNVALID_IR_OR_DR_LEN;
}

int chr2hex(char ch)
{
    if (ch >= 'a' && ch <= 'f')
        return ch - 0x57;

    if (ch >= 'A' && ch <= 'F')
        return ch - 0x37;

    if (ch >= '0' && ch <= '9')
        return ch - 0x30;

    return -ERR_OUT_OF_BOUNDS;
}

int binArrayToInt(uint8_t* arr, int len, uint32_t* out)
{	
    uint32_t integer = 0;
    uint32_t mask = 1;

    if (len > 32){
        Serial.print("\nbinArrayToInt: array size too large");
        Serial.println("\nBad conversion.");
        Serial.flush();
        return -ERR_BAD_CONVERSION;
    }

    for (int i = 0; i < len; i++) {
        if (arr[i]) {
            integer |= mask;
        }
        mask = mask << 1;
    }

    *out = integer;
    return OK;
}

int binStringToInt(String str, uint32_t* out)
{
    uint32_t integer = 0;
    uint32_t mask = 1;

    if (str.length() > 32){
        Serial.println("\nbinStrToInt: string length too large");
        Serial.println("Bad conversion.");
        return -ERR_BAD_CONVERSION;
    }

    for (int i = str.length() - 1; i >= 0; i--)
    {
        if (str[i] == '1')
            integer |= mask;

        mask = mask << 1;
    }

    *out = integer;
    return OK;
}

void clear_serial_rx_buf() {
    while (Serial.available())
    {
        Serial.read();
    }
}

int binStrToBinArray(uint8_t* arr, int arrSize, String str ,int strSize)
{
    int i = 0;

    if (strSize > arrSize)
    {
        Serial.print("\nbinStrToBinArray: size of string is larger than destination array.");
        Serial.print("\nDestination array size: "); Serial.print(arrSize);
        Serial.print("\nString requires: "); Serial.print(strSize);
        Serial.println("\nBad Conversion");
        Serial.flush();
        return -ERR_BAD_CONVERSION;
    }

    clear_reg(arr, arrSize);

    // last digit in received string is the least significant
    for (i = strSize - 1; i >= 0; i--)
        arr[strSize - 1 - i] = str[i] - 0x30; // ascii to unsigned int    

    // fill the remaining array elements with zeros
    for (i = strSize; i < arrSize; i++)
        arr[i] = 0;

    return OK;
}

int hexStrToBinArray(uint8_t* arr, int arrSize, String str, int strSize)
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
            Serial.print("\nhexStrToBinArray: destination array not large enough, ");
            Serial.print("size: "); Serial.print(arrSize);
            Serial.print("\nString requires size: "); Serial.print(strSize * 4);
            Serial.print("\nVacant bits: "); Serial.print(vacantBits);
            Serial.println("\nBad Conversion");
            Serial.flush();
            return -ERR_BAD_CONVERSION;
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

        if (n == -1)
        {
            Serial.println("\nhexStrToHexArray: bad digit type");
            Serial.println("Bad Conversion");
            return -ERR_BAD_CONVERSION;
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

        j += 4; // update destination array index
    }

    return OK;
}

int decStrToBinArray(uint8_t* arr, int arrSize, String str, int strSize)
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
            Serial.print("\ndecStrToBinArray: destination array not large enough");
            Serial.print("\nDestination array size in bits: "); Serial.print(arrSize);
            Serial.print("\nString requires size: "); Serial.print(strSize * 4);
            Serial.print("\nVacant bits: "); Serial.print(vacantBits);
            Serial.println("\nBad Conversion");
            Serial.flush();
            return -ERR_BAD_CONVERSION;
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

        if (n == -1)
        {
            Serial.println("\nhexStrToHexArray: bad digit type");
            Serial.println("Bad Conversion");
            return -ERR_BAD_CONVERSION;
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

    return OK;
}

int intToBinArray(uint8_t* arr, uint32_t n, uint16_t len)
{
    if (len > 32) // TODO: increase this to 64 or 256 or max_dr_len actually ?
    {
        Serial.print("\nintToBinArray: array size is larger than 32");
        Serial.println("\nBad Conversion");
        return -ERR_BAD_CONVERSION;
    }
    uint32_t mask = 1;

    clear_reg(arr, len);

    for (int i = 0; i < len; i++) {
        if (n & mask)
            arr[i] = 1;
        else
            arr[i] = 0;
        mask <<= 1;
    }

    return OK;
}

void notify_input_and_busy_wait_for_serial_input(const char* message)
{
    clear_serial_rx_buf(); // first, clean the input buffer
    Serial.print(message); // notify user to input a value
    Serial.flush();
    while (Serial.available() == 0) {}
}

char getCharacter(const char* message)
{
    char inChar[1] = {0};

    notify_input_and_busy_wait_for_serial_input(message);
    Serial.readBytesUntil('\n', inChar, 1);
    char chr = inChar[0];

#if DEBUGSERIAL
    Serial.print("\nchar: "); Serial.println(chr);
    Serial.flush();
#endif
    return chr;
}


String getString(const char* message)
{
    String str;

    notify_input_and_busy_wait_for_serial_input(message);
    str = Serial.readStringUntil('\n');

#if DEBUGSERIAL
    Serial.print("\nstring: ");	Serial.println(str);
    Serial.print("string length = "); Serial.println(str.length());
    Serial.flush();
#endif
    return str;
}

void fetchNumber(const char* message)
{
    notify_input_and_busy_wait_for_serial_input(message);
    digits = Serial.readStringUntil('\n');

#if DEBUGSERIAL
    Serial.print("\ndigits: ");	Serial.println(digits);
    Serial.print("digits length = "); Serial.println(digits.length());
    Serial.flush();
#endif
}

uint32_t getInteger(int num_bytes, const char* message)
{
    char myData[num_bytes];

    size_t m = Serial.readBytesUntil('\n', myData, num_bytes);
    myData[m] = '\0';  // insert null charcater

#if DEBUGSERIAL
    // shows: the hexadecimal string from user
    Serial.print("myData: "); Serial.println(myData);
#endif
    // convert string to hexadeciaml value
    uint32_t z = strtol(myData, NULL, 16);

#if DEBUGSERIAL
    Serial.print("received: 0x");
    Serial.println(z, HEX); // shows 12A3
    Serial.flush();
#endif
    return z;
}

int parseNumber(uint8_t* dest, uint16_t size, const char* message, uint32_t* out)
{
    int rc = OK;
    char prefix = '0';
    
    // set a default parsed value
    *out = 0;

    // fetch the digits from the Serial interface
    fetchNumber(message);
    
    // received hex or bin number with prefix or a decimal witout prefix
    if (digits.length() >= 2)
        prefix = digits[1];	
    // else, no prefix, just a decimal number with 1 digit

    switch (prefix)
    {
    // user sent hexadecimal format
    case 'x':
        // cut out the digits without the prefix
        digits = digits.substring(2);

        if (dest != NULL) {
            rc = hexStrToBinArray(dest, size, digits, digits.length());
            if (rc != OK)
                goto exit;
        }

        // Prepare the character array (the buffer)
        char tmp[digits.length() + 1];  // with 1 extra char for '/0'
        // convert String to char array
        digits.toCharArray(tmp, digits.length() + 1);
        // add null terminator
        tmp[digits.length()] = '\0';
        // convert to unsigned int
        *out = strtoul(tmp, NULL, 16);
        break;

    // user sent binary format
    case 'b':
        // cut out the digits without the prefix
        digits = digits.substring(2);

        // convert binary to integer
        if (dest != NULL) {
            rc = binStrToBinArray(dest, size, digits, digits.length());
            if (rc != OK)
                goto exit;
        }

        rc = binStringToInt(digits, out);
        break;

    // user sent decimal format
    default:
        if (isDigit(prefix) && digits.length() > 0)
        {
            // construct a whole decimal from the string
            char tmp[digits.length() + 1];
            digits.toCharArray(tmp, digits.length() + 1);
            tmp[digits.length()] = '\0';
            *out = strtoul(tmp, NULL, 10);

            if (dest != NULL) {
                rc = intToBinArray(dest, *out, size);
            }
        } else {
            Serial.println("\nBad prefix, didn't get number");
            rc = -ERR_BAD_PREFIX_OR_SUFFIX;
        }
        break;
    }

exit:
    return rc;
}

void reset_tap()
{
#if PRINT_RESET_TAP
    Serial.print("\nResetting TAP\n");
#endif
    for (uint8_t i = 0; i < 5; ++i)
    {
        digitalWrite(TMS, 1);
        digitalWrite(TCK, 0); HC;
        digitalWrite(TCK, 1); HC;
    }
    current_state = TEST_LOGIC_RESET;
}

void insert_dr(uint8_t* dr_in, uint8_t dr_len, uint8_t end_state, uint8_t* dr_out)
{
    // make sure that current state is TLR
    uint16_t i = 0;

    advance_tap_state(RUN_TEST_IDLE);
    advance_tap_state(SELECT_DR);
    advance_tap_state(CAPTURE_DR);
    advance_tap_state(SHIFT_DR);

    // shift data bits into DR. make sure that first bit is LSB
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

void insert_ir(uint8_t* ir_in, uint8_t ir_len, uint8_t end_state, uint8_t* ir_out)
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
        ir_out[i] = digitalRead(TDO);  // read the shifted out bits. LSB first
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

void clear_reg(uint8_t* reg, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++)
        reg[i] = 0;
}

void flush_ir_dr(uint8_t* ir_reg, uint8_t* dr_reg, uint16_t ir_len, uint16_t dr_len)
{
    clear_reg(ir_reg, ir_len);
    clear_reg(dr_reg, dr_len);
}

uint32_t detect_dr_len(uint8_t* instruction, uint8_t ir_len, uint32_t process_ticks)
{	
    // make sure that current state is TLR prior this calling this function.

    // temporary array to strore the shifted out bits from IR
    uint8_t tmp[ir_len];
    uint32_t i = 0;
    uint32_t counter = 0;

    // insert the instruction we wish to check into ir
    insert_ir(instruction, ir_len, RUN_TEST_IDLE, tmp);
    
    // a couple of clock cycles to process the instruction
    for (i = 0; i < process_ticks; i++)
    {
        HC; HC;
    }

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

int discovery(uint32_t first, uint32_t last, uint16_t max_dr_len, uint8_t* ir_in, uint8_t* ir_out)
{
    uint32_t instruction, len = 0;
    int rc = OK;

    // discover all dr lengths corresponding to their ir.
    Serial.print("\n\nDiscovery of instructions from 0x"); Serial.print(first, HEX);
    Serial.print(" to 0x"); Serial.println(last, HEX);

    for (instruction=first; instruction <= last; instruction++)
    {
        // reset tap
        reset_tap();
        len = 0;

        // prepare to shift instruction
        intToBinArray(ir_in, instruction, ir_len);

        Serial.print("\nDetecting DR length for IR: ");
        printArray(ir_in, ir_len);
        Serial.print(" (0x"); Serial.print(instruction, HEX); Serial.print(")");
        Serial.flush();

        len = detect_dr_len(ir_in, ir_len, 4);
        if (len == max_dr_len) {
            Serial.println("\nDiscovery: TDO is stuck at 1");
            rc = -ERR_TDO_STUCK_AT_1;
            break;
        }

        Serial.print(" ... "); Serial.print(len, DEC);
        Serial.flush();
    }

    reset_tap();
    Serial.println("\n\n   Done");
    return rc;
}

void taps_init(tap_t* taps)
{
    for (size_t i = 0; i < MAX_ALLOWED_TAPS; i++)
    {
        taps[i].num = i;
        taps[i].idcode = 0;
        taps[i].ir_len = 0;
        taps[i].name = {0};
        taps[i].is_jtag_swd = 0; // jtag=0, swd=1
    }
}

tap_t* tap_selector(tap_t* taps, int which)
{

}

int advance_tap_state(uint8_t next_state)
{
    int rc = OK;

    switch ( current_state )
    {
        case TEST_LOGIC_RESET:
            if (next_state == RUN_TEST_IDLE) {
                // go to run test idle
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = RUN_TEST_IDLE;
            }
            else if (next_state == TEST_LOGIC_RESET) {
                // stay in test logic reset
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            break;

        case RUN_TEST_IDLE:
            if (next_state == SELECT_DR) {
                // go to select dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SELECT_DR;
            }
            else if (next_state == RUN_TEST_IDLE) {
                // stay in run test idle
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            break;

        case SELECT_DR:
            if (next_state == CAPTURE_DR) {
                // go to capture dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = CAPTURE_DR;
            }
            else if (next_state == SELECT_IR) { 
                // go to select ir
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SELECT_IR;
            }
            break;

        case CAPTURE_DR:
            if (next_state == SHIFT_DR) {
                // go to shift dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SHIFT_DR;
            }
            else if (next_state == EXIT1_DR) { 
                // go to exit1 dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT1_DR;
            }
            break;

        case SHIFT_DR:
            if (next_state == SHIFT_DR) {
                // stay in shift dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            else if (next_state == EXIT1_DR) {
                // go to exit1 dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT1_DR;
            }
            break;

        case EXIT1_DR:
            if (next_state == PAUSE_DR) {
                // go to pause dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = PAUSE_DR;
            }
            else if (next_state == UPDATE_DR) {
                // go to update dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = UPDATE_DR;
            }
            break;

        case PAUSE_DR:
            if (next_state == PAUSE_DR) {
                // stay in pause dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            else if (next_state == EXIT2_DR) {
                // go to exit2 dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT2_DR;
            }
            break;

        case EXIT2_DR:
            if (next_state == SHIFT_DR) {
                // go to shift dr
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SHIFT_DR;
            }
            else if (next_state == UPDATE_DR) {
                // go to update dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = UPDATE_DR;
            }
            break;

        case UPDATE_DR:
            if (next_state == RUN_TEST_IDLE) {
                // go to run test idle
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = RUN_TEST_IDLE;
            }
            else if (next_state == SELECT_DR) {
                // go to select dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SELECT_DR;
            }
            break;

        case SELECT_IR:
            if (next_state == CAPTURE_IR) {
                // go to capture ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = CAPTURE_IR;
            }
            else if (next_state == TEST_LOGIC_RESET) {
                // go to test logic reset
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = TEST_LOGIC_RESET;
            }
            break;

        case CAPTURE_IR:
            if (next_state == SHIFT_IR) {
                // go to shift ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SHIFT_IR;
            }
            else if (next_state == EXIT1_IR) {
                // go to exit1 ir
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT1_IR;
            }
            break;

        case SHIFT_IR:
            if (next_state == SHIFT_IR) {
                // stay in shift ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            else if (next_state == EXIT1_IR) {
                // go to exit1 ir
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT1_IR;
            }
            break;

        case EXIT1_IR:
            if (next_state == PAUSE_IR) {
                // go to pause ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = PAUSE_IR;
            }
            else if (next_state == UPDATE_IR) {
                // go to update ir
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = UPDATE_IR;
            }
            break;

        case PAUSE_IR:
            if (next_state == PAUSE_IR) {
                // stay in pause ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
            }
            else if (next_state == EXIT2_IR) {
                // go to exit2 dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = EXIT2_IR;
            }
            break;

        case EXIT2_IR:
            if (next_state == SHIFT_IR) {
                // go to shift ir
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SHIFT_IR;
            }
            else if (next_state == UPDATE_IR) {
                // go to update ir
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = UPDATE_IR;
            }
            break;

        case UPDATE_IR:
            if (next_state == RUN_TEST_IDLE) {
                // go to run test idle
                digitalWrite(TMS, 0);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = RUN_TEST_IDLE;
            }
            else if (next_state == SELECT_DR) {
                // go to select dr
                digitalWrite(TMS, 1);
                digitalWrite(TCK, 0); HC;
                digitalWrite(TCK, 1); HC;
                current_state = SELECT_DR;
            }
            break;

        default:
            Serial.println("Error: incorrent TAP state !");
            rc = -ERR_BAD_TAP_STATE;
            break;
    }
#if DEBUGTAP
    Serial.print("\ntap state: ");
    Serial.print(current_state, HEX);
#endif
    return rc;
}

char serialEvent(char character)
{
  char inChar = '\0';

  while (Serial.available() == 0)
  {
    // get the new byte:
    inChar = (char)Serial.read();
    // if the incoming character equals to the argument, 
    // break from while and proceed to main loop
    // do something about it:
    if (inChar == character) {
      break;
    }
  }

  Serial.flush();
  return inChar;
}

void printArray(uint8_t* arr, uint32_t len)
{
    for (int16_t i = len - 1; i >= 0; i--)
        Serial.print(arr[i], HEX);

    Serial.flush();
}

void sendDataToHost(uint8_t* buf, uint16_t chunk_size)
{
    for (int i = 0; i < chunk_size; ++i)
        Serial.write(buf[i]);

    Serial.flush();
}

void print_welcome()
{
    Serial.println();
    Serial.write(art0, sizeof(art0)); Serial.flush();
    Serial.write(art1, sizeof(art1)); Serial.flush();
    Serial.write(art2, sizeof(art2)); Serial.flush();
    Serial.write(art3, sizeof(art3)); Serial.flush();
    Serial.write(art4, sizeof(art4)); Serial.flush();
    Serial.write(art5, sizeof(art5)); Serial.flush();
    Serial.write(art6, sizeof(art6)); Serial.flush();
    Serial.write(art7, sizeof(art7)); Serial.flush();
    Serial.write(art8, sizeof(art8)); Serial.flush();
}

void print_main_menu()
{
    Serial.flush();	
    Serial.print("\n---------\nMain Menu\n\n");
    Serial.print("\tAll numerical parameters should be passed in the format: {0x || 0b || decimal}\n\n");
    Serial.print("c - Connect to chain\n");
    Serial.print("d - Discovery\n");
    Serial.print("i - Insert IR\n");
    Serial.print("l - Detect DR length\n");
    Serial.print("r - Insert DR\n");
    Serial.print("t - Reset TAP state machine\n");
    Serial.print("q - Toggle TRST line\n");
    Serial.print("m - MAX10 FPGA commands\n");
    Serial.print("h - Show this menu\n");
    Serial.print("z - Exit\n");
    Serial.flush();
}

void setup()
{
    // initialize mode for standard IEEE 1149.1 JTAG pins
    pinMode(TCK, OUTPUT);
    pinMode(TMS, OUTPUT);
    pinMode(TDI, OUTPUT);
    pinMode(TDO, INPUT_PULLUP);
    pinMode(TRST, OUTPUT);

    // initialize pins state
    digitalWrite(TCK, 0);
    digitalWrite(TMS, 1);
    digitalWrite(TDI, 1);
    digitalWrite(TRST, 1);

    // initialize possible TAPs in chain
    taps_init();

    // initialize serial communication
    Serial.begin(115200);
    while (!Serial) { }
    Serial.setTimeout(500); // set timeout for various serial R/W funcs
}

void loop()
{
    char command = '0';
    int rc = OK;
    uint32_t num = 0;
    uint32_t dr_len = 0;
    uint32_t nbits, first_ir, final_ir = 0;
    uint32_t max_dr_len = 0;
    current_state = TEST_LOGIC_RESET;

    // to begin session
    String start = getString("Insert 'start' > ");
    if (start != "start") {
        Serial.println("Invalid, reset the Arduino and try again");
        while(1);
    }
    
    print_welcome();

    // detect chain and read idcode
    Serial.println("Attempting to connect to chain");
    rc = detect_chain(&ir_len);
    if (rc != OK) {
        Serial.print("Could not detect valid IR length\n");
        goto inf_loop;
    }
    Serial.print("IR length: "); Serial.println(ir_len, DEC);

    if (ir_len <= 0) {
        Serial.print("IR length must be > 0 to perform any useful JTAG operations\n");
        Serial.println("You must reset the Arduino to retry");
        goto inf_loop;
    }

    // define ir register according to ir length
    uint8_t ir_in[ir_len];
    uint8_t ir_out[ir_len];
    reset_tap();

    print_main_menu();

    while (true)
    {
        num = 0;
        command = getCharacter("\ncmd > ");

        switch (command)
        {
        case 'c':
            // attempt to connect to chain and read idcode
            rc = detect_chain(&ir_len);
            Serial.print("IR length: "); Serial.print(ir_len);
            break;

        case 'd':
            // discovery of existing IRs
            rc = parseNumber(NULL, 20, "First IR > ", &first_ir);
            if (rc != OK) break;
            rc = parseNumber(NULL, 20, "Final IR > ", &final_ir);
            if (rc != OK) break;
            rc = parseNumber(NULL, 20, "Max allowed DR length > ", &max_dr_len);
            if (rc != OK) break;

            discovery(first_ir, final_ir, max_dr_len, ir_in, ir_out);
            break;

        case 'i':
            // insert ir
            rc = parseNumber(ir_in, ir_len, "\nShift IR > ", &num);
            if (rc != OK) break;
            insert_ir(ir_in, ir_len, RUN_TEST_IDLE, ir_out);

            Serial.print("\nIR  in: ");
            printArray(ir_in, ir_len);
            
            // print the hex value if length is not to large
            if (ir_len <= 32) {
                binArrayToInt(ir_in, ir_len, &num); // TODO: do I need this aftwr parseNumber ?
                Serial.print(" | 0x"); Serial.print(num, HEX);
            }

            Serial.print("\nIR out: ");
            printArray(ir_out, ir_len);

            // print the hex value if length is not to large
            if (ir_len <= 32) {
                binArrayToInt(ir_out, ir_len, &num);
                Serial.print(" | 0x"); Serial.print(num, HEX);
            }
            break;

        case 'l':
            // detect current dr length
            dr_len = detect_dr_len(ir_in, ir_len, 4);
            if (dr_len == 0) {
                Serial.println("\nDidn't find the current DR length, TDO is stuck");
            }
            else {
                Serial.print("\nDR length: ");
                Serial.print(dr_len);
            }
            break;

        case 'm':
            // TODO: this letter should lead to a menu of all existing targets-
            // instead of just a single target
            // entering into MAX10 FPGA command menu
            max10_main(ir_len, ir_in, ir_out, dr_in, dr_out);
            break;

        case 'r':
            // insert dr
            rc = parseNumber(NULL, 32, "Enter amount of bits to shift > ", &nbits);
            if (nbits == 0 || rc != OK)
                break;

            rc = parseNumber(dr_in, nbits, "\nShift DR > ", &nbits);
            if (rc != OK) break;

            insert_dr(dr_in, nbits, RUN_TEST_IDLE, dr_out);

            Serial.print("\nDR  in: ");
            printArray(dr_in, nbits);
            
            // print the hex value if lenght is not large enough
            if (nbits <= 32) {
                binArrayToInt(dr_in, nbits, &num); // TODO needed after parseNum ?
                Serial.print(" | 0x"); Serial.print(num, HEX);
            }
            
            Serial.print("\nDR out: ");
            printArray(dr_out, nbits);
            
            // print the hex value if lenght is not large enough
            if (nbits <= 32) {
                binArrayToInt(dr_out, nbits, &num);  // TODO same as above
                Serial.print(" | 0x"); Serial.print(num, HEX);
            }
            break;

        case 't':
            Serial.println("Resetting TAP");
            reset_tap();
            break;
        
        case 'q':
            Serial.println("Toggling TRST line");
            digitalWrite(TRST, 0);
            HC; HC; HC; HC; HC; HC; HC; HC;
            digitalWrite(TRST, 1);
            break;

        case 'h':
            print_main_menu();
            break;

        case 'z':
            Serial.print("\nExiting...\nReset Arduino to start again");
            reset_tap();
            goto inf_loop;

        default:
            Serial.println("Invalid Command");
            break;
        }
    }

    reset_tap();
inf_loop:
    Serial.end();
    while(1); // loop in place
}

