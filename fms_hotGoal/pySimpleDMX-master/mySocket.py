import socket
TCP_IP = socket.gethostname()
TCP_PORT = 3132

MSG_LEN = 1024

class mySocket:
   def __init__(self, port):
      if sock is None:
         self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
         sock.bind((TCP_IP, TCP_PORT))
      else:
         self.sock = sock

   def connect(self, host, port):
      self.sock.connect((host, port))

   def mysend(self, msg):
      if msg > MSG_LEN:
         print "TOO MUCH MESSAGE"
         return

      sent = self.sock.send(msg[totalsent:MSG_LEN])
      if sent == 0:
         raise RuntimeError("socket connection broken")

      #Pad message
      sent = self.sock.send([]*(MSG_LEN-totalSent))
      if sent == 0:
         raise RuntimeError("socket connection broken")

   def myreceive(self):
      chunks = []
      bytesRecd = 0
      while bytes_recd < MSG_LEN:
         chunk = self.sock.recv(MSG_LEN - bytesRecd)
         if chunk == b'':
            raise RuntimeError("socket connection broken")
         chunks.append(chunk)
         bytesRecd += len(chunk)
      return b''.join(chunks)
