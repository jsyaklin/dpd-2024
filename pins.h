#pragma once

#include <avr/io.h>

#define DDRx(p) (*(&p-1))
#define PINx(p) (*(&p-2))
#define PORTx(p) p

//display pins
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

//brushed motor pins
#define BRUSHED_1_A_PORT PORTD
#define BRUSHED_1_A_PIN (1<<4)

#define BRUSHED_1_B_PORT PORTD
#define BRUSHED_1_B_PIN (1<<5)

#define BRUSHED_2_A_PORT PORTD
#define BRUSHED_2_A_PIN (1<<6)

#define BRUSHED_2_B_PORT PORTD
#define BRUSHED_2_B_PIN (1<<7)

//brushless motor pins
#define BRUSHLESS_1_PORT PORTB
#define BRUSHLESS_1_PIN (1<<6)

//sd card pins
#define SD_MISO_PORT PORTC //master in slave out
#define SD_MISO_PIN (1<<0)

#define SD_SCK_PORT PORTC //s clock
#define SD_SCK_PIN (1<<1)

#define SD_SS_PORT PORTE //slave select
#define SD_SS_PIN (1<<2)

#define SD_MOSI_PORT PORTE //master out slave in
#define SD_MOSI_PIN (1<<3)

//battery voltage read pin
#define BATTERY_VOLTAGE_PORT PORTC
#define BATTERY_VOLTAGE_PIN (1<<3)

//button pin
#define BUTTON_PORT PORTC
#define BUTTON_PIN (1<<2)

#define WRITE_PIN(port, pin, state) do { \
    if (state) PORTx(port) |= (pin); \
    else PORTx(port) &= ~(pin); \
    } while (0)

#define READ_PIN(port, pin) (PINx(port) & (pin))