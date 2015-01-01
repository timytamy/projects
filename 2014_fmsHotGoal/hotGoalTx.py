#!/usr/bin/env python
import random
import socket
import sys
import time

from hotGoalNetworked import *

def main ():
    if (len(sys.argv) < 3):
        print "Usage:", sys.argv[0], "[ADDRESS] [FUNCTION] [OPTIONAL VALUES]"
        return

    random.seed()
    makeTcpConnection(sys.argv[1], TCP_PORT)

    line = sys.argv[2].lower()

    if line in "autonomous hotgoal":
        while (True):
            txAutoSig()
            time.sleep(15)

    elif line in "countdown":
        while (True):
            txCountDownSig()
            time.sleep(15)

    elif line in "fun":
        while (True):
            txHaveFun()
            time.sleep(1)

    elif line in "rgb":
        while (True):
            rgbVals = [0]*3
            if (len(sys.argv) >= 3+3):
                rgbVals[0] = int(sys.argv[3+0])
                rgbVals[1] = int(sys.argv[3+1])
                rgbVals[2] = int(sys.argv[3+2])
            else:
                rgbVals[0] = random.randint(0, 255)
                rgbVals[1] = random.randint(0, 255)
                rgbVals[2] = random.randint(0, 255)

            setRgbValues(rgbVals[0], rgbVals[1], rgbVals[2]);
            time.sleep(1)

    elif line in "eachrgb":
        while (True):
            eaRgbVals = [0]*12
            if (len(sys.argv) >= 3+12):
                for i in range(0, 12):
                    eaRgbVals[i] = sys.argv[1+i]
            else:
                for i in range(0, 12):
                    eaRgbVals[i] = random.randint(0, 255)

            setEaRgbValues(eaRgbVals);
            time.sleep(1)

    elif line in "runmatch":
        while (True):
            txAutoSig()
            time.sleep(140)
            txCountDownSig()
            time.sleep(15)

def txAutoSig ():
    txMessage(MSG_AUTO_START)

def txCountDownSig ():
    txMessage(MSG_COUNTDOWN);

def txHeartbeat ():
    txMessage(MSG_PRE_HBEAT + time.strftime("%H%M%S"))

def txHaveFun ():
    txMessage(MSG_HAVE_FUN)

def setRgbValues(r, g, b):

    timePrint("Setting RGB values to ("+str(r)+" "+str(g)+" "+str(b)+")")

    # Can't transmit NULL equivilent, so 1 ~= 0
    if (r == 0): r = 1
    if (g == 0): g = 1
    if (b == 0): b = 1

    rgbString = chr(r) + chr(g) + chr(b)

    txMessage(MSG_PRE_RGB + rgbString)

def setEaRgbValues(eaRgb):
    if (eaRgb.Length != 12):
       timePrint("Invalid values for setEaRgbValues")
       return

    timePrint("Setting individual RGB values to")
    for i in range(0, len(eaRgb), 3):
        timePrint(eaRgb[i+0] +" "+ eaRgb[i+1] +" "+ eaRgb[i+2]);

    # Can't transmit NULL equivilent, so 1 ~= 0
    for i in range(0, len(eaRgb)):
        if (eaRgb[i] == 0): eaRgb[i] = 1

    for i in range(0, len(eaRgb)):
        rgbString[i] = chr(eaRgb[i])

    txMessage(MSG_PRE_RGB_EA + rgbString)

def makeTcpConnection (tcpAddr, tcpPort):
    global sock
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    while True:
        try:
            timePrint("Connecting to " + str(tcpAddr) +":"+ str(tcpPort) +"...")
            sock.connect((tcpAddr, tcpPort))
            timePrint("...DONE")
            return;
        except:
            timePrint("...Error connecting, trying again")
            time.sleep(1)

def txMessage (message):
    if (len(message) > MSG_SIZE):
        timePrint("message \"" + message + "\" is too long")
        return;

    timePrint("TX: \"" + message + "\"")

    while (len(message) < MSG_SIZE):
        message = message + "\0"

    try:
        sock.send(message)
    except:
        timePrint("Transmit faild:")
        timePrint("Connection closed, attempting reconect")
        sock.close()
        makeTcpConnection(TCP_ADDR, TCP_PORT)

if (__name__ == "__main__"): main() # Hack way to allow forward declerations
