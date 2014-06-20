#!/usr/bin/env python
import socket, time
import hotGoalSystem

TCP_IP = "127.0.0.1"
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "FMS:T000"

COMPORT = '/dev/ttyUSB0'

def timePrint(string):
   print time.strftime("%H%M%S"), string

#### Start of the main-ish function ####

goals = hotGoalSystem.HotGoalSystem(COMPORT)

socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
while True:
   try:
      timePrint("Attempting to bind to socket...")
      socket.bind((TCP_IP, TCP_PORT))
      socket.listen(1)
      timePrint("...DONE")
      break #if succesful, break out of loop
   except:
      timePrint("...Error binding to socket, trying again")
      time.sleep(1)

#TODO: This could probably be a bit more elegant/rhobust
while True:
   conn, addr = socket.accept()
   timePrint("Connection recieved from " + str(addr))

   while True:
      msg = conn.recv(BUFFER_SIZE)

      if not msg: #if diconnected
         timePrint("Connection closed by peer")
         timePrint("waiting for reconect")
         break

      timePrint("Rx: \"" + str(msg) + "\"")
      if msg == AUTO_START_MSG:
         timePrint("Starting hotGoalSequence")
         goals.runAutoSequence()

   conn.close()
