/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
//implemented with help from: http://phoenix.goucher.edu/~kelliher/s2011/cs325/feb25.html
public class UDPAdapter implements IOInterface, Server
{	
	private int generalPortNumber = 9001;  //Port where we expect to receive packets from apps, etc.
	private int receiveFromAgentPortNumber = 9002;  //used to communicate with agent
	private byte[] receiveData = new byte[1024];
	public static DatagramSocket generalServerSocket;
	public static DatagramSocket agentServerSocket;
	public static DatagramPacket receivePacket;
	private static DatagramPacket sendPacket;

	@Override
	public void doOpen() {  //doesn't really establish a 'connection'. UDP is connection-less,simply opens socket and binds to port.
		try {
			generalServerSocket = new DatagramSocket(generalPortNumber);
			agentServerSocket = new DatagramSocket(receiveFromAgentPortNumber);
		} 
		catch (SocketException e) {e.printStackTrace();}
	} 

	@Override
	public String doRead() {
		receiveData = new byte[1024];
		String readData = null;
		try {
			receivePacket = new DatagramPacket(receiveData, receiveData.length);
			ConsoleOutputFormatter.print("--Blocking--", "[waiting for App]");
			generalServerSocket.receive(receivePacket);  //blocking call, waits for incoming packet.

			readData = new String(receivePacket.getData(), 0, receivePacket.getLength());
			if(readData.length() > 10){  //makes output pretty
				ConsoleOutputFormatter.print("<<Unblocked>>", "[Message received]:");
				ConsoleOutputFormatter.print(" ", readData);
			}
			else
				ConsoleOutputFormatter.print("<<Unblocked>>", "[Message received]:" + readData);
		} 
		catch (IOException e) {e.printStackTrace();}

		return readData;  //extract String from packet
	}

	@Override
	public void doWrite(byte[] sendData) {
		InetAddress IPAddress = receivePacket.getAddress();  //IP address this packet originated from
		int port = receivePacket.getPort();

		sendPacket = new DatagramPacket(sendData, sendData.length,IPAddress, port);  //preparing packet
		try {generalServerSocket.send(sendPacket);}  //sending packet off
		catch (IOException e) {e.printStackTrace();}

		System.out.println("Response contents:");
		
		ConsoleOutputFormatter.print("", new String(sendPacket.getData(), 0, sendPacket.getLength()));
		String ip = IPAddress.toString().substring(1, IPAddress.toString().length());  //removes the '/' in ip
		ConsoleOutputFormatter.print(" ", "Reply IP:"+ip+",Port:"+port);
		System.out.println("\n");
	}

	@Override
	public void doClose() {
		//Disconnect socket after each write, but don't close it! We need to keep it open.
		generalServerSocket.disconnect();
	}
}
