/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;

import org.quartz.Job;
import org.quartz.JobExecutionContext;
import org.quartz.JobExecutionException;

public class ScanJob implements Job
{
	private int fmTunerAgentPortNumber = 9002;  //port to use when sending packets destined for FM Tuner Agent.
	private String fmTunerAgentIpAddress = "137.140.8.82";
	private DatagramPacket receivePacket = null;
	private InetAddress IPAddress = null;
	private byte[] receiveData = null;

	@Override
	public void execute(JobExecutionContext arg0) throws JobExecutionException {//executed when immediate or hourly scan is needed.
		try {
			IPAddress = InetAddress.getByName(fmTunerAgentIpAddress);
			byte[] sendData = new byte[1024];
			sendData = "s".getBytes();  //data being sent to agent is 's'
			DatagramPacket sendPacket = new DatagramPacket(sendData, sendData.length, IPAddress, fmTunerAgentPortNumber);
			UDPAdapter.agentServerSocket.send(sendPacket);  //send the scan request
			ConsoleOutputFormatter.print("Request sent to FM agent:", "s");
		} 
		catch (SocketException e) {ConsoleOutputFormatter.print("No Response from agile2:", "");}
		catch (UnknownHostException e) {
			ConsoleOutputFormatter.print("Communication error:", "IP address of FM agent (agile 2) \"couldn't be determined\"");
		} 
		catch (IOException e) {
			ConsoleOutputFormatter.print("I/O issue has occured.", "");
			e.printStackTrace();
		}

		String stationList = doRead();
		MyScheduler.stationList = stationList;  //update stationList
	}

	public String doRead() {  //mainly copied from UDPAdapter.java
		receiveData = new byte[1024];
		boolean socketTimedOut = false;
		try {
			receivePacket = new DatagramPacket(receiveData, receiveData.length);

			ConsoleOutputFormatter.print("--Blocking--", "[Waiting on stations from FM agent]");

			UDPAdapter.agentServerSocket.setSoTimeout(60000);  //timeout is 1 minute
			UDPAdapter.agentServerSocket.receive(receivePacket);  //blocking call, waits for incoming packet.
			MyScheduler.responseFromFmAgentSucceeded = true;

			ConsoleOutputFormatter.print("<<Unblocked>>", "[Station list received]");
			ConsoleOutputFormatter.print("Station list:", new String(receivePacket.getData(), 0, receivePacket.getLength()));
		} 
		catch(SocketTimeoutException e){
			socketTimedOut = true;
			ConsoleOutputFormatter.print("", "No response from agent.");
			ConsoleOutputFormatter.print("--Blocking--", "[waiting for App]");
		}
		catch (IOException e){
			System.out.println("\n\nIO error has occured.. Stack trace:");
			e.printStackTrace();
			System.out.println("\n\n");
			socketTimedOut = true;  //in case SocketTimeoutException doesn't catch the exception.
			ConsoleOutputFormatter.print("--Blocking--", "[waiting for App]");
		}

		if(socketTimedOut)
			return MyScheduler.stationList;
		else
			return new String(receivePacket.getData(), 0, receivePacket.getLength());  //extract String from packet
	}
}
