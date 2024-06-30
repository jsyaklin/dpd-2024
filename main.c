#include <avr/io.h>
#include "pins.h"
#include <stdint.h>

void set_register(uint8_t v) {
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

int main(void) {
    // set pin directions
    DDRx(DISP_SER_PORT) |= DISP_SER_PIN; // sets pin as output
    DDRx(DISP_SRCLOCK_PORT) |= DISP_SRCLOCK_PIN;
    DDRx(DISP_RCLOCK_PORT) |= DISP_RCLOCK_PIN;
    DDRx(DISP_DIGIT_1_PORT) |= DISP_DIGIT_1_PIN;
    DDRx(DISP_DIGIT_2_PORT) |= DISP_DIGIT_2_PIN;
    DDRx(DISP_DIGIT_3_PORT) |= DISP_DIGIT_3_PIN;

    // set desired digit select to high
    WRITE_PIN(DISP_DIGIT_1_PORT, DISP_DIGIT_1_PIN, 0);
    WRITE_PIN(DISP_DIGIT_2_PORT, DISP_DIGIT_2_PIN, 1);
    WRITE_PIN(DISP_DIGIT_3_PORT, DISP_DIGIT_3_PIN, 0);

    set_register(0);

    while(1) {

    }
}