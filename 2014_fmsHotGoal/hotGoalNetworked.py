#!/usr/bin/env python
import socket
import time
import hotGoalSystem

TCP_ADDR = hotGoalSystem.getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32
MSG_AUTO_START = "FIELD:T000"
MSG_PRE_RGB = "DORGB:"

COMPORT = "/dev/ttyUSB0"

timePrint = hotGoalSystem.timePrint

#### Start of the main-ish function ####

goals = hotGoalSystem.HotGoalSystem(COMPORT)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
while True:
    try:
        timePrint("Binding to " + str(TCP_ADDR) +":"+ str(TCP_PORT))
        sock.bind((TCP_ADDR, TCP_PORT))
        sock.listen(1)
        timePrint("...DONE")
        break #if succesful, break out of loop
    except:
        timePrint("...Error binding to socket, trying again")
        time.sleep(1)
        
        
timePrint("Wating for connection...")
 
#TODO: This could probably be a bit more elegant/rhobust
while True:
    conn, addr = sock.accept()
    timePrint("Connection recieved from " + str(addr))

    while True:
        print "\n" + "*"*80
        goals.printFirstHotGoal()
        timePrint("Hot Goal system IS ready")
        print "*"*80 + "\n"
        
        msg = conn.recv(MSG_SIZE)

        if not msg: #if diconnected
            print "\n" + "*"*80
            timePrint("Connection closed, Waiting for reconnect")
            timePrint("Hot Goal system IS NOT ready")
            print "*"*80 + "\n"
            break
            
        msg = msg.translate(None, '\0') # Removes padding
        timePrint("Rx: \"" + str(msg) + "\"")
        if (MSG_AUTO_START in msg):
            timePrint("Starting hotGoalSequence")
            goals.runAutoSequence()
            
        elif (MSG_PRE_RGB in msg):
            timePrint("Setting goals to RGB val")
            try:
                msg = msg.replace(chr(1), chr(0))
                r = ord(msg[5+1])
                g = ord(msg[5+2])
                b = ord(msg[5+3])
                goals.setAllRgb(r, g, b)
            except:
                timePrint("Invalid RGB values")

    conn.close()
