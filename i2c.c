#include "i2c.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/twi.h>
#include <util/delay.h>
#include <stdint.h>

#define DEV_ADDR 0x1E // device address

#define FLAG_NACK 0x1
#define FLAG_BUSY 0x2
#define FLAG_READ 0x4 // if 1, the next transfer will be a read

#define SENDBUF_SIZE 16
#define RECVBUF_SIZE 16

// (2*BR*Pre + 16)*SCL = F_CPU
// 2*BR*Pre = F_CPU/SCL - 16
#define I2C_FREQ 50000
#define I2C_DIV ((F_CPU / I2C_FREQ - 16) / 2)

static volatile uint8_t flags = 0;

static volatile uint8_t send_count = 0;
static volatile uint8_t recv_count = 0;

static volatile uint8_t send_pos = 0;
static volatile uint8_t recv_pos = 0;

static volatile uint8_t send_buf[SENDBUF_SIZE];
static volatile uint8_t recv_buf[RECVBUF_SIZE];

ISR(TWI0_vect)
{
    switch (TWSR0 & TW_STATUS_MASK)
    {
        case TW_START:
        case TW_REP_START:
            TWDR0 = (DEV_ADDR << 1);
            if (flags & FLAG_READ) TWDR0 |= 1; // if FLAG_READ, set read bit
            TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            break;
        case TW_MT_SLA_ACK:
        case TW_MT_DATA_ACK:
            if (send_count > 0)
            {
                TWDR0 = send_buf[send_pos];
                TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
                send_count--;
                send_pos++;
            }
            else
            {
                TWCR0 = (1 << TWSTO) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
                flags &= ~(FLAG_BUSY);
            }
            break;
        case TW_MT_SLA_NACK:
        case TW_MT_DATA_NACK:
            // cancel the transfer
            send_count = 0;
            TWCR0 = (1 << TWSTO) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            flags |= FLAG_NACK;
            flags &= ~(FLAG_BUSY);
            break;
            // master receive mode
        case TW_MR_SLA_ACK:
            if (recv_count == 1)
            {
                // receive data but send NACK
                TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            else
            {
                TWCR0 = (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            break;
        case TW_MR_SLA_NACK:
            TWCR0 = (1 << TWSTO) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            flags |= FLAG_NACK;
            flags &= ~(FLAG_BUSY);
            break;
        case TW_MR_DATA_ACK:
            // byte received
            // recv_count assumed to be > 1
            recv_buf[recv_pos] = TWDR0;
            recv_pos++;
            recv_count--;
            if (recv_count == 1)
            {
                // next byte is last; send NACK after
                TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            else
            {
                TWCR0 = (1 << TWEA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            }
            break;
        case TW_MR_DATA_NACK:
            // last byte received
            recv_buf[recv_pos] = TWDR0;
            recv_count = 0;
            // transfer done, send stop condition
            TWCR0 |= (1 << TWSTO);
            TWCR0 = (1 << TWSTO) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            flags &= ~(FLAG_BUSY);
            break;
        default:
            TWCR0 = (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
            break;
    }
}

void i2c_init(void)
{
    TWCR0 = (1 << TWEN) | (1 << TWIE);
    TWBR0 = I2C_DIV;
    TWSR0 = (1 << TWPS0);
}

uint8_t i2c_send(const uint8_t* data, uint8_t len)
{
    // truncate length to the send buffer size
    if (len > SENDBUF_SIZE) len = SENDBUF_SIZE;

    for (uint8_t i = 0; i < len; ++i)
    {
        send_buf[i] = data[i];
    }
    send_count = len;
    send_pos = 0;
    flags |= FLAG_BUSY;
    flags &= ~(FLAG_READ);
    flags &= ~(FLAG_NACK);
    // send start bit
    TWCR0 = (1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
    while (flags & FLAG_BUSY);
    _delay_ms(1);
    return (flags & FLAG_NACK) == 0;
}

uint8_t i2c_recv(uint8_t* data, uint8_t len)
{
    if (len > RECVBUF_SIZE) len = RECVBUF_SIZE;
    
    recv_count = len;
    recv_pos = 0;
    flags |= FLAG_BUSY;
    flags |= FLAG_READ;
    flags &= ~(FLAG_NACK);

    TWCR0 = (1 << TWSTA) | (1 << TWINT) | (1 << TWEN) | (1 << TWIE);
    while (flags & FLAG_BUSY);
    _delay_ms(1);
    if (flags & FLAG_NACK)
    {
        return 0;
    }
    else
    {
        for (uint8_t i = 0; i < len; ++i)
        {
            data[i] = recv_buf[i];
        }
        return 1;
    }
}
