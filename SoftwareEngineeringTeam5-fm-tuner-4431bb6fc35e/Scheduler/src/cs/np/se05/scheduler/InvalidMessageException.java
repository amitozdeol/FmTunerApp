package cs.np.se05.scheduler;

public class InvalidMessageException extends IllegalArgumentException
{
	private static final long serialVersionUID = -1136986995961110951L;
	String errorString = null;

	public InvalidMessageException() {}

	public InvalidMessageException(String aString){
		super(aString);
		errorString = aString;
	}

	public void prettyPrint(){
		ConsoleOutputFormatter.print("Invalid request", "#Ignoring \""+errorString+"\"");
		System.out.println("\n");
	}
}
