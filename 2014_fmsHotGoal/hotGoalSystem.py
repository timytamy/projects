import random
import socket
import time
import mySimpleDmx

DEBUG = False

#DMX Stuff
RGB_COLD = [0, 0, 0]
RGB_HOT = [255, 255, 255]
RGB_CLEAR = [0, 255, 0]
GOAL_DMX_PATCH = [
    [121], # BLU_L
    [133], # BLU_R
    [145], # RED_L
    [157]  # RED_R
]

NUM_GOALS = 4

BLU_L = 0
BLU_R = 1
RED_L = 2
RED_R = 3

LEFT, RIGHT = 0, 1
COLD, HOT = "COLD", "HOT"

class HotGoalSystem (object):

    def __init__(self, comport):
        timePrint ("Connecting to DMX widget...")
        #self.dmx = mySimpleDmx.FakeDmxWidget(comport)
        self.dmx = mySimpleDmx.DmxWidget(comport)
        timePrint("...DONE")

        if DEBUG:
            for i in range(0, 256+1):
                self.dmx.setChannel(i, 255, True)
                time.sleep(0.05)
                self.dmx.setChannel(i, 0, True)
            self.dmx.clear()

        self.goalIsHot = [False]*NUM_GOALS

        self.firstHotGoal = None
        self.randomiseHotGoal()

    def runAutoSequence (self):
        timePrint("****Starting hot goal sequence****")
        self.setAllGoalsCold()

        if (self.firstHotGoal == LEFT):
            self.setGoalTemp(BLU_L, HOT)
            self.setGoalTemp(RED_L, HOT)
        else:
            self.setGoalTemp(BLU_R, HOT)
            self.setGoalTemp(RED_R, HOT)

        self.renderGoals()

        time.sleep(5)

        timePrint("********Swapping hot goals********")
        self.swapHotGoals(BLU_L, BLU_R)
        self.swapHotGoals(RED_L, RED_R)

        self.renderGoals()

        time.sleep(5)

        self.setAllGoalsCold()
        self.renderGoals()

        timePrint("****Hot goal sequence complete****")
        
        self.randomiseHotGoal()
    
    def setAllClear (self):
        self.setAllRgb(RGB_CLEAR[0], RGB_CLEAR[1], RGB_CLEAR[2])
    
    def setAllRgb (self, r, g, b, render = True):
        for goal in range(0, NUM_GOALS):
            self.setGoalRgb(goal, r, g, b)
        if (render == True):
            self.renderGoals()

    def setGoalRgb (self, goal, r, g, b, render = False):
        self.goalIsHot[goal] = False
        for channel in GOAL_DMX_PATCH[goal]:
            self.dmx.setChannel(channel+0, r)
            self.dmx.setChannel(channel+1, g)
            self.dmx.setChannel(channel+2, b)
        if (render == True):
            self.renderGoals

    def renderGoals (self):
        self.dmx.render()
            
    def printFirstHotGoal (self):
        if (self.firstHotGoal == LEFT):
            timePrint("The LEFT goal will be hot first")
        else:
            timePrint("The RIGHT goal will be hot first")
            
################ Helper Functions ################
    def randomiseHotGoal (self):
        if (random.randint(0,1) == 0):
            self.firstHotGoal = LEFT
        else:
            self.firstHotGoal = RIGHT

    def setAllGoalsCold (self):
        for goal in range(0, NUM_GOALS):
            self.setGoalTemp(goal, COLD)
        self.renderGoals()

    def swapHotGoals (self, goalA, goalB):
        if (self.goalIsHot[goalA] == True):
            self.setGoalTemp(goalA, COLD)
            self.setGoalTemp(goalB, HOT)
        else:
            self.setGoalTemp(goalB, COLD)
            self.setGoalTemp(goalA, HOT)

    def setGoalTemp (self, goal, temp):
        if (temp == HOT):
            self.setGoalRgb(goal, RGB_HOT[0], RGB_HOT[1], RGB_HOT[2])
            self.goalIsHot[goal] = True
        elif (temp == COLD):
            self.setGoalRgb(goal, RGB_COLD[0], RGB_COLD[1], RGB_COLD[2])
            self.goalIsHot[goal] = False

# Other helper functions
def timePrint (string):
    print time.strftime("%H%M%S"), string            
            
def getLocalIp():
    # Copied and modified from http://stackoverflow.com/a/23822431
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.connect(('10.0.0.4', 0)) # FMS Router Address
        return sock.getsockname()[0]
    except:
        return "127.0.0.1"
