#pragma once

#define WHO_AM_I 0x0F //read only register fixed at 41h (65 decimal)
#define CTRL1 0x20 //control register 1 (r/w)
#define CTRL2 0x21 //control register 2 (r/w)
#define CTRL3 0x22 //control register 3 (r/w)
#define CTRL4 0x23 //control register 4 (r/w)
#define CTRL5 0x24 //control register 5 (r/w)
#define CTRL6 0x25 //control register 6 (r/w)
#define CTRL7 0x26 //control register 6 (r/w)
#define STATUS 0x27 //control register 6 (r/w)

#define OUT_X_L 0x28 //x-axis output register (r)
#define OUT_X_H 0x29

#define OUT_Y_L 0x2A //y-axis output register (r)
#define OUT_Y_H 0x2B

#define OUT_Z_L 0x2C //z-axis output register (r)
#define OUT_Z_H 0x2D