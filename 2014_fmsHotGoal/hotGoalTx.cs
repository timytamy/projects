using System;
using System.IO;
using System.Text;
using System.Net.Sockets;

public class HotGoal {
    // Set to true to print out timesampts + messages
    private const bool DEBUG = true;

    // Messages to send to python script
    private const string MSG_AUTO_START = "FIELD:T000";
    private const string MSG_COUNTDOWN = "FIELD:T140";
    private const string MSG_PRE_HBEAT = "HBEAT:";
    private const string MSG_PRE_RGB = "DORGB:";
    private const string MSG_PRE_RGB_EA = "EARGB:";

    // TCP stuff
    public TcpClient tcpConn = null;
    private string tcpAddr = null;
    private int tcpPort = 0;
    private const int MSG_SIZE = 32;
    private const int CONNECT_TRYS = 3;

    public HotGoal (string tcpAddrIn, int tcpPortIn) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;
        MakeTcpConnection(tcpAddr, tcpPort);
    }

    public void TxAutoSig () {
        TxMessage(MSG_AUTO_START);
    }
    
    public void TxCountDownSig () {
        TxMessage(MSG_COUNTDOWN);
    }

    public void TxHeartbeat () {
        TxMessage(MSG_PRE_HBEAT + DateTime.Now.ToString("HHmmssfff"));
    }
    
    public void SetRgbValues(byte r, byte g, byte b) {
        
        TimePrint("Setting RGB values to (" + r +" "+ g +" "+ b + ")");

        // Can't transmit NULL equivilent, so 1 ~= 0
        if (r == 0) r = 1;
        if (g == 0) g = 1;
        if (b == 0) b = 1;
        
        byte[] rgb = new byte[3] {r, g, b};
        string rgbString = Encoding.GetEncoding(28591).GetString(rgb);

        TxMessage(MSG_PRE_RGB + rgbString);

    }
    
    public void SetEaRgbValues(byte[] eaRgb) {
        if (eaRgb.Length > 12) {
            TimePrint("Invalid values for SetEaRgbValues");
        }
        
        TimePrint("Setting individual RGB values to");
        for (int i = 0; i < eaRgb.Length; i += 3) {
            TimePrint(eaRgb[i+0] + "" +eaRgb[i+1] + "" + eaRgb[i+2]);
        }
        
        // Can't transmit NULL equivilent, so 1 ~= 0
        for (int i = 0; i < eaRgb.Length; i++) {
            if (eaRgb[i] == 0) eaRgb[i] = 1;
        }
        
        string rgbString = Encoding.GetEncoding(28591).GetString(eaRgb);

        TxMessage(MSG_PRE_RGB_EA + rgbString);
    }

    public void MakeTcpConnection (string tcpAddrIn, int tcpPortIn) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;
        
        tcpConn = new TcpClient();

        int attempts = 0;
        while (attempts < CONNECT_TRYS) {
            if (tcpAddr == "0" || tcpPort == 0) {
                TimePrint("Incorrect Parameters, please set and connect");
                return;
            }

            try {
                TimePrint("Connecting to " + tcpAddr + ":" + tcpPort);
                tcpConn.Connect(tcpAddr, tcpPort);
                tcpConn.NoDelay = true;
                TimePrint("...DONE");
                break;
            } catch (Exception) {
                TimePrint("...Error connecting, trying again");
                System.Threading.Thread.Sleep(1 * 1000);
                attempts++;
            }     
        }

        if (attempts >= CONNECT_TRYS) {
            TimePrint("Attempted and Failed " + CONNECT_TRYS + " times");
            
        }
    }

    private void TxMessage (string msg) {
        if (msg.Length > MSG_SIZE) {
            TimePrint("message \"" + msg + "\" is too long");
            return;
        }

        TimePrint("Tx: \"" + msg + "\"");

        while (msg.Length < MSG_SIZE) {
            msg = (msg + "\0");
        }

        byte[] msgBytes = Encoding.GetEncoding(28591).GetBytes(msg);
        
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

    private void TimePrint (string strIn) {
        if (DEBUG != true) return; // Escape if not debuging

        string strPre = DateTime.Now.ToString("HHmmss ");
        Console.WriteLine(strPre + strIn);
    }
}

public class TestProgram {

    public static void Main () {
        byte[] temp = new byte[12]
          {0, 0, 0, 111, 111, 111, 222, 222, 222, 33, 33, 33};  
        HotGoal hotGoal = new HotGoal("10.0.100.101", 3132);
        //hotGoal.SetEaRgbValues(temp);
        //hotGoal.SetRgbValues(0, 123, 234);
        //hotGoal.TxCountDownSig();
        for (int i = 0; i < 5; i++) {
            hotGoal.TxHeartbeat();
            System.Threading.Thread.Sleep(1000);
        }

        while (true) {
            //hotGoal.TxAutoSig();
            //hotGoal.SetEaRgbValues(temp);
            //hotGoal.SetRgbValues(0, 123, 234);
            hotGoal.TxCountDownSig();
            for (int i = 0; i < 15; i++) {
                hotGoal.TxHeartbeat();
                System.Threading.Thread.Sleep(1000);
            }
        }
    }
}
