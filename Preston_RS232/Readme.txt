README for PrestonDuino

This is an Arduino library for communicating with a Preston MDR. It works with MDR2, MDR3,
	and MDR4, and supports all commands as specified in the Preston RS-232 Communication
	Protocol. Please contact techsupport@prestoncinema.com to obtain a copy of the protocol
	spec.
	
Tips:
	1) Make sure you're properly installing the library in your IDE. That process is
	explained here: https://www.baldengineer.com/installing-arduino-library-from-github.html
	
	2) There are two class definitions (libraries), PrestonDuino and PrestonPacket.
	Both need to be installed. PrestonDuino manages actual serial communication with the 
	MDR, while PrestonPacket defines the structure of data sent and received from the MDR.
	
	3) You will need an RS-232 level shifter in order to connect to the MDR. Voltage levels
	of RS-232 are +/-12v, which is much more than the Arduino can handle natively. See the
	tutorial at https://www.arduino.cc/en/Tutorial/ArduinoSoftwareRS232 for information on
	setting up a level shifter. I'm also working on building a custom hardware solution for
	this to make it simpler.
	
	4) See Preston_RS232.ino for an example of how to use PrestonDuino.