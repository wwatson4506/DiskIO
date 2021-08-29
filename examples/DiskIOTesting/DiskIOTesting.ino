// diskIOtesting,ino
// A simple sketch to demonstrate some DiskIO capabilities.

#include "Arduino.h"
#include "mscFS.h"
#if defined(ARDUINO_TEENSY41)
#include "LittleFS.h"
#endif
#include "diskIO.h"

diskIO dio;  // One instance of diskIO.
int br = 0;
int bw = 0;
char buff[8192]; // Disk IO buffer.
char sbuff[256]; // readLine buffer.

// A small hex dump function
void hexDump(const void *ptr, uint32_t len) {
  uint32_t  i = 0, j = 0;
  uint8_t   c=0;
  const uint8_t *p = (const uint8_t *)ptr;

  Serial.printf("BYTE      00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\n");
  Serial.printf("---------------------------------------------------------\n");
  for(i = 0; i <= (len-1); i+=16) {
   Serial.printf("%4.4lx      ",i);
   for(j = 0; j < 16; j++) {
      c = p[i+j];
      Serial.printf("%2.2x ",c);
    }
    Serial.printf("  ");
    for(j = 0; j < 16; j++) {
      c = p[i+j];
      if(c > 31 && c < 127)
        Serial.printf("%c",c);
      else
        Serial.printf(".");
    }
    Serial.printf("\n");
  }
}

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
// followed with a colon before the path name. 32 partitions are allowed. 0-23
// used for PFsVolumes. 24-31 for LittlFS devices.
// Use: 'listAvailableDrives(&Serial)' to list attached available volume labels
// and logical drive numbers.

char *device = "0:test1.txt"; // First logical drive on a USB physical drive.
//char *device = "/16GEXFATP2/test1.txt"; // Second logical drive on a USB physical drive.
//char *device = "/128GFAT32/test1.txt"; // Partition label name
//char *device = "24:test1.txt"; // Logical drive number (in this case QPINAND T4.1 only). 

void setup() {
  // Open serial communications and wait for port to open:
   while (!Serial) {
    SysCall::yield(); // wait for serial port to connect.
  }
#if defined(ARDUINO_TEENSY41)
  if(CrashReport)
    Serial.print(CrashReport);
#endif  
  // This line will only work with VT100 capable terminal program.
  Serial.printf("%c",12); // Clear screen (VT100).
  
  Serial.printf(F("DiskIO Testing\r\n\r\n"));
  Serial.printf(F("Initializing, please wait...\r\n\r\n"));
  
  // All initialization is done here.
  dio.init();

  // Show a list of mounted partitions.
  Serial.printf(F("\r\nFound and mounted %d logical drives.\r\n"), dio.getVolumeCount());
  dio.listAvailableDrives(&Serial);
  Serial.printf(F("\r\n"));

  // List default logical drive and directory.
  if(!dio.lsDir((char *)""))
	Serial.printf(F("lsDir() Failed: %s, Code: %d\r\n\r\n"), dio.cwd(), dio.error());

  // Setup string of text to write to a file (test1.txt).
  sprintf(buff,"%s",(char *)"This is a test line to test diskIO write() function");

  // Create an instance of PFsFile and File.
  PFsFile mscfl; // For SD, SDIO and MSC devices.
  File lfsfl;    // For LittleFS devices.

  // Open and create a file for write.
  Serial.printf(F("\r\nOpening this file for write: '%s'\r\n"), device);
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if(!dio.open(&mscfl,(char *) device, O_WRONLY | O_CREAT | O_TRUNC))
	  Serial.printf(F("open() Failed: %s, Code: %d\r\n"), device, dio.error());
  } else { //LFS
#if defined(ARDUINO_TEENSY41)
	if(!dio.lfsOpen(&lfsfl,(char *)device, FILE_WRITE_BEGIN))
	  Serial.printf(F("open() Failed: %s, Code: %d\r\n"), device, dio.error());
#endif
  }

  // Write our string to the open file.
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    bw = dio.write(&mscfl, buff, strlen(buff));
	if(bw != (int)strlen(buff))
	  Serial.printf(F("write() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    bw = dio.lfsWrite(&lfsfl, buff, strlen(buff));
	if(bw != (int)strlen(buff))
	  Serial.printf(F("write() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
#endif
  }

  // Show the string we wrote to the file and the number of bytes written.
  Serial.printf("What we wrote: '%s'",buff);
  Serial.printf(F("\r\n%u bytes written to file.\r\n"), bw);

  // Sync the file.
  Serial.printf(F("flushing buff\r\n"));
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    dio.fflush(&mscfl);
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    dio.lfsFflush(&lfsfl);
#endif
  }

  //  Close the file.
  Serial.printf(F("closing file: %s\r\n"), device);
  if(dio.getOsType(device) == PFSFILE_TYPE) {
	if(!dio.close(&mscfl))
		Serial.printf(F("close() Failed: %s\r\n\r\n"), device, dio.error());
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
	if(!dio.lfsClose(&lfsfl))
		Serial.printf(F("close() Failed: %s\r\n\r\n"), device, dio.error());
#endif
  }

  // Re-open same file for read.
  Serial.printf(F("Opening this file for read: '%s'\r\n"), device);
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if(!dio.open(&mscfl, (char *)device, FILE_READ))
	  Serial.printf(F("open() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    if(!dio.lfsOpen(&lfsfl, (char *)device, FILE_READ))
	  Serial.printf(F("open() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
#endif
  }	  

  // Seekfile to position 10.
  Serial.printf(F("Seeking to file position 10\r\n"));
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if(!dio.lseek(&mscfl, 10, SEEK_SET))
      Serial.printf(F("lseek() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    if(!dio.lfsLseek(&lfsfl, 10, SEEK_SET))
      Serial.printf(F("lseek() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
#endif
  }

  // Check the current file position.
  Serial.printf(F("Getting current file position\r\n"));
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if(!dio.ftell(&mscfl))
   	  Serial.printf(F("ftell() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
    else
  	  Serial.printf(F("ftell() returned %d\r\n"), dio.ftell(&mscfl));
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    if(!dio.lfsFtell(&lfsfl))
   	  Serial.printf(F("lfsFtell() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
    else
  	  Serial.printf(F("lfsFtell() returned %d\r\n"), dio.lfsFtell(&lfsfl));
#endif
  }

  // Read in the line of text.    
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if((br = dio.read(&mscfl, buff, 8192)) < 0)
      Serial.printf(F("read() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
  } else {
#if defined(ARDUINO_TEENSY41)
    if((br = dio.lfsRead(&lfsfl, buff, 8192)) < 0)
      Serial.printf(F("read() Failed: %s Code: %d\r\n\r\n"), device, dio.error());
#endif
  }
  buff[br] = '\0';
  // Show the line and the number of bytes read.
  Serial.printf(F("What we read: '%s'"), buff);
  Serial.printf(F("\r\nbytes read = %d\r\n"), br);

  //  Close the file.
  Serial.printf(F("closing file: %s\r\n"), device);
  if(dio.getOsType(device) == PFSFILE_TYPE) {
    if(!dio.close(&mscfl))
      Serial.printf(F("close() Failed: %s code: %d\r\n\r\n"), device, dio.error());
  } else { // LFS
#if defined(ARDUINO_TEENSY41)
    if(!dio.lfsClose(&lfsfl))
      Serial.printf(F("close() Failed: %s code: %d\r\n\r\n"), device, dio.error());
#endif
  }

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
  
  dio.listAvailableDrives(&Serial);

  Serial.printf(F("Press enter to continue...\r\n"));
  readLine((char *)sbuff);
}
