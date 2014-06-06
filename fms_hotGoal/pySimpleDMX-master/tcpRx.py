#!/usr/bin/env python

import socket

TCP_IP = socket.gethostname()
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "Match Start Auto"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.bind((TCP_IP, TCP_PORT))
s.listen(1)


# Yes this falls over if it recieves anything larger than AUTO_START_MSG
# Yes I will fix it later
while 1:
   conn, addr = s.accept()
   print 'Connection address:', addr
   
   while 1:
      data = conn.recv(BUFFER_SIZE)
      
      if not data:
         print "Connection closed, waiting for reconect"
         break
      
      print "received data:", data
      if data == AUTO_START_MSG:
         print data
         
   conn.close()
   
