#ifndef __MAX10_FUNCS_H__
#define __MAX10_FUNCS_H__
#include "Arduino.h"


uint32_t max10_read_user_code(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out);
void max10_read_ufm_range(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out, uint32_t const start, uint32_t const num);
void max10_read_ufm_range_burst(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out, uint32_t const start, uint32_t const num);
void max10_readFlashSession(uint8_t * ir_in, uint8_t * ir_out, uint8_t * dr_in, uint8_t * dr_out);
void max10_erase_device(uint8_t * ir_in, uint8_t * ir_out);
void max10_main(const uint8_t ir_len,
                uint8_t * ir_in,
                uint8_t * ir_out,
                uint8_t * dr_in,
                uint8_t * dr_out);

#endif /* __MAX10_FUNCS_H__ */
