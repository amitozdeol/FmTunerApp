/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

import org.quartz.SchedulerException;

public class TuneMessage implements Message
{
	private String unparsedMessage;
	private String agentMessage, year, month, day, hour, minute;
	private int yearIndex, monthIndex, dayIndex, hourIndex, minuteIndex;  //starting index within string, see big comment below.

	public TuneMessage(String unparsedMessage){
		this.unparsedMessage = unparsedMessage;
	}

	@Override
	public String process(){
		yearIndex = unparsedMessage.indexOf("Y");
		monthIndex = unparsedMessage.indexOf("M");
		dayIndex = unparsedMessage.indexOf("D");
		hourIndex = unparsedMessage.indexOf("H");
		minuteIndex = unparsedMessage.indexOf("m");

		agentMessage = unparsedMessage.substring(0, yearIndex);  //agentMessage will contain the frequency
		year = unparsedMessage.substring(yearIndex+1, monthIndex);
		month = unparsedMessage.substring(monthIndex+1, dayIndex);
		day = unparsedMessage.substring(dayIndex+1, hourIndex);
		hour = unparsedMessage.substring(hourIndex+1, minuteIndex);
		minute = unparsedMessage.substring(minuteIndex+1, unparsedMessage.length());

		//assumes month is given in Calendar class format (ie: 2 is provided for march).
		try {
			GregorianCalendar tuneCalendar = new GregorianCalendar();
			tuneCalendar.set(Calendar.YEAR, Integer.parseInt(year));
			tuneCalendar.set(Calendar.MONTH, Integer.parseInt(month));
			tuneCalendar.set(Calendar.DATE, Integer.parseInt(day));
			tuneCalendar.set(Calendar.HOUR_OF_DAY, Integer.parseInt(hour));
			tuneCalendar.set(Calendar.MINUTE, Integer.parseInt(minute));
			
			//check if date is valid (must be right now or after this moment)
			if(isValidDate(tuneCalendar))
				QuartzWrapper.scheduleTuneOn(tuneCalendar, agentMessage);
			else
				throw new SchedulerException();
		}  
		catch (SchedulerException e) {
			ConsoleOutputFormatter.print("Failed to schedule tune.", " ");
			return "";  //indicates problem
		}
		
		//converting military time to standard time for nice output (log file or terminal)
		SimpleDateFormat tempParse = new SimpleDateFormat("hh:mm");
		Date temp = null;
		try {temp = tempParse.parse(hour+":"+minute);} 
		catch (ParseException e) {e.printStackTrace();}
		SimpleDateFormat finalParse = new SimpleDateFormat("hh:mm aa");
		String iAmTimeOfTune = finalParse.format(temp);
		
		String tuneSummary = (Integer.parseInt(month) + 1) + "/" + day + "/" + year +", at " + iAmTimeOfTune;
		ConsoleOutputFormatter.print("Scheduled tune on:", tuneSummary);
		
		return "Ok";
	}
	
	private boolean isValidDate(GregorianCalendar requestedDate){  //accurate up to one minute ago
		Calendar todaysDate = GregorianCalendar.getInstance();
		if(todaysDate.compareTo(requestedDate) <= 0) //is the requested tune date now or after this point in time?
			return true;
		
		return false;  //date passed already. Invalid time!
	}
}

/**An example on how time and dates are parsed. Consider, tuning to 100.3 FM on May 1st, 2014...at 2pm:
 * 
 * public String process() assumes the input string will be in the following form
 * "t 100.3Y2014M4D1H14m00"
 *         |    | | |  |
 *        year  | | |  |
 *             /  | |   \
 *         Month  |  \   \
 *               /   Hour \
 *             Day         \
 *                        Minute (must provide two int's)
 *                        
 * Letters have been strategically placed throughout the string to aid in parsing it. Note: Month 
 * and Minute both use 'm' as a separator. However, only one is lower case. Gregorian calendar counts
 * months from 0-11, not 1-12.
 */
