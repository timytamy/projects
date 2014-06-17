import random, time
import mySimpleDmx

DEBUG = False

#DMX Stuff
RGB_MIX = [255, 128, 128]
GOAL_DMX_PATCH = [
   [129, 132, 135, 1],
   [138, 141, 144, 7],
   [147, 150, 153, 13],
   [156, 159, 162, 19]
]

NUM_GOALS = 4
BLU_L = 0
BLU_R = 1
RED_L = 2
RED_R = 3

LEFT = 0
RIGHT = 1

COLD = 0
HOT = 255


class HotGoalSystem(object):

   def __init__(self, comport):
      self.timePrint ("Connecting to DMX widget...")
      self.dmx = mySimpleDmx.DmxWidget(comport)
      self.dmx.blackout()
      self.timePrint ("...DONE")
      
      if DEBUG:
         for i in range(0, 512):
            self.dmx.setChannel(i, 255, True)
            time.sleep(0.1)
            self.dmx.setChannel(i, 0, True)
         self.dmx.blackout()
      
      self.goalIsHot = [False]*4
      self.firstHotGoal = LEFT
      self.randomiseHotGoal()
   
   def runAutoSequence(self):
      self.timePrint("Starting hot goal sequence")
      self.setAllGoalsCold()
   
      if self.firstHotGoal == LEFT:
         self.setGoalTemp(BLU_L, HOT)
         self.setGoalTemp(RED_L, HOT)
      else:
         self.setGoalTemp(BLU_R, HOT)
         self.setGoalTemp(RED_R, HOT)

      time.sleep(5)
      self.timePrint("Swapping hot goals")
      self.swapHotGoals(BLU_L, BLU_R)
      self.swapHotGoals(RED_L, RED_R)

      time.sleep(5)
      self.timePrint("Setting both goals back to cold")
      self.setAllGoalsCold()
      self.randomiseHotGoal()
      
      self.timePrint("Hot goal sequence complete")
    
######## Helper Functions ########
   
   def randomiseHotGoal (self):
      if random.randint(0,1) == 0:
         self.firstHotGoal = LEFT
         self.timePrint("The LEFT goal will be hot first")
      else:
         self.firstHotGoal = RIGHT
         self.timePrint("The RIGHT goal will be hot first")
   
   def swapHotGoals (self, goalA, goalB):
      if self.goalIsHot[goalA] == True:
         self.setGoalTemp(goalA, COLD)
         self.setGoalTemp(goalB, HOT)
      else:
         self.setGoalTemp(goalB, COLD)
         self.setGoalTemp(goalA, HOT)
      
   def setGoalTemp (self, goal, temp, val = [0]*3):
      if temp == HOT:
         val = RGB_MIX
         self.goalIsHot[goal] = True
      elif temp == COLD:
         val = [0]*3
         self.goalIsHot[goal] = False      

      for channel in GOAL_DMX_PATCH[goal]:
         self.dmx.setChannel(channel+0, val[0])
         self.dmx.setChannel(channel+1, val[1])
         self.dmx.setChannel(channel+2, val[2])
      
      self.dmx.render()

   def setAllGoalsCold (self):
      for goal in range(0, NUM_GOALS):
         self.setGoalTemp(goal, COLD)
         
   def timestamp (self):
      return time.strftime("%H:%M:%S")
      
   def timePrint (self, string):
      print time.strftime("%H:%M:%S"), string
