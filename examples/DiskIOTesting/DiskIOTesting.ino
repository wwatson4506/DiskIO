// diskIOtesting,ino
// A simple sketch to demonstrate some DiskIO capabilities.

#include "Arduino.h"
#include "mscFS.h"
#include "diskIO.h"

diskIO dio;  // One instance of diskIO.
int br = 0;
int bw = 0;
char buff[8192]; // Disk IO buffer.
char sbuff[256]; // readLine buffer.

// A simple read line function.
char *readLine(char *s) {
	char *s1 = s;
	int	c;
	for(;;) {
		while(!Serial.available());
		switch(c = Serial.read()) {
		case 0x08:
			if(s == s1)
				break;
			s1--;
			Serial.printf("%c%c%c",c,0x20,c);
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

// Change 'device' to the volume name of one of your drives.
// Or you can specify a logical drive number (partition number)
// followed with a colon before the path name. 24 patitions are allowed. 0-23.
// Use: 'listAvailableDrives(&Serial)' to list attached available volume labels
// and logical drive numbers.
const char *device = "0:test1.txt";
//const char *device = "/32GEXFATP3/test1.txt";

void setup() {
  // Open serial communications and wait for port to open:
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }
  
  // This line is use with VT100 capable terminal program.
  Serial.printf("%c",12); // Clear screen (VT100).
  
  Serial.printf(F("DiskIO Testing\r\n\r\n"));
  Serial.printf(F("Initializing, please wait...\r\n\r\n"));
  
  // All initialization is done here.
  dio.init();
  
  // Show a list of mounted partitions.
  Serial.printf(F("\r\nFound and mounted %d logical drives.\r\n"), dio.getVolumeCount());
  dio.listAvailableDrives(&Serial);
  Serial.printf(F("\r\n"));

  if(!dio.lsDir((char *)"/"))
	Serial.printf(F("lsDir() Failed: %s, Code: %d\r\n\r\n"), device, dio.error());
  
  // Setup string of text to write to a file (test1.txt).
  sprintf(buff,"%s",(char *)F("This is a test line to test diskIO write() function"));

  // Create an instance of PFsFile.
  PFsFile mscfl; 

  // Open and create a file for write.
  Serial.printf(F("Opening this file for write: '%s'\r\n"), device);
  if(!dio.open(&mscfl,(const char *) device, O_WRONLY | O_CREAT | O_TRUNC))
	Serial.printf(F("open() Failed: %s, Code: %d\r\n"), device, dio.error());

  // Write our string to the open file.
  bw = dio.write(&mscfl, buff, strlen(buff));
  if(bw != (int)strlen(buff))
  	Serial.printf(F("write() Failed: %s Code: %d\r\n\r\n"), device, dio.error());

  // Show the string we wrote to the file and the number of bytes written.
  Serial.printf("What we wrote: '%s' to: %s\r\n",buff, device);
  Serial.printf(F("%u bytes written to file.\r\n"), bw);

  // Sync the file.
  Serial.printf(F("flushing buff\r\n"));
  dio.fflush(&mscfl);

  //  Close the file.
  Serial.printf(F("closing file: %s test1.txt\r\n"), device);
  if(!dio.close(&mscfl))
    Serial.printf(F("close() Failed: %s\r\n\r\n"), device, dio.error());

  // Re-open same file for read.
  Serial.printf(F("Opening this file for read: '%s'\r\n"), device);
  if(!dio.open(&mscfl, (const char *)device, O_RDONLY))
	Serial.printf(F("open() Failed: %s Code: %d\r\n\r\n"), device, dio.error());

  // Seekfile to position 10.
  Serial.printf(F("Seeking to file position 10\r\n"));
  if(!dio.lseek(&mscfl, 10, SEEK_SET))
  	Serial.printf(F("lseek() Failed: %s Code: %d\r\n\r\n"), device, dio.error());

  // Check the current file position.
  Serial.printf(F("Getting current file position\r\n"));
  if(!dio.ftell(&mscfl))
  	Serial.printf(F("ftell() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  else
  	Serial.printf(F("ftell() returned %d\r\n"), dio.ftell(&mscfl));

  // Read in the line of text.    
  if((br = dio.read(&mscfl, buff, 8192)) < 0)
  	Serial.printf(F("read() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  buff[br] = '\0';
  
  // Show the line and the number of bytes read.
  Serial.printf(F("What we read '%s' from %s\r\n"), buff, device);
  Serial.printf(F("bytes read = %d\r\n"), br);

  //  Close the file.
  if(!dio.close(&mscfl))
    Serial.printf(F("close() Failed: %s code: %d\r\n\r\n"), device, dio.error());

  // List the directory of device.
  Serial.printf(F("\r\n"));
  if(!dio.lsDir((char *)device))
	Serial.printf(F("lsDir() Failed: %s, Code: %d\r\n\r\n"), device, dio.error());
  Serial.printf(F("Press enter to continue...\r\n"));
  readLine((char *)sbuff);
}

void loop(void) {

  Serial.printf(F("\r\n********************************************************\r\n"));
  Serial.printf("Unplug a USB drive then press enter.\r\n");
  Serial.printf("Plug the same USB drive in then press enter again.\r\n");
  Serial.printf("This shows hot plugging at work. USB drives only so far.\r\n");
  Serial.printf("Four USB drives are supported (SD's not yet).\r\n");
  Serial.printf(F("********************************************************\r\n\r\n"));
  
  Serial.printf(F("checking for device #%d\r\n"), 0);
  if(!dio.isConnected(0))
    Serial.printf(F("USB Drive 0 not connected: Code 0x%2.2x\r\n"),dio.error());

  Serial.printf(F("checking for device #%d\r\n"), 1);
  if(!dio.isConnected(1))
    Serial.printf(F("USB Drive 1 not connected: Code 0x%2.2x\r\n"),dio.error());

  Serial.printf(F("checking for device #%d\r\n"), 2);
  if(!dio.isConnected(2))
    Serial.printf(F("USB Drive 2 not connected: Code 0x%2.2x\r\n"),dio.error());

  Serial.printf(F("checking for device #%d\r\n"), 3);
  if(!dio.isConnected(3))
    Serial.printf(F("USB Drive 3 not connected: Code 0x%2.2x\r\n"),dio.error());

  dio.listAvailableDrives(&Serial);
  Serial.printf(F("Press enter to continue...\r\n"));

  readLine((char *)sbuff);
}
