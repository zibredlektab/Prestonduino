README for PrestonDuino

Created by Max Batchelder, max@first.ac. To support further development, please consider
	sending me some cash. http://paypal.me/MaxBatchelder

This is an Arduino library for communicating with a Preston MDR. It works with MDR2, MDR3,
	and MDR4, and supports all commands as specified in the Preston RS-232 Communication
	Protocol. Please contact techsupport@prestoncinema.com to obtain a copy of the protocol
	spec.
	
	If all you need is lens data, these are the helper methods to access those variables:
		uint32_t getFocusDistance() returns the focus distance, in mm (1mm precision)
		int getFocalLength(); returns the focal length, in mm (1mm precision)
		int getAperture(); returns the aperture (*100, ex T-5.6 returns as 560)
		char* getLensName();  returns the lens name, as assigned in hand unit
	
See Preston_RS232.ino for an example of how to use PrestonDuino.
	
Tips:
	1) Make sure you're properly installing the library in your IDE. That process is
	explained here: https://www.baldengineer.com/installing-arduino-library-from-github.html
	
	2) There are two class definitions, PrestonDuino and PrestonPacket.
	Both need to be installed. PrestonDuino manages actual serial communication with the 
	MDR, while PrestonPacket defines the structure of data sent and received from the MDR.
	
	3) You will need an RS-232 level shifter in order to connect to the MDR. Voltage levels
	of RS-232 are +/-12v, which is much more than the Arduino can handle natively. See the
	tutorial at https://www.arduino.cc/en/Tutorial/ArduinoSoftwareRS232 for information on
	setting up a level shifter. I'm also working on building a custom hardware solution for
	this to make it simpler.
	
	4) The library currently only works with a hardware Serial connection, not SoftwareSerial.
	In my initial testing, it seemed that SoftwareSerial wasn't reliable enough. I still
	want to go back and get soft serial working, but for now you'll need two hardware serial
	lines to do anything useful with the library, meaning Arduino Uno won't cut it. I
	recommend Arduino Mega.
	
	5) PrestonDuino only stores the most recent data received from the MDR, if you will
	need to access historical data it's up to you to store it elsewhere.
	
TODO:
	- Get SoftwareSerial working, to support simpler Arduino systems
	- Properly handle commands that only support specific MDR models
	- Resend MDR messages on a NAK response
	- Test the more obscure commands
	- Investigate why info(0x0) responds without an ACK (update MDR3 fw), possibly change
		to support this behavior (depending on response from Preston)
	- Actually check the checksum for package validity
