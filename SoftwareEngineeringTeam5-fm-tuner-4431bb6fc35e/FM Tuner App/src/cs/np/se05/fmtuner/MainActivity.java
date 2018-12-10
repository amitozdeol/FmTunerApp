/**
 * @author Mark Zgaljic
 * 
 * NOTE:
 * 50-60% of the code in this java file was copy-pasted from a
 * java class named PullToRefreshListActivity. The original
 * contents of this class (before I made small modifications)
 * came from a sample project. This sample project was packaged
 * with the Android-PullToRefresh library. It is available on
 * github by >>Chris Banes<<. It was distributed with an apache 
 * 2.0 commons license. The License file which came with the 
 * original download package has been included within this 
 * android project. All credit for the pull-to-refresh and list 
 * view functionality goes to Chris Banes. All credit for all other
 * code goes to Myself (Mark Zgaljic).
 * 
 * github --> https://github.com/chrisbanes/Android-PullToRefresh
 */
package cs.np.se05.fmtuner;
import java.io.IOException;
import java.net.DatagramSocket;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.Arrays;
import java.util.Calendar;
import java.util.Date;
import java.util.LinkedList;

import android.app.Dialog;
import android.app.ListActivity;
import android.content.Context;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiManager.MulticastLock;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.text.format.DateUtils;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.AdapterView;
import android.widget.AdapterView.AdapterContextMenuInfo;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import com.handmark.pulltorefresh.library.PullToRefreshBase;
import com.handmark.pulltorefresh.library.PullToRefreshBase.OnLastItemVisibleListener;
import com.handmark.pulltorefresh.library.PullToRefreshBase.OnRefreshListener;
import com.handmark.pulltorefresh.library.PullToRefreshBase.State;
import com.handmark.pulltorefresh.library.PullToRefreshListView;
import com.handmark.pulltorefresh.library.extras.SoundPullEventListener;

public class MainActivity extends ListActivity 
{
	private static MulticastLock ml;
	private volatile Calendar userSelectedCalendar;
	private volatile static boolean userDonePickingDateTime = false;
	private CustomDateTimePicker dateTimePicker;
	public static DatagramSocket socket = null;
	private static boolean startupScreen = true;
	private static int typeOfTune = -1;
	private static boolean isReturnedFromActivity = false;
	private volatile static String currentTuneStationRequest = null;  //stores name of current station request

	//variables from library (see comment at top of file).
	static final int MENU_MANUAL_REFRESH = 0;
	static final int MENU_DISABLE_SCROLL = 1;
	static final int MENU_SET_MODE = 2;
	static final int MENU_DEMO = 3;
	private LinkedList<String> mListItems;
	private PullToRefreshListView mPullRefreshListView;
	private ArrayAdapter<String> mAdapter;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		// remove title-bar, make fullscreen
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);
        
		setContentView(R.layout.activity_main);
		
		onCreateSetup();
	}

	@Override
	public void onStart(){
		super.onStart();
		if(isReturnedFromActivity){
			mPullRefreshListView.onRefreshComplete();  //list hasn't REALLY been changed but force list to redraw itself.
		}
		isReturnedFromActivity = false;
	}

	private void onCreateSetup(){
		//code from library (see comment at top of this file), mixed with my own

		mPullRefreshListView = (PullToRefreshListView) findViewById(R.id.pull_to_refresh_listview);

		// Set a listener to be invoked when the list should be refreshed.
		mPullRefreshListView.setOnRefreshListener(new OnRefreshListener<ListView>() {
			@Override
			public void onRefresh(PullToRefreshBase<ListView> refreshView) {
				String label = DateUtils.formatDateTime(getApplicationContext(), System.currentTimeMillis(),
						DateUtils.FORMAT_SHOW_TIME | DateUtils.FORMAT_SHOW_DATE | DateUtils.FORMAT_ABBREV_ALL);

				// Update the LastUpdatedLabel
				refreshView.getLoadingLayoutProxy().setLastUpdatedLabel(label);

				//needed for some devices...
				WifiManager wifi;
				wifi = (WifiManager) getSystemService(Context.WIFI_SERVICE);
				ml = wifi.createMulticastLock("Marks tag  :)");
				ml.acquire();  //required on certain devices

				// Do work to refresh the list here.
				new UDPTask().execute(new String[]{"s"});  //initiates a scan request by launching an AsyncTask
			}
		});

		// Add an end-of-list listener
		mPullRefreshListView.setOnLastItemVisibleListener(new OnLastItemVisibleListener() {
			@Override
			public void onLastItemVisible() {
				//this code allows us to display a Toast for a duration less than Toast.short. 690 milsecd
				if(!startupScreen){
					final Toast superShortToast = Toast.makeText(getBaseContext(), "End of List", Toast.LENGTH_SHORT);
					superShortToast.show();

					Handler handler = new Handler();
					handler.postDelayed(new Runnable() {
						@Override
						public void run() {
							superShortToast.cancel(); 
						}
					}, 690);
				}
			}
		});

		ListView actualListView = mPullRefreshListView.getRefreshableView();

		// Need to use the Actual ListView when registering for Context Menu
		registerForContextMenu(actualListView);

		mListItems = new LinkedList<String>();
		mListItems.addAll(Arrays.asList(mStrings));

		mAdapter = new ArrayAdapter<String>(this, android.R.layout.simple_list_item_1, mListItems);

		/**
		 * Add Sound Event Listener
		 */
		SoundPullEventListener<ListView> soundListener = new SoundPullEventListener<ListView>(this);
		soundListener.addSoundEvent(State.PULL_TO_REFRESH, R.raw.pull_event);
		soundListener.addSoundEvent(State.RESET, R.raw.reset_sound);
		soundListener.addSoundEvent(State.REFRESHING, R.raw.refreshing_sound);
		mPullRefreshListView.setOnPullEventListener(soundListener);

		// You can also just use setListAdapter(mAdapter) or
		// mPullRefreshListView.setAdapter(mAdapter)
		actualListView.setAdapter(mAdapter);
	}

	//private inner class (also from library. Please see comment at top of this file).
	private class UDPTask extends AsyncTask<String, Void, String[]> 
	{
		//used for estimating duration of task
		long startTime;  
		long estimatedTime;

		/**
		 * Scan message format: 's'
		 * Tune message format: 't ___Y__M__D__H__m'. Y is year, M is month, D is date,
		 * H is hour, and m is minute.
		 * 
		 * @param tuneOrScanMessage - a scan or tune message. Will be parsed and determined by 
		 * doInBackground(String[] ... tuneOrScanMessage).
		 */
		@Override
		protected String[] doInBackground(String... tuneOrScanMessage) {  //communicate with scheduler here, and other 'hard' work.
			startTime = System.nanoTime();

			if(! isNetworkAvailable())
				return new String [] {"No internet connection!"};  //will display in onPostExecute

			String sendMe = tuneOrScanMessage[0];
			String udpResponse = null;
			String errorMessage = null;
			String[] reusableTemp = null;
			String[] processedStationList = null;
			String[] returnArray = null;

			if(sendMe.equals("s")){
				//scan message detected.
				try {UDPWrapper.sendToScheduler(sendMe);} 
				catch (IOException e) {errorMessage = "Error: can't contact scheduler to request a new list of stations..";}
				try{udpResponse = UDPWrapper.recieveFromScheduler();}
				catch (IOException e){errorMessage = "Error: can't retrieve list of available stations at this time.";}
				
				if(errorMessage == null && udpResponse.length() < 2){  //detect if scheduler screwed up, ask for stations again.
					try {UDPWrapper.sendToScheduler(sendMe);} 
					catch (IOException e) {errorMessage = "Error: can't contact scheduler to request a new list of stations..";}
					try{udpResponse = UDPWrapper.recieveFromScheduler();}
					catch (IOException e){errorMessage = "Error: can't retrieve list of available stations at this time.";}
				}
		
				//if no exceptions occurred, start preparing station list
				if(errorMessage == null){
					reusableTemp = StationListUtils.parseStationList(udpResponse);
					reusableTemp = StationListUtils.sortParsedStationList(reusableTemp);  

					//obtain a userPresentable array of stations
					processedStationList = StationListUtils.appendSuffixToStations(reusableTemp);  //appends 'FM' back to sorted [].

					//make space for error message in return [].
					returnArray = new String[processedStationList.length + 1];
					for(int i = 1; i < returnArray.length; i++)
						returnArray[i] = processedStationList[i - 1];
					returnArray[0] = "";  //error message placeholder.
					estimatedTime = System.nanoTime() - startTime;
					sleepThreadIfNecessary(estimatedTime);
					return returnArray;
				}
				else{
					estimatedTime = System.nanoTime() - startTime;
					sleepThreadIfNecessary(estimatedTime);
					return new String [] {errorMessage, ""};
				}
			} else if(sendMe.charAt(0) == 't' && sendMe.length() <= 23){  //longest valid tune message is length 23
				//tune message detected.
				try {UDPWrapper.sendToScheduler(sendMe);} 
				catch (IOException e) {errorMessage = "Error: problem contacting scheduler service to schedule the tune.";}
				try{udpResponse = UDPWrapper.recieveFromScheduler();}
				catch (IOException e){errorMessage = "Error: It appears the scheduler service has not confirmed the tune request.";}

				//if exception occurred...
				if(errorMessage != null){
					estimatedTime = System.nanoTime() - startTime;
					sleepThreadIfNecessary(estimatedTime);
					return new String [] {errorMessage};	
				}	
				else{
					estimatedTime = System.nanoTime() - startTime;
					sleepThreadIfNecessary(estimatedTime);
					return new String[] {"", udpResponse};
				}	
			}
			else{  //somehow, crazy input was generated
				if(ml.isHeld())
					ml.release();  //required on certain devices
				mPullRefreshListView.onRefreshComplete();  //list hasn't REALLY been refreshed but pretend it has.
				cancel(true);  //skip onPostExecute
				estimatedTime = System.nanoTime() - startTime;
				sleepThreadIfNecessary(estimatedTime);
				return new String[] {"", ""};
			}
		}

		@Override
		protected void onPostExecute(String[] result) {  //repopulate stationList here! Clear it first of course.
			//error messages placed in index 0. UDP Response placed in the remaining indices of result[].

			if(ml.isHeld())
				ml.release();  //required on certain devices

			//figure out which type of toast to display...
			
			if(result[0].length() > 0 || result[1].equalsIgnoreCase("ok") || result[1].equalsIgnoreCase("")){
				//display response from scheduler
				if(result[1].equalsIgnoreCase("ok"))
					Toast.makeText(getBaseContext(), result[1], Toast.LENGTH_SHORT).show();
				
				//Scheduler responds with empty string if scheduling tune failed.
				else if(result[1].equalsIgnoreCase("") && result[0].length() == 0)
					Toast.makeText(getBaseContext(), "The scheduler has notified us that there was a problem scheduling your tune request. Please try again!",
							Toast.LENGTH_LONG).show();
				
				//display some other error message (created in doInBackground())
				else
					Toast.makeText(getBaseContext(), result[0], Toast.LENGTH_LONG).show();
				mPullRefreshListView.onRefreshComplete();
				super.onPostExecute(result);
				return;
			}

			//remove old elements if any
			while(true){  
				if(mListItems.peekLast() == null)
					break;
				mListItems.removeLast();
			}
			for(int i = 1; i < result.length; i++)
				mListItems.addLast(result[i]);  //array already in ascending order, preserving order with 'addLast'

			startupScreen = false;
			mAdapter.notifyDataSetChanged();
			mPullRefreshListView.onRefreshComplete();  //Call when the list has been refreshed.
			super.onPostExecute(result);
		}

		//keeps the loading spinner on screen longer so its readable.
		private void sleepThreadIfNecessary(long elapsedTime){
			if(elapsedTime < 850000000){
				try {Thread.sleep(600);} 
				catch (InterruptedException e) {e.printStackTrace();}
			}
		}

		@Override
		protected void onProgressUpdate(Void ... params) {}

		private boolean isNetworkAvailable() {
			try {
				//make a URL to a known source
				URL url = new URL("http://www.google.com");

				//open a connection to that source
				HttpURLConnection urlConnect = (HttpURLConnection)url.openConnection();

				//trying to retrieve data from the source. If there is no connection, this line will fail
				urlConnect.getContent();
			} catch (Exception e) {return false;}

			return true;
		}
	}

	@Override
	public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {  //shows menu/options on long press

		AdapterContextMenuInfo info = (AdapterContextMenuInfo) menuInfo;

		menu.setHeaderTitle("" + getListView().getItemAtPosition(info.position));  //displays station as title on long press

		String[] menuItems = {"Tune now","Schedule a tune","","Cancel"};
		for (int i = 0; i < menuItems.length; i++) 
			menu.add(Menu.NONE, i, i, menuItems[i]);
		super.onCreateContextMenu(menu, v, menuInfo);
	}

	@Override
	public boolean onContextItemSelected(MenuItem item) {  //lets us give each item an action.
		final AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
		final int clickedListItem = (int) info.id;
		currentTuneStationRequest = mListItems.get(clickedListItem);
		typeOfTune = item.getItemId();
		switch(item.getItemId())
		{
		//Tune now
		case 0:		
			if(startupScreen)
				break;
			accomplishAppropriateTune();
			break;

			//Schedule a tune
		case 1:
			if(startupScreen)
				break;
			accomplishAppropriateTune();
			break;

			//Empty row
		case 2: 		
			return false;  //Returning false to let android process this item.

			//Cancel
		case 3:
			return false;  //see case 2 comment

		default: 	
			return false;  //see case 2 comment
		} 
		return true;
	}

	private void getDateTime(){
		dateTimePicker = new CustomDateTimePicker(MainActivity.this, new CustomDateTimePicker.ICustomDateTimeListener() {
			@Override
			public void onSet(Dialog dialog, Calendar calendarSelected, Date dateSelected, int year, 
					String monthFullName, String monthShortName, int monthNumber, int date,
					String weekDayFullName, String weekDayShortName, int hour24, int hour12, int min,
					int sec, String AM_PM){

				userSelectedCalendar = calendarSelected;
				userDonePickingDateTime = true;
				System.out.println(userSelectedCalendar.toString());
			}
			@Override
			public void onCancel() {}
		});

		//setting this to true breaks some functionality with the scheduler. Perhaps the dateTimePicker support for 24 hour time is broken?
		dateTimePicker.set24HourFormat(false);
		dateTimePicker.setDate(Calendar.getInstance());  //show current date & time when initially displayed.
		dateTimePicker.showDialog();
	}

	private String createTuneString(Calendar calendar, String userDisplayedStation){
		String frequency = userDisplayedStation.substring(0, userDisplayedStation.indexOf('F') - 1);
		//check for two spaces here (occurs if station frequency is less than 100. ie: 99.9)
		if(frequency.charAt(0) == ' ')
			frequency = frequency.substring(1); 

		//largest string we can create is 24 in length (refer to TuneMessage.java (Scheduler) documentation if confused)
		StringBuffer sb = new StringBuffer(24);
		sb.append("t ");
		sb.append(frequency);
		frequency = null;
		sb.append("Y");
		sb.append(calendar.get(Calendar.YEAR));
		sb.append("M");
		sb.append(calendar.get(Calendar.MONTH));  //dates run from 0-11, not a problem! scheduler parses in this format.
		sb.append("D");
		sb.append(calendar.get(Calendar.DAY_OF_MONTH));
		sb.append("H");
		sb.append(calendar.get(Calendar.HOUR_OF_DAY));  //scheduler parses hours in 24 hour format
		System.out.println("24 hour of day sending to scheduler:"+Calendar.HOUR_OF_DAY);
		sb.append("m");
		sb.append(calendar.get(Calendar.MINUTE));
		return sb.toString();
	}

	private void accomplishAppropriateTune(){
		switch(typeOfTune)
		{
		//Tune now
		case 0:		
			new UDPTask().execute(new String[] {createTuneString(Calendar.getInstance(), currentTuneStationRequest)});
			break;

			//Schedule a tune
		case 1:
			getDateTime();
			new GetDateTimeTask().execute(new String[] {});  //waits for user input and then calls the appropriate methods to tune.
			break;

		default: 	
			break;  //just do nothing
		} 
	}

	//private inner class
	private class GetDateTimeTask extends AsyncTask<String, Void, String[]> 
	{
		@Override
		protected String[] doInBackground(String... ignoreMe) {
			while(true){
				if(userDonePickingDateTime)
					break;
				try {Thread.sleep(400);} 
				catch (InterruptedException e) {e.printStackTrace();}
			}
			userDonePickingDateTime = false;
			//sleeping thread was here
			return ignoreMe;
		}

		@Override
		protected void onPostExecute(String[] result) {
			//create tune message and send tune to scheduler
			new UDPTask().execute(new String[]{createTuneString(userSelectedCalendar, currentTuneStationRequest)});
			super.onPostExecute(result);
		}

	}

	private String[] mStrings = { "Welcome, please pull down", "to fetch/refresh the most", "recent stations.", "", "Tap a row and hold to",
			"schedule a tune event.", "", "Happy Scheduling!"};
}


