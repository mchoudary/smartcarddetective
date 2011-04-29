import serial
import sys
#import os
import shlex, subprocess
import time
import argparse # you need Python v2.7 or later
from atcmds import *


def serial_command(port, command, wait = False):
    """Sends a command to the SCD via the serial port."""
    """port is the port to send data, command is the data to send
    and wait has the meaning of waiting for a response (if True)."""
    """returns True if success, False if error"""

    ser = serial.Serial(port)
    ser.write(command)
    if wait == True:
        line = ser.readline()
        ser.close();
        if line.find('AT OK') >= 0:
            return True
        else:
            return False
    else:
        ser.close();
        return True;


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

    fid.close()
    ser.close()

def serial_terminal(port, fid = sys.stdin):
    """Requests the SCD to act as an interactive terminal. A card must be inserted into the SCD."""
    """fid is the file descriptor for the file containing the sequence of commands"""

    ser = serial.Serial(port)
    ser.write(AT_CMD.AT_CCINIT)
    line = ser.readline()
    if line.find('AT OK') < 0:
        print 'Error initialising card'
        ser.close()
        fid.close()
        return

    while True:
        line = fid.readline()
        if line.find('0000000000') == 0:
            print 'End of transaction'
            ser.write(AT_CMD.AT_CCEND)
            ser.close()
            fid.close()
            return
        else:
            print 'Sending CAPDU: ', line.rstrip('\n')
            cmd = 'AT+CCAPDU=' + line.rstrip('\n') + "\r\n"
            ser.write(cmd)
            line = ser.readline()
            print 'Response: ', line


def main():
    """SCD command line for Serial (RS232 or USB-Serial) communication"""

    parser = argparse.ArgumentParser(description='SCD Command Line over serial port')
    parser.add_argument('port', help='the name of the serial port to communicate to the SCD, e.g. /dev/ttyACM0')
    parser.add_argument('--reset', action = 'store_true',  help='reset the SCD')
    parser.add_argument('--terminal', action = 'store_true',  help='run the terminal application available on the SCD')
    parser.add_argument('--logt', action = 'store_true',  help='log a transaction')
    parser.add_argument('--userterminal', nargs = '?', type = argparse.FileType('r'), const = sys.stdin,
            default = False, metavar = 'filename',
            help='Requests the SCD to act as a terminal and send the commands (CAPDUs) from the given filename (default stdin).\
            The CAPDUs must be complete commands (header + data) with no spaces in between; one per line.\n\
            The file (or sequence of commands) should end with the string "0000000000".\n\
            Example:                         \n\
            00A4040007A0000000048002  (SELECT) \n\
            80A80000028300            (GET PROCESSING OPTS)\n\
            .... \n\
            0000000000'),
    parser.add_argument('--geteepromhex', nargs = '?', type = argparse.FileType('w'), const = sys.stdout,
            default = False, metavar = 'filename',
            help='retrieve the EEPROM contents as an Intel Hex file and save to specified file (default stdout)')
    parser.add_argument('--eraseeeprom', action = 'store_true',  help='erase EEPROM contents')
    parser.add_argument('--bootloader', action = 'store_true',  help='go into bootloader (perhaps for USB programming)')
    parser.add_argument('--programhex', nargs = 1, default = False, metavar = 'filename',
            help='program the SCD via USB with the given Intel Hex file')
    parser.add_argument('-v', '--verbose', action = 'store_true', help='be more verbose')
    args = parser.parse_args()

    if args.reset == True:
        print "Sending reset command..."
        try:
            serial_command(args.port, AT_CMD.AT_CRST)
            time.sleep(3)
            print "Done"
        except:
            print "Error sending command"

    if args.terminal == True:
        print "Ready for terminal application, please insert card..."
        try:
            serial_command(args.port, AT_CMD.AT_CTERM)
        except:
            print "Error sending command"

    if args.userterminal != False:
        try:
            print "Starting user terminal...\n"
            serial_terminal(args.port, args.userterminal)
            print "Done"
        except:
            print "Error occurred"
            raise

    if args.logt == True:
        try:
            serial_command(args.port, AT_CMD.AT_CLET)
            print "SCD ready to log transaction..."
        except:
            print "Error sending command"

    if args.geteepromhex != False:
        try:
            print "Retrieving EEPROM contents..."
            serial_geteepromhex(args.port, args.geteepromhex)
            print "Done"
        except:
            print "Error occurred"
            raise

    if args.eraseeeprom == True:
        try:
            print "Erasing EEPROM contents..."
            serial_command(args.port, AT_CMD.AT_CEEE, True)
            print "Done"
        except:
            print "Error sending command"

    if args.bootloader == True:
        try:
            serial_command(args.port, AT_CMD.AT_CGBM)
            print "Going into bootloader..." 
        except:
            print "Error sending command"

    if args.programhex != False:
        try:
            print "Going into bootloader..."
            serial_command(args.port, AT_CMD.AT_CGBM)
            time.sleep(5)

            cmd = "dfu-programmer at90usb1287 erase";
            print "Running: ", cmd, "..."
            cargs = shlex.split(cmd)
            p = subprocess.call(cargs)

            cmd = "dfu-programmer at90usb1287 flash " + args.programhex[0]
            print "Running: ", cmd, "..."
            cargs = shlex.split(cmd)
            p = subprocess.call(cargs)

            cmd = "dfu-programmer at90usb1287 reset"
            print "Running: ", cmd, "..."
            cargs = shlex.split(cmd)
            p = subprocess.call(cargs)
            p = subprocess.call(cargs)

            print "Done"
        except:
            print "Error while uploading program"

if __name__ == '__main__':
    main()

