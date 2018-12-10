/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.util.Calendar;

import org.quartz.SchedulerException;

public class ScanMessage implements Message  
{	
	/**If a recent stationList was saved, it is returned immediately, otherwise 
	 * a scan request is sent to the FM Tuner Agent. The Scheduler will wait up to 2 minutes
	 * for the FM Tuner Agent to respond. If it does not, then the empty string will be returned,
	 * which indicates the connection failed. */
	@Override
	public String process() {
		
		if(isOnTheHour() || MyScheduler.stationList.length() == 0){  //scan this instant
			if(MyScheduler.stationList.length() == 0)
				ConsoleOutputFormatter.print("Performing scan", " ");
			else if(isOnTheHour())
				ConsoleOutputFormatter.print("Performing hourly scan", " ");
			try {QuartzWrapper.scanRightNow();}  //triggers scan, performs all communication with FM tuner agent.
			catch (SchedulerException e) {ConsoleOutputFormatter.print("!Scheduler Problem!", "Could not schedule an immediate scan.");}
			
			//wait for other thread (launched by quartzWrapper) to receive data. Implement more elegant approach if time permits!
			long start = System.currentTimeMillis();
			long end = start + 61*1000; //waits for 1 minute and 1 second
			while (System.currentTimeMillis() < end)  //waste time until 1 minute pass (socket timeout for agent response)
			{
				try {Thread.sleep(2);}
				catch (InterruptedException e) {e.printStackTrace();}
				
				if(MyScheduler.responseFromFmAgentSucceeded == true)  //...or exit loop early if FM Agent responds before timeout.
					break; 
			}
			MyScheduler.responseFromFmAgentSucceeded = false;  //reset.
			
			try {Thread.sleep(50);}  //giving quartz thread a chance to update MyScheduler.stationList
			catch (InterruptedException e) {e.printStackTrace();}
			
			return MyScheduler.stationList;  //returns list of stations (success) or empty string (failure).
		}
		else  //return most recent list of stations.
			return MyScheduler.stationList;
	}
	
	private boolean isOnTheHour(){
		Calendar calendar = Calendar.getInstance();
		int minute = calendar.get(Calendar.MINUTE);
		int second = calendar.get(Calendar.SECOND);
		if(minute == 0 && (second < 5))
			return true;  //current time is an hour(3pm, 4pm, etc.), and second is no greater than 5.
		else              //this detects if an hourly scan is being performed with reasonable accuracy.
			return false;
	}
}
