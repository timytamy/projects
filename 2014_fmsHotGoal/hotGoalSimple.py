#!/usr/bin/env python
import time

from hotGoalSys import timePrint, HotGoalSystem

COMPORT = '/dev/ttyUSB0'

def main ():
   goals = HotGoalSystem(COMPORT)

   while True:
      timePrint("Press Enter to start the hot goal sequence")
      raw_input()
      goals.runAutoSequence()

if __name__ == "__main__": main()
