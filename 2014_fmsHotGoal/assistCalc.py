#~/usr/bin/env python

import sys

def main ():
    if (len(sys.argv) < 1 + (3*3)):
        print "Usage:", sys.argv[0], 
        print "[R1] [R2] [R3] [W1] [W2] [W3] [B1] [B2] [B3]"
        return

    grid = [[0,0,0], [0,0,0], [0,0,0]]
    for i in range(0, 3):
        for j in range(0, 3):
            grid[i][j] = int(sys.argv[1+(i*3)+j])
            if ((grid[i][j] != 0) and (grid[i][j] != 1)):
                grid[i][j] = 1

    print "The combination:"
    printgrid(grid)
    print "Has", calcAssists(grid), "assist(s)"

def calcAssists (grid):
    asstCombs = [0]*(6) #3! Combinations

    asstCombs[0] = calcDiagonal([grid[0], grid[1], grid[2]])
    asstCombs[1] = calcDiagonal([grid[0], grid[2], grid[1]])
    asstCombs[2] = calcDiagonal([grid[1], grid[0], grid[2]])
    asstCombs[3] = calcDiagonal([grid[1], grid[2], grid[0]])
    asstCombs[4] = calcDiagonal([grid[2], grid[0], grid[1]])
    asstCombs[5] = calcDiagonal([grid[2], grid[1], grid[0]])

    return max(asstCombs)

def calcDiagonal (grid):
    return (grid[0][0] + grid[1][1] + grid[2][2])

def printgrid (grid):
    for i in range(0, 3):
        print str(grid[i][0]), str(grid[i][1]), str(grid[i][2])

if __name__ == "__main__": main()
