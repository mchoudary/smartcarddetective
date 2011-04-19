##
## Script to decode EEPROM SCD traces
##

import string
import sys
from binascii import b2a_hex, a2b_hex
from tlv import TLV

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
    def __init__(self, hexstring):
        self.hexstring = hexstring
        self.errors = []
        self.warnings = []

        # List of (command, response instance) 
        self.items = []

        try:
            self.parse(hexstring)
        except Exception, e:
            raise ParsingError(str(e))

    def parse(self, hexstring):
        """
        Parse the trace and get the different commands and responses
        """
        startstr = "DDDDDDDDDD"
        commandstr = "CCCCCCCCCC"
        responsestr = "AAAAAAAAAA"
        endstr = "BBBBBBBBBB"
        end = len(hexstring)

        # Find the beginning of the transaction (DDDDDDDDDD)
        start = hexstring.find(startstr, 0, end)
        if start < 0:
            return

        pos = hexstring.find(commandstr, start + len(startstr), end)
        if pos < 0:
            return

        done = 0
        pos2 = 0
        while done == 0:
            # get command and response pairs
            pos2 = hexstring.find(responsestr, pos, end)
            if pos2 < 0:
                return
            command = CAPDU(hexstring[pos + len(commandstr):pos2])

            pos = hexstring.find(commandstr, pos2, end)
            if pos < 0:
                pos = hexstring.find(endstr, pos2, end)
                if pos < 0:
                    return
                else:
                    done = 1
            response = RAPDU(hexstring[pos2 + len(responsestr):pos])
            self.items.append((command, response))

    def pretty_print(self, indent="", increment="    "):
        k = 1
        result_string = "%s"%"\n".join(self.errors)
        for command, response in self.items:
            result_string += indent + "message %d:\n"%(k)
            
            result_string += indent + "command: " + command.hexstring + "\n"
            result_string += increment + "header: " + command.header + "\n"
            result_string += increment + "data: " + command.data + "\n"

            result_string += indent + "response: " + response.hexstring + "\n"
            result_string += increment + "status: " + response.status + "\n"
            result_string += increment + "data: " + response.data + "\n"
            if len(response.data) > 0:
                tlv = TLV(response.data, False)
                result_string += tlv.pretty_print(increment + increment, increment)
            result_string += "\n"
            k = k + 1
                
        return result_string

def parseHexfile(filename):
    '''Parses an Intel Hex file containing an SCD trace and returns a list with all the
    different transaction bytes, removing starting and trailing Intel Hex bytes'''
    tracelist = []
    bigtrace = ""
    start = "DDDDDDDDDD"
    end = "BBBBBBBBBB"

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

    i = 0
    while i < len(bigtrace):
        j = bigtrace.find(start, i)
        if j < 0:
            break
        k = bigtrace.find(end, j)
        if k < 0:
            break
        tracelist.append(bigtrace[j: k + len(end)])
        i = k + len(end)

    return tracelist

def main():

    if len(sys.argv) == 1:
        print "Please give the name of a trace file to parse. Exiting..."
        return

    fname = sys.argv[1]
    traces = parseHexfile(fname)

    count = 0
    for trace in traces:
        print "\nTransaction %d"%(count)
        strace = SCDTrace(trace)
        print "Number of messages: %d"%(len(strace.items))
        print strace.pretty_print()
        count = count + 1


if __name__ == "__main__":
    main()

