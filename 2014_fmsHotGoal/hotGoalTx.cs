using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Net.Sockets;

public class HotGoal {
    // Set to true to print out timesampts + messages
    // Call HotGoal.debug = true; or HotGoal.debug = false; to change
    public bool debug = true;

    // Messages to send to python script
    private const string MSG_AUTO_START = "FIELD:T000";
    private const string MSG_COUNTDOWN = "FIELD:T140";
    private const string MSG_PRE_HBEAT = "HBEAT:";
    private const string MSG_PRE_RGB = "DORGB:";
    private const string MSG_PRE_RGB_EA = "EARGB:";
    private const string MSG_HAVE_FUN = "HVFUN:";

    // TCP stuff
    public TcpClient tcpConn = null;
    public string tcpAddr = null;
    public int tcpPort = 0;
    private const int MSG_SIZE = 32;
    private const int CONNECT_TRYS = 3;

    private Encoding extAscii = Encoding.GetEncoding(28591);

    public HotGoal (string tcpAddrIn, int tcpPortIn) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;
        MakeTcpConnection(tcpAddr, tcpPort);
    }

    // Sends message to start hot goals
    public void TxAutoSig () {
        TxMessage(MSG_AUTO_START);
    }

    // Sends message to start 10s countdown sequence
    public void TxCountDownSig () {
        TxMessage(MSG_COUNTDOWN);
    }

    // Sends a message that just checks the connection is still open
    public void TxHeartbeat () {
        TxMessage(MSG_PRE_HBEAT + DateTime.Now.ToString("HHmmssfff"));
    }

    public void TxHaveFun () {
        TxMessage(MSG_HAVE_FUN);
    }

    // Sets all goals to RGB value
    public void SetRgbValues(byte r, byte g, byte b) {

        TimePrint("Setting RGB values to (" + r +" "+ g +" "+ b + ")");

        // Can't transmit NULL equivilent, so 1 ~= 0
        if (r == 0) r = 1;
        if (g == 0) g = 1;
        if (b == 0) b = 1;

        byte[] rgb = new byte[3] {r, g, b};
        string rgbString = extAscii.GetString(rgb);

        TxMessage(MSG_PRE_RGB + rgbString);
    }

    // Sets all goals to individual RGB values.
    // Goes in a clockwise direction from Blue Left
    public void SetEaRgbValues(byte[] eaRgb) {
        if (eaRgb.Length > 12) {
            TimePrint("Invalid values for SetEaRgbValues");
        }

        TimePrint("Setting individual RGB values to");
        for (int i = 0; i < eaRgb.Length; i += 3) {
            TimePrint(eaRgb[i+0] + " " + eaRgb[i+1] + " " + eaRgb[i+2]);
        }

        // Can't transmit NULL equivilent, so 1 ~= 0
        for (int i = 0; i < eaRgb.Length; i++) {
            if (eaRgb[i] == 0) eaRgb[i] = 1;
        }

        string rgbString = extAscii.GetString(eaRgb);

        TxMessage(MSG_PRE_RGB_EA + rgbString);
    }

    // Makes/Remakes the connection
    public void MakeTcpConnection (string tcpAddrIn, int tcpPortIn,
      int numTrys = CONNECT_TRYS) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;

        tcpConn = new TcpClient();

        bool forceConnect = false;
        if (numTrys <= 0) {
            numTrys = CONNECT_TRYS;
            forceConnect = true;
        }

        for (int trys = 0; trys < numTrys; trys++) {
            if (tcpAddr == "0" || tcpPort == 0) {
                TimePrint("Incorrect Parameters, please set and connect");
                return;
            }

            try {
                TimePrint("Connecting to " + tcpAddr + ":" + tcpPort + "...");
                tcpConn.Connect(tcpAddr, tcpPort);
                tcpConn.NoDelay = true;
                TimePrint("...DONE");
                return;
            } catch (Exception) {
                TimePrint("...Error connecting, trying again");
                Thread.Sleep(1 * 1000);
                if (forceConnect) trys = 0;
            }
        }

        // On connection, this code is not executed
        TimePrint("Tried and failed " + numTrys + " times");
    }
  
    public bool IsConnected () {
        return tcpConn.Client.Connected;
    }

    // Actually sends message
    private void TxMessage (string msg) {
        if (msg.Length > MSG_SIZE) {
            TimePrint("message \"" + msg + "\" is too long");
            return;
        }

        TimePrint("Tx: \"" + msg + "\"");

        // Pad message out to correct length
        while (msg.Length < MSG_SIZE) msg = (msg + "\0");

        byte[] msgBytes = extAscii.GetBytes(msg);

        try {
            Stream stm = tcpConn.GetStream();
            stm.Write(msgBytes, 0, msgBytes.Length);
        } catch (Exception) {
            TimePrint("Transmit faild;");
            TimePrint("Connection closed, attempting re-connect");
            tcpConn.Close();
            MakeTcpConnection(tcpAddr, tcpPort);
        }
    }

    // Prints out message with a timestamp
    public void TimePrint (string strIn) {
        if (debug != true) return; // Escape if not debuging

        string timeStamp = DateTime.Now.ToString("HHmmss ");
        Console.WriteLine(timeStamp + strIn);
    }
}

public class TestProgram {

    public static void Main (string[] args) {
        if (args.Length < 2) {
            Console.WriteLine("Usage: [ADDRESS] [FUNCTION] [VALUES]");
            return;
        }

        Random rand = new Random();
        HotGoal hotGoal = new HotGoal(args[0], 3132);

        if (!hotGoal.IsConnected()) {
            TimePrint("Forcing retry until connect");
            hotGoal.MakeTcpConnection(hotGoal.tcpAddr, hotGoal.tcpPort, 0);
        }

        string line = args[1].ToLower();
        if ("autonomous hotgoal".Contains(line)) {
            while (true) {
                hotGoal.TxAutoSig();
                Thread.Sleep(15*1000);
             }

        } else if ("countdown".Contains(line)) {
            while (true) {
                hotGoal.TxCountDownSig();
                Thread.Sleep(15*1000);
            }

        } else if ("havefun".Contains(line)) {
            while (true) {
                 hotGoal.TxHaveFun();
                 Thread.Sleep(1000);
            }

        } else if ("rgb".Contains(line)) {
            while (true) {
                byte r, g, b;
                if (args.Length >= 2+3) {            
                    r = Convert.ToByte(args[2+0]);
                    g = Convert.ToByte(args[2+1]);
                    b = Convert.ToByte(args[2+2]);
                } else {
                    r = Convert.ToByte(rand.Next(256));
                    g = Convert.ToByte(rand.Next(256));
                    b = Convert.ToByte(rand.Next(256));
                }

                hotGoal.SetRgbValues(r, g, b);
                Thread.Sleep(1000);
            }

        } else if ("eachrgb".Contains(line)) {
            while (true) {
                byte[] eaRgbVals = new byte[12];
                if (args.Length >= 2+12) {
                    for (int i = 0; i < 12; i++) {
                        eaRgbVals[i] = Convert.ToByte(args[2+i]);
                    }
                } else {
                    for (int i = 0; i < 12; i++) {
                        eaRgbVals[i] = Convert.ToByte(rand.Next(256));
                    }
                }

                hotGoal.SetEaRgbValues(eaRgbVals);
                Thread.Sleep(1000);
            }

        } else if ("runmatch".Contains(line)) {
            while (true) {
                hotGoal.TxAutoSig();
                Thread.Sleep(140*1000);
                hotGoal.TxCountDownSig();
                Thread.Sleep(15*1000);
            }
        }
    }

    public static void TimePrint (string strIn) {
        string timeStamp = DateTime.Now.ToString("HHmmss ");
        Console.WriteLine(timeStamp + strIn);
    }
}
