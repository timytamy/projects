#!/usr/bin/env python
import socket, time


TCP_IP = "127.0.0.1"
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "FMS:T000"

def timePrint(string):
   print time.strftime("%H%M%S"), string

socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

while True:   
   try:
      timePrint("Connecting to " + str(TCP_IP) + ":" + str(TCP_PORT))
      socket.connect((TCP_IP, TCP_PORT))
      timePrint("...DONE")
      break;
   except:
      timePrint("...Error connecting, trying again")
      time.sleep(1)

time.sleep(5)

while True:
   timePrint("TX: \"" + str(AUTO_START_MSG) + "\"") 
   socket.send(AUTO_START_MSG)
   time.sleep(15)
socket.close()
