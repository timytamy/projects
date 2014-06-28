#!/usr/bin/env python
import fileinput
import socket
import sys
import time

import hotGoalSys

timePrint = hotGoalSys.timePrint
getLocalIp = hotGoalSys.getLocalIp

TCP_ADDR = "10.0.100.101" #getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32
MSG_AUTO_START = "FIELD:T000"
MSG_COUNTDOWN = "FIELD:T140"
MSG_PRE_HBEAT = "HBEAT:"
MSG_PRE_RGB = "DORGB:"
MSG_PRE_RGB_EA = "EARGB:"
MSG_HAVE_FUN  = "HVFUN:"

def makeTcpConnection(tcpAddr, tcpPort):
    global sock
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    while True:
        try:
            timePrint("Connecting to " + str(tcpAddr) +":"+ str(tcpPort))
            sock.connect((tcpAddr, tcpPort))
            timePrint("...DONE")
            break;
        except:
            timePrint("...Error connecting, trying again")
            time.sleep(1)

def txMessage(message):
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

def txAutoSig():
    txMessage(MSG_AUTO_START)
    
def txCountDownSig ():
    txMessage(MSG_COUNTDOWN);

def txHeartbeat():
    txMessage(MSG_PRE_HBEAT + time.strftime("%H%M%S"))
    
def txHaveFun():
    txMessage(MSG_HAVE_FUN)

#### Start of main program ####

makeTcpConnection(TCP_ADDR, TCP_PORT)

if True:
    line = sys.argv[1].lower()
    if line in "auto":
        while (True):
            txAutoSig()
            time.sleep(15)

    elif line in "countdown":
        while (True):
            txCountDownSig()
            time.sleep(15)

    elif line in "fun":
        while (True):
            time.sleep(0.1)
            txHaveFun()
    
