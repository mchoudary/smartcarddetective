##
## Modified version of emv.py
##

## TODO: do something with parsing errors

import string
from binascii import b2a_hex, a2b_hex

valid_ascii = string.digits + \
              string.letters + \
              string.punctuation + \
              " "

MODE_CAP = 1
MODE_EMV = 2

def to_ascii(s, strip=False):
    l=[]
    for c in s:
        if c in valid_ascii:
            if strip:
                l.append(c)
            else:
                l.append(c+" ")
        else:
            if strip:
                l.append(".")
            else:
                l.append("..")
    return "".join(l)

def emv_int(hexnum):
    return int(hexnum, 16)

def emv_ber_len(hexstring):
    '''Decode an ASN BER-TLV length field
    Return length and byte position after length
    that the value field starts'''
    ## First byte
    first = emv_int(hexstring[:2])
    if first < 128:
        ## Length is just one byte
        return first, 0

    ## Otherwise first byte is number of bytes to follow
    more = first - 128
    return emv_int(hexstring[2:((more+1)*2)]), more

def hexstring(asciistring):
    return b2a_hex(asciistring)

def strip_sw(hexstring):
    return hexstring[:-4]

class ParsingError(Exception):
    pass

class TLV:
    def __init__(self, hexstring, strip=False):
        self.hexstring = hexstring
        self.errors = []
        self.warnings = []

        ## List of (tag, TLV instance) or (tag, string) or (None, string)
        self.items = []

        try:
            self.parse(hexstring, strip)
        except Exception, e:
            raise ParsingError(str(e))
    
    def parse(self, hexstring, strip=False):
        """
        This function parses down a hex string and matches against the tags we know
        """

        if strip:
            rest = strip_sw(hexstring)
        else:
            rest = hexstring

        while len(rest) > 0:
            try:
                # check if there is a tag
                orig = rest
                constructed, tag, _, length, value, rest \
                  = self.get_one_tlv(rest)

                if constructed:
                    parsed_item = TLV(value)
                else:
                    parsed_item = value
                self.items.append((tag, parsed_item))
            except Exception, e:
                self.errors.append("Failed to parse item <%s> (%s)"%(orig,e))
                self.items.append((None, orig))
                return
            
    def get_one_tlv(self, hexstring):
        ## First byte of tag
        tag = hexstring[0:2].upper()
        
        ## First byte of tag as an integer
        i_tag = emv_int(tag)

        ## Check whether tag is one or two bytes
        if (i_tag & 0x1F) == 0x1F:
            ## Two byte tag
            tag = hexstring[0:4].upper()
            claimed_length, offset = emv_ber_len(hexstring[4:])
            offset *= 2 # Offset in hex characters
            value = hexstring[6+offset:6+offset+(claimed_length*2)]
            rest = hexstring[6+offset+(claimed_length*2):]
        else:
            ## Tag is just one byte (already extracted)
            claimed_length, offset = emv_ber_len(hexstring[2:])
            offset *=2 # Offset in hex characters
            value = hexstring[4+offset:4+offset+(claimed_length*2)]
            rest = hexstring[4+offset+(claimed_length*2):]

        if len(value) != claimed_length*2:
            self.warnings.append("Short value for tag %s, expected length %s, got %s"%
              (tag,claimed_length,len(value)/2))
            
        ## Check if primative or constructed
        constructed = (i_tag & 0x20) == 0x20
            
        ## Return TLV info (parameter 2 is the tag name, which is set to
        ## None in this version of the code)
        return constructed, tag, None, claimed_length, value, rest

    def pretty_print(self, indent="", increment="    "):
        result_string = "%s"%"\n".join(self.errors)
        for tag, value in self.items:
            if not type(value) is str:
                result_string += indent + "$$ " + tag + ": \n" 
                result_string += value.pretty_print(indent+increment, increment)
            else:
                result_string += indent + tag + ": " + value + " - " + to_ascii(a2b_hex(value)) + "\n"

        return result_string

TEST_TLV = [
    ("7081a08f010190818046a3bdadd7c707ffcd3039397c06daf37d6d101b8f9c8b4bc311653ef8a974aa44c4273e1178b0cfc9b3dc077cfeb7e30e2f2624b634074d639fec03df30ade0639741b27b64514021d159a9a3650d68ae484c04b2d8fdc69381e0f3e569cfc59425581bdd3e47d6274dae3ceb2be8f48e1ef6b6f1fb043eac9544f971ea450b9f32010392145af1f3196fc626e3115f047278789510054569059000", True),
    ("7040611e4f07a000000029101050104c494e4b202020202020202020202020870101611e4f07a0000000031010501056495341204445424954202020202020870102", False),
    ("70488c159f02069f03069f1a0295055f2a029a039c019f37048d178a029f02069f03069f1a0295055f2a029a039c019f37049f0802008c5f300202015f280208269f420208269f4401029000", True),
]

def main():
    for hexstring, strip in TEST_TLV:
        print "Correct case"
        tlv = TLV(hexstring, strip)
        print tlv.pretty_print()

        print "Error case"
        tlv = TLV(hexstring, not strip)
        print tlv.pretty_print()

if __name__ == "__main__":
    main()
