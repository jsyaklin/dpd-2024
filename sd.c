#include <avr/io.h>
#include "pins.h"
#include <stdint.h>
#include <avr/pgmspace.h>

static inline void init_sd(void) {
    //set MOSI and SCK output, all others input
    DDRx(SD_SCK_PORT) |= SD_SCK_PIN;
    DDRx(SD_MOSI_PORT) |= SD_MOSI_PIN;
    DDRx(SD_MISO_PORT) |= !SD_MISO_PIN;
    DDRx(SD_SS_PORT) |= !SD_SS_PIN;

    CMD0();
    PORTB |= (1 << )
}