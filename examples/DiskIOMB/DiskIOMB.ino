// DiskIOMB
// This is a version of microBox hacked for testing the diskIO library.

#include "Arduino.h"
#include "diskIOMB.h"
//#include "diskIO.h"

extern USBHost myusb;

char historyBuf[100];
char hostname[] = "Teensy";


PARAM_ENTRY Params[]=
{
  {"hostname", hostname, PARTYPE_STRING | PARTYPE_RW, sizeof(hostname), NULL, NULL, 0}, 
  {NULL, NULL}
};

// Added for diskIO
void getMillis(char **param, uint8_t parCnt)
{
  Serial.println(millis());
}

#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
// Added for diskIO
void freeRam(char **param, uint8_t parCnt) 
{
//  Serial.println(memfree());
}
#endif

void setup()
{
  while (!Serial) {
    ; // wait for serial port to connect.
  }

#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
  if(CrashReport)
	Serial.print(CrashReport);
#endif
  
  // This line works only with VT100 capable terminal program.
  Serial.printf("%c[H%c[2J",ASCII_ESC,ASCII_ESC); // Home cursor, Clear screen (VT100).

  Serial.printf(F("DiskIOMB\r\n\r\n"));
  Serial.printf(F("The original version of microBox found here:\r\n"));
  Serial.printf(F(" http://sebastian-duell.de/en/microbox/index.html\r\n\r\n"));
  Serial.printf(F("Initializing, please wait...\r\n\r\n"));
  Serial.printf(F("Type 'help' for a list of commands...\r\n\r\n"));

  microbox.begin(&Params[0], hostname, true, historyBuf, 100);
#if defined(ARDUINO_TEENSY41) || defined(ARDUINO_TEENSY40)
  microbox.AddCommand("free", freeRam);
#endif
  microbox.AddCommand("millis", getMillis);
}

void loop()
{ 
  myusb.Task();
  microbox.cmdParser(); // Monitor cmd input.
}
