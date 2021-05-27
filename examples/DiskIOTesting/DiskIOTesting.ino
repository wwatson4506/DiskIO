// diskIOtesting,ino
#include "Arduino.h"
#include "mscFS.h"
#include "diskIO.h"

diskIO dio;  // One instance of diskIO.
bool rslt = false;
int br = 0;
size_t bw = 0;
char buff[8192]; // Disk IO buffer.
char sbuff[256]; // readLine buffer.

// A simple read line function.
char *readLine(char *s)
{
	char *s1 = s;
	int	c;
	for(;;) {
		while(!Serial.available());
		switch(c = Serial.read()) {
		case 0x08:
			if(s == s1)
				break;
			s1--;
			Serial.printf("%c",c);
			Serial.printf(" ");
			Serial.printf("%c",c);
			break;
		case '\n':
		case '\r':
			Serial.printf("%c",c);
			*s1 = 0;
			return s;
		default:
			if(c <= 0x7E)
				Serial.printf("%c",c);
			*s1++ = c;
			break;
		}
	}
}

// Change '32GSDFAT32' to a volume name of your drives.
char *device = "/32GSDFAT32/test1.txt";

void setup()
{
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }

  Serial.printf("%c",12); // Clear screen (VT100).
  Serial.printf(F("DiskIO Testing\r\n\r\n"));
  Serial.printf(F("Initializing, please wait...\r\n"));

  dio.init();

  Serial.printf(F("\r\nFound and mounted %d logical drives.\r\n"), dio.getVolumeCount());
  dio.listAvailableDrives(&Serial);

  Serial.printf(F("\r\n"));

  
  PFsFile mscfl; 

  Serial.printf(F("Opening file for write'%s'\r\n"), device);
  rslt = dio.open(&mscfl,(const char *) device, O_WRONLY | O_CREAT | O_TRUNC);
  if(!rslt) {
	Serial.printf(F("open() Failed: s, code %d\r\n\r\n"), device, dio.error(dio.getCDN()));
  }

  char *buff = (char *)F("This is a test line to test diskIO write() function\r\n");
  bw = dio.write(&mscfl, buff, strlen(buff));
  buff[bw] = '\0';
  Serial.printf(F("%u bytes written to file.\r\n"), bw);
  Serial.printf("%s",buff);

  Serial.printf(F("flushing buff\r\n"));
  dio.fflush(&mscfl);

  Serial.printf(F("closing file test1.txt\r\n"));
  if(!dio.close(&mscfl))
    Serial.printf(F("close() Failed: %s\r\n\r\n"), device, rslt);

  Serial.printf(F("Opening file for read'%s'\r\n"), device);
  if(!dio.open(&mscfl, (const char *)device, O_RDONLY))
	Serial.printf(F("open() Failed: %s Code: %d\r\n\r\n"), device, dio.error(dio.getCDN()));

  Serial.printf(F("Seeking to file position %d\r\n"),dio.lseek(&mscfl, 10, SEEK_SET));
  Serial.printf(F("ftell returned %d\r\n"), dio.ftell(&mscfl));

  br = dio.read(&mscfl, buff, 8192);
  buff[br] = '\0';
  Serial.printf(F("%sbr = %d\r\n\r\n"),buff, br);

  if(!dio.close(&mscfl))
    Serial.printf(F("close() Failed: %s\r\n\r\n"), device, rslt);
  dio.lsDir((char *)device);
}

void loop(void) {


  readLine((char *)sbuff);

  Serial.printf(F("checking for device #%d\r\n"), 0);
  if(!dio.isConnected(0))
    Serial.printf(F("USB Drive 0 not connected: Code 0x%2.2x\r\n"),dio.error(0));

  Serial.printf(F("checking for device #%d\r\n"), 1);
  if(!dio.isConnected(1))
    Serial.printf(F("USB Drive 1 not connected: Code 0x%2.2x\r\n"),dio.error(1));

  Serial.printf(F("checking for device #%d\r\n"), 2);
  if(!dio.isConnected(2))
    Serial.printf(F("USB Drive 2 not connected: Code 0x%2.2x\r\n"),dio.error(2));

  dio.listAvailableDrives(&Serial);
 
}
