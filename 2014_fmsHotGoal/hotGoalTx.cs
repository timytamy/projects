using System;
using System.IO;
using System.Text;
using System.Net.Sockets;

public class HotGoal {
    // Set to true to print out timesampts + messages
    private const bool DEBUG = true;

    // Message to send to python script
    private const string MSG_AUTO_START = "FIELD:T000";
    private const string MSG_PRE_HBEAT = "HBEAT:";
    private const string MSG_PRE_RGB = "DORGB:";

    // TCP stuff
    private TcpClient tcpConn = null;
    private string tcpAddr = null;
    private int tcpPort = 0;
    private const int MSG_SIZE = 32;

    public HotGoal (string tcpAddrIn, int tcpPortIn) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;
        MakeTcpConnection();
    }

    public void TxAutoSig () {
        TxHeartbeat();
        TxMessage(MSG_AUTO_START);
        TxHeartbeat();
    }
    
    public void TxHeartbeat () {
        TxMessage(MSG_PRE_HBEAT + DateTime.Now.ToString("HHmmssfff"));
    }

    public void SetRgbValues(byte r, byte g, byte b) {
        TxHeartbeat();
        
        TimePrint("Setting RGB values to " + r + ", " + g + ", " + b);

        if (r == 0) r = 1;
        if (g == 0) g = 1;
        if (b == 0) b = 1;
        
        byte[] rgb = new byte[3] {r, g, b};
        string rgbString = Encoding.GetEncoding(28591).GetString(rgb);

        TxMessage(MSG_PRE_RGB + rgbString);
        
        TxHeartbeat();
    }

    public void MakeTcpConnection () {
        tcpConn = new TcpClient();

        while (true) {
            try {
                TimePrint("Connecting to " + tcpAddr + ":" + tcpPort);
                tcpConn.Connect(tcpAddr, tcpPort);
                tcpConn.NoDelay = true;
                TimePrint("...DONE");
                break;
            } catch (Exception ex) {
                TimePrint("...Error connecting, trying again");
                System.Threading.Thread.Sleep(1*1000);
            }
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
        } catch (Exception ex) {
            TimePrint("Transmit faild:");
            TimePrint("Connection closed, attempting re-connect");
            tcpConn.Close();
            MakeTcpConnection();
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

        HotGoal hotGoal = new HotGoal("10.0.1.122", 3132);
        
        for (int i = 0; i < 5; i++) {
            hotGoal.TxHeartbeat();
            hotGoal.SetRgbValues(29, 1, 201);
            System.Threading.Thread.Sleep(1000);
        }

        while (true) {
            hotGoal.TxAutoSig();
            for (int i = 0; i < 15; i++) {
                hotGoal.TxHeartbeat();
                hotGoal.SetRgbValues(29, 123, 0);
                System.Threading.Thread.Sleep(1000);
            }
        }
    }
}
