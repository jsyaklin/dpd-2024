#pragma once

#include <stdint.h>

void i2c_init(void);
uint8_t i2c_send(const uint8_t* data, uint8_t len); //data should be a POINTER to the data being sent (using the & operator)
uint8_t i2c_recv(uint8_t* data, uint8_t len); //data should be a POINTER to the variable that receives the data (using the & operator)
