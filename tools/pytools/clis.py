# This file implements a command line script to communicate with the SCD
#
# Copyright (C) 2013 Omar Choudary (omar.choudary@cl.cam.ac.uk)
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# - Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# - Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

import serial
import sys
import shlex, subprocess
import time
import argparse # you need Python v2.7 or later
from atcmds import *
from scdtrace import *


def serial_command(port, command, wait = False):
  """
  Sends a command to the SCD via the serial port.

  Args:
    port is the port to send data, command is the data to send
    wait has the meaning of waiting for a response (if True).

  Returns:
    True if success, False if error.
  """

  ser = serial.Serial(port)
  ser.write(command)
  ser.flush()
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


def serial_geteepromhex(port, filename):
  """
  Requests the SCD to send the EEPROM contents in Intel Hex format via the serial port

  Args:
    port: the virtual port to communicate with the SCD
    filename: path of the file to store the EEPROM contents
  """

  fid = open(filename, 'w')
  ser = serial.Serial(port)
  ser.write(AT_CMD.AT_CGEE)
  ser.flush()

  while True:
    line = ser.readline()
    if line.find('AT OK') >= 0:
      break
    line.rstrip('\r\n')
    fid.write(line)
    fid.flush()

  fid.close()
  ser.close()

def serial_terminal(port, fid = sys.stdin):
  """
  Requests the SCD to act as an interactive terminal. A card must be inserted into the SCD.

  Args:
    port is the serial port used for communication between host and SCD.
    fid is the file descriptor for the file containing the sequence of commands.

  Returns: True if ended correctly, False otherwise
  """

  ser = serial.Serial(port)
  ser.write(AT_CMD.AT_CCINIT)
  ser.flush()
  line = ser.readline()
  if line.find('AT OK') < 0:
    print 'Error initialising card'
    ser.close()
    fid.close()
    return False

  while True:
    line = fid.readline()
    if line.find('0000000000') == 0:
      fid.close()
      ser.write(AT_CMD.AT_CCEND)
      ser.flush()
      line = ser.readline()
      print 'Response: ', line
      ser.close();
      if line.find('AT OK') >= 0:
        return True
      else:
        return False
    else:
      print 'Sending CAPDU: ', line.rstrip('\n')
      cmd = 'AT+CCAPDU=' + line.rstrip('\n') + "\r\n"
      ser.write(cmd)
      ser.flush()
      line = ser.readline()
      print 'Response: ', line

def serial_card(port, fid = sys.stdin):
  """
  Requests the SCD to act as an interactive card. A terminal should be
  connected when requested.

  Args:
    port is the 
    fid is the file descriptor for the file containing the sequence of responses.

  Returns: True if ended correctly, False otherwise.
  """
  ser = serial.Serial(port)
  ser.write(AT_CMD.AT_CTUSB)
  ser.flush()
  line = ser.readline()
  if line.find('AT OK') < 0:
    print 'Error initialising card'
    ser.close()
    fid.close()
    return False

  while True:
    line = fid.readline()
    if line.find('0000000000') == 0:
      fid.close()
      print 'Waiting for final result'
      ser.write(AT_CMD.AT_CCEND)
      ser.flush()
      line = ser.readline()
      ser.close();
      if line.find('AT OK') >= 0:
        return True
      else:
        return False
    else:
      print 'Sending data: ', line.rstrip('\n')
      cmd = 'AT+UDATA=' + line.rstrip('\n') + "\r\n"
      ser.write(cmd)
      ser.flush()
      line = ser.readline()
      print 'Data from Terminal: ', line
      if line.find('AT OK') >= 0:
        return True
      elif line.find('AT BAD') >= 0:
        return False


def visualise_scd_eeprom(port, filename):
  """
  Retrieves the EEPROM trace from the SCD and parses the information
  showing details about an EMV transaction (if available).

  Args:
      port: the virtual serial port to communicate with the SCD
      filename: the path to a file used to store the EEPROM hex
      file which will be parsed.
  """
  serial_geteepromhex(port, filename)
  trace = SCDTrace(filename)
  trace.process_data(True)

def main():
  """SCD command line for Serial (RS232 or USB-Serial) communication"""

  parser = argparse.ArgumentParser(description='SCD Command Line over serial port')
  parser.add_argument(
      'port',
      help='the name of the serial port to communicate to the SCD, e.g. /dev/ttyACM0')
  parser.add_argument(
      '--reset',
      action = 'store_true',
      help='reset the SCD')
  parser.add_argument(
      '--terminal',
      action = 'store_true',
      help='run the terminal application available on the SCD and log transaction on EEPROM')
  parser.add_argument(
      '--logt',
      action = 'store_true',
      help= 'log a card-reader transaction.')
  parser.add_argument(
      '--dummypin',
      action = 'store_true',
      help= 'log a card-reader transaction with dummy PIN (i.e. the real PIN sent to the card,\
          when using offline plain text PIN, will be modified with a dummy value like 1234).')
  parser.add_argument(
      '--userterminal',
      nargs = '?',
      type = argparse.FileType('r'),
      const = sys.stdin,
      default = False, metavar = 'filename',
      help='Requests the SCD to act as a terminal and send the commands (CAPDUs) from the given filename (default stdin).\
          The CAPDUs must be complete commands (header + data) with no spaces in between; one per line.\n\
          The file (or sequence of commands) should end with the string "0000000000".\n\
          Example:                         \n\
          00A4040007A0000000048002  (SELECT) \n\
          80A80000028300            (GET PROCESSING OPTS)\n\
          .... \n\
          0000000000'),
  parser.add_argument(
      '--usercard',
      nargs = '?',
      type = argparse.FileType('r'),
      const = sys.stdin,
      default = False, metavar = 'filename',
      help='Requests the SCD to act as a card and send the commands (ATR and RAPDUs) from the given filename (default stdin).\
          The RAPDUs must be complete responses (status + data) with no spaces in between; one per line.\n\
          The file must start with the ATR without TS (i.e. without the first byte of the ATR),\n\
          then a sequence of commands and should end with the string "0000000000".\n\
          Example:                         \n\
          3B6500002063CB6600  (ATR) \n\
          611B  (More data available)\n\
          .... \n\
          0000000000'),
  parser.add_argument(
      '--geteepromhex',
      nargs = 1,
      default = False,
      metavar = 'filename',
      help='retrieve the EEPROM contents as an Intel Hex file and save to specified file')
  parser.add_argument(
      '--eraseeeprom',
      action = 'store_true',
      help='erase EEPROM contents')
  parser.add_argument(
      '--vet',
      nargs = 1,
      default = False,
      metavar = 'filename',
      help='visualise the EEPROM traces from SCD, storing the contents in\
          the specified file')
  parser.add_argument(
      '--bootloader',
      action = 'store_true',
      help='go into bootloader (perhaps for USB programming)')
  parser.add_argument('--programhex',
      nargs = 1,
      default = False,
      metavar = 'filename',
      help='program the SCD via USB with the given Intel Hex file')
  parser.add_argument('-v',
      '--verbose',
      action = 'store_true',
      help='be more verbose')
  args = parser.parse_args()

  if args.reset == True:
    print "Sending reset command..."
    try:
      serial_command(args.port, AT_CMD.AT_CRST)
      time.sleep(3)
      print "Done"
    except:
      print "Error sending command"
  elif args.terminal == True:
    print "Ready for terminal application, follow SCD screen..."
    try:
      result = serial_command(args.port, AT_CMD.AT_CTERM, True)
      if result == True:
        print "All done"
      else:
        print "Some error ocurred during communication, check log"
    except:
      print "Error sending command"
  elif args.userterminal != False:
    try:
      print "Starting user terminal...\n"
      result = serial_terminal(args.port, args.userterminal)
      if result == True:
        print "All done"
      else:
        print "Some error ocurred during communication, check log"
    except:
      print "Error occurred"
      raise
  elif args.usercard != False:
    try:
      print "Starting user card, follow SCD screen..."
      result = serial_card(args.port, args.usercard)
      if result == True:
        print "All done"
      else:
        print "Some error ocurred during communication, check log"
    except:
      print "Error occurred"
      raise
  elif args.logt == True:
    try:
      print "Preparing to log transaction, follow SCD screen..."
      result = serial_command(args.port, AT_CMD.AT_CLET, True)
      if result == True:
        print "All done"
      else:
        print "Some error ocurred during communication, check log"
    except:
      print "Error sending command"
  elif args.dummypin == True:
    try:
      print "Preparing to log transaction with dummy PIN, follow SCD screen..."
      result = serial_command(args.port, AT_CMD.AT_CDPIN, True)
      if result == True:
        print "All done"
      else:
        print "Some error ocurred during communication, check log"
    except:
      print "Error sending command"
  elif args.geteepromhex != False:
    try:
      print "Retrieving EEPROM contents..."
      serial_geteepromhex(args.port, args.geteepromhex[0])
      print "Done"
    except:
      print "Error occurred"
      raise
  elif args.eraseeeprom == True:
    try:
      print "Erasing EEPROM contents..."
      serial_command(args.port, AT_CMD.AT_CEEE, True)
      print "Done"
    except:
      print "Error sending command"
  elif args.vet != False:
    try:
      print "Parsing EEPROM trace..."
      visualise_scd_eeprom(args.port, args.vet[0])
      print "Done"
    except:
      print "Error occurred"
      raise
  elif args.bootloader == True:
    try:
      serial_command(args.port, AT_CMD.AT_CGBM)
      print "Going into bootloader..." 
    except:
      print "Error sending command"
  elif args.programhex != False:
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

