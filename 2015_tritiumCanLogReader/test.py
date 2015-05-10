#!/user/bin/env python

import csv
import sys

DEBUG = True
FIELDNAMES = ['Recv time', 'Packet num', 'ID', 'flags', 'data', 'float[1]' , 'float[0]' , 'sender addr', ' ']

def main():
    if len(sys.argv) < 4:
        print "Usage: python", sys.argv[0], "[FILE IN] [FILE OUT] [ID]..."
        return

    #set up files
    fileIn = open(sys.argv[1], 'rb')
    reader = csv.reader(fileIn)
    fileOut = open(sys.argv[2], "wb")    
    writer = csv.writer(fileOut, delimiter=',')

    #copy first line
    writer.writerow(reader.next())

    #sets up list of keys
    keys =  [''] * len(sys.argv)
    for i in range(0, (len(sys.argv)-3)): keys[i] = sys.argv[3+i]

    #filter through each row
    for row in reader:
        for i in range(0, len(keys)):
            if row[2] == ' ' + keys[i]:
                writer.writerow(row)

    #clean up
    fileIn.close()
    fileOut.close()


if (__name__ == "__main__"): main()
