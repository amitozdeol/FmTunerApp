/**
 * @author Mark Zgaljic
 */
package cs.np.se05.fmtuner;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class UDPWrapper 
{
	//if this class doesn't work, check if network permissions are enabled for this app in manifest!
	private static int schedulerPortNumber = 9001;  //port to use when sending packets destined for scheduler.
	private static String schedulerIpAddress = "54.86.73.155";  //ip address of scheduler
	private static InetAddress IPAddress = null;

	static void sendToScheduler(String msg) throws IOException{
		IPAddress = InetAddress.getByName(schedulerIpAddress);
		byte[] sendData = new byte[1024];
		sendData = msg.getBytes();
		DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, IPAddress, schedulerPortNumber);
		if(MainActivity.socket == null)
			MainActivity.socket = new DatagramSocket(schedulerPortNumber);
		MainActivity.socket.send(sendPacket);  //send the scan request
	}

	static String recieveFromScheduler() throws IOException{
		byte[] receiveData = new byte[1024];
		
		DatagramPacket receivePacket = new DatagramPacket(receiveData, receiveData.length);

		MainActivity.socket.setSoTimeout(70000);  //timeout is 1 minute and 10 seconds
		MainActivity.socket.receive(receivePacket);  //blocking call, waits for incoming packet.
		String response = new String(receivePacket.getData(), 0, receivePacket.getLength());  //extract String from packet and return it.
		return response;  //return schedulers response if no problems arose.
	}
}
