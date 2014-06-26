#!/usr/bin/env python
import socket
import time

import hotGoalSystem

AUTO_START_MSG = "FIELD:T000"

TCP_ADDR = hotGoalSystem.getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32

timePrint = hotGoalSystem.timePrint

def makeTcpConnection():
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
        makeTcpConnection()


#### Start of main program ####

tcpAddr = TCP_ADDR
tcpPort = TCP_PORT

makeTcpConnection()

for i in range(0, 5):
    txMessage("HBEAT:" + time.strftime("%H%M%S"))
    time.sleep(1)

while True:
    txMessage("HBEAT:" + time.strftime("%H%M%S"))
    txMessage(AUTO_START_MSG)
    txMessage("HBEAT:" + time.strftime("%H%M%S"))
    
    for i in range(0, 15):
        txMessage("HBEAT:" + time.strftime("%H%M%S"))
        txMessage("DORGB:" + chr(0) + chr(123) + chr(234))
        time.sleep(1)
