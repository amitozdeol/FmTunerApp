/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;

import org.quartz.Job;
import org.quartz.JobDataMap;
import org.quartz.JobExecutionContext;
import org.quartz.JobExecutionException;

public class TuneJob implements Job
{
	private int fmTunerAgentPortNumber = 9002;  //port to use when sending packets destined for FM Tuner Agent.
	DatagramPacket sendPacket;
	InetAddress IPAddress;
	String fmTunerAgentIpAddress = "137.140.8.82";
	
	//executed if tune even was scheduled for the current time.
	@Override
	public void execute(JobExecutionContext quartzContext) throws JobExecutionException {  
		JobDataMap data = quartzContext.getJobDetail().getJobDataMap();
		
		//retrieves the message for the agent which was saved in TuneMessage. ie: t 100.3
		byte[] sendData = data.getString("agentMessage").getBytes(); 

		try {
		//	clientSocket = new DatagramSocket();
			IPAddress = InetAddress.getByName(fmTunerAgentIpAddress);
			sendPacket = new DatagramPacket(sendData, sendData.length,IPAddress, fmTunerAgentPortNumber);
			UDPAdapter.agentServerSocket.send(sendPacket);
		} catch (IOException e) {e.printStackTrace();}
		ConsoleOutputFormatter.print("Tune request sent(Agile2):", data.getString("agentMessage"));
		System.out.println("\n");
	}
}
