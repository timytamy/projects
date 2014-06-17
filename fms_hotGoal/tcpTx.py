#!/usr/bin/env python

import socket, time


TCP_IP = socket.gethostname()
TCP_PORT = 3132
BUFFER_SIZE = 1024
MESSAGE = "Match Start Auto"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((TCP_IP, TCP_PORT))
while True:
   s.send(MESSAGE)
   time.sleep(11)
s.close()
