#!/usr/bin/env python
import socket
import sys
import time

from hotGoalSys import getLocalIp, timePrint, HotGoalSystem

DEBUG = False

TCP_ADDR = getLocalIp()
TCP_PORT = 3132
MSG_SIZE = 32
MSG_AUTO_START = "FIELD:T000"
MSG_COUNTDOWN = "FIELD:T140"
MSG_PRE_HBEAT = "HBEAT:"
MSG_PRE_RGB = "DORGB:"
MSG_PRE_RGB_EA = "EARGB:"
MSG_HAVE_FUN  = "HVFUN:"

COMPORT_DEFAULT = "/dev/ttyUSB0"

def main ():

    comport = None
    if (len(sys.argv) < 2):
         timePrint("Usage: " + sys.argv[0] + " [COMPORT]")
         timePrint("Trying with default comport: " + COMPORT_DEFAULT)
         timePrint("")
         comport = COMPORT_DEFAULT
    else: comport = sys.argv[1]

    goals = HotGoalSystem(comport)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    while True:
        try:
            timePrint("Binding to " + str(TCP_ADDR) +":"+ str(TCP_PORT) +"...")
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
            if (newData or DEBUG):
                print "\n" + "*"*80
                goals.printFirstHotGoal()
                timePrint("Hot Goal system IS ready")
                print "*"*80 + "\n"
                newData = False

            msg = None
            try: msg = conn.recv(MSG_SIZE)
            except: msg = None

            if not msg: #if diconnected
                print "\n" + "*"*80
                timePrint("Connection closed")
                timePrint("Waiting for reconnect to "+str(TCP_ADDR)+":"+str(TCP_PORT))
                timePrint("Hot Goal system IS NOT ready")
                print "*"*80 + "\n"
                break

            msg = msg.translate(None, '\0') # Removes padding
            if (DEBUG): timePrint("Rx: \"" + str(msg) + "\"")

            if (MSG_AUTO_START in msg):
                newData = True
                timePrint("Starting hotGoalSequence")
                goals.runAutoSeq()

            elif (MSG_COUNTDOWN in msg):
                newData = True
                timePrint("Starting countdown sequence")
                goals.runCountDownSeq();

            elif (MSG_PRE_HBEAT in msg):
                if (DEBUG): timePrint("Recieved a heartbeat!")

            elif (MSG_PRE_RGB in msg):
                newData = True
                timePrint("Setting goals to RGB val")
                try:
                    msg = msg.replace(chr(1), chr(0)) # Fix 0 vals
                    r = ord(msg[6+0])
                    g = ord(msg[6+1])
                    b = ord(msg[6+2])
                    if (DEBUG):
                        debugString = str(r)+" "+str(g) +" "+str(b)
                        timePrint("All @ ("+debugString+")")
                    goals.setAllRgb(r, g, b)
                    goals.renderGoals()
                except:
                    timePrint("Invalid RGB values")

            elif (MSG_PRE_RGB_EA in msg):
                newData = True
                timePrint("Setting goals to individual RGB values")
                try:
                    msg = msg.replace(chr(1), chr(0)) # Fix 0 vals
                    for goal in range(0, hotGoalSys.NUM_GOALS):
                        r = ord(msg[6+(goal*3)+0])
                        g = ord(msg[6+(goal*3)+1])
                        b = ord(msg[6+(goal*3)+2])
                        if (DEBUG):
                            debugString = str(r)+" "+str(g)+" "+str(b)
                            timePrint(str(goal)+" @ ("+debugString+")")
                        goals.setGoalRgb(goal, r, g, b)
                    goals.renderGoals()
                except:
                    timePrint("Invalid individual RGB values")

            elif (MSG_HAVE_FUN in msg):
                newData = True
                timePrint("Having fun :)")
                goals.haveFun()

        conn.close()

if __name__ == "__main__": main()
