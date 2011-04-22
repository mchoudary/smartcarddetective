import serial
import sys
import time
import argparse # you need Python v2.7 or later
from atcmds import *


def serial_command(port, command):
    """Sends a command to the SCD via the serial port"""

    ser = serial.Serial(port)
    ser.write(command)
    ser.close();
    #time.sleep(1)
    #print ser.readline()

def serial_geteepromhex(port, fid = sys.stdout):
    """Requests the SCD to send the EEPROM contents in Intel Hex format via the serial port"""
    """fid is the file descriptor"""

    ser = serial.Serial(port)
    ser.write(AT_CMD.AT_CGEE)

    while True:
        line = ser.readline()
        if line.find('AT OK') >= 0:
            break
        line.rstrip('\r\n')
        fid.write(line)

    ser.close();


def main():
    """SCD command line for Serial (RS232 or USB-Serial) communication"""

    parser = argparse.ArgumentParser(description='SCD Command Line over serial port')
    parser.add_argument('port', help='the name of the serial port to communicate to the SCD, e.g. /dev/ttyACM0')
    parser.add_argument('--reset', action = 'store_true',  help='reset the SCD')
    parser.add_argument('--terminal', action = 'store_true',  help='run the terminal application')
    parser.add_argument('--logt', action = 'store_true',  help='log a transaction')
    parser.add_argument('--geteepromhex', nargs = '?', type = argparse.FileType('w'), const = sys.stdout,
            default = False, metavar = 'filename',
            help='retrieve the EEPROM contents as an Intel Hex file and save to specified file (default stdout)')
    parser.add_argument('--eraseeeprom', action = 'store_true',  help='erase EEPROM contents')
    parser.add_argument('--bootloader', action = 'store_true',  help='go into bootloader (perhaps for USB programming)')
    parser.add_argument('-v', '--verbose', action = 'store_true', help='be more verbose')
    args = parser.parse_args()

    if args.reset == True:
        print 'Sending reset command...'
        try:
            serial_command(args.port, AT_CMD.AT_CRST)
            time.sleep(3)
            print 'Done'
        except:
            print "Error sending command"

    if args.terminal == True:
        print 'Ready for terminal application, please insert card...'
        try:
            serial_command(args.port, AT_CMD.AT_CTERM)
        except:
            print "Error sending command"

    if args.logt == True:
        try:
            serial_command(args.port, AT_CMD.AT_CLET)
            print 'SCD ready to log transaction...'
        except:
            print "Error sending command"

    if args.geteepromhex != False:
        try:
            print 'Retrieving EEPROM contents...'
            serial_geteepromhex(args.port, args.geteepromhex)
            print 'Done'
        except:
            print "Error occurred"
            raise

    if args.eraseeeprom == True:
        try:
            print 'Erasing EEPROM contents...'
            serial_command(args.port, AT_CMD.AT_CEEE)
            print 'Done'
        except:
            print "Error sending command"

    if args.bootloader == True:
        try:
            serial_command(args.port, AT_CMD.AT_CGBM)
            print 'Going into bootloader... (not implemented)'
        except:
            print "Error sending command"

if __name__ == '__main__':
    main()

