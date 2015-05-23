#!/user/bin/env python

import math
import string
import sys

def main():
    if len(sys.argv) <= 2: printUsage(); return
    elif len(sys.argv) == 3: #Cipher and text is given
        cipher, command, layers, strIn = sys.argv[1].lower(), "",  1, sys.argv[2]
    elif len(sys.argv) == 4: #Cipher, encode/decode and text is given
        cipher, command, layers, strIn = sys.argv[1].lower(), sys.argv[2].lower(), 1, sys.argv[3]
    elif len(sys.argv) == 5: #Cipher, encode/decode, numOfIterations and text is given
        cipher, command, layers, strIn = sys.argv[1].lower(), sys.argv[2].lower(), int(sys.argv[3]), sys.argv[4]

    if command in "encode encrypt": decrypt = False
    elif command in "decode decrypt crack": decrypt = True
    else: decrypt = False

    strOut = strIn
    for i in range(layers):
        if (cipher in "pennycode"): strOut = pennyCode(strOut, decrypt)
        elif (cipher in "rot13"): strOut = rot13(strOut)
        elif (cipher in "rot47"): strOut = rot47(strOut)
        elif (cipher in "rotx"): strOut = rotX(strOut, decrypt, layers); break
        elif (cipher in "blakeapproves"): strOut = blakeApproves(strOut, decrypt)
        elif (cipher in "lazysod"): strOut = lazySod(strOut, decrypt)
        else: print "ERROR: unrecognised cipher"; return

    if decrypt:
        print "\nDECODING: \"" + strIn + "\"\n"
    else:
        print "\nENCODING: \"" + strIn + "\"\n"

    print "OUTPUT: \"" + strOut + "\""


def pennyCode (strIn, decrypt = False):
    pennyPlain  = "`1234567890-= qwertyuiop[]\ asdfghjkl;' zxcvbnm,./ ~!@#$%^&*()_+ QWERTYUIOP{}| ASDFGHJKL:\" ZXCVBNM<>?"
    pennyCipher = "1234567890-=` wertyuiop[]\q sdfghjkl;'a xcvbnm,./z !@#$%^&*()_+~ WERTYUIOP{}|Q SDFGHJKL:\"A XCVBNM<>?Z"
    tableEncode = string.maketrans(pennyPlain, pennyCipher)
    tableDecode = string.maketrans(pennyCipher, pennyPlain)

    if (decrypt): strOut = strIn.translate(tableDecode)
    else: strOut = strIn.translate(tableEncode)

    return strOut


def rot13 (strIn):
    charsOut = []
    for i in range(len(strIn)):
        if strIn[i] in string.uppercase:
            charsOut.append(chr(65 + (ord(strIn[i]) - 52) % 26)) #13-65 == -52 
        elif strIn[i] in string.lowercase:
            charsOut.append(chr(97 + (ord(strIn[i]) - 84) % 26)) #13-97 == -83
        else:
            charsOut.append(strIn[i])

    return ''.join(charsOut)


def rot47 (strIn):
    return rotX(strIn, True, 47)


def rotX (strIn, decrypt, rotValue):
    charsOut = []
    if (decrypt):
        for i in range(len(strIn)):
            if (33 <= ord(strIn[i]) and ord(strIn[i]) <= 126):
                charsOut.append(chr(33 + (ord(strIn[i]) + 61 - rotValue) % 94)) 
            else:
                charsOut.append(strIn[i])
    else:
        for i in range(len(strIn)):
            if (33 <= ord(strIn[i]) and ord(strIn[i]) <= 126):
                charsOut.append(chr(33 + (ord(strIn[i]) - 33 + rotValue) % 94)) 
            else:
                charsOut.append(strIn[i])

    return ''.join(charsOut)


def blakeApproves (strIn, decrypt = False, key = ["Blake", "Approves", ".com"]):
    if (decrypt):
        strSplit = strIn.split(key[2]) #Split into each character
        charsOut = []
        for i in range(len(strSplit)):
            wordSplit = strSplit[i].split(" ") #Split into each bit
            charCurr = 0;
            for j in range(len(wordSplit)):
                if wordSplit[len(wordSplit)-1-j] == key[1]: charCurr += 2**j #for all 1s, add 2^j
            charsOut.append(chr(charCurr)) #List of chars
        strOut = ''.join(charsOut) #Join string up
    else:
        strOut = (key[2].join((" ".join(key[0] if bit == "0" else key[1] for bit in bin(ord(char))[2:])) for char in strIn)) #wtf

    return strOut


def lazySod (strIn, decrypt = False):
    return blakeApproves(strIn, decrypt, ["Lazy", "Sod", ".com "])


def printUsage ():
    print "USAGE:\t python " + sys.argv[0] + " [CODE] [TEXT]"
    print "\t python " + sys.argv[0] + " [CODE] [ENCRYPT/DECRYPT] [TEXT]"
    print "\t python " + sys.argv[0] + " [CODE] [ENCRYPT/DECRYPT] [NUM ITERATIONS] [TEXT]"


if (__name__ == "__main__"): main()
