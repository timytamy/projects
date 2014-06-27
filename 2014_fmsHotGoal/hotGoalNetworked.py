#!/usr/bin/env python
import socket
import time
import hotGoalSys

DEBUG = False
PRINT_RX = False

TCP_ADDR = hotGoalSys.getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32
MSG_AUTO_START = "FIELD:T000"
MSG_COUNTDOWN = "FIELD:T144"
MSG_PRE_RGB = "DORGB:"
MSG_PRE_RGB_EA = "EARGB:"

AUTO_CNTDN_DELAY = (150-10-6)

COMPORT = "/dev/ttyUSB0"

timePrint = hotGoalSys.timePrint

#### Start of the main-ish function ####

goals = hotGoalSys.HotGoalSystem(COMPORT)

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
        
timePrint("Waiting for connection...")
 
#TODO: This could probably be a bit more elegant/rhobust
while True:
    conn, addr = sock.accept()
    timePrint("Connection recieved from " + str(addr))
    newData = True
    while True:
        if  (newData or DEBUG):
            print "\n" + "*"*80
            goals.printFirstHotGoal()
            timePrint("Hot Goal system IS ready")
            print "*"*80 + "\n"
            newData = False

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
        if (PRINT_RX or DEBUG): timePrint("Rx: \"" + str(msg) + "\"")

        if (MSG_AUTO_START in msg):
            newData = True
            timePrint("Starting hotGoalSequence")
            goals.runAutoSequence()
            timePrint("Waiting until the end of match")
            time.sleep(AUTO_CNTDN_DELAY)
            timePrint("Starting countdown sequence")
            goals.runCountDownSeq()
            
        elif (MSG_COUNTDOWN in msg):
            newData = True
            timePrint("Starting countdown sequence")
            goals.runCountDownSeq();
            
        elif (MSG_PRE_RGB in msg):
            newData = True
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
                goals.renderGoals()
            except:
                timePrint("Invalid RGB values")
        elif (MSG_PRE_RGB_EA in msg):
            newData = True
            timePrint("Setting goals to individual RGB val")
            try:
                msg = msg.replace(chr(1), chr(0)) # Accounts for 0 vals
                for goal in range(0, hotGoalSys.NUM_GOALS):
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
