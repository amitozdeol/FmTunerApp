/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;
//factory that creates a Scan or Tune Message object.

public class MessageFactory 
{
	public static Message createMessage(String aString) throws InvalidMessageException{
		if(aString.charAt(0) == 's' || aString.charAt(0) == 'S'){
			return new ScanMessage();
		} else if(aString.charAt(0) == 't' || aString.charAt(0) == 'T'){
			return new TuneMessage(aString);
		} else
			throw new InvalidMessageException(aString);
	}
}

interface Message 
{
	String process();
}
