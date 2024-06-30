#!/usr/bin/env python3
import argparse
import sys

# Converts command line arguments to fuse bytes and vice versa
# This is designed for the atmega328pb and probably needs to be adjusted for other devices
# Ideally you would have a database for every single avr micro ever, but I don't have time for that
# I have no idea why I did this; I have more important things to do

# Fuse options:
# EFUSE (default = FE)
# bit 7 | reserved      | 1
# bit 6 | reserved      | 1
# bit 5 | reserved      | 1
# bit 4 | reserved      | 1
# bit 3 | CFD           | 0
# bit 2 | BODLEVEL[2]   | 1
# bit 1 | BODLEVEL[1]   | 1
# bit 0 | BODLEVEL[0]   | 1
#
# HFUSE (default = DF)
# bit 7 | RSTDISBL      | 1
# bit 6 | DWEN          | 1
# bit 5 | SPIEN         | 0
# bit 4 | WDTON         | 1
# bit 3 | EESAVE        | 1
# bit 2 | BODLEVEL[2]   | 1
# bit 1 | BODLEVEL[1]   | 1
# bit 0 | BODLEVEL[0]   | 1
#
# LFUSE (default = 62)
# bit 7 | CKDIV8        | 0
# bit 6 | CKOUT         | 1
# bit 5 | SUT[1]        | 1
# bit 4 | SUT[0]        | 0
# bit 3 | CKSEL[3]      | 0
# bit 2 | CKSEL[2]      | 0
# bit 1 | CKSEL[1]      | 1
# bit 0 | CKSEL[0]      | 0

# CFD (--cfd-enable) - Clock failure detection
#  1    | --cfd-enable | CFD enabled
#  0    |              | CFD disabled (default)
# 
# BODLEVEL[2:0] (--bod-level=<opt>) - Brown-out detector setting
#  111  | off | BOD disabled        (default)
#  110  | 1v8 | Vbot = 1.8V typical
#  101  | 2v7 | Vbot = 2.7V typical
#  100  | 4v3 | Vbot = 4.3V typical
#
# RSTDISBL (--reset-disable) - disable the reset pin (!)
#  1    |                 | Reset pin enabled (default)
#  0    | --reset-disable | Reset pin disabled
#
# DWEN (--debugwire-enable) - enable debugWIRE functionality
#  1    |                    | debugWIRE disabled (default)
#  0    | --debugwire-enable | debugWIRE enabled
#
# SPIEN (--spi-disable) - disable SPI programming (!)
#  1    | --spi-disable | SPI programming disabled
#  0    |               | SPI programming enabled (default)
#
# WDTON (--wdt-enable) - force enable the watchdog timer
#  1    |              | Watchdog timer not enabled (default)
#  0    | --wdt-enable | Watchdog timer forced to System Reset mode
#
# EESAVE (--ee-save) - keep EEPROM across chip erases
#  1    |           | EEPROM not saved (default)
#  0    | --ee-save | EEPROM saved
# 
# CKDIV8 (--div8-disable) - disable the system clock divider
#  1    | --div8-disable | System clock not divided
#  0    |                | System clock divided by 8 (default)
#
# CKOUT (--clk-out) - output the system clock to PB0
#  1    |           | Clock output disabled (default)
#  0    | --clk-out | Clock output enabled
#
# CKSEL+SUT (--clk-sel=<opt>) - set the start-up time
#  1xx1 11 | cry-<speed>-slow        | Crystal oscillator, slowly rising power
#  1xx1 10 | cry-<speed>-fast        | Crystal oscillator, fast rising power
#  1xx1 01 | cry-<speed>-bod         | Crystal oscillator, BOD enabled (no extra delay needed)
#  1xx1 00 | cer-<speed>-slow        | Ceramic resonator, slowly rising power
#  1xx0 11 | cer-<speed>-fast        | Ceramic resonator, fast rising power
#  1xx0 10 | cer-<speed>-bod         | Ceramic resonator, BOD enabled (no extra delay needed)
#  1xx0 01 | cer-<speed>-slow-fstart | Ceramic resonator, slowly rising power, fast startup
#  1xx0 00 | cer-<speed>-fast-fstart | Ceramic resonator, fast rising power, fast startup
#  0101 10 | lf-slow                 | Low frequency (32kHz) oscillator, slowly rising power
#  0101 01 | lf-fast                 | Low frequency (32kHz) oscillator, fast rising power
#  0101 00 | lf-bod                  | Low frequency (32kHz) oscillaotr, BOD enabled
#  0100 10 | lf-slow-fstart          | Low frequency (32kHz) oscillator, slowly rising power, fast startup
#  0100 01 | lf-fast-fstart          | Low frequency (32kHz) oscillator, fast rising power, fast startup
#  0100 00 | lf-bod-fstart           | Low frequency (32kHz) oscillaotr, BOD enabled, fast startup
#  0010 10 | rc-slow                 | Internal RC oscillator (8MHz), slowly rising power (default)
#  0010 01 | rc-fast                 | Internal RC oscillator (8MHz), fast rising power
#  0010 00 | rc-bod                  | Internal RC oscillator (8MHz), BOD enabled
#  0011 10 | int-slow                | Internal oscillator (128kHz), slowly rising power
#  0011 01 | int-fast                | Internal oscillator (128kHz), fast rising power
#  0011 00 | int-bod                 | Internal oscillator (128kHz), BOD enabled
#  0000 10 | ext-slow                | External clock, slowly rising power
#  0000 01 | ext-fast                | External clock, fast rising power
#  0000 00 | ext-bod                 | External clock, BOD enabled
#  External crystal/ceramic speed options:
#  11 | 8m  | 8-16    MHz
#  10 | 3m  | 3-8     MHz
#  01 | 0m9 | 0.9-3   MHz
#  00 | 0m4 | 0.4-0.9 MHz

def get_cksel_val(opt):
    if opt is None:
        return 0b100010 # default, rc-slow
    o = opt.split('-')
    cksel = 0 # cksel + SUT fuses combined (top 2 bits are SUT) 
    try:
        if o[0] == "cry" or o[0] == "cer":
            cksel = 0b1000

            speed = o[1]
            rate = o[2]
            start = o[3] if len(o) == 4 else None
            if   speed == '8m':  cksel |= 0b0110
            elif speed == '3m':  cksel |= 0b0100
            elif speed == '0m9': cksel |= 0b0010
            elif speed == '0m4': cksel |= 0b0000
            else: return None

            if   (o[0],rate,start) == ('cry','slow',None):     cksel |= 0b110001
            elif (o[0],rate,start) == ('cry','fast',None):     cksel |= 0b100001
            elif (o[0],rate,start) == ('cry','bod' ,None):     cksel |= 0b010001
            elif (o[0],rate,start) == ('cer','slow',None):     cksel |= 0b000001
            elif (o[0],rate,start) == ('cer','fast',None):     cksel |= 0b110000
            elif (o[0],rate,start) == ('cer','bod' ,None):     cksel |= 0b100000
            elif (o[0],rate,start) == ('cer','slow','fstart'): cksel |= 0b010000
            elif (o[0],rate,start) == ('cer','fast','fstart'): cksel |= 0b000000
            else: return None
        else:
            rate = o[1]
            if o[0] == 'lf':
                start = o[2] if len(o) == 3 else None
                cksel = 0b0100
                if start is None:       cksel |= 0b0001
                elif start == 'fstart': cksel |= 0b0000
                else: return None

            elif o[0] == 'rc':  cksel = 0b0010
            elif o[0] == 'int': cksel = 0b0011
            elif o[0] == 'ext': cksel = 0b0000
            else: return None

            if   rate == 'slow': cksel |= 0b100000
            elif rate == 'fast': cksel |= 0b010000
            elif rate == 'bod':  cksel |= 0b000000
    except IndexError:
        return None

    return cksel

def get_cksel_opt(cksel):
    out = None
    if cksel & 0b1000:
        if   (cksel & 0b0110) == 0b0110: speed = '8m'
        elif (cksel & 0b0110) == 0b0100: speed = '3m'
        elif (cksel & 0b0110) == 0b0010: speed = '0m9'
        elif (cksel & 0b0110) == 0b0000: speed = '0m4'

        if   (cksel & 0b110001) == 0b110001: temp = "cry-%s-slow"
        elif (cksel & 0b110001) == 0b100001: temp = "cry-%s-fast"
        elif (cksel & 0b110001) == 0b010001: temp = "cry-%s-bod"
        elif (cksel & 0b110001) == 0b000001: temp = "cer-%s-slow"
        elif (cksel & 0b110001) == 0b110000: temp = "cer-%s-fast"
        elif (cksel & 0b110001) == 0b100000: temp = "cer-%s-bod"
        elif (cksel & 0b110001) == 0b010000: temp = "cer-%s-slow-fstart"
        elif (cksel & 0b110001) == 0b000000: temp = "cer-%s-fast-fstart"
        out = temp % speed
    else:
        if   (cksel & 0b110000) == 0b100000: rate = "slow"
        elif (cksel & 0b110000) == 0b010000: rate = "fast"
        elif (cksel & 0b110000) == 0b000000: rate = "bod"
        else: return None

        if   (cksel & 0b0111) == 0b0101: temp = "lf-%s"
        elif (cksel & 0b0111) == 0b0100: temp = "lf-%s-fstart"
        elif (cksel & 0b0111) == 0b0010: temp = "rc-%s"
        elif (cksel & 0b0111) == 0b0011: temp = "int-%s"
        elif (cksel & 0b0111) == 0b0000: temp = "ext-%s"
        else: return None

        out = temp % rate
    return out

def main():
    p = argparse.ArgumentParser()
    p.add_argument("-c", "--cfd-enable", dest="cfd_enable", action="store_true",
                   help="Enable clock failure detection (CFD)")
    p.add_argument("-b", "--bod-level", dest="bodlevel", metavar="opt",
                   help="Set brown-out voltage (see script for options)")
    p.add_argument("--reset-disable", dest="reset_disable", action="store_true",
                   help="Disable the reset pin (requires HV programming to undo!)")
    p.add_argument("-w", "--debugwire-enable", dest="debugwire_enable", action="store_true",
                   help="Enable debugWIRE functionality")
    p.add_argument("--spi-disable", dest="spi_disable", action="store_true",
                   help="Disable SPI programming (requires parallel/HV programming to restore!)")
    p.add_argument("-t", "--wdt-enable", dest="wdt_enable", action="store_true",
                   help="Force-enable the watchdog timer in system reset mode")
    p.add_argument("-e", "--ee-save", dest="ee_save", action="store_true",
                   help="Save EEPROM across chip erase cycles")
    p.add_argument("-d", "--div8-disable", dest="div8_disable", action="store_true",
                   help="Disable 1/8 clock divider")
    p.add_argument("-o", "--clk-out", dest="clk_out", action="store_true",
                   help="Enable clock output on PB0")
    p.add_argument("-s", "--clk-sel", dest="clk_sel", metavar="opt",
                   help="Choose clock source and start-up time (see script for options)")
    p.add_argument("fuses", metavar="lfuse,hfuse,efuse", nargs="?",
                   help="Generate arguments for the given fuse bytes (ignore all other options)")

    args = p.parse_args()

    if args.fuses is None:
        # convert arguments to fuse bytes
        efuse = 0xf0
        hfuse = 0
        lfuse = 0
        valid = True

        if   args.cfd_enable:        efuse |= 0x08
        
        if   args.bodlevel == 'off': efuse |= 0x07; hfuse |= 0x07
        elif args.bodlevel == '1v8': efuse |= 0x06; hfuse |= 0x06
        elif args.bodlevel == '2v7': efuse |= 0x05; hfuse |= 0x05
        elif args.bodlevel == '4v3': efuse |= 0x04; hfuse |= 0x04
        elif args.bodlevel is None:  efuse |= 0x07; hfuse |= 0x07 # default=off
        else: valid = False

        if not args.reset_disable:    hfuse |= 0x80
        if not args.debugwire_enable: hfuse |= 0x40
        if     args.spi_disable:      hfuse |= 0x20
        if not args.wdt_enable:       hfuse |= 0x10
        if not args.ee_save:          hfuse |= 0x08

        if     args.div8_disable:     lfuse |= 0x80
        if not args.clk_out:          lfuse |= 0x40
        
        cksel = get_cksel_val(args.clk_sel)
        if cksel is None:
            valid = False
        else:
            lfuse |= cksel

        if valid:
            print("%02x %02x %02x" % (lfuse,hfuse,efuse))
            return 0
        else:
            return 1
    else:
        lfuse, hfuse, efuse = args.fuses.split(',')
        lfuse = int(lfuse, 16)
        hfuse = int(hfuse, 16)
        efuse = int(efuse, 16)

        args = []
        valid = True

        if     (efuse & 0x08):         args.append("--cfd-enable")
        if     (efuse & 0x07) == 0x07: args.append("--bod-level=off")
        elif   (efuse & 0x07) == 0x06: args.append("--bod-level=1v8")
        elif   (efuse & 0x07) == 0x05: args.append("--bod-level=2v7")
        elif   (efuse & 0x07) == 0x04: args.append("--bod-level=4v3")
        else: valid = False

        if not (hfuse & 0x80):         args.append("--reset-disable")
        if not (hfuse & 0x40):         args.append("--debugwire-enable")
        if     (hfuse & 0x20):         args.append("--spi-disable")
        if not (hfuse & 0x10):         args.append("--wdt-enable")
        if not (hfuse & 0x08):         args.append("--ee-save")
        
        if     (lfuse & 0x80):         args.append("--div8-disable")
        if not (lfuse & 0x40):         args.append("--clk-out")

        cksel_opt = get_cksel_opt(lfuse & 0x3f)
        if cksel_opt is None: valid = False
        else: args.append("--clk-sel=%s" % cksel_opt)

        if valid:
            print(' '.join(args))
            return 0
        else:
            return 1

if __name__ == "__main__":
    sys.exit(main())
