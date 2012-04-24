import platform
import string

## Windows PCSC compatibility flag
if platform.system() in [ 'Windows', 'Microsoft' ]:
    windows = True
else:
    windows = False

def fixPathForPlatform(path):
    if not path: return None
    
    '''fixes up a path to be appropriate for the platform'''
    if windows:
        return path.replace('/','\\')
    else:
        return path.replace('\\','/')

UTILS_valid_ascii = string.digits + \
              string.letters + \
              string.punctuation

def to_ascii(s,short=False):
    l=[]
    for c in s:
        if c in UTILS_valid_ascii:
            l.append(c)
            if not short: l.append(' ')
        else:
            l.append(".")
            if not short: l.append('.')
    return "".join(l)
