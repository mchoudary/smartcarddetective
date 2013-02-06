# This file implements a command line script to decode EEPROM SCD traces
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

import argparse
import string
import sys
from binascii import b2a_hex, a2b_hex
from tlv import T
import emv_commands

class CAPDU:
    def __init__(self, hexstring):
        self.hexstring = hexstring
        self.header = ""
        self.data = ""

        if len(hexstring) >= 10:
            self.header = hexstring[:10]
            hexstring = hexstring[10:]

        if len(hexstring) > 0:
            self.data = hexstring

class RAPDU:
    def __init__(self, hexstring):
        self.hexstring = hexstring
        self.status = ""
        self.data = ""

        if len(hexstring) >= 4:
            self.status = hexstring[:4]
            hexstring = hexstring[4:]

        if len(hexstring) > 0:
            self.data = hexstring


class SCDTrace:
    """
    Class defining a log trace of the Smart Card Detective.

    @Methods:
        __init__: constructor
        parse_data: not sure yet
        process_data: performs all the necessary parsing of a file. Use this!
        parse_intel_hex: parse a file in Intel Hex format (such as SCD EEPROM)
        extract_log_data: get log data from the larger parsed EEPROM contents
        split_events: split bytes into clusters of events
        print_events: print event information on standard output
    """

    def __init__(self, filename):
        """
        Constructor for the SCDTrace class.

        @Args:
            filename: the name of the file containing the SCD EEPROM data to be
            parsed

        @Return:
            None
        """
        self.filename = filename
        self.event_dict = {
                0x00: "ATR Byte from ICC",
                0x01: "ATR Byte to Terminal",
                0x02: "Byte to Terminal",
                0x03: "Byte from Terminal",
                0x04: "Byte to ICC",
                0x05: "Byte from ICC",
                0x06: "ATR from USB",
                0x10: "Terminal clock active",
                0x11: "Terminal reset low",
                0x12: "Terminal timed out",
                0x13: "Error receiving byte from terminal",
                0x14: "Error sending byte to terminal",
                0x15: "No clock from terminal",
                0x20: "ICC activated",
                0x21: "ICC deactivated",
                0x22: "ICC reset high",
                0x23: "Error receiving byte from ICC",
                0x24: "Error sending byte to ICC",
                0x25: "ICC inserted",
                0x30: "Time data sent to ICC",
                0x31: "Time for a general event",
                0x32: "Error allocating memory",
                0x33: "Watchdog timer reset",
                }
        #self.errors = []
        #self.warnings = []
        #self.bigtrace = self.parse_intel_hex(filename)
        # List of (command, t_command, response, t_response instance) 
        # self.items = []
        #try:
        #    self.parse(hexstring, verbose)
        #except Exception, e:
        #    raise ParsingError(str(e))
        #command = CAPDU(result_string)
        #response = RAPDU(result_string)

    def pretty_print(self, indent="", increment="    ", verbose=True):
        k = 1
        result_string = "%s"%"\n".join(self.errors)
        for command, t_command, response, t_response in self.items:
            result_string += indent + "message %d:\n"%(k)
            
            print "here"
            result_string += indent + "time: " + str(t_command) + "\n"
            result_string += increment + "command: " + command.hexstring + "\n"
            result_string += increment + "header: " + command.header + "\n"
            result_string += increment + "data: " + command.data + "\n"

            result_string += indent + "time: " + str(t_response) + "\n"
            result_string += increment + "response: " + response.hexstring + "\n"
            result_string += increment + "status: " + response.status + "\n"
            result_string += increment + "data: " + response.data + "\n"
            if len(response.data) > 0:
                if verbose:
                    print "response data: ", response.data
                tlv = TLV(response.data, False)
                result_string += tlv.pretty_print(
                        increment + increment,
                        increment,
                        verbose)
            result_string += "\n"
            k = k + 1
                
        return result_string

    def process_data(self, verbose=False):
        """
        Performs all the necessary processing for the given file and then
        prints the result to standard output.

        @Args:
            verbose: set to true to get more verbose output

        @Returns:
            None
        """
        self.bigtrace = self.parse_intel_hex(self.filename)
        self.log_data = self.extract_log_data(self.bigtrace)
        if len(self.log_data) < 2:
            print "No data available"
            return
        if verbose:
            print "Log bytes: \n", self.log_data
        self.events_list = self.split_events(self.log_data)
        self.print_events(self.events_list, verbose)

    def parse_intel_hex(self, filename):
        """
        Parses an Intel Hex file containing an SCD trace and returns a
        string of bytes, removing starting and trailing Intel Hex bytes.

        @Args:
            filename: the name of the file to be parsed

        @Returns:
            a string of bytes representing the parsed file.
        """
        bigtrace = ""

        f = open(filename, 'r')

        #first we get the bytes, removing format
        for line in f:
            if line[0] != ':':
                break
            if line[1] == '1':
                llen = 43
            else:
                llen = 75
            if len(line) < llen:
                break

            line = line[9:llen - 2]
            bigtrace = bigtrace + line

        f.close()

        return bigtrace

    def extract_log_data(self, bigtrace):
        """
        Extracts the log data bytes from the parsed full log trace.
        This method checks the length of the current log and then extracts just
        the bytes that actually contain log data.

        The EEPROM of the SCD has 4K. The first 128 bytes contain metadata, with
        the following important fields (starting from 0):
        bytes 4-7: last counter value
        bytes 72-73: address of last log byte
        byte 128: start of log data
        
        In the following take in consideration that each character in the
        bigtrace string actually represents a nibble (i.e. half a byte).

        @Args:
            bigtrace: the string of bytes representing the parsed EEPROM data

        @Returns:
            a string of bytes representing the log data
        """
        last_byte = int(bigtrace[72*2:74*2], 16)
        return bigtrace[128*2:last_byte*2]

    def split_events(self, data):
        """
        Split a string of bytes representing a parsed log from the SCD and
        clusters the bytes into separate events. Consecutive bytes of the same
        event type are added together.

        Each entry in the log is composed of at least 2 bytes: L1 L2 ....
        L1 = XXXXXXYY defines what the next byte(s) mean, where XXXXXX is
        used for the encoding of the type (6 bits) and YY (2 bits) to specify
        how many bytes follow (b'00 -> 1, b'01 -> 2, b'10 -> 3 or b'11 -> 4).
        
        @Args:
            data: string of bytes containing a log from the SCD.

        @Returns:
            list of (type, data) items

        @Throws:
            None
        """

        events_list = []
        data_len = len(data)
        last_type = 0xFF
        event_data = ""
        i = 0
        while i < data_len:
            byte_value = int(data[i:i+2], 16)
            i += 2
            byte_type = (byte_value & 0xFF) >> 2
            bytes_following = (byte_value & 0x03) + 1

            # If this happens then either the file is corrupted or we have
            # reached the end of the log data. In either case we stop.
            if bytes_following * 2 > data_len - i:
                break

            if last_type == 0xFF:
                last_type = byte_type

            if byte_type != last_type:
                events_list.append((last_type, event_data))
                last_type = byte_type
                event_data = ""

            for k in range(bytes_following):
                event_data += data[i:i+2]
                i += 2
        #end while

        # append also the last type
        events_list.append((last_type, event_data))

        return events_list
        
    def print_events(self, events_list, verbose=False):
        """
        Prints the contents of an events_list. 
        
        This list should have been generated
        by a call to split_events

        @Args:
            events_list: list of (events, data) tuples, as that generated by a
            call to to split_events()
            verbose: set to True to get more verbose output

        @Returns:
            None

        @Throws:
            None
        """
        for event_type, data in events_list:
            len_data = len(data)
            print("event: ", hex(event_type), self.event_dict[event_type])
            print("data: ", data)
            if event_type == 0x30 or event_type == 0x31:
                time = data[6:8] + data[4:6] + data[2:4] + data[0:2]
                print("time in ms: ", int(time, 16) * 1024 / 1000)
            if event_type == 0x02 or event_type == 0x05:
                if len_data > 6:
                    try:
                        t = T(data[2:len_data-4])
                        print t.dump()
                    except Exception as (e):
                        print e
                        pass
                elif len_data == 4:
                    print emv_commands.response_name(data[0:4].lower())
            if event_type == 0x03 or event_type == 0x04:
                if len_data == 10:
                    print emv_commands.command_name(data[0:4].lower())

            print("\n")


def main():
    """Command line tool to parse SCD log files in Intel hex format."""

    parser = argparse.ArgumentParser(description='SCD log parser')
    parser.add_argument(
            'log_file',
            help='the file containing the log (Intel hex format)')
    parser.add_argument('-v',
            '--verbose',
            action = 'store_true',
            help='be more verbose')
    args = parser.parse_args()
    

    fname = args.log_file
    trace = SCDTrace(fname)
    trace.process_data(True)

if __name__ == "__main__":
    main()

