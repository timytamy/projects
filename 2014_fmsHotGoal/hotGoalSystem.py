import random, time
import mySimpleDmx

DEBUG = False

#DMX Stuff
RGB_MIX_COLD = [0, 0, 0]
RGB_MIX_HOT = [255, 255, 255]
GOAL_DMX_PATCH = [
   [121], # BLU_L
   [127], # BLU_R
   [133], # RED_L
   [139]  # RED_R
]

NUM_GOALS = 4

BLU_L = 0
BLU_R = 1
RED_L = 2
RED_R = 3

LEFT, RIGHT = 0, 1
COLD, HOT = 0, 255

class HotGoalSystem(object):

   def __init__(self, comport):
      self.timePrint ("Connecting to DMX widget...")
      self.dmx = mySimpleDmx.FakeDmxWidget(comport)
      self.timePrint("...DONE")

      if DEBUG:
         for i in range(0, 512+1):
            self.dmx.setChannel(i, 255, True)
            time.sleep(0.05)
            self.dmx.setChannel(i, 0, True)
         self.dmx.clear()

      self.goalIsHot = [False]*NUM_GOALS

      self.firstHotGoal = None
      self.randomiseHotGoal()

   def runAutoSequence(self):
      self.timePrint("****Starting hot goal sequence****")
      self.setAllGoalsCold()

      if self.firstHotGoal == LEFT:
         self.setGoalTemp(BLU_L, HOT)
         self.setGoalTemp(RED_L, HOT)
      else:
         self.setGoalTemp(BLU_R, HOT)
         self.setGoalTemp(RED_R, HOT)

      self.renderGoals()

      time.sleep(5)

      self.timePrint("********Swapping hot goals********")
      self.swapHotGoals(BLU_L, BLU_R)
      self.swapHotGoals(RED_L, RED_R)

      self.renderGoals()

      time.sleep(5)

      self.setAllGoalsCold()
      self.renderGoals()

      self.timePrint("****Hot goal sequence complete****")

      self.randomiseHotGoal()
######## Helper Functions ########

   def randomiseHotGoal (self):
      if random.randint(0,1) == 0:
         self.firstHotGoal = LEFT
         self.starPrint("The LEFT goal will be hot first")
      else:
         self.firstHotGoal = RIGHT
         self.starPrint("The RIGHT goal will be hot first")

   def swapHotGoals (self, goalA, goalB):
      if self.goalIsHot[goalA] == True:
         self.setGoalTemp(goalA, COLD)
         self.setGoalTemp(goalB, HOT)
      else:
         self.setGoalTemp(goalB, COLD)
         self.setGoalTemp(goalA, HOT)

   def setGoalTemp (self, goal, temp, val = [0]*3):
      if temp == HOT:
         val = RGB_MIX_HOT
         self.goalIsHot[goal] = True
      elif temp == COLD:
         val = RGB_MIX_COLD
         self.goalIsHot[goal] = False
      else:
         return

      for channel in GOAL_DMX_PATCH[goal]:
         self.dmx.setChannel(channel+0, val[0])
         self.dmx.setChannel(channel+1, val[1])
         self.dmx.setChannel(channel+2, val[2])

   def setAllGoalsCold (self):
      for goal in range(0, NUM_GOALS):
         self.setGoalTemp(goal, COLD)

   def renderGoals (self):
      self.dmx.render()

   def starPrint (self, string):
      print "*"*80
      self.timePrint(string)
      print "*"*80

   def timePrint (self, string):
      print time.strftime("%H%M%S"), string
         
