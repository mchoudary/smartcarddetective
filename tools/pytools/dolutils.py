import tlv

dol_values = {}

dol_values_cap = {
    "9f03": "000000000000", # Amount, Other (Numeric)
    "9c":   "00", # Transaction Type
    "9f1a": "0000", # Terminal Country Code
    "95":   "8000000000", # Terminal Verification Results
    "5f2a": "0000", # Transaction Currency Code
    "9a":   "000000", # Transaction Date for application xxx48xxx (Natwest)
    #"9a":   "010101", # Transaction Date for application xxx38xxx (Barclays)
    "8a":   "5a33"
    }

def build_dol(dol, compat_mode, extra_values):
    '''Return DOL'''
    built_dol = []

    i = 0
    while i < len(dol):
        tag = dol[i:i+2]
        i_tag = tlv.emv_int(tag)
        
        ## Check whether tag is one or two bytes, and get it
        if (i_tag & 0x1F) == 0x1F:
            ## Two byte tag
            tag = dol[i:i+4].upper()
            i += 4
        else:
            ## One byte tag
            i += 2

        ## Get the length
        length = tlv.emv_int(dol[i:i+2])
        i += 2

        ## Add the value onto the built DOL
        built_dol.append(get_value(tag, length, compat_mode, extra_values))
        
    return "".join(built_dol)

def get_value(tag, length, compat_mode, extra_values):
    ## Convert length to nibbles
    length *= 2

    ## Search order for values
    if compat_mode == tlv.MODE_CAP:
        val_list = [dol_values_cap]
    else:
        val_list = [dol_values]

    if extra_values != None:
        val_list.insert(0, extra_values)
    
    ## Get the tag value
    val = None
    for d in val_list:
        val = d.get(tag.lower(), None)
        if val != None:
            break
    if val == None:
        val = ""
        
    ## Truncation/padding actually depends on format (numeric,
    ## compressed numeric or other). For simplicity we just use
    ## trailing zeros, applicable for "other" types

    ## Truncate rightmost bytes
    val = val[:length]

    ## Pad on right with zeros
    pad_chars = length - len(val)
    val = val + "0"*pad_chars

    return val
