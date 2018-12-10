/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.util.Calendar;
import java.util.GregorianCalendar;

import org.quartz.CronScheduleBuilder;
import org.quartz.JobBuilder;
import org.quartz.JobDetail;
import org.quartz.Scheduler;
import org.quartz.SchedulerException;
import org.quartz.Trigger;
import org.quartz.TriggerBuilder;
import org.quartz.impl.StdSchedulerFactory;

public class QuartzWrapper 
{
	private static Scheduler scanScheduler;
	private static Scheduler scheduleScanNow;
	private static Scheduler tuneScheduler;
	private static JobDetail job;
	private static Trigger trigger;
	private static int jobAndTriggerCount = 0;

	static void scheduleHourlyScan() throws SchedulerException{
		//Grab the scheduler instance from the Factory
		scanScheduler = StdSchedulerFactory.getDefaultScheduler();

		//and start it off
		scanScheduler.start();

		//define the job and tie it to our job class
		job = JobBuilder.newJob(ScanJob.class).withIdentity(getNextJobName(), "group1").build();

		//Trigger a scan job to run now
		Trigger trigger = TriggerBuilder.newTrigger().withIdentity(getNextTriggerName(), "group1").withSchedule(CronScheduleBuilder.
				cronSchedule("0 0 * * * ?")).build();  //this cryptic cronExpression schedules a scan at hourly intervals.

		//Tell quartz to schedule the job using our trigger
		scanScheduler.scheduleJob(job, trigger);
	}

	static void scanRightNow() throws SchedulerException{
		//Grab the scheduler instance from the Factory
		scheduleScanNow = StdSchedulerFactory.getDefaultScheduler();

		//and start it off
		scheduleScanNow.start();

		//define the job and tie it to our job class
		job = JobBuilder.newJob(ScanJob.class).withIdentity(getNextJobName(), "group2").build();

		//Trigger a scan job to run now
		Trigger trigger = TriggerBuilder.newTrigger().withIdentity(getNextTriggerName(), "group2").startNow().build();

		//Tell quartz to schedule the job using our trigger
		scheduleScanNow.scheduleJob(job, trigger);
	}

	static void scheduleTuneOn(GregorianCalendar calendarDateAndTime, String tuneMessageForAgent) throws SchedulerException{
		Calendar timeToSchedule = calendarDateAndTime;
		
		//Grab the scheduler instance from the Factory
		tuneScheduler = StdSchedulerFactory.getDefaultScheduler();

		//and start it off
		tuneScheduler.start();
		
		//define the job and tie it to our job class
		job = JobBuilder.newJob(TuneJob.class).withIdentity(getNextJobName(), "group3").build();
		job.getJobDataMap().put("agentMessage", tuneMessageForAgent);

		//Trigger a scan job to run at scheduled time
		trigger = TriggerBuilder.newTrigger().withIdentity(getNextTriggerName(),"group3").startAt(timeToSchedule.getTime()).build();

		//Tell quartz to schedule the job using our trigger
		tuneScheduler.scheduleJob(job, trigger);
	}
	
	private static String getNextJobName(){
		String returnMe = "j" + jobAndTriggerCount;
		jobAndTriggerCount++;
		return returnMe;
	}

	private static String getNextTriggerName(){
		String returnMe = "t" + jobAndTriggerCount;
		jobAndTriggerCount++;
		return returnMe;
	}
	
	//quartz is never shut down. Look in documentation if you'd like to do this!
}
