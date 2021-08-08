// DiskIOMB
// This is a version of microBox hacked for testing the diskIO library.

#include "Arduino.h"
#include "diskIOMB.h"
#include "mscFS.h"
#include "diskIO.h"
#include "T4_PowerButton.h" // To get free mem left.

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

// Added for diskIO
void freeRam(char **param, uint8_t parCnt) 
{
  Serial.println(memfree());
}

void setup()
{
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }
  
  if(CrashReport)
	Serial.print(CrashReport);
  
  // This line works only with VT100 capable terminal program.
  Serial.printf("%c",12); // Clear screen (VT100).

  Serial.printf(F("DiskIOMB\r\n\r\n"));
  Serial.printf(F("The original version of microBox found here:\r\n"));
  Serial.printf(F(" http://sebastian-duell.de/en/microbox/index.html\r\n\r\n"));
  Serial.printf(F("Initializing, please wait...\r\n\r\n"));
  microbox.begin(&Params[0], hostname, true, historyBuf, 100);
  microbox.AddCommand("free", freeRam);
  microbox.AddCommand("millis", getMillis);
}

void loop()
{ 
  microbox.cmdParser(); // Monitor cmd input.
}
