import emv
import emvtags

dol_values = {
    ## For PDOL
    #"9f33": "60c0a0", # Terminal capabilities (03) (everything)
    "9f33": "60c000", # Terminal capabilities (03) (no security)
    ## "9f1a": "0276",  # Terminal country code (02) -- gb
    "9f35": "14", # Terminal type (01) -- attended/merchant 
    "9f40": "8e00b05005", # Additional terminal capabilities (05) -- everything on

    ## For CDOL1/2
    "9f02": "000000000175", # Amount, Authorised (Numeric)
    "9f03": "000000000000", # Amount, Other (Numeric)
    "9f1a": "0826", # Terminal Country Code
    "95":   "0000008000", # Terminal Verification Results
    "5f2a": "0826", # Transaction Currency Code
    "9a":   "051007", # Transaction Date
    #"9c":   "00", # Transaction Type
    "9f37": "5648685d", # Unpredictable Number
    "9f34": "000000", # CVM results
    
    "8a":   "3030", # Authorisation Response Code

    "9f4c": "", # ICC dynamic number (8 bytes)
    "9f45": "", # Data authentication code (2 bytes)
    "9f34": "010002", # CVM results

    "91": "00000000000000003030", # Issuer authentication data (16 bytes)
    }

dol_values_cap = {
    "9f1a": "0000", # Terminal Country Code
    "95":   "8000000000", # Terminal Verification Results
    "5f2a": "0000", # Transaction Currency Code
    #"9a":   "000000", # Transaction Date for application xxx48xxx (Natwest)
    "9a":   "010101", # Transaction Date for application xxx38xxx (Barclays)
    "8a":   "3030", # Authorisation response code (MKB prev resp code 5a33 was not decimal)
    "9f35": "34", # Terminal type
    }

def build_dol(dol, compat_mode, extra_values):
    '''Return DOL'''
    print "DOL", dol

    built_dol = []

    i = 0
    while i < len(dol):
        tag = dol[i:i+2]
        i_tag = emv.emv_int(tag)
        
        ## Check whether tag is one or two bytes, and get it
        if (i_tag & 0x1F) == 0x1F:
            ## Two byte tag
            tag = dol[i:i+4].upper()
            i += 4
        else:
            ## One byte tag
            i += 2

        ## Get the length
        length = emv.emv_int(dol[i:i+2])
        i += 2

        ## Add the value onto the built DOL
        built_dol.append(get_value(tag, length, compat_mode, extra_values))
        
    return "".join(built_dol)

def get_value(tag, length, compat_mode, extra_values):
    ## Convert length to nibbles
    length *= 2

    ## Search order for values
    val_list = [dol_values]
    if compat_mode == emv.MODE_CAP:
        val_list.insert(0, dol_values_cap)
    if extra_values != None:
        val_list.insert(0, extra_values)
    
    ## Get the tag value
    val = None
    for d in val_list:
        val = d.get(tag.lower(), None)
        if val != None:
            if emvtags.TAGS.has_key(tag.upper()):
		print "%4s %s: %s"%(tag,emvtags.TAGS[tag.upper()], val)
	    else:
		print "Unknown: %s"%(val)
            break
    if val == None:
        raise Exception("Cannot find default value for tag", tag)

    ## Truncation/padding actually depends on format (numeric,
    ## compressed numeric or other). For simplicity we just use
    ## trailing zeros, applicable for "other" types

    ## Truncate rightmost bytes
    val = val[:length]

    ## Pad on right with zeros
    pad_chars = length - len(val)
    val = val + "0"*pad_chars

    return val

def fixed_dol(dol):
    '''Deprecated'''
    if dol.lower()=="9f33039f1a029f35019f4005":
        return (
            "830b"
            "60c0a0" # Terminal capabilities (03)
            "0276" # Terminal country code (02) -- gb
            "14" # Terminal type (01) -- attended/merchant
            "8e00b05005")  # Additional terminal capabilities (05) -- everything on
    else:
        return "8300"

