
/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;

public class ConsoleOutputFormatter 
{
	public static final String NEW_LINE = System.getProperty("line.separator");
	private static int lastDayOfMonthPrinted = -1;

	/**
	 * Formats and prints to standard out. Two columns may be specified,
	 * and the third column is a system generated time-stamp.
	 * 
	 * @param firstColumn - string to appear in column 1 of 3 (left justified). Length no greater than 20.
	 * @param secondColumn - string to appear in column 2 of 3 (left justified). Length of any size.
	 */
	public static void print(String firstColumn, String secondColumn){  //have NOT tested all edge cases.
		String firstLineFormat = "%-26s%-38s%-28s%n";
		String otherLinesFormat = "%-26s%-66s%n";
		if(secondColumn.length() <= 35){
			int numNewLines = countOccurence(secondColumn, NEW_LINE);
			if(numNewLines == 0)
				if(secondColumn.contains("Reply IP"))  //special case, don't want to display timestamp here.
					System.out.printf(firstLineFormat, firstColumn, secondColumn, "");
			else
				System.out.printf(firstLineFormat, firstColumn, secondColumn, getTimeStamp());
			else
				newLineExcessPrintHelper(otherLinesFormat, firstColumn, secondColumn);			
		}
		else{	
			int maxLinewidth = 52;
			int numNewLines = countOccurence(secondColumn.substring(0, 35), NEW_LINE);
			if(numNewLines == 0){
				System.out.printf(firstLineFormat, firstColumn, secondColumn.substring(0, 35), getTimeStamp());  //print indices 0 ->34
				String[] pieces = splitStringEvery(secondColumn.substring(35), maxLinewidth);
				for(int i = 0; i < pieces.length; i++)
					System.out.printf(otherLinesFormat, "", pieces[i]);
			}
			else{
				String pattern = "[" + NEW_LINE + "]";
				String[] array = secondColumn.split(pattern);  //break up at \n
				for(int i = 0; i < array.length; i++)  //now make sure all  carriage returns are gone.
					array[i] = array[i].replaceAll("\\r", "");

				boolean[] lineTooBig = new boolean [array.length];
				for(int i = 0; i < array.length; i++){  //find lines that don't fit
					if(array[i].length() > maxLinewidth)
						lineTooBig[i] = true;
				}

				for(int i = 0; i < array.length; i++){  //go through each line and display it
					if(! lineTooBig[i])
						System.out.printf(otherLinesFormat, "", array[i]);  //worry about when to show timestamp at the end
					else{  //convert this line that is too big into smaller lines and print them
						String[] pieces = splitStringEvery(array[i], maxLinewidth);  //break ONLY the line thats too big into pieces
						for(int j = 0; j < pieces.length; j++){
							System.out.printf(otherLinesFormat, "", pieces[i]);
						}
					}
				}
			}
		}
	}

	private static int countOccurence(String sourceString, String target){
		return sourceString.length() - sourceString.replace(target, "").length();
	}

	private static void newLineExcessPrintHelper(String formatString, String firstColumn, String secondColumn){
		String pattern = "[" + NEW_LINE + "]";
		String[] array = secondColumn.split(pattern);
		for(int i = 0; i < array.length; i++){
			if(i == 0)
				System.out.printf(formatString, "", array[i], getTimeStamp());
			else
				System.out.printf(formatString, "", array[i], "");
		}
	}

	//great algorithm, found on: http://stackoverflow.com/questions/12295711/split-a-string-at-every-nth-position
	private static String[] splitStringEvery(String s, int interval){
		int arrayLength = (int) Math.ceil(((s.length() / (double)interval)));
		String[] result = new String[arrayLength];

		int j = 0;
		int lastIndex = result.length - 1;
		for (int i = 0; i < lastIndex; i++) {
			result[i] = s.substring(j, j + interval);
			j += interval;
		} //Add the last bit
		result[lastIndex] = s.substring(j);

		return result;
	}

	/**
	 * Returns a string containing the current time stamp. 
	 * @return - a string.
	 * */
	private static String getTimeStamp() {
		if(getCurrentDayOfMonth() != lastDayOfMonthPrinted){
			Calendar todaysDate = new GregorianCalendar();
			
			lastDayOfMonthPrinted = todaysDate.get(Calendar.DATE);
			
	        SimpleDateFormat df = new SimpleDateFormat();
	        df.applyPattern("MM/dd/yyyy");
	        String newDate = df.format(todaysDate.getTime());
	        return new SimpleDateFormat("hh:mm:ss aa").format(new Date())+" EST, " + newDate;  //displays date if its a new day
		}
		else
			//still same date (since last time stamp). Show concise time to user.
			return new SimpleDateFormat("hh:mm:ss aa").format(new Date());
	}
	
	private static int getCurrentDayOfMonth(){
		Calendar today = new GregorianCalendar();
		 return today.get(Calendar.DATE);
	}
}
