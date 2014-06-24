#!/usr/bin/env python
import socket, time

AUTO_START_MSG = "FIELD:T000"

TCP_IP = "127.0.0.1"
TCP_PORT = 3132
MSG_SIZE = 32

def timePrint(string):
   print time.strftime("%H%M%S"), string

def makeTcpConnection():
   global sock
   sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

   while True:
      try:
         timePrint("Connecting to " + str(TCP_IP) + ":" + str(TCP_PORT))
         sock.connect((TCP_IP, TCP_PORT))
         timePrint("...DONE")
         break;
      except:
         timePrint("...Error connecting, trying again")
         time.sleep(1)

def txMessage(message):
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
      time.sleep(1)
