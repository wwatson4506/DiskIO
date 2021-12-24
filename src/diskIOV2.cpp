// diskIO.cpp

#include <stdio.h>
#include <fcntl.h>

#include "Arduino.h"
#include "diskIO.h"
#include "Arduino.h"
//#include "mscFS.h"
#include "TimeLib.h"

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// MSC objects.
msController drive1(myusb);
msController drive2(myusb);
msController drive3(myusb);
msController drive4(myusb);

msFilesystem msFS1(myusb);
msFilesystem msFS2(myusb);
msFilesystem msFS3(myusb);
msFilesystem msFS4(myusb);
msFilesystem msFS5(myusb);
msFilesystem msFS6(myusb);
msFilesystem msFS7(myusb);
msFilesystem msFS8(myusb);
msFilesystem msFS9(myusb);
msFilesystem msFS10(myusb);
msFilesystem msFS11(myusb);
msFilesystem msFS12(myusb);
msFilesystem msFS13(myusb);
msFilesystem msFS14(myusb);
msFilesystem msFS15(myusb);
msFilesystem msFS16(myusb);

// Quick and dirty
msFilesystem *pmsFS[] = {&msFS1, &msFS2, &msFS3, &msFS4, &msFS5, &msFS6, &msFS7, &msFS8,
						 &msFS9, &msFS10, &msFS11, &msFS12, &msFS13, &msFS14, &msFS15, &msFS16};
#define CNT_MSC  (sizeof(pmsFS)/sizeof(pmsFS[0]))
char  pmsFS_display_name[CNT_MSC][20];

msController *pdrives[] {&drive1, &drive2, &drive3, &drive4};
#define CNT_DRIVES  (sizeof(pdrives)/sizeof(pdrives[0]))
bool drive_previous_connected[CNT_DRIVES+2] = {false, false, false, false, false, false};

typedef struct {
  uint8_t csPin;
  const char *name;
  SDClass sd;

} SDList_t;
SDList_t sdfs[] = {
  {CS_SD, "SDIO"},
  {SD_SPI_CS, "SDSPI"}
};

static uint8_t mscError = DISKIO_PASS;

//------------------------------------------------------------------------
// Call back for file timestamps.  Only called for file create and sync().
// Modified for use with timeLib.h.
//------------------------------------------------------------------------
void dateTime(uint16_t* date, uint16_t* time, uint8_t* ms10) {

	// Return date using FS_DATE macro to format fields.
	*date = FS_DATE(year(), month(), day());
	// Return time using FS_TIME macro to format fields.
	*time = FS_TIME(hour(), minute(), second());
	// Return low time bits in units of 10 ms, 0 <= ms10 <= 199.
	*ms10 = second() & 1 ? 100 : 0;
}

// -------------------------------------
// Return last error for last operation.
// -------------------------------------
uint8_t diskIO::error(void) {
	uint8_t error = mscError;
	mscError = DISKIO_PASS; // Clear error.
	return error;
}

// -----------------------------------
// Set an DISK IO error code.
// -----------------------------------
void diskIO::setError(uint8_t error) {
	mscError = error; // Set error code.
}

// -----------------------------------
// Find next available drive.
// -----------------------------------
void diskIO::findNextDrive(void) {
	for(uint8_t i = 0;  i < CNT_PARITIONS; i++) {
		if(drvIdx[i].valid == true) {
			currDrv = i;
			changeVolume(currDrv);     // Change the volume to this logical drive.
			sprintf(drvIdx[currDrv].fullPath,"/%s%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
			break;
		}
	}
}

//--------------------------------------------------------------
// Display info on available mounted devices and logical drives.
//--------------------------------------------------------------
void diskIO::listAvailableDrives(print_t* p) {
	count_mp = 0;
	p->print(F("\r\nLogical Drive Information For Attached Drives\r\n"));
	connectedMSCDrives();
	
    for(uint8_t i = 0; i < CNT_PARITIONS; i++) {
	if(drvIdx[i].valid) {
		count_mp++;
		p->printf(F("Logical Drive #: %2u: | Volume Label: %11s | valid: %u | "),
                                                           drvIdx[i].ldNumber,
                                                           drvIdx[i].name,
                                                           drvIdx[i].valid);
		switch (drvIdx[i].driveType) {
			case SD_CARD_TYPE_SD1:
				p->printf(F("Drive Type: SD1\r\n"));
				break;
			case SD_CARD_TYPE_SD2:
				p->printf(F("Drive Type: SD2\r\n"));
				break;
			case SD_CARD_TYPE_SDHC:
				p->printf(F("Drive Type: SDHC/SDXC\r\n"));
				break;
			case SD_CARD_TYPE_USB:
				p->printf(F("Drive Type: USB\r\n"));
				break;
#if defined(ARDUINO_TEENSY41)
			case LFS_TYPE:
				p->printf(F("Drive Type: LFS\r\n"));
				break;
#endif
			default:
				p->printf(F("Unknown\r\n"));
		}
	}
  }
  Serial.printf("%d Logical Drives Found\r\n",count_mp);
  Serial.printf("Default Logical Drive: /%s/ (%d:)\r\n",drvIdx[currDrv].name,drvIdx[currDrv].ldNumber);
}

//------------------------------------------------------
// Check for disconnected/newly connected MSC drives.
// If a drive has been disconnected find the next valid
// drive and set as default drive including SD and LFS.
//------------------------------------------------------
void diskIO::connectedMSCDrives() {
#ifdef TalkToMe
  Serial.printf("connectedMSCDrives()...\r\n");
#endif
  myusb.Task();
  
  USBMSCDevice mscDrive;
  char tempName[32] = {};
  
  // Check for recently connected/disconnected drives.
  for (uint8_t i=0; i < CNT_DRIVES; i++) {
    if (*pdrives[i]) {
      if (!drive_previous_connected[i]) {
        if (mscDrive.begin(pdrives[i])) {
          drive_previous_connected[i] = true;
        }
      }
    } else {
      drive_previous_connected[i] = false;
    }
  }
  for (uint8_t i = 0; i < CNT_MSC; i++) {
	drvIdx[i].ldNumber = i;
    if (*pmsFS[i] && drvIdx[i].valid == false) {
      // Lets see if we can get the volume label:
      if (pmsFS[i]->mscfs.getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name))) {
	    sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
		drvIdx[i].fstype = pmsFS[i];
	    drvIdx[i].currentPath[0] = '\0';
        drvIdx[i].fatType = pmsFS[i]->mscfs.fatType();
        drvIdx[i].driveType = pmsFS[i]->mscfs.usbDrive()->usbType();
        drvIdx[i].ifaceType = USB_TYPE;
        drvIdx[i].valid = true;
      }
    } else if (!*pmsFS[i] && drvIdx[i].valid == true) {
		// Device disconnected reset device descriptor.
		drvIdx[i].fstype = nullptr;
		drvIdx[i].valid = false;
		drvIdx[i].name[0] = 0;
		drvIdx[i].currentPath[0] = 0;
		drvIdx[i].fullPath[0] = 0;
		drvIdx[i].driveType = 0;
		drvIdx[i].fatType = 0;
		drvIdx[i].ifaceType = 0;
		// Else find and setup next avaiable device.
		findNextDrive();
    } else if(*pmsFS[i] && drvIdx[i].valid == true) {
	  // Default drive changed to another device?
	  // Then reload device descriptor with new parameters for device.
	  pmsFS[i]->mscfs.getVolumeLabel(tempName, sizeof(tempName));
      if(strcmp(tempName, drvIdx[i].name) != 0) {
	    sprintf(drvIdx[i].name ,"%s", tempName);
	    sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
		drvIdx[i].fstype = pmsFS[i];
	    drvIdx[i].currentPath[0] = '\0';
        drvIdx[i].fatType = pmsFS[i]->mscfs.fatType();
        drvIdx[i].driveType = pmsFS[i]->mscfs.usbDrive()->usbType();
        drvIdx[i].ifaceType = USB_TYPE;
        drvIdx[i].valid = true;
      }
	}
  }
  checkSDDrives();
}

void diskIO::checkSDDrives() {
	// Check for recently connected/disconnected SD drives.
	uint8_t slot = 0;
	bool connected = false;

	// Do first time begin() if not done yet.
	for(uint8_t i = SLOT_OFFSET; i < 6; i++) {
		if (!drive_previous_connected[i]) {
			if (sdfs[i-SLOT_OFFSET].sd.begin(sdfs[i-SLOT_OFFSET].csPin)) {
				drive_previous_connected[i] = true;
			}
		}
	}
	slot = LOGICAL_DRIVE_SDIO * SLOT_OFFSET;
	connected = sdfs[0].sd.mediaPresent();
//Serial.printf("sdfs[0].sd.mediaPresent() = %d\r\n", connected);
    if (connected && drvIdx[slot].valid == false) {
		drvIdx[slot].ldNumber = slot;
		strcpy(drvIdx[slot].name, sdfs[0].name);
		sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
		drvIdx[slot].fstype = &sdfs[0].sd;
		drvIdx[slot].currentPath[0] = '\0';
		drvIdx[slot].fatType = sdfs[0].sd.sdfs.fatType();
		drvIdx[slot].driveType = sdfs[0].sd.sdfs.card()->type();
		drvIdx[slot].ifaceType = SDIO_TYPE;
		drvIdx[slot].valid = true;
    } else if (!connected && drvIdx[slot].valid == true) {
		drvIdx[slot].fstype = nullptr;
		drvIdx[slot].valid = false;
		drvIdx[slot].name[0] = 0;
		drvIdx[slot].currentPath[0] = 0;
		drvIdx[slot].fullPath[0] = 0;
		drvIdx[slot].driveType = 0;
		drvIdx[slot].fatType = 0;
		drvIdx[slot].ifaceType = 0;
		// Else find and setup next avaiable device.
		findNextDrive();
    }
	// Check External SPI Drive.
	connected = sdfs[1].sd.mediaPresent();
//Serial.printf("sdfs[1].sd.mediaPresent() = %d\r\n", connected);
	slot = LOGICAL_DRIVE_SDSPI * SLOT_OFFSET;
    if (connected && drvIdx[slot].valid == false) {
		drvIdx[slot].ldNumber = slot;
		strcpy(drvIdx[slot].name, sdfs[1].name);
		sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
		drvIdx[slot].fstype = &sdfs[1].sd;
		drvIdx[slot].currentPath[1] = '\0';
		drvIdx[slot].fatType = sdfs[1].sd.sdfs.fatType();
		drvIdx[slot].driveType = sdfs[1].sd.sdfs.card()->type();
		drvIdx[slot].ifaceType = SDIO_TYPE;
		drvIdx[slot].valid = true;
    } else if (!connected && drvIdx[slot].valid == true) {
		drvIdx[slot].fstype = nullptr;
		drvIdx[slot].valid = false;
		drvIdx[slot].name[0] = 0;
		drvIdx[slot].currentPath[0] = 0;
		drvIdx[slot].fullPath[0] = 0;
		drvIdx[slot].driveType = 0;
		drvIdx[slot].fatType = 0;
		drvIdx[slot].ifaceType = 0;
		// Else find and setup next avaiable device.
		findNextDrive();
    }
}

//---------------------------------------------------------------------------
// Create filesystem from path spec.
// Will fail if drive does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::mkfs(char *path, int fat_type) {
#ifdef TalkToMe
    Serial.printf(F("mkfs(%s, %d)\r\n"),path, fat_type);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
 	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;
	
    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,path);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		// First check if we are using a LFS device.
	if(fat_type < 0 || fat_type > 2) {
		setError(FORMAT_ERROR);
		goto Fail;
	}
#if defined(ARDUINO_TEENSY41)
		if(drvIdx[currDrv].ifaceType == LFS_TYPE) {
			if(!drvIdx[currDrv].valid) {
				setError(LDRIVE_NOT_FOUND);
				goto Fail;
			}
			if(!drvIdx[currDrv].fstype->format(fat_type, '*', Serial)) {
				setError(FORMAT_ERROR);
				goto Fail;
			}
		} else {
#endif
			if(!drvIdx[currDrv].fstype->format(fat_type, '*', Serial)) {
				setError(FORMAT_ERROR);
				goto Fail;
			}
#if defined(ARDUINO_TEENSY41)
		}
#endif
	}
Fail:
	if(currDrv != drive) {
		changeVolume(drive); // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}
// -------------------------------------------------------------------------
// Initialize diskIO system. Find available drives and partitions.
// -------------------------------------------------------------------------
bool diskIO::init() {
#ifdef TalkToMe
  Serial.printf("init()\r\n");
#endif
	uint8_t i = 0;
	setSyncProvider((getExternalTime)rtc_get);	// the function to get the time from the RTC
	FsDateTime::setCallback(dateTime);		// Set callback
	
	// Initialize USBHost_t36
	myusb.begin();

	elapsedMillis em = 0;
	while (em < MEDIA_READY_TIMEOUT) {
		myusb.Task();
		for (i = 0; i < CNT_DRIVES; i++) {
			 if (pdrives[i]) break;
		}
	}
	// Process MSC drives (4 MAX).
	connectedMSCDrives(); // Modified version of KurtE's version.
	// Initialize SD drives (SDIO and SPI).
//	processSDDrive();
//	checkSDDrives();
//	ProcessSPISD();
#if defined(ARDUINO_TEENSY41)
	ProcessLFS(LFS_DRIVE_QPINAND, "QPINAND");
	ProcessLFS(LFS_DRIVE_QSPIFLASH, "QSPIFLASH");
	ProcessLFS(LFS_DRIVE_QPINOR5, "SPIFLASH5");
	ProcessLFS(LFS_DRIVE_QPINOR6, "SPIFLASH6");
	ProcessLFS(LFS_DRIVE_SPINAND3, "SPINAND3");
	ProcessLFS(LFS_DRIVE_SPINAND4, "SPINAND4");
#endif	

//	if(chdir((char *)"0:"))
//		currDrv = 0;	      // Set default drive to 0
	findNextDrive(); // Find first avalable drive.
	mscError = 0;		  // Clear errors
	return true;
}

//----------------------------------------------------------------
// Mount the SDIO card if possible.
// (KurtE).
//----------------------------------------------------------------
bool diskIO::processSDDrive(void)
{
#ifdef TalkToMe
  Serial.printf(F("Initialize SDIO SD card...\r\n"));
#endif
  if (!sdfs[0].sd.begin(sdfs[0].csPin)) return false; // SDIO Not available
  uint8_t slot = LOGICAL_DRIVE_SDIO * SLOT_OFFSET;
  drvIdx[slot].ldNumber = slot;
  strcpy(drvIdx[slot].name, sdfs[0].name);
  sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
  drvIdx[slot].fstype = &sdfs[0].sd;
  drvIdx[slot].currentPath[0] = '\0';
  drvIdx[slot].fatType = sdfs[0].sd.sdfs.fatType();
  drvIdx[slot].driveType = sdfs[0].sd.sdfs.card()->type();
  drvIdx[slot].ifaceType = SDIO_TYPE;
  drvIdx[slot].valid = true;
  return true;
}

// --------------------------------------
// Mount External SPI SDcard if possible.
// (KurtE).
// --------------------------------------
bool diskIO::ProcessSPISD(void) {
#ifdef TalkToMe
    Serial.printf(F("Initialize SPI SD card...\r\n"));
#endif

  if (!sdfs[1].sd.begin(sdfs[1].csPin)) return false; // SDSPI Not available

  uint8_t slot = LOGICAL_DRIVE_SDSPI * SLOT_OFFSET;
  drvIdx[slot].ldNumber = slot;
  strcpy(drvIdx[slot].name, sdfs[1].name);
  sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
  drvIdx[slot].fstype = &sdfs[1].sd;
  drvIdx[slot].currentPath[1] = '\0';
  drvIdx[slot].fatType = sdfs[1].sd.sdfs.fatType();
  drvIdx[slot].driveType = sdfs[1].sd.sdfs.card()->type();
  drvIdx[slot].ifaceType = SPI_TYPE;
  drvIdx[slot].valid = true;
  return true;
}

#if defined(ARDUINO_TEENSY41)
// --------------------------------------
// Mount LFS device if possible.
// (KurtE).
// --------------------------------------
bool diskIO::ProcessLFS(uint8_t drive_number, const char *name) {
#ifdef TalkToMe
    Serial.printf(F("Initialize LFS device %d...\n"),drive_number);
#endif
  // TODO: Proccess all other possible LFS devices.
  // Only QPINAND tested at this time.
  uint8_t slot = drive_number;
  drvIdx[slot].ldNumber = slot;

  switch(drive_number) {
	case LFS_DRIVE_QPINAND:
		if (!QPINandFS.begin()) return false;
		drvIdx[slot].fstype = &QPINandFS;
		break;
	case LFS_DRIVE_QSPIFLASH:
		if (!QSpiFlashFS.begin()) return false;
		drvIdx[slot].fstype = &QSpiFlashFS;
		break;
	case LFS_DRIVE_QPINOR5:
      pinMode(5,OUTPUT);
      digitalWriteFast(5,HIGH);
		if (!SPIFlashFS[0].begin(5,SPI)) return false;
		drvIdx[slot].fstype = &SPIFlashFS[0];
		break;
	case LFS_DRIVE_QPINOR6:
      pinMode(6,OUTPUT);
      digitalWriteFast(6,HIGH);
		if (!SPIFlashFS[1].begin(6,SPI)) return false;
		drvIdx[slot].fstype = &SPIFlashFS[1];
		break;
	case LFS_DRIVE_SPINAND3:
      pinMode(3,OUTPUT);
      digitalWriteFast(3,HIGH);
		if (!SPINandFS[0].begin(3,SPI)) return false;
		drvIdx[slot].fstype = &SPINandFS[0];
		break;
	case LFS_DRIVE_SPINAND4:
      pinMode(4,OUTPUT);
      digitalWriteFast(4,HIGH);
		if (!SPINandFS[1].begin(4,SPI)) return false;
		drvIdx[slot].fstype = &SPINandFS[1];
		break;
	default:
		return false;
  }
  drvIdx[slot].ldNumber = slot;
  strcpy(drvIdx[slot].name, name);
  sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
  drvIdx[slot].currentPath[0] = '\0';
  drvIdx[slot].fatType = LFS_TYPE;
  drvIdx[slot].driveType = LFS_TYPE;
  drvIdx[slot].ifaceType = LFS_TYPE;
  drvIdx[slot].valid = true;
  return true;
}
#endif
//--------------------------------------------------
// Return number of volumes found and mounted.
//--------------------------------------------------
uint8_t diskIO:: getVolumeCount(void) {
	uint8_t validDrvCount = 0;
	
	for(int i = 0; i < CNT_PARITIONS; i++)
		if(drvIdx[i].valid) validDrvCount++;
	return validDrvCount;
}

// ----------------------------------------------------------------------------
// Find a device and return the logical drive index number for that device and
// the path spec with volume label stripped off of the path spec.
// param in:  full path.
// param out: device index number or (-1 not found). 'path' name stripped of
// volume label. Processes both '/volume name/' and 'n:' drive specs.
// If an invalid path spec or none existent drive then set mscError code.
// ----------------------------------------------------------------------------
int diskIO::getLogicalDriveNumber(char *path) {
#ifdef TalkToMe
  Serial.printf("getLogicalDriveNumber(%s)\r\n", path);
#endif
	char str1[256];
	char *tempPath;
	char tempChar, ldNumber[256];
	const char *strPtr;
	char pathChar;
	int i = 0, volume = -1;
	uint8_t cntDigits = 0;
	connectedMSCDrives();
	setError(DISKIO_PASS); // Clear existing error code (if any)
	strcpy(str1,path);  // Isolate pointer to path from changes below.
	tempPath = str1;
	// If no drive or path spec, return current default logical drive number.
	if(tempPath[0] == '\0') {
		// Check default logical drive is still connected if no drive spec provided.
		if(drvIdx[currDrv].fstype == nullptr) {
			setError(LDRIVE_NOT_FOUND); // Error number 252.
			return -1; // Indicate failure. Logical drive not found.
		} else {
			return currDrv; // If no drive or path spec, return current drive index number.
		}
	}
	// Check if using logical device number ("0:" etc...).
	sprintf(ldNumber,"%s",path); 
	// Look for a colon in the path spec. Terminate on end of string
	// or if colon found.
	do {
		tempChar = ldNumber[cntDigits];
		cntDigits++; // Inc digit count for atoi().
	} while ((uint8_t)tempChar != '\0' && tempChar != ':');
	if (tempChar == ':') {	// Is this a volume number?
		cntDigits--;		// Backup to ':' position.
		i = atoi(ldNumber); // numeric value of number string.
		if (i < CNT_PARITIONS) {	// Don't overrun device descriptor array.
			volume = i;		// Found valid volume. Return index number.
			ldNumber[cntDigits] = '/';  // Change ':' to '/'.
			sprintf(path, "%s", ldNumber+cntDigits);	// Strip off the drive number.
			// Make sure device is still connected (USB only at this time).
			if(drvIdx[i].fstype == nullptr) {
				setError(LDRIVE_NOT_FOUND); // Error number 252.
				return -1; // Indicate failure. Logical drive not found.
			}
			return volume; // Return Logical drive number.
		} else { // Drive Spec given but not found.
			setError(LDRIVE_NOT_FOUND); // Error number 252.
			return -1;
		}
	}
    // Look for logical device name (/volume name/).
	if (*tempPath == '/') { // Find first '/'
		do {
			strPtr = drvIdx[i].name;	// Volume label without '/../'.
			tempPath = str1;			// Path to search.
			// Compare the volume label with path name.
			do {
				// Get a character to compare (with inc).
				pathChar = *strPtr++; tempChar = *(++tempPath);
				if (ifLower(pathChar)) pathChar -= 0x20; // Make upper case.
				if (ifLower(tempChar)) tempChar -= 0x20; // Ditto.
			} while (pathChar && (char)pathChar == tempChar);
		  // check each label until there is a match.
		} while ((pathChar || (tempChar != '/')) && ++i < CNT_PARITIONS);
		// If a volume label is found, get the drive number and strip label from path.
		if (i < CNT_PARITIONS) {	// Don't overrun device descriptor array.
			volume = i;		// Found valid volume. Return index number.
			strcpy(path, tempPath); // Strip off the logical drive name (leave last '/').
			// Make sure device is still connected (USB only at this time).
			if(drvIdx[i].fstype == nullptr) {
				setError(LDRIVE_NOT_FOUND); // Error number 252.
				return -1; // Indicate failure. Logical drive not found.
			}
			return volume; // Return Logical drive number.
		} else { // Drive Spec given but not found.
			setError(LDRIVE_NOT_FOUND); // Error number 252.
			return -1;
		}
	} 
	// Make sure current device is still connected (USB only at this time).
	if(drvIdx[i].fstype == nullptr) {
		setError(LDRIVE_NOT_FOUND); // Error number 253.
		return -1; // Indicate failure. Device not connected.
	}
	return currDrv;	// Return default logical drive. (No Drive spec given) 
}

// Check for a valid drive spec ('/volume name/' or 'logical drive number:').
// If preservePath == true then return full path spec with drive spec.
// Else return path spec stripped of drive spec.
// Return -1 for drive spec or the logical drive number not given.
// If -1 is returned then check mscError for possible error.
int diskIO::isDriveSpec(char *driveSpec, bool preservePath) {
#ifdef TalkToMe
    Serial.printf(F("isDriveSpec(%s)\r\n"),driveSpec);
#endif

	char tempDS[256] = {""};
	strcpy(tempDS,driveSpec); // Isolate driveSpec for now.
	int  rslt = getLogicalDriveNumber(tempDS);
	if((rslt < 0) && (mscError != DISKIO_PASS)) { // Returned -1, Missing or invalid drive spec. 
		return rslt;
	}
	if(!preservePath)
		strcpy(driveSpec,tempDS); // Save file path name stripped of drive spec.
	return rslt;
}

//------------------------------------------------------
// Parse the path spec. Handle ".", ".." and "../" OP's
//------------------------------------------------------
bool diskIO::parsePathSpec(const char *pathSpec) {
	char pathOut[256] = {""};
    if(!relPathToAbsPath(pathSpec, pathOut, 256)) return false;
	strcpy((char *)pathSpec,pathOut);
	return true;
}

void diskIO::changeVolume(uint8_t volume) {
#ifdef TalkToMe
    Serial.printf(F("changeVolume(%d)\r\n"),volume);
#endif
	switch(drvIdx[volume].ifaceType) {
		case USB_TYPE:
			pmsFS[volume]->mscfs.chvol(); // Change the volume logical USB drive.
			break;
		case SDIO_TYPE:
			sdfs[0].sd.sdfs.chvol(); // Change the volume logical USB drive.
			break;
		case SPI_TYPE:
			sdfs[1].sd.sdfs.chvol(); // Change the volume logical USB drive.
			break;
		default:
			return;
	}
}

//------------------------------------------------
// Change drive.
// param in: Drive spec. ("/logicalDriveName/...")
//------------------------------------------------
int diskIO::changeDrive(char *driveSpec) {
#ifdef TalkToMe
    Serial.printf(F("changeDrive(%s)\r\n"),driveSpec);
#endif
	int rslt = isDriveSpec(driveSpec, false); // returns ds stripped of drive spec.
	if(rslt < 0) { // Returned -1, Not Found or invalid drive spec. 
		return rslt;
	}
	currDrv = rslt;       // Set the current working drive number.
	changeVolume(rslt);
	return rslt;
}

//-------------------------------------------------------------------------
// Checks for a drive spec (/volume name/ or n:) and changes to volume.
// Also checks that drive is still connected and if connected then adds the
// given path to the current path for that logical drive. Finally it parses
// the path spec processing any relative path specs to absolute path specs.
//-------------------------------------------------------------------------
bool diskIO::processPathSpec(char *path) {
#ifdef TalkToMe
    Serial.printf(F("processPathSpec(%s)\r\n"),path);
#endif
 	int ldNum = 0;
	char tempPath[256];
	// Check for a drive spec. 
	// Return -1 if failed or logical drive number.
	ldNum = isDriveSpec(path, false);
	if((ldNum < 0) && (mscError != DISKIO_PASS)) {
		return false; // Return false. mscError holds error code.
	} else if((currDrv != ldNum) && (mscError == DISKIO_PASS)) {
		currDrv = (uint8_t)ldNum;	// Set new logical drive index.
		changeVolume(ldNum);     // Change the volume to this logical drive.
	}
	// Get current path spec and add '/' + given path spec to it.
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	// Check for '.', '..', '../'.
	if(!parsePathSpec(tempPath)) {
		setError(INVALID_PATH_NAME);
		return false;	// Invalid path name.
	}
	strcpy(path, tempPath); // Set path to proccessed path.
	return true;
}

//---------------------------------------------------------------------------
// Change directory.
// param in: "path name". Also processes drive spec and changes drives.
//---------------------------------------------------------------------------
bool diskIO::chdir(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("chdir(%s)\r\n", dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	char tempPath[256];
	char path[256];

	strcpy(path,dirPath); // Isolate original path pointer from changes below.
	// Check if we are changing to the root directory.
	if((strlen(path) == 1) && (path[0] == '/')) { // Yes, 
		strcpy(drvIdx[currDrv].currentPath, ""); // clear currentPath.
		strcpy(tempPath, "/"); // Setup tempPath for root directory.
	} else { // No,
		strcpy(tempPath,path); // Setup tempPath with given path spec.
		// And check for a logical drive change.
		if(changeDrive(path) < 0 && mscError != DISKIO_PASS) return false;
	}
	// Get current path spec and add '/' + given path spec to it.
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		setError(INVALID_PATH_NAME);
		return false; // Invalid path spec.
	}
#if defined(ARDUINO_TEENSY41)
	// LittleFS does not have a chdir() function so we just test for the
	// existence of the sub directory. If it is there then we add it to 
	// the current path and 'currentPath' will act as a cwd().
	// If not found return false. 
	if(drvIdx[currDrv].ifaceType == LFS_TYPE && currDrv == drvIdx[currDrv].ldNumber) {
		if(!QPINandFS.exists(tempPath)) {
			setError(PATH_NOT_EXIST);
			return false;
		}
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
		return true;
	}
#endif
	switch(drvIdx[currDrv].ifaceType) {
		case USB_TYPE:
			if(!pmsFS[currDrv]->mscfs.chdir((const char *)tempPath))
			return false;
			sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
			break;
		case SDIO_TYPE:
			if(!sdfs[0].sd.sdfs.chdir((const char *)tempPath))
			return false;
			sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
			break;
		case SPI_TYPE:
			if(!sdfs[1].sd.sdfs.chdir((const char *)tempPath))
			return false;
			sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
			break;
		default:
			return false;
	}
	return true;
}

//------------------------------------------
// Get current working logical drive number.
//------------------------------------------
uint8_t diskIO::getCDN(void) {
#ifdef TalkToMe
  Serial.printf("getCDN()\r\n");
#endif
	connectedMSCDrives();
	return currDrv;
}

//------------------------------------------
// Set current working logical drive number.
//------------------------------------------
void diskIO::setCDN(uint8_t drive) {
#ifdef TalkToMe
  Serial.printf("setCDN(%d)\r\n",drive);
#endif
	currDrv = drive;
	connectedMSCDrives();
}

//---------------------------------------------------------
// Get current working directory (with logical drive name).
//---------------------------------------------------------
char *diskIO::cwd(void) {
#ifdef TalkToMe
  Serial.printf("cwd()\r\n");
#endif
	sprintf(drvIdx[currDrv].fullPath,"/%s%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
	return drvIdx[currDrv].fullPath;
}

//------------------------------------------------------------
// Make an absolute path string from a relative path string.
// Process ".", ".." and "../". Returns absolute path string
// in path_out.
// This function found on internet as an example. Needs to be
// optimized. Modified for use with diskIO.
//------------------------------------------------------------
bool diskIO::relPathToAbsPath(const char *path_in, char * path_out, int outLen)
{
#ifdef TalkToMe
  Serial.printf("relPathToAbsPath(%s,%s,%d)\r\n", path_in, path_out, outLen);
#endif
    char *dirs[MAX_SUB_DEPTH];
    int depth = 0;
    char *dstptr = path_out;
    const char *srcptr = path_in;

    *dstptr++ = DIRECTORY_SEPARATOR[0];
    dirs[0] = dstptr;
    dirs[1] = NULL;
    depth++;
    do {
        if ((srcptr[0] == '.') && isDirSeparator(srcptr[1])) {
            srcptr += 2;
        } else if (srcptr[0] == '.' && srcptr[1] == '.' && isDirSeparator(srcptr[2])) {
            if (depth > 1) {
                dirs[depth] = NULL;
                depth--;
                dstptr = dirs[depth-1];
            } else {
                dstptr = dirs[0];
            }
            srcptr += 3;
        } else if (srcptr[0] == '.' && srcptr[1] == '.' && srcptr[2] == 0) {
            if (depth == 1) {
                srcptr += 2;
            } else {
                depth--;
                dstptr = dirs[depth-1];
                srcptr += 2;
            }
        } else {
            while (!isDirSeparator(srcptr[0]) && srcptr[0]) {
                *dstptr++ = *srcptr++;
            }
            if (srcptr[0] == 0) {
                if (dstptr != dirs[0] && isDirSeparator(dstptr[-1])) {
                    dstptr[-1] = 0;
                }
                dstptr[0] = 0;
                return true;
            } else if (isDirSeparator(srcptr[0])) {
                if (dstptr == dirs[0]) {
                    srcptr++;
                } else {
                    *dstptr++ = *srcptr++;
                    dirs[depth] = dstptr;
                    depth++;
                }
                while (isDirSeparator(srcptr[0]) && srcptr[0]) {
                    srcptr++;
                }
            } else {
                path_out[0] = 0;
                return false;
            }
        }
    } while(1);
	return false;
}

// ---------------------------------------------------------
//Wildcard string compare.
//Written by Jack Handy - jakkhandy@hotmail.com
//http://www.codeproject.com/KB/string/wildcmp.aspx
// ---------------------------------------------------------
// Based on 'wildcmp()' (original) and modified for use with
// diskIO by Warren Watson.
// Found and taken from SparkFun's OpenLog library.
// ---------------------------------------------------------
// Check for filename match to wildcard pattern.
// ---------------------------------------------------------
bool diskIO::wildcardMatch(const char *pattern, const char *filename) {
#ifdef TalkToMe
  Serial.printf("wildcardMatch(%s,%s)\r\n",pattern, filename);
#endif
  const char *charPointer = 0;
  const char *matchPointer = 0;

  while(*filename && (*pattern != '*')) {
    if((*pattern != *filename) && (*pattern != '?')) return false;
    pattern++;
    filename++;
  }
  while(*filename) {
    if(*pattern == '*') {
      if(!(*(++pattern))) return true;
      matchPointer = pattern;
      charPointer = filename + 1;
    } else if((*pattern == *filename) || (*pattern == '?')) {
      pattern++;
      filename++;
    } else {
      pattern = matchPointer;
      filename = charPointer++;
    }
  }
  while(*pattern == '*') pattern++;
  return !(*pattern);
}

//---------------------------------------------------------------
// Get path spec and wildcard pattern
// params[in]
//	specs:   Pointer to path spec buffer.
//	pattern: Pointer to buffer to hold wild card pattern if there.
// params[out]
//	returns: true for wildcard pattern found else false.
//----------------------------------------------------------------
bool diskIO::getWildCard(char *specs, char *pattern)
{
#ifdef TalkToMe
  Serial.printf("getWildCard(%s,%s)\r\n", specs, pattern);
#endif
	int index, count, len, i;
	len = strlen(specs);
	// If no '.' or len = 0 then no wildcards.
	if(!len || !strchr(specs,'.')) {
		return false;
	}
	index = len;
	// Start at end of string and walk backwards through it until '/' reached.
	while((specs[index] != '/') && (index != 0))
		index--;
	count = len - index; // Reduce length of string (count).
	for(i = 0; i < count; i++) {
			pattern[i] = specs[i+index+1]; // Copy wildcards to pattern string.
	}
	pattern[i] = '\0'; // Terminate strings.
	specs[index+1] ='\0';
	if(pattern[0])
		return true;
	else
		return false;
}

//------------------------------------------------------------------------------
// Directory Listing functions.
//------------------------------------------------------------------------------
// Display file date and time. (SD/MSC)
void diskIO::displayDateTime(uint16_t date, uint16_t time) {
#ifdef TalkToMe
  Serial.printf("displayDateTime(%d,%d)\r\n", date, time);
#endif
    DateTimeFields tm;
	const char *months[12] = {
		"January","February","March","April","May","June",
		"July","August","September","October","November","December"
	};
					tm.sec = FS_SECOND(time);
					tm.min = FS_MINUTE(time);
					tm.hour = FS_HOUR(time);
					tm.mday = FS_DAY(date);
					tm.mon = FS_MONTH(date) - 1;
					tm.year = FS_YEAR(date) - 1900;
					for(uint8_t i = 0; i <= 10; i++) Serial.printf(F(" "));
					Serial.printf(F("%9s %02d %02d %02d:%02d:%02d\r\n"), // Show date/time.
									months[tm.mon],tm.mday,tm.year + 1900, 
									tm.hour,tm.min,tm.sec);
}

// List a directory from given path. Can include a volume name delimited with '/'
// '/name/' at the begining of path string.
bool diskIO::lsDir(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("lsDir %s...\r\n", dirPath);
#endif
	File dir; // SD, MSCFS.

	char savePath[256];
	savePath[0] = 0;
	char pattern[256];
	pattern[0] = 0;

	bool wildcards = false;
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;
	// Show current logical drive name.
	Serial.printf(F("Volume Label: %s\r\n"), drvIdx[currDrv].name);
	// Show full path name (with logical drive name).
	// If no path spec given then show current path.
	if(dirPath == (const char *)"")
		Serial.printf(F("Full Path: %s\r\n"), cwd());
	else
		Serial.printf(F("Full Path: %s\r\n"), dirPath);
	// wildcards = true if any wildcards used else false.
	wildcards = getWildCard((char *)savePath,pattern);  

	if(!(dir = drvIdx[currDrv].fstype->open(savePath))) {
		setError(INVALID_PATH_NAME);
		return false;
	}
	// If wildcards given just list files that match (no sub directories) else
	// List sub directories first then files.
	if(wildcards) {
		lsFiles(&dir, pattern, wildcards);
	} else {
		lsSubDir(&dir);
		lsFiles(&dir, pattern, wildcards);
	}
	dir.close();		// Done. Close directory base file.

	// Show free space left on this device.
	switch(drvIdx[currDrv].ifaceType) {
		case USB_TYPE:
			Serial.printf("Free Space: %llu\r\n",
			pmsFS[currDrv]->totalSize()-pmsFS[currDrv]->usedSize());
			break;
		case SDIO_TYPE:
			Serial.printf("Free Space: %llu\r\n",
			sdfs[0].sd.totalSize()-sdfs[0].sd.usedSize());
			break;
		case SPI_TYPE:
			Serial.printf("Free Space: %llu\r\n",
			sdfs[1].sd.totalSize()-sdfs[1].sd.usedSize());
			break;
#if defined(ARDUINO_TEENSY41)
		case LFS_TYPE:
			Serial.printf("Free Space: %llu\r\n",
			drvIdx[currDrv].fstype->totalSize()-drvIdx[currDrv].fstype->usedSize());
			break;
#endif
	}
	if(currDrv != drive) {
		changeVolume(drive);  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return true;
}

// List directories only.
bool diskIO::lsSubDir(void *dir) {
#ifdef TalkToMe
  Serial.printf(F("lsSubDir()...\r\n"));
#endif
	File fsDirEntry; // FS.
	File *fsDir = reinterpret_cast < File * > ( dir );
	fsDir->rewindDirectory(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
	while ((fsDirEntry = fsDir->openNextFile(O_RDONLY))) {
		if(fsDirEntry.isDirectory()) { // Only list sub directories.
			Serial.printf(F("%s/"),fsDirEntry.name()); // Display SubDir filename + '/'.
			for(uint8_t i = 0; i <= (45-strlen(fsDirEntry.name())); i++) Serial.print(" ");
			Serial.printf(F("<DIR>\r\n"));
		}
	}
	fsDirEntry.close(); // Done. Close sub-entry file.
	return true;
}

// List files only. Proccess wildcards if specified.
bool diskIO::lsFiles(void *dir, char *pattern, bool wc) {
#ifdef TalkToMe
  Serial.printf(F("lsFiles()...\r\n"));
#endif
    DateTimeFields tm;
	const char *months[12] = {
		"January","February","March","April","May","June",
		"July","August","September","October","November","December"
	};
	File fsDirEntry; // LittleFS.
	File *fsDir = reinterpret_cast < File * > ( dir );
	fsDir->rewindDirectory(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
	while ((fsDirEntry = fsDir->openNextFile(O_RDONLY))) {
		if (wc && !wildcardMatch(pattern, fsDirEntry.name())) continue;
		if(!fsDirEntry.isDirectory()) {
			Serial.printf(F("%s "),fsDirEntry.name()); // Display filename.
			for(uint8_t i = 0; i <= (40-strlen(fsDirEntry.name())); i++) Serial.print(" ");
			Serial.printf(F("%10ld"),fsDirEntry.size()); // Then filesize.
			fsDirEntry.getModifyTime(tm);
			for(uint8_t i = 0; i <= 10; i++) Serial.printf(F(" "));
			Serial.printf(F("%9s %02d %02d %02d:%02d:%02d\r\n"), // Show date/time.
							months[tm.mon],tm.mday,tm.year + 1900, 
							tm.hour,tm.min,tm.sec);
		}
	}
	fsDirEntry.close(); // Done. Close sub-entry file.
	return true;
}

//---------------------------------------------------------------------------
// Test for existance of file or directory.
// Will fail if directory/file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::exists(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("exists(%s)\r\n", dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	bool rslt = false;
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;
	
	if(!(rslt = drvIdx[currDrv].fstype->exists(savePath))) {
		setError(PATH_NOT_EXIST);
	} else {
		setError(PATH_EXISTS);
	}

	if(currDrv != drive) {
		changeVolume(drive); // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return rslt;
}

//---------------------------------------------------------------------------
// Create directory from path spec.
// Will fail if directory exists or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::mkdir(char *path) {
#ifdef TalkToMe
    Serial.printf(F("mkdir(%s)\r\n"),path);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
 	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;
	
    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,path);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		// First check if we are using a LFS device.
		if(!drvIdx[currDrv].fstype->mkdir(savePath)) setError(MKDIR_ERROR);
	}
	if(currDrv != drive) {
		changeVolume(drive); // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// Remove directory from path spec.
// Will fail if directory does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::rmdir(char *dirPath) {
#ifdef TalkToMe
    Serial.printf(F("rmdir(%s)\r\n"),dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		if(!drvIdx[currDrv].fstype->rmdir(savePath)) setError(RMDIR_ERROR);
	}
	if(currDrv != drive) {
		changeVolume(drive); // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// Remove a file from path spec.
// Will fail if file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::rm(char *dirPath) {
#ifdef TalkToMe
    Serial.printf(F("rm(%s)\r\n"),dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
			if(!drvIdx[currDrv].fstype->remove(savePath)) setError(RM_ERROR);
	}
	if(currDrv != drive) {
		changeVolume(currDrv);
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// Rename file or directory.
// Will fail if directory/file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::rename(char *oldpath, char *newpath) {
#ifdef TalkToMe
	Serial.printf(F("rename()\r\n"));
#endif
	char savePath[256];
	savePath[0] = 0;
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,oldpath);

	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		if(!drvIdx[currDrv].fstype->rename(savePath, newpath))	setError(RENAME_ERROR);
	}
	if(currDrv != drive) {
		changeVolume(currDrv);
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// Open file or directory.
//---------------------------------------------------------------------------
bool diskIO::open(File *fp, char* dirPath, oflag_t oflag) {
#ifdef TalkToMe
	Serial.printf(F("open()\r\n"));
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
 
    // Preserve original path spec. Should only change if chdir() is used.	
	char savePath[256] = {""};
	strcpy(savePath,dirPath);

	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;
	// Check for missing path name.
	if((savePath[0] = '/') && (strlen(savePath) == 1)) {
		setError(INVALID_PATH_NAME);
	} else {
		if(!(*fp = drvIdx[currDrv].fstype->open(savePath, oflag))) {
			setError(OPEN_FAILED);
		}
	}
	if(currDrv != drive) {
		changeVolume(drive); // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// close file or directory.
//---------------------------------------------------------------------------
bool diskIO::close(File *fp) {
#ifdef TalkToMe
	Serial.printf("close()\r\n");
#endif
	fp->close(); // Does not return results !!!
	return true;
}

//---------------------------------------------------------------------------
// Read from an open file.
//---------------------------------------------------------------------------
int diskIO::read(File *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("read()\r\n");
#endif
	int32_t br = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	br = fp->read(buf, count);
	if(br <= 0) {
		setError(READ_ERROR);
		return br;
	}
	return br;
}

//---------------------------------------------------------------------------
// Write to an open file.
//---------------------------------------------------------------------------
size_t diskIO::write(File *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("write()\r\n");
#endif
	int32_t bw = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	bw = fp->write(buf, count);
	if(bw != (int)count) {
		setError(WRITE_ERROR);
		return bw;
	}
	return bw;
}

//---------------------------------------------------------------------------
// Seek to an offset in an open file.
//---------------------------------------------------------------------------
off_t diskIO::lseek(File *fp, off_t offset, int whence) {
#ifdef TalkToMe
    Serial.printf(F("lseek(%lu, %d, %d)\r\n"),*fp, offset, whence);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	if(!fp->seek(offset, whence)) {
		setError(SEEK_ERROR);
		return -1;
	}
	return fp->position();
}

//---------------------------------------------------------------------------
// Flush an open file. Sync all un-written data to an open file.
//---------------------------------------------------------------------------
void diskIO::fflush(File *fp) {
#ifdef TalkToMe
    Serial.printf(F("fflush(%lu)\r\n"),*fp);
#endif
	fp->flush();
}

//---------------------------------------------------------------------------
// Return current file position.
//---------------------------------------------------------------------------
int64_t diskIO::ftell(File *fp) {
#ifdef TalkToMe
    Serial.printf(F("ftell(%lu)\r\n"),*fp);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint64_t filePos = 0;
	filePos = fp->position();
	if(filePos < 0 ) {
		setError(FTELL_ERROR);
		return -1;
	}
    return filePos;
}
