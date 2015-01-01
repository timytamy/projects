## Copied and moidifed from
## https://github.com/c0z3n/pySimpleDMX

import serial
import sys
import time

COM_BAUD = 57600
COM_TIMEOUT = 1

DMX_FRAME_SIZE = 512
MSG_START_VAL = 0x7E
MSG_END_VAL = 0xE7
LABELS = {
    'GET_WIDGET_PARAMETERS'     :3,  #unused
    'SET_WIDGET_PARAMETERS'     :4,  #unused
    'RX_DMX_PACKET'             :5,  #unused
    'TX_DMX_PACKET'             :6,
    'TX_RDM_PACKET_REQUEST'     :7,  #unused
    'RX_DMX_ON_CHANGE'          :8,  #unused
    'RX_DMX_ON_CHANGE_PACKET'   :9,  #unused
    'GET_WIDGET_SERIAL_NUM'     :10, #unused
    'TX_RDM_DISCOVERY_REQUEST'  :11  #unused
}

class DmxWidget (object):
    def __init__(self, comport, verbose = False):
        self.verbose = verbose

        self.dmxFrame = [0]*DMX_FRAME_SIZE
        self.inBlackout = False

        #open com
        self.com = None
        try:
            self.com = serial.Serial(comport)
            self.com.baudrate = COM_BAUD
            self.com.timeout = COM_TIMEOUT
        except:
            self.myPrint("TODO: LOTS OF NICE ERROR INFO HERE")
            raise IOError("Could not open %s" %comport)

        self.myPrint("Opened " + self.com.name)
        self.clear()

    # takes channel and value arguments to set a channel level
    # in the local dmx frame, to be rendered the next time the
    # render() method is called
    def setChannel (self, chan, val, autorender=False):
        if (chan < 1) or (DMX_FRAME_SIZE < chan): return

        val = self.constrain(val, 0, 255)

        self.myPrint("Setting@" + chan + "@" + val)
        self.dmxFrame[chan] = val

        if autorender: self.render()

    def readChannel (self, chan):
        if (chan < 1) or (DMX_FRAME_SIZE < chan): return None
        return self.dmxFrame[chan]

    #  clears all channels to zero.
    def clear (self):
        self.myPrint("Clearing all channels to 0")
        self.dmxFrame = [0]*DMX_FRAME_SIZE
        self.render()

    #  toggles blackout
    def blackout (self):
        if self.inBlackout == False:
            tempDmxFrame = list(self.dmxFrame)
            self.clear()
            self.dmxFrame = list(tempDmxFrame)
            self.inBlackout = True
        else:
            self.render()
            self.inBlackout = False

    # updates the dmx output from the USB DMX Pro Widget
    # with the values from self.dmxFrame
    def render (self):
        self.myPrint("Rendering all channels")

        self.inBlackout = False

        #Make the packet
        packet = []

        #Packet header
        packet.append(chr(MSG_START_VAL))
        packet.append(chr(LABELS['TX_DMX_PACKET']))
        packet.append(chr(len(self.dmxFrame) & 0xFF))
        packet.append(chr((len(self.dmxFrame) >> 8) & 0xFF))

        #DMX data
        for i in xrange(len(self.dmxFrame)):
            packet.append(chr(self.dmxFrame[i]))

        #Packet footer
        packet.append(chr(MSG_END_VAL))

        #Write packet
        self.com.write(''.join(packet))

    def close (self):
        self.myPrint("Closing " + self.com.name)
        self.com.close()

    def myPrint (self, string):
        if (self.verbose == False): return
        print time.strftime("%H%M%S"), "DmxWidget:", string

    def constrain (val, lo, hi):
        if val < lo: val = lo
        if val > hi: val = hi
        return int(round(val))

class FakeDmxWidget (object):
    def __init__ (self, comport, verbose = False):
        self.fakePrint("__init__(" + comport + ")")

    def setChannel (self, chan, val, autorender=False):
        self.fakePrint("setChannel("+str(chan)+", "+str(val)+")")

    def clear (self):
        self.fakePrint("clear()")

    def blackout (self):
        self.fakePrint("blackout()")

    def render (self):
        self.fakePrint("render()")

    def fakePrint (self, string):
        print time.strftime("%H%M%S") + " ?!? fake dmxWidget." + string
