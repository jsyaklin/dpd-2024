#include <avr/io.h>
#include "pins.h"
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define DISPLAY_DUTY_CYCLE 24 // digit is active 1/N of the time, minimum 3

volatile uint8_t digit1 = 0xFF;
volatile uint8_t digit2 = 0xFF;
volatile uint8_t digit3 = 0xFF;

uint8_t digit_index = 0;

static const uint8_t PROGMEM digit_array_1[10] = { //add 1 to add a decimal point
    0xf6, 0xc0, 0x6e, 0xea, 0xd8, 0xba, 0xbe, 0xe0, 0xfe, 0xfa
};

static const uint8_t PROGMEM digit_array_2[10] = { //add 2 to add a decimal point
    0xfc, 0x84, 0xd9, 0xd5, 0xa5, 0x75, 0x7d, 0xc4, 0xfd, 0xf5
};

static inline void set_digits(uint16_t x) {
    uint8_t x_hundreds = (x/100) % 10;
    uint8_t x_tens = (x/10) % 10;
    uint8_t x_ones = x % 10;

    digit1 = ~pgm_read_byte(&digit_array_1[x_ones]);
    digit2 = ~pgm_read_byte(&digit_array_2[x_tens]);
    digit3 = ~pgm_read_byte(&digit_array_2[x_hundreds]);

}

static inline void init_timer_0(void) {
    TCCR0A = (1 << WGM01); //waveform generation mode set to CTC (clear timer on compare match) 
    TCCR0B = (1 << CS01) | (1 << CS00); //clock select bit set to internal clock divided by 64
    TIMSK0 = (1 << OCIE0A); //interrupt enabled for timer output compare match A
    OCR0A = 128; //output compare match A when the timer counts up to 255
    //interrupt should occur at 8000000 Hz / (2*64*(1+OCR0A)) i.e. at 484 Hz
}

static inline void set_register(uint8_t v) {
    for (uint8_t i = 0; i < 8; i++) {
        WRITE_PIN(DISP_SER_PORT, DISP_SER_PIN, v & 1);
        WRITE_PIN(DISP_SRCLOCK_PORT, DISP_SRCLOCK_PIN, 1);
        asm volatile("nop\n\t");
        WRITE_PIN(DISP_SRCLOCK_PORT, DISP_SRCLOCK_PIN, 0);
        v >>= 1;
    } 
    WRITE_PIN(DISP_RCLOCK_PORT, DISP_RCLOCK_PIN, 1);
    asm volatile("nop\n\t");
    WRITE_PIN(DISP_RCLOCK_PORT, DISP_RCLOCK_PIN, 0);
}

ISR(TIMER0_COMPA_vect) //timer 0 interrupt (7seg display)
{
    WRITE_PIN(DISP_DIGIT_1_PORT, DISP_DIGIT_1_PIN, 0);
    WRITE_PIN(DISP_DIGIT_2_PORT, DISP_DIGIT_2_PIN, 0);
    WRITE_PIN(DISP_DIGIT_3_PORT, DISP_DIGIT_3_PIN, 0);

    switch(digit_index) { //each digit is active at 1/8 duty cycle, 1/8 out of phase from the other digits
        case 1:
            set_register(digit1);
            WRITE_PIN(DISP_DIGIT_1_PORT, DISP_DIGIT_1_PIN, 1);
            break;
        case 2:
            set_register(digit2);
            WRITE_PIN(DISP_DIGIT_2_PORT, DISP_DIGIT_2_PIN, 1);
            break;
        case 3:
            set_register(digit3);
            WRITE_PIN(DISP_DIGIT_3_PORT, DISP_DIGIT_3_PIN, 1);
            break;
        case DISPLAY_DUTY_CYCLE:
            digit_index = 0;
            break;
    }
    
    digit_index++;
}

int main(void) {
    // set pin directions
    DDRx(DISP_SER_PORT) |= DISP_SER_PIN; // sets pin as output
    DDRx(DISP_SRCLOCK_PORT) |= DISP_SRCLOCK_PIN;
    DDRx(DISP_RCLOCK_PORT) |= DISP_RCLOCK_PIN;
    DDRx(DISP_DIGIT_1_PORT) |= DISP_DIGIT_1_PIN;
    DDRx(DISP_DIGIT_2_PORT) |= DISP_DIGIT_2_PIN;
    DDRx(DISP_DIGIT_3_PORT) |= DISP_DIGIT_3_PIN;

    init_timer_0();
    sei(); //enable all interrupts

    set_digits(345);

    while(1) {

    }
}