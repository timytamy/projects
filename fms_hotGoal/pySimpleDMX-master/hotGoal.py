import socket, time
import mySimpleDmx

TCP_IP = socket.gethostname()
TCP_PORT = 3132
BUFFER_SIZE = 1024
AUTO_START_MSG = "Match Start Auto"

COMPORT = '/dev/ttyUSB0'

GOAL_DMX_PATCH = {
   'BLU_L' : [129, 132, 135],
   'BLU_R' : [138, 141, 144],
   'RED_L' : [147, 150, 153],
   'RED_R' : [156, 159, 162]
}

goalIsHot = {
   'BLU_L' : False,
   'BLU_R' : False,
   'RED_L' : False,
   'RED_R' : False,
}

def setGoalHot(goal):
   goalIsHot[goal] = True
   for channel in GOAL_DMX_PATCH[goal]:
      dmx.setChannel(channel+2, 255)
   dmx.render()

def setGoalCold(goal):
   goalIsHot[goal] = False
   for channel in GOAL_DMX_PATCH[goal]:
      dmx.setChannel(channel+2, 0)
   dmx.render()

def setAllGoalsCold():
   for goal in GOAL_DMX_PATCH:
      setGoalCold(goal)

def swapHotGoals(goalA, goalB):
   if goalIsHot[goalA] == True:
      setGoalCold(goalA)
      setGoalHot(goalB)
   else:
      setGoalCold(goalB)
      setGoalHot(goalA)

def runHotGoalSequence():
   print timestamp(), "Starting hot goal sequence"
   setAllGoalsCold()

   if False: # TODO: make pseudo-random
      setGoalHot('BLU_L')
   else:
      setGoalHot('BLU_R')

   if True: # TODO: make pseudo-random
      setGoalHot('RED_L')
   else:
      setGoalHot('RED_R')

   time.sleep(5)
   print timestamp(), "Swapping hot goals"
   swapHotGoals('BLU_L', 'BLU_R')
   swapHotGoals('RED_L', 'RED_R')

   time.sleep(5)
   print timestamp(), "Setting both goals back to cold"
   setAllGoalsCold()

def timestamp():
   return time.strftime("%H:%M:%S")

# Setup DMX
dmx = mySimpleDmx.DMXConnection(COMPORT)

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
         runHotGoalSequence()

   conn.close()
