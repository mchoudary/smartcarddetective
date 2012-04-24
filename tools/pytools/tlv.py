# Mike's much better TLV class

from itertools import chain

from binascii import b2a_hex
from binascii import a2b_hex as fromhex
def tohex(string): return b2a_hex(string).upper()

from utils import * 
from emvtags import tagname


TLV_TABSIZE=4
DEBUG=False

#---------------------------- TLV STUFF --------------------------------------

def maketlv(tag,value):
    if len(value) < 256:
        return tag + chr(len(value)) + value

    return '\x82' + pack('!H',len(value)) + value

def constructed(tag):
    assert len(tag) in [1,2]
    return ord(tag[0]) & 0x20 == 0x20

def primitive(tag):
    return not constructed(tag)

def validhex(data):
    for c in data: assert c in [ '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' ]
    return True
    
def validhextag(tag):
    assert type(tag) == str
    assert len(tag) in [2,4]
    assert validhex(tag)
    return True

#---------------------------- MAIN CLASS --------------------------------------
    
class T:
    '''
    EXTERNALLY ACCESSIBLE READ-ONLY FILEDS
    --------------------------------------
    
    self.t  - tag in binary
    self.l  - length in encoded binary (not implemented yet)
    self.v  - value in binary
        
    self.th - tag in hex
    self.lh - lengh in encoded form, in hex (not implemented yet)
    self.vh - value in hex
        
    self.tag - tag as integer
    self.len - len of value as integer
    self.value - value as string (primitive only, see self.items for constructed)

    self.raw  - raw value of entire TLV object when encoded, binary
    self.rawh - raw value of entire TLV object when encoded, hex
    
    self.constructed - True or False depending on if tag constructed or primitive
    self.items - Array of T objects composing this structured tag
    
    METHOD SUMMARY
    --------------
    
    __init__(self,data)        -- parse T object from HEX data
    __init__(self,tag,value)   -- build T object from tag and value (string
                                  or list of (T objects/strings))
    
    add(self,data) -- adds data (a parsable TLV string or a T object)
                      to list of items if this is a constructed tag
    dump(self)  -- return pretty print of the tag contents
    __repr__(self) -- return condensed one-line content of tag
    
    INTERFACES SUMMARY
    ------------------
    
    1. ITERATION if the tag is constructed, you can iterate over its members using as follows:
    
    for tlv in constructed_tlv_obj:
        tlv.dump()
        
    2. TAG ACCESS SHORTCUT MEMBERS
    
    when the TLV object is parsed (or updated using self.add(data), shortcut members are
    added to the class of the form T<tag> or T<tag>_00 ... T<tag>_nn in the case of a list
    of TLV objects, to allow you to access inner data quickly. So you can write:
    
    print tlvobj.TA5.T5F2D.dump()
    
    to display the content of tag 5F2D within tag A5 of the overall T object tlvobj
    
    3. MEMBERSHIP TESTING
    
    supports checking whether a particular tag (supplied in hex) is present within the
    immediate top level items list of a constructed tag as follows:
    
    if '6A' in tlvobject:
        dosomething()
        
    '''

    def __init__(self,arg1,arg2=None,depth=0):
        '''
        make a TLV object, either by parsing data (1 arg) or constructing from tag+value (2 args)
        PARSING:  arg1=data in hex
                  arg2=None
                  
        BUILDING: arg1=tag in hex
                  arg2=value as hex or T object (for a primitive or a singleton constructed T object)
                       list of hex or T objects (for a constructed T object)
        '''
        
        self.depth = depth                        # (int)    nest depth of this TLV item
        self._d = ' ' * (depth * TLV_TABSIZE)     # (int)    depth as a string of spaces         
        self.raw = ''                             # (binary) raw value of entire TLV item (covers everything)
        
        if arg1 != None and arg2 != None:
            if DEBUG: print self._d, 'BUILD',arg1,arg2
            # BUILDING MODE: tag and value
            assert validhex(arg1)
            tag = fromhex(arg1)
            assert validhextag(arg1)
            
            if primitive(tag):
                assert type(arg2) == str
                value = fromhex(arg2)
                data = maketlv(tag,value)
                if DEBUG: print self._d, 'PRIM:',tohex(data)
            else:                                 
                items = []                     # constructed tag has a list of items which can either be T objects or raw TLV strings
                value = arg2
                if type(value) != list:
                    value = [value]                
                for item in value:
                    if type(item) == str:
                        if DEBUG: print self._d, 'MAKEITEM',item
                        items.append(T(item,depth=self.depth+1))   # parse string into T object
                    elif item.__class__ == T:
                        if DEBUG: print self._d, 'ADDITEM',item
                        item._setdepth(self.depth+1)
                        items.append(item)      # just add the T object
                    else:
                        raise Exception('arg2 must be either a hex string or a T object')
                value = "".join( t.raw for t in items )
                if DEBUG: print self._d, 'TOMAKE', tohex(tag), tohex(value)
                data = maketlv(tag,value)
            if DEBUG: print self._d, 'BUILT',tohex(data)
            self._parse(data)
        elif arg1:
            # PARSING MODE: data to parse
            if DEBUG: print self._d, 'PARSE',arg1
            self._parse(fromhex(arg1))
        else:
            raise Exception('Cannot create TLV object without any data or tag ID')
        if DEBUG: print 'COMPLETE',self.rawh,len(self.rawh),len(self.raw)

    def add(self,item):
        items = self.items + [item]
        value = "".join( t.raw for t in items )
        data = maketlv(self.tag,value)
        self._parse(data)
        
    def dump(self,showlen=False):
        result = ''
        if self.constructed:
            result += self._d + '> ' + tagname(self.tagh) + ' (' + self.tagh + '):\n'
            result += self._d + (' '*TLV_TABSIZE) + '|\n'
            itc = self.items
            while len(itc) > 0:
                item = itc[0]
                result += item.dump() + '\n'
                if len(itc) > 1:
                    result += self._d + (' '*TLV_TABSIZE) + '|\n'
                itc = itc[1:]
        else:
            result += self._d + '|-' + tagname(self.tagh) + ' (' + self.tagh + ')\n'
            if len(self.v) > 10:
                result += self._d + '|' + (' '*TLV_TABSIZE) + self.vh + '\n'
                result += self._d + '|' + (' '*TLV_TABSIZE) + to_ascii(self.v) + '\n'
            else:
                result += self._d + '|' + (' '*TLV_TABSIZE) + self.vh + ' - ' + to_ascii(self.v,short=True) + '\n'
        return result[:-1]
    
    def findone(self,tag):
        '''finds exactly one tag in this struct using a search'''
        results = self.search(tag)
        if len(results) == 1: return results[0]
        if len(results) == 0: return None
        raise Exception("Found %d tags when expected to find exactly 1 during tag search" % len(results))
        
    def search(self,tag):
        '''return a list of T objects matching at all depths'''
        if self.constructed:
            return list(chain(*[ t.search(tag) for t in self.items ]))

        # primitive
        if tag == self.tagh:
            return [ self ]
        return []
            
    def __contains__(self,data):
        for t in self.items:
            if t.tagh == data:
                return True
        return False
        
    def _setdepth(self,depth):
        self.depth = depth
        self._d = ' ' * (depth * TLV_TABSIZE)
        if self.constructed:
            for i in self.items:
                i._setdepth(depth+1)
                
    def _parse(self,data):
        if DEBUG: print self._d,tohex(data)
        raw = data
        t = data[0]
        if ord(t) & 0x1F == 0x1F:
            t += data[1]
            
        data = data[len(t):]

        if ord(data[0]) & 0x80 == 0x80:
            l = ord(data[0]) * 256 + ord(data[1])
            self.lenb = data[:2]
            data = data[2:]
        else:
            l = ord(data[0])
            self.lenb = data[:1]
            data = data[1:]

        v = data[:l]
        data = data[l:]
        self.tag = t
        self.tagh = tohex(t)
        self.len = l
        self.raw = raw
        self.rawh = tohex(self.raw)
        
        self.header = self.tag + self.lenb
        self.remlen = len(self.header) + len(v)
        
        self.constructed = ord(t[0]) & 0x20 == 0x20
        
        if self.constructed:    #constructed
            #print self.d, 'CONSTRUCTED',hex(ord(self.tag))
            self.count = 0
            self.items = []
            
            self.v = v
            self.vh = tohex(self.v)
            
            while len(v) > 0:
                self.count += 1
                #print self.d, 'LENV', len(v), tohex(v)
                ncls = T(tohex(v),depth=self.depth+1)
                #print self.d, 'SETVAL',hex(ord(self.tag))
                #print 'NCLS', ncls.tag, ncls.len, ncls.vh
                #print 'NCLS RAW', ncls.rawh, len(ncls.raw)
                #print self.d, 'VBEF', tohex(v)
                v = v[ncls.remlen:]                
                #print self.d, 'VAFT', tohex(v)
                
                tagName = 'T' + tohex(ncls.tag).upper()
                
                if tagName in self.__dict__:
                    # rename single tag to a numbered tag
                    tagVal = self.__dict__[tagName]
                    del self.__dict__[tagName]
                    self.__dict__[tagName + '_00'] = tagVal

                    # find next free numbered tag
                    ctr = 0
                    while (tagName + '_%02d' % ctr) in self.__dict__:
                        ctr+=1

                    # add in new tag
                    self.__dict__[tagName + '_%02d' % ctr] = ncls
                else:
                    self.__dict__[tagName] = ncls                 
                self.items.append(ncls)
                #print self.d, 'LOOP ITEMS=',len(self.items)
        else:
            tagName = 'T' + tohex(t).upper()
            #print self.d, 'PRIMITIVE', tagName
            self.items = []
            self.count = 0
            
            #self.raw = self.raw[len(self.tag):len(self.tag)+self.len]
            
            self.v = v
            self.vh = tohex(self.v)
                        
            if tagName in self.__dict__:
                # rename single tag to a numbered tag
                tagVal = self.__dict__[tagName]
                del self.__dict__[tagName]
                self.__dict__[tagName + '_00'] = tagVal

                # find next free numbered tag
                ctr = 0
                while (tagName + '_%02d' % ctr) in self.__dict__:
                    ctr+=1

                # add in new tag
                self.__dict__[tagName + '_%02d' % ctr] = v
            else:
                self.__dict__[tagName] = v
                
    def __repr__(self):
        if self.constructed:
            res = 'T' + tohex(self.tag)
            res +='={'
            for k in self.items:
                res += ' ' + k.__repr__()
            res +=' }'
        else:
            res = 'T_' + self.tagh + '=' + self.vh + ''
        return res
    
    def __iter__(self):
        return TI(self)
        
class TI:
    '''T object iterator class'''
    
    def __init__(self,t):
        self.t = t
        self.i = 0
        
    def __iter__(self):
        return self
        
    def next(self):
        if self.i == self.t.count:
            raise StopIteration
        self.i += 1
        return self.t.items[self.i-1]    

# RANDOM OLD FUNCTIONS        
#def constructed(tlv):
#    return ord(tlv[0][0]) & 0x20 == 0x20
#def xor(x,y):
#    assert len(x) == len(y)
#    return "".join( [ chr( ord(xx) ^ ord(yy) ) for (xx,yy) in zip(x,y) ] )
#def binbyte(c):
#    return (bin(ord(c)))[2:].rjust(8,'0')

#------------------------- TEST ROUTINES -------------------------------

def run_tests():
    print 'TEST1: parse a TLV string'
    print '========================='
    
    t = T('6F1A840E315041592E5359532E4444463031A5088801025F2D02656E')
    print t.dump()
    
    print 'TEST2: make a TLV object using nested constructors'
    print '=================================================='
    t = T('6F', [ T('84',tohex('HELLOWORLD')) , T('A5', [ T('88','02') , T('5F2D',tohex('en')) ] ) ] )
    print t.dump()
    print 
    print t.T84.dump()
    print
    print t.TA5.T5F2D.dump()
    print
    print t.TA5.T88.dump()    
    
    print 'TEST3: make a TLV object using makeempty and add'
    print '================================================'
    t = T('6F', [])
    print t
    t.add( T('84',tohex('HELLOWORLD')) )
    print t
    t.add( T('A5', [ T('88','02') , T('5F2D',tohex('en')) ] ) )
    print t
    print t.TA5.dump()
    print
    print t.TA5.T88.dump()
    print
    print t.dump()
    
if __name__ == '__main__':
    run_tests()
