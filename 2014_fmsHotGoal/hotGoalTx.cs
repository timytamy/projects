using System;
using System.IO;
using System.Text;
using System.Net.Sockets;

public class HotGoal {
    // Set to true to print out timesampts + messages
    private const bool DEBUG = true;

    // Message to send to python script
    private string autoMsg = "FIELD:T000";

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
        TxMessage(autoMsg);
    }

    public void TxHeartbeat () {
        TxMessage("HBEAT:" + DateTime.Now.ToString("HHmmss"));
    }

    private void MakeTcpConnection () {
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

    private void TxMessage (string message) {
        TimePrint("Tx: \"" + message + "\"");
        while (message.Length < MSG_SIZE) message = (message + "\n");
        byte[] msgBytes = Encoding.ASCII.GetBytes(message);

        try {
            Stream stm = null;
            stm = tcpConn.GetStream();
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

        HotGoal hotGoal = new HotGoal("127.0.0.1", 3132);
        for (int i = 0; i < 5; i++) {
            hotGoal.TxHeartbeat();
            System.Threading.Thread.Sleep(1000);
        }

        while (true) {
            hotGoal.TxAutoSig();
            for (int i = 0; i < 15; i++) {
                hotGoal.TxHeartbeat();
                System.Threading.Thread.Sleep(1000);
            }
        }
    }
}
