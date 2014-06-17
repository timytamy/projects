## Copied and moidifed from
## https://github.com/c0z3n/pySimpleDMX

import serial, sys, time

COM_BAUD = 57600
COM_TIMEOUT = 1

DMX_FRAME_SIZE = 512
MSG_START_VAL = 0x7E
MSG_END_VAL = 0xE7
LABELS = {
   'GET_WIDGET_PARAMETERS'    :3,  #unused
   'SET_WIDGET_PARAMETERS'    :4,  #unused
   'RX_DMX_PACKET'            :5,  #unused
   'TX_DMX_PACKET'            :6,
   'TX_RDM_PACKET_REQUEST'    :7,  #unused
   'RX_DMX_ON_CHANGE'         :8,  #unused
   'RX_DMX_ON_CHANGE_PACKET'  :9,  #unused
   'GET_WIDGET_SERIAL_NUM'    :10, #unused
   'TX_RDM_DISCOVERY_REQUEST' :11  #unused
}

class DmxWidget(object):

   def __init__(self, comport):

      self.dmxFrame = [0]*DMX_FRAME_SIZE
      self.inBlackout = False

      #open com
      self.com = 0
      try:
         self.com = serial.Serial(comport, baudrate=COM_BAUD, timeout=COM_TIMEOUT)
      except:
         self.timePrint ("Could not open %s, aborting" % (comport))
         self.timePrint ("To see available serial ports run")
         self.timePrint ("\"python -m serial.tools.list_ports\"")
         sys.exit(0)

      print time.strftime("%H:%M:%S"), "Opened %s" % (self.com.portstr)
      self.blackout()

   #  takes channel and value arguments to set a channel level in the local
   #  dmx frame, to be rendered the next time the render() method is called
   def setChannel(self, chan, val, autorender=False):
      if (chan < 1) or (DMX_FRAME_SIZE <= chan): return
      if val < 0: val=0
      if val > 255: val=255

      self.dmxFrame[chan] = val

      if autorender: self.render()

   #  clears all channels to zero.
   def clear(self):
      for i in xrange (1, DMX_FRAME_SIZE, 1):
         self.dmxFrame[i]=0

   #  toggles blackout
   def blackout(self):
      if self.inBlackout == False:
         tempDmxFrame = list(self.dmxFrame)
         self.dmxFrame = [0]*DMX_FRAME_SIZE
         self.render()
         self.dmxFrame = tempDmxFrame
         self.inBlackout = True
      else:
         self.render()
         self.inBlackout = False


   #  updates the dmx output from the USB DMX Pro with the values from self.dmxFrame
   def render(self):
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

   def close(self):
      self.com.close()
   
   def timePrint(self, string):
      print time.strftime("%H:%M:%S"), string
