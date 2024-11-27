#include <avr/io.h>
#include "pins.h"
#include "i2c.h"
#include "lis2hh12_registers.h"
#include "util.h"
#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#define DISPLAY_DUTY_CYCLE 24 //digit is active 1/N of the time, minimum 3
#define BRUSHED_PWM_DIVISOR 486 //determines PWM frequency for brushed motor (260.1 Hz)
#define BRUSHLESS_PWM_DIVISOR 2550 //determines PWM frequency for brushless motor (50 Hz)
#define PWM_MARGIN 1 //if the motor power is within margin of 0 or 255, it will snap to 0 or 255 so the interrupts don't overlap
#define VOLTAGE_DIVISOR 458; //65472/(1.1*13*10)

#define FLIP_DEADZONE 2 //if orientation is within plus or minus this value, drive train power will be set to 0
#define FLIP_TIMEOUT 50 //orientation_filtered must stay constant for this many timer cycles (about 0.2s) to update

// lis2hh12 register values
#define CTRL1_VAL 0x4F //01001111

volatile uint8_t i2c_address = 0;

volatile int16_t xyz[3]; //1g = 16384
const int16_t xyz_down[3] = { //vector indicating which way is down, 1g = 16.384
    0, 12, -9
};
volatile int8_t downness = 0; //ranges from -16 (upside down) to 16 (right side up), not filtered
volatile int8_t orientation_mult = 1;
volatile int8_t orientation_filtered = 1; //for hysteresis noise filtering

volatile uint16_t timer_counter = 0; //max 65536
volatile uint16_t voltmeter_counter = 0; //another timer counter
volatile uint16_t i2c_counter = 0;
volatile uint16_t orientation_counter = 0;
volatile uint16_t motor_update_counter = 0;

volatile uint16_t speed_ramp = 0;

volatile uint8_t digit1 = 0xFF;
volatile uint8_t digit2 = 0xFF;
volatile uint8_t digit3 = 0xFF;

volatile uint16_t voltage;

volatile uint8_t digit_index = 0;

volatile int16_t brushed_1_power = 0; //-255 to 255 (+ is in the direction given by right hand rule with thumb matching shaft, - is opposite)
volatile int16_t brushed_2_power = 0; //-255 to 255 (+ is in the direction given by right hand rule with thumb matching shaft, - is opposite)
volatile int16_t brushless_power = 0;

static const uint8_t PROGMEM digit_array_1[10] = { //add 1 to add a decimal point
    0xf6, 0xc0, 0x6e, 0xea, 0xd8, 0xba, 0xbe, 0xe0, 0xfe, 0xfa
};

static const uint8_t PROGMEM digit_array_2[10] = { //add 2 to add a decimal point
    0xfc, 0x84, 0xd9, 0xd5, 0xa5, 0x75, 0x7d, 0xc4, 0xfd, 0xf5
};

static inline void adc_init(void) {
    PRR0 |= (0 << PRADC); //disable ADC power reduction
    ADMUX = (1 << REFS1) | (1 << REFS0); //select Vref (1.1V) as reference for ADC
    ADMUX |= (1 << ADLAR); //left adjust result (outputs are 8 bits instead of 10)
    ADMUX |= (1 << MUX1) | (1 << MUX0); //select ADC3 (PC3) pin for conversion
    ADCSRA = (1 << ADPS2) | (1 << ADPS1); //ADC input clock divided by 64
    ADCSRA |= (1 << ADEN) | (1 << ADSC); //enable ADC and perform single conversion as required part of initialization
}

static inline void set_digits(uint16_t x) { //display 3 digits
    uint8_t x_hundreds = (x/100) % 10;
    uint8_t x_tens = (x/10) % 10;
    uint8_t x_ones = x % 10;

    digit1 = ~pgm_read_byte(&digit_array_1[x_ones]);
    digit2 = ~pgm_read_byte(&digit_array_2[x_tens]);
    digit3 = ~pgm_read_byte(&digit_array_2[x_hundreds]);
}

static inline void set_digits_signed(int16_t x) { //display 2 digits with a sign
    uint8_t sign = x < 0;
    if (sign) x = -x;
    uint8_t x_tens = (x/10) % 10;
    uint8_t x_ones = x % 10;

    digit1 = ~pgm_read_byte(&digit_array_1[x_ones]);
    digit2 = ~pgm_read_byte(&digit_array_2[x_tens]);
    digit3 = ~(sign<<0); //location of g segment (the one in the center of the digit)
}

static inline void init_timer_0(void) { //display PWM timer
    TCCR0A = (1 << WGM01); //waveform generation mode set to CTC (clear timer on compare match) 
    TCCR0B = (1 << CS01) | (1 << CS00); //clock select bit set to internal clock divided by 64
    TIMSK0 = (1 << OCIE0A); //interrupt enabled for timer output compare match A
    OCR0A = 128; //output compare match A when the timer counts up to this value
    //interrupt should occur at 8000000 Hz / (2*64*(1+OCR0A)) i.e. at 484 Hz
}

static inline void init_timer_1(void) { //brushed motor 1 PWM timer
    TCCR1B = (1 << WGM12); //waveform generation mode set to CTC1 (clear timer on compare match) 
    TCCR1B |= (1 << CS11) | (1 << CS10); //clock select bit set to internal clock divided by 64
    TIMSK1 = (1 << OCIE1A) | (1 << OCIE1B); //interrupt enabled for timer output compare match A and B
    OCR1A = BRUSHED_PWM_DIVISOR; //output compare match A when the timer counts up to this value
    OCR1B = 0; //this variable is the duty cycle times BRUSHED_PWM_DIVISOR 
    //interrupt should occur at 8000000 Hz / (2*64*(1+OCR0A)) i.e. at 260.4 Hz
}

static inline void init_timer_3(void) { //brushed motor 2 PWM timer
    TCCR3B = (1 << WGM32); //waveform generation mode set to CTC1 (clear timer on compare match) 
    TCCR3B |= (1 << CS31) | (1 << CS30); //clock select bit set to internal clock divided by 64
    TIMSK3 = (1 << OCIE3A) | (1 << OCIE3B); //interrupt enabled for timer output compare match A and B
    OCR3A = BRUSHED_PWM_DIVISOR; //output compare match A when the timer counts up to this value
    OCR3B = 0; //this variable is the duty cycle times BRUSHED_PWM_DIVISOR 
    //interrupt should occur at 8000000 Hz / (2*64*(1+OCR0A)) i.e. at 260.4 Hz
}

static inline void init_timer_4(void) { //brushless motor PWM timer
    TCCR4B = (1 << WGM42); //waveform generation mode set to CTC1 (clear timer on compare match) 
    TCCR4B |= (1 << CS41) | (1 << CS40); //clock select bit set to internal clock divided by 64
    TIMSK4 = (1 << OCIE4A) | (1 << OCIE4B); //interrupt enabled for timer output compare match A and B
    OCR4A = BRUSHLESS_PWM_DIVISOR; //output compare match A when the timer counts up to this value
    OCR4B = 0; //this variable is the duty cycle times BRUSHED_PWM_DIVISOR 
    //interrupt should occur at 8000000 Hz / (2*64*(1+OCR0A)) i.e. at 260.4 Hz
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
    
    //things that count up at 484 Hz
    digit_index++; 
    timer_counter++;
    voltmeter_counter++;
    orientation_counter++;
}

static inline uint16_t get_battery_voltage(void) {
    ADCSRA |= (1 << ADSC); //ADC single conversion
    while(!(ADCSRA & (1<<ADIF)));
    ADCSRA |= (1<<ADIF);
    return ((uint16_t)((ADCH << 8) | ADCL))/VOLTAGE_DIVISOR;
}

static inline void set_brushed_duty(void) { //call every time brushed_#_power changes
    OCR1B = (uint16_t)((((uint32_t)abs_int(brushed_1_power)) * BRUSHED_PWM_DIVISOR) / 255);
    OCR3B = (uint16_t)((((uint32_t)abs_int(brushed_2_power)) * BRUSHED_PWM_DIVISOR) / 255);
}

static inline void set_brushless_duty(void) { //call every time brushed_#_power changes
    OCR4B = (uint16_t)(((uint32_t)(brushless_power+765) * BRUSHLESS_PWM_DIVISOR) / (255 * 10 * 4)); //should range from 0.05 duty cycle (ccw) to 0.1 duty cycle (cw)
}

ISR(TIMER1_COMPA_vect) { //pwm 1 on
    if (abs_int(brushed_1_power) >= PWM_MARGIN) { //i.e. if we don't want the signal snapped to always off
        WRITE_PIN(BRUSHED_1_A_PORT, BRUSHED_1_A_PIN, brushed_1_power<0);
        WRITE_PIN(BRUSHED_1_B_PORT, BRUSHED_1_B_PIN, brushed_1_power>0);
    }
}

ISR(TIMER1_COMPB_vect) { //pwm 1 off
    if (abs_int(brushed_1_power) <= (255 - PWM_MARGIN)) { //i.e. if we don't want the signal snapped to always on
        WRITE_PIN(BRUSHED_1_A_PORT, BRUSHED_1_A_PIN, 1);
        WRITE_PIN(BRUSHED_1_B_PORT, BRUSHED_1_B_PIN, 1);
    }
}

ISR(TIMER3_COMPA_vect) { //pwm 2 on
    if (abs_int(brushed_2_power) >= PWM_MARGIN) { //i.e. if we don't want the signal snapped to always off
        WRITE_PIN(BRUSHED_2_A_PORT, BRUSHED_2_A_PIN, brushed_2_power<0);
        WRITE_PIN(BRUSHED_2_B_PORT, BRUSHED_2_B_PIN, brushed_2_power>0);
    }
}

ISR(TIMER3_COMPB_vect) { //pwm 2 off
    if (abs_int(brushed_2_power) <= (255 - PWM_MARGIN)) { //i.e. if we don't want the signal snapped to always on
        WRITE_PIN(BRUSHED_2_A_PORT, BRUSHED_2_A_PIN, 1);
        WRITE_PIN(BRUSHED_2_B_PORT, BRUSHED_2_B_PIN, 1);
    }
}

ISR(TIMER4_COMPA_vect) { //pwm 3 on
    WRITE_PIN(BRUSHLESS_1_PORT, BRUSHLESS_1_PIN, 1);
}

ISR(TIMER4_COMPB_vect) { //pwm 3 off
    WRITE_PIN(BRUSHLESS_1_PORT, BRUSHLESS_1_PIN, 0);
}

int main(void) {
    // set pin directions
    //display
    DDRx(DISP_SER_PORT) |= DISP_SER_PIN; // sets pin as output (if a pin is not set here it defaults to being an input)
    DDRx(DISP_SRCLOCK_PORT) |= DISP_SRCLOCK_PIN;
    DDRx(DISP_RCLOCK_PORT) |= DISP_RCLOCK_PIN;
    DDRx(DISP_DIGIT_1_PORT) |= DISP_DIGIT_1_PIN;
    DDRx(DISP_DIGIT_2_PORT) |= DISP_DIGIT_2_PIN;
    DDRx(DISP_DIGIT_3_PORT) |= DISP_DIGIT_3_PIN;

    //brushed motors
    DDRx(BRUSHED_1_A_PORT) |= BRUSHED_1_A_PIN;
    DDRx(BRUSHED_1_B_PORT) |= BRUSHED_1_B_PIN;
    DDRx(BRUSHED_2_A_PORT) |= BRUSHED_2_A_PIN;
    DDRx(BRUSHED_2_B_PORT) |= BRUSHED_2_B_PIN;

    //brushless motor
    DDRx(BRUSHLESS_1_PORT) |= BRUSHLESS_1_PIN;

    //button pullup
    WRITE_PIN(BUTTON_PORT, BUTTON_PIN, 1);

    i2c_init();
    adc_init();

    init_timer_0();
    init_timer_1();
    init_timer_3();
    //init_timer_4(); //REENABLE TO RUN THE BRUSHLESS MOTOR
    sei(); //enable all interrupts

    set_brushed_duty();
    set_brushless_duty();

    voltage = get_battery_voltage();
    set_digits(voltage);

    uint8_t i2c_send_packet[2] = {CTRL1, CTRL1_VAL};
    i2c_send(&i2c_send_packet[0], 2); //tell the accelerometer what register is being accessed

    while(1) {
        if (timer_counter > 5) {
            timer_counter = 0;
            
            i2c_address = OUT_X_L;
            i2c_send(&i2c_address, 1); //tell the accelerometer what register is being accessed
            i2c_recv(&xyz, 6); //receive data from the register and assign the value to x_axis_h

            downness = ((xyz[0]/1000)*xyz_down[0] + (xyz[1]/1000)*xyz_down[1] + (xyz[2]/1000)*xyz_down[2])/16;

            if (downness <= -FLIP_DEADZONE) orientation_mult = -1; 
            else if (downness >= FLIP_DEADZONE) orientation_mult = 1;
            else orientation_mult = 0;

            //set_digits_signed(orientation_filtered);
        }
        
        if (orientation_filtered != orientation_mult) {
            if (orientation_counter > FLIP_TIMEOUT) orientation_filtered = orientation_mult;
        } else {
            orientation_counter = 0;
        }

        // brushed_1_power = 200*orientation_filtered;
        // brushed_2_power = -200*orientation_filtered;

        if (voltmeter_counter > 4840) { //once every 10 seconds or so
            voltage = get_battery_voltage();
            set_digits(voltage);
            voltmeter_counter = 0;
        }
        
        if (!READ_PIN(BUTTON_PORT, BUTTON_PIN)) {
            set_digits(888);
        }
        
        set_brushed_duty();
    }
}