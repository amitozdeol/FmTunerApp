/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.util.TimeZone;

class MyScheduler
{
	private static Message aMessage;
	private static Server server;
	public static volatile String stationList = "";
	public static volatile boolean responseFromFmAgentSucceeded = false;

	public static void main(String args[]) throws Exception{
		TimeZone.setDefault(TimeZone.getTimeZone("America/New_York"));
		server = ServerFactory.createUdpOrStdioObject(getUserInput(args));  

		server.doOpen();

		QuartzWrapper.scheduleHourlyScan();  //schedules hourly scan for stations to the FM Tuner Agent
		System.out.println("[Team 5]\nScheduler online...");
		byte[] sendData = null;
		
		while(true){
			try{
				String messageData = server.doRead();
				aMessage = MessageFactory.createMessage(messageData);  
				sendData = aMessage.process().getBytes();  //process message, capture returned string as byte array
				server.doWrite(sendData);
				server.doClose();
			} catch(InvalidMessageException e){e.prettyPrint();}
		}  //end while
	}

	private static int getUserInput(String[] cmdLine){  // returns 0 for stdio, 1 for udp
		if(cmdLine.length == 0)
			return 1;  //default behavior is udp
		if(cmdLine[0].equals("stdio"))
			return 0;
		else if(cmdLine.equals("udp"))
			return 1;
		else
			return 1;  //default behavior is udp
	}
}