using System;
using System.IO;
using System.Text;
using System.Net.Sockets;

public class HotGoal {
    private const bool DEBUG = true;
    private string autoMsg = "FMS:T000";
    private string tcpAddr = null;
    private int tcpPort = 0;
    
    private TcpClient tcpConn = null;
    
    public HotGoal (string tcpAddrIn, int tcpPortIn) {
        tcpAddr = tcpAddrIn;
        tcpPort = tcpPortIn;
        MakeTcpConnection(tcpAddr, tcpPort);
    }
    
    public void TxAutoSig () {
        try {
            TimePrint("Tx: \"" + autoMsg + "\"");
            Stream stm = tcpConn.GetStream();
            byte[] msg = Encoding.ASCII.GetBytes(autoMsg);
            stm.Write(msg, 0, msg.Length);
        } catch (Exception e) {
            TimePrint("Transmit faild:");
            TimePrint("Connection closed, attempting re-connect");
            tcpConn.Close();
            MakeTcpConnection(tcpAddr, tcpPort);
        }
    }
    
    private void MakeTcpConnection (string tcpAddr, int tcpPort) {
        tcpConn = new TcpClient();
        while (true) {
            try {
                TimePrint("Connecting to " + tcpAddr + ":" + tcpPort);
                tcpConn.Connect(tcpAddr, tcpPort);
                TimePrint("...DONE");
                break;
            } catch (Exception e) {
                TimePrint("...Error connecting, trying again");
                System.Threading.Thread.Sleep(1*1000);
            }
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
        System.Threading.Thread.Sleep(5*1000);
    
        while (true) {
            hotGoal.TxAutoSig();
            System.Threading.Thread.Sleep(15*1000);
        }      
    }
}
