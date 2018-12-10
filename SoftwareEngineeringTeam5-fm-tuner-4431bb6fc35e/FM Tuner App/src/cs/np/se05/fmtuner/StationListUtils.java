/**
 * @author Mark Zgaljic
 */
package cs.np.se05.fmtuner;

import java.util.Arrays;

//utils class to assist in parsing stationLists, sorting them, and making them presentable to the user.
public class StationListUtils 
{
	 static String[] parseStationList(String unparsedStationList){
		String[] splitArray = unparsedStationList.split("[FM]+");  //split on newline, and don't allow empty lines.
		String temp;
		for(int i = 0; i < splitArray.length; i++){
			//strip 'FM'
			temp = splitArray[i].replaceAll("\r|\n", "");
			splitArray[i] = temp;
		}

		return splitArray;
	}

	 static String[] sortParsedStationList(String[] parsedStationList){
		//convert to double array
		Double[] sortArray = new Double[parsedStationList.length]; 
		for(int i = 0; i < parsedStationList.length; i++)
			sortArray[i] = Double.valueOf(parsedStationList[i]);
		//sort the stations
		Arrays.sort(sortArray);

		//reuse method parameter/variable and convert sorted Double[] back to a String[].
		for(int i = 0; i < sortArray.length; i++){
			parsedStationList[i] = new String(sortArray[i].toString());
			//did parsedStationList[i] have 2 digits before a decimal? If so, add a space to make everything uniform.
			if(parsedStationList[i].indexOf('.') == 2)
				parsedStationList[i] = " " + parsedStationList[i];
		}

		return parsedStationList;
	}

	 static String[] appendSuffixToStations(String[] parsedStationList){
		String[] appendedArray = new String[parsedStationList.length]; 
		for(int i = 0; i < appendedArray.length; i++)
			appendedArray[i] = parsedStationList[i] + " FM";

		return appendedArray;
	}
}
