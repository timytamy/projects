#!/usr/bin/env python
import time
import hotGoalSystem

COMPORT = '/dev/ttyUSB0'

goals = hotGoalSystem.HotGoalSystem(COMPORT)

while True:
   goals.timePrint("Press Enter to start the hot goal sequence")
   raw_input()
   goals.runAutoSequence()
   

