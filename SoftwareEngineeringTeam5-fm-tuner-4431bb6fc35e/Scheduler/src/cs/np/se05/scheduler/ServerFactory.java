/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

public class ServerFactory 
{
	public static Server createUdpOrStdioObject(int userInput){
		if(userInput == 0){
			return new StdioAdapter();
		} else if(userInput == 1){
			return new UDPAdapter();
		} else
			return null;
	}
}


interface Server  //helps determines dynamically at run time if UDP or stdio will be used (polymorphism).
{
	void doOpen();
	String doRead();
	void doWrite(byte[] sendData);
	void doClose();
}

interface IOInterface
{
	void doOpen();
	String doRead();
	void doWrite(byte[] sendData);
	void doClose();
}
