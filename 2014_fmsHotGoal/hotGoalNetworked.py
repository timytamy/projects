#!/usr/bin/env python
import socket
import time
import hotGoalSystem

DEBUG = True
PRINT_RX = False

TCP_ADDR = hotGoalSystem.getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32
MSG_AUTO_START = "FIELD:T000"
MSG_PRE_RGB = "DORGB:"
MSG_PRE_RGB_EA = "EARGB:"

COMPORT = "/dev/ttyUSB0"

timePrint = hotGoalSystem.timePrint

#### Start of the main-ish function ####

goals = hotGoalSystem.HotGoalSystem(COMPORT)

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
while True:
    try:
        timePrint("Binding to " + str(TCP_ADDR) + ":" + str(TCP_PORT))
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
    newinfo = int(0)
    while True:
        if  (newinfo == 0):
            print "\n" + "*"*80
            goals.printFirstHotGoal()
            timePrint("Hot Goal system IS ready")
            print "*"*80 + "\n"
            newinfo = int(1)

        msg = None
        try:
            msg = conn.recv(MSG_SIZE)
        except:
            msg = None

        if not msg: #if diconnected
            print "\n" + "*"*80
            timePrint("Connection closed, Waiting for reconnect")
            timePrint("Hot Goal system IS NOT ready")
            print "*"*80 + "\n"
            break
            
        msg = msg.translate(None, '\0') # Removes padding
        if (PRINT_RX): timePrint("Rx: \"" + str(msg) + "\"")

        if (MSG_AUTO_START in msg):
            newinfo = 0            
            timePrint("Starting hotGoalSequence")
            goals.runAutoSequence()
        elif (MSG_PRE_RGB in msg):
            newinfo = 0
            timePrint("Setting goals to RGB val")
            r = 0; g = 0; b = 0;
            try:
                msg = msg.replace(chr(1), chr(0)) # Accounts for 0 vals
                r = ord(msg[6+0])
                g = ord(msg[6+1])
                b = ord(msg[6+2])
                if (DEBUG):
                    debugString = str(r) + " " + str(g) + " " + str(b)
                    timePrint("All @ (" + debugString + ")")
                goals.setAllRgb(r, g, b)
            except:
                timePrint("Invalid RGB values")
        elif (MSG_PRE_RGB_EA in msg):
            newinfo = 0
            timePrint("Setting goals to individual RGB val")
            try:
                msg = msg.replace(chr(1), chr(0)) # Accounts for 0 vals
                for goal in range(0, hotGoalSystem.NUM_GOALS):
                    r = ord(msg[6+(goal*3)+0])
                    g = ord(msg[6+(goal*3)+1])
                    b = ord(msg[6+(goal*3)+2])
                    if (DEBUG):
                        debugString = str(r) +" "+ str(g) +" "+ str(b)
                        timePrint(str(goal) + " @ (" + debugString + ")")
                    goals.setGoalRgb(goal, r, g, b)
                goals.renderGoals()
            except:
                timePrint("Invalid individual RGB values")

    conn.close()
