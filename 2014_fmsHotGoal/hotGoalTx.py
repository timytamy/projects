#!/usr/bin/env python
import socket, time

TCP_IP = "127.0.0.1"
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "FMS:T000"

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

#### Start of main program ####
makeTcpConnection()

time.sleep(5)

while True:
   try:
      timePrint("TX: \"" + str(AUTO_START_MSG) + "\"") 
      sock.send(AUTO_START_MSG)
   except:
      timePrint("Transmit faild:")
      timePrint("Connection closed, attempting reconect")
      sock.close()
      makeTcpConnection()
   time.sleep(15)
