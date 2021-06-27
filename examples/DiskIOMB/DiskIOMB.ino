#include "Arduino.h"
#include "diskIOMB.h"
#include "mscFS.h"
#include "diskIO.h"
#include "T4_PowerButton.h"


char historyBuf[100];
char hostname[] = "T4.1";

PARAM_ENTRY Params[]=
{
  {"hostname", hostname, PARTYPE_STRING | PARTYPE_RW, sizeof(hostname), NULL, NULL, 0}, 
  {NULL, NULL}
};

void getMillis(char **param, uint8_t parCnt)
{
  Serial.println(millis());
}

void freeRam(char **param, uint8_t parCnt) 
{
  Serial.println(memfree());
}

void setup()
{
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  // This line is use with VT100 capable terminal program.
  Serial.printf("%c",12); // Clear screen (VT100).

  Serial.printf(F("Initializing, please wait...\r\n\r\n"));

  
  microbox.begin(&Params[0], hostname, true, historyBuf, 100);
  microbox.AddCommand("free", freeRam);
  microbox.AddCommand("millis", getMillis);
}

void loop()
{ 
  microbox.cmdParser();
}
