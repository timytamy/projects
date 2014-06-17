import socket, time
import hotGoalSystem

TCP_IP = socket.gethostname()
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "Match Start Auto"

COMPORT = '/dev/ttyUSB0'

def timestamp():
   return time.strftime("%H:%M:%S")

goals = hotGoalSystem.HotGoalSystem(COMPORT)

# Setup and listen on socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
while True:
   try:
      s.bind((TCP_IP, TCP_PORT))
      break #ic succesful, break out of loop
   except:
      print timestamp(), "Error binding to socket, trying again in 10s"
      time.sleep(10)

print timestamp(), "Socket bind succesfull"
s.listen(1)

# Yes this probably falls over if it recieves anything not AUTO_START_MSG
# in quick succesion
# TODO: Fix that  issue^^
while True:
   conn, addr = s.accept()
   print timestamp(), 'Established connection:', addr

   while True:
      data = conn.recv(BUFFER_SIZE)

      if not data:
         print timestamp(), "Connection closed by peer"
         print timestamp(), "waiting for reconect"
         break

      print timestamp(), "received data:", data
      if data == AUTO_START_MSG:
         print timestamp(), "Starting hotGoalSequence"
         goals.runAutoSequence()

   conn.close()
