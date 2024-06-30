#pragma once

#include <avr/io.h>

#define DDRx(p) (*(&p-1))
#define PINx(p) (*(&p-2))
#define PORTx(p) p

#define DISP_SER_PORT PORTB
#define DISP_SER_PIN (1<<4)

#define DISP_SRCLOCK_PORT PORTB
#define DISP_SRCLOCK_PIN (1<<3)

#define DISP_RCLOCK_PORT PORTB
#define DISP_RCLOCK_PIN (1<<5)

#define DISP_DIGIT_1_PORT PORTB
#define DISP_DIGIT_1_PIN (1<<2)

#define DISP_DIGIT_2_PORT PORTB
#define DISP_DIGIT_2_PIN (1<<1)

#define DISP_DIGIT_3_PORT PORTB
#define DISP_DIGIT_3_PIN (1<<0)

#define WRITE_PIN(port, pin, state) do { \
    if (state) PORTx(port) |= (pin); \
    else PORTx(port) &= ~(pin); \
    } while (0)
