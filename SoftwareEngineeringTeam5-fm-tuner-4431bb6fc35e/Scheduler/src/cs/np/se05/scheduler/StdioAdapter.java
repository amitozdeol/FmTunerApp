/**
 * @author Mark Zgaljic
 */
package cs.np.se05.scheduler;

import java.util.Scanner;

public class StdioAdapter implements IOInterface, Server
{	
	private Scanner kb = new Scanner(System.in);
	
	@Override
	public void doOpen() {}

	@Override
	public String doRead() {
		System.out.println("Enter a command.");
		String input = kb.nextLine();
		return input;
	}

	@Override
	public void doWrite(byte[] sendData) {System.out.println("\n"+ new String(sendData));}

	@Override
	public void doClose() {}
}