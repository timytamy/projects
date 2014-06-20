using System;
using System.IO;
using System.Text;
using System.Net.Sockets;

public class HotGoal {
    private const bool DEBUG = true;
    private byte[] autoMsg = Encoding.ASCII.GetBytes("FMS:T000");
    
    private TcpClient tcpConn = new TcpClient();
    
    public HotGoal (string tcpAddr, int tcpPort) {
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
    
    public void TxAutoSig () {
        Stream stm = tcpConn.GetStream();
        TimePrint("Tx: \"" + Encoding.ASCII.GetString(autoMsg) + "\"");
        stm.Write(autoMsg, 0, autoMsg.Length);
    }
    
    private void TimePrint (string strIn) {
        if (DEBUG != true) return; // Escape if not debuging
        
        string strPre = DateTime.Now.ToString("HHmmss");
        Console.WriteLine(strPre + " " + strIn);
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
