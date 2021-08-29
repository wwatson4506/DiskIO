// diskIO.cpp

#include <stdio.h>
#include <fcntl.h>

#include "Arduino.h"
#include "diskIO.h"
#include "Arduino.h"
#include "mscFS.h"
#include "TimeLib.h"

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// Create CNT_MSDRIVES instances of msController.
msController msDrives[CNT_MSDRIVES](myusb); // CNT_MSDRIVES at 4 currently.

static uint8_t mscError = DISKIO_PASS;
static uint8_t volNum2drvNum[] = {0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5};

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

// ------------------------------------------------
// Break down time and date values
// ------------------------------------------------
struct tm decode_fattime (uint16_t td, uint16_t tt)
{
    struct tm tx;
	tx.tm_year= (td>>9) + 1980;
	tx.tm_mon= (td>>5) & (16-1);
	tx.tm_mday= td& (32-1);
	
	tx.tm_hour= tt>>11;
	tx.tm_min= (tt>>5) & (64-1);
	tx.tm_sec= 2*(tt& (32-1));
	return tx;
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
// Return bytes used for this device.
// -----------------------------------
uint64_t diskIO::usedSize(uint8_t drive_number) {
		switch (drvIdx[drive_number].ifaceType) {
			case SPI_TYPE:
				return (uint64_t)(sdSPI.clusterCount() - sdSPI.freeClusterCount())
						* (uint64_t)sdSPI.bytesPerCluster();
				break;
			case SDIO_TYPE:
				return (uint64_t)(sd.clusterCount() - sd.freeClusterCount())
						* (uint64_t)sd.bytesPerCluster();
				break;
			case USB_TYPE:
				return (uint64_t)(msc[drvIdx[drive_number].driveNumber].clusterCount()
						- msc[drvIdx[drive_number].driveNumber].freeClusterCount())
						* (uint64_t)msc[drive_number].bytesPerCluster();
				break;
			default:
				return 0;
		}
}

// -----------------------------------------------
// Return total capacity in bytes for this device.
// -----------------------------------------------
uint64_t diskIO::totalSize(uint8_t drive_number) {
		switch (drvIdx[drive_number].ifaceType) {
			case SPI_TYPE:
				return (uint64_t)sdSPI.clusterCount() * (uint64_t)sdSPI.bytesPerCluster();
				break;
			case SDIO_TYPE:
				return (uint64_t)sd.clusterCount() * (uint64_t)sd.bytesPerCluster();
				break;
			case USB_TYPE:
				return (uint64_t)msc[drvIdx[drive_number].driveNumber].clusterCount()
						* (uint64_t)msc[drvIdx[drive_number].driveNumber].bytesPerCluster();
				break;
			default:
			return 0;
		}
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
	
	// Initialize Physical and logical drive numbers.
	for(i = 0; i < CNT_PARITIONS; i++) {
			drvIdx[i].ldNumber = i;
			drvIdx[i].driveNumber = (uint8_t)(i / SLOT_OFFSET);
	}
	
		
	if (!msDrives[0]) {
#ifdef TalkToMe
		Serial.println(F("Waiting up to 5 seconds for a USB drives\n"));
#endif
		elapsedMillis em = 0;
		while (em < 5000) {
			myusb.Task();
			for (i = 0; i < CNT_MSDRIVES; i++) if (msDrives[i]) break;
		}
	}

	for (i = 0; i < CNT_MSDRIVES; i++) {
		if (msDrives[i]) {
			processMSDrive(i, msDrives[i], msc[i]);
		}    
	}
	processSDDrive(LOGICAL_DRIVE_SDIO);
	ProcessSPISD(LOGICAL_DRIVE_SDSPI);
#if defined(ARDUINO_TEENSY41)
	ProcessLFS(LOGICAL_DRIVE_LFS);
#endif	
	chdir((char *)"0:");
	currDrv = 0;	      // Set default drive to 0
	mscError = 0;		  // Clear errors
	mp[currDrv].chvol();  // Change the volume to this logical drive.
	return true;
}

// ---------------------------------------------------
// Check drive connected/initialized. 
// If not remove device and patitions for that device.
// If it is the current drive then set the default
// drive to the next available logical drive.
// If the device is not valid then assume it has just
// been plugged in and initialize it.
// ---------------------------------------------------
bool diskIO::isConnected(uint8_t deviceNumber) {
#ifdef TalkToMe
  Serial.printf("isConnected(%d)...\r\n", deviceNumber);
#endif
	//TODO: SDIO and SPI SD cards.
	// Only USB devices checked at this time.
	// Invalidate any partitions for device (MSC drives).
	if((mscError = msDrives[deviceNumber].checkConnectedInitialized()) != 0) {
		for(uint8_t i = 0; i < CNT_PARITIONS; i++) { 
			if((drvIdx[i].driveNumber == deviceNumber) && (drvIdx[i].valid == true)) {
				drvIdx[i].valid = false;
				drvIdx[i].name[0] = 0;
				drvIdx[i].currentPath[0] = 0;
				drvIdx[i].fullPath[0] = 0;
				drvIdx[i].driveType = 0;
				drvIdx[i].devAddress = 0;
				drvIdx[i].fatType = 0;
				drvIdx[i].ifaceType = 0;
			}
		}
		// Reduce partition count by 4.
		if(count_mp >= SLOT_OFFSET)	count_mp -= SLOT_OFFSET;

		// If current device is valid stay with it and return tested device as false.
		if(drvIdx[currDrv].valid == true) return false;

		// Else find and setup next avaiable device.
		for(uint8_t i = 0;  i < CNT_PARITIONS; i++) {
			if(drvIdx[i].valid == true) {
				currDrv = i;
				mp[i].chvol();     // Change the volume to this logical drive.
				sprintf(drvIdx[currDrv].fullPath,"/%s%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
				break;
			}
		}
		return false;
	}
	// Init and mount partitions on an MSC drive if connected and not validated.
	// Only init and mount if device number is between 0-3 (4 MSC drives max) and
	// valid = false.
	if((drvIdx[deviceNumber * SLOT_OFFSET].valid == false) && (deviceNumber < CNT_MSDRIVES)) {
		processMSDrive(deviceNumber, msDrives[deviceNumber], msc[deviceNumber]);	 
	}
	mp[currDrv].chvol(); // Make sure we stay with the current working volume.
	return true;
}

//----------------------------------------------------------------
// Function to handle one MS Drive...
// (KurtE).
// ---------------------------------------
void diskIO::processMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc)
{
#ifdef TalkToMe
  Serial.printf(F("Initialize USB drive %u...\n"), drive_number);
#endif
  uint8_t slot = drive_number * SLOT_OFFSET;
  uint8_t i = 0;
  if (!msc.begin(&msDrive)) {
    Serial.println("");
    msc.errorPrint(&Serial);
	Serial.printf(F("initialization drive %u failed.\n"), drive_number);
    return;
  }
  // lets see if we have any partitions to add to our list...
  for (i = slot; i < (slot + SLOT_OFFSET); i++) {
    if (count_mp >= CNT_PARITIONS) return; // don't overrun
    if (mp[i].begin((USBMSCDevice*)msc.usbDrive(), true, (i - slot) + 1)) {
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].driveType = msc.usbDrive()->usbType();
      drvIdx[i].ifaceType = USB_TYPE;
      drvIdx[slot].osType = PFSFILE_TYPE;
      drvIdx[i].valid = true;
      count_mp++;
    }
  }
  // Move index to next set of 4 entries for next drive.
  while((count_mp % SLOT_OFFSET)) count_mp++;
}

//----------------------------------------------------------------
// Mount the SDIO card if possible.
// (KurtE).
//----------------------------------------------------------------
void diskIO::processSDDrive(uint8_t drive_number)
{
#ifdef TalkToMe
  Serial.printf(F("Initialize SDIO SD card %d...\n"), drive_number);
#endif

  uint8_t slot = drive_number * SLOT_OFFSET;
  uint8_t i = 0;
  if (!sd.begin(SD_CONFIG)) {
    Serial.println(F("SDIO card initialization failed."));
    return;
  }
  for (i = slot; i < (slot + SLOT_OFFSET); i++) {
    if (count_mp >= CNT_PARITIONS) return; // don't overrun
    if (mp[i].begin(sd.card(), true, (i - slot) + 1)) {
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].driveType = sd.card()->type();
      drvIdx[i].ifaceType = SDIO_TYPE;
      drvIdx[slot].osType = PFSFILE_TYPE;
      drvIdx[i].valid = true;
      count_mp++;
    }
  }
  // Move index to next set of 4 entries for next drive.
  while((count_mp % SLOT_OFFSET)) count_mp++;
}

// --------------------------------------
// Mount External SPI SDcard if possible.
// (KurtE).
// --------------------------------------
void diskIO::ProcessSPISD(uint8_t drive_number) {
#ifdef TalkToMe
    Serial.printf(F("Initialize SPI SD card %d...\n"),drive_number);
#endif
  uint8_t slot = drive_number * SLOT_OFFSET;
  uint8_t i = 0;
  if(!sdSPI.begin(SdSpiConfig(SD_SPI_CS, SHARED_SPI, SPI_SPEED))) {
#ifdef TalkToMe
    Serial.println(F("External SD card initialization failed."));
#endif
    return;
  }
  for (i = slot; i < (slot + SLOT_OFFSET); i++) {
  if (count_mp >= CNT_PARITIONS) return; // don't overrun
    if (mp[i].begin(sdSPI.card(), true, (i - slot) + 1)) {
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].driveType = sdSPI.card()->type();
      drvIdx[i].ifaceType = SPI_TYPE;
      drvIdx[slot].osType = PFSFILE_TYPE;
      drvIdx[i].valid = true;
      count_mp++;
    }
  }
  // Move index to next set of 4 entries for next drive.
  while((count_mp % SLOT_OFFSET)) count_mp++;
}

#if defined(ARDUINO_TEENSY41)
// --------------------------------------
// Mount LFS device if possible.
// (KurtE).
// --------------------------------------
void diskIO::ProcessLFS(uint8_t drive_number) {
#ifdef TalkToMe
    Serial.printf(F("Initialize LFS device %d...\n"),drive_number);
#endif
  // TODO: Proccess all other possible LFS devices.
  // Only QPINAND tested at this time.
  uint8_t slot = drive_number * SLOT_OFFSET;
  if (!myfs.begin()) {
	Serial.printf("Error starting %s\n", memDrvName);
	return;
  }
      strcpy(drvIdx[slot].name, memDrvName);
	  sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);
	  drvIdx[slot].currentPath[0] = '\0';
      drvIdx[slot].fatType = LFS_TYPE;
      drvIdx[slot].driveType = LFS_TYPE;
      drvIdx[slot].ifaceType = LFS_TYPE;
      drvIdx[slot].osType = FILE_TYPE;
      drvIdx[slot].valid = true;

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

//----------------------------------------------------------
// Check availablity of all USB drives that are plugged in.
// TODO: Check SDIO and external drives as well.
//----------------------------------------------------------
void diskIO::checkDrivesConnected(void) {
	for(uint8_t i = 0; i < CNT_MSDRIVES; i++) {
		isConnected(i);
	}
}

//--------------------------------------------------------------
// Display info on available mounted devices and logical drives.
//--------------------------------------------------------------
void diskIO::listAvailableDrives(print_t* p) {
	p->print(F("\r\nLogical Drive Information For Attached Drives\r\n"));
	checkDrivesConnected(); // find and mount connected MSC drives.
    for(uint8_t i = 0; i < CNT_PARITIONS; i++) {
	if(drvIdx[i].valid) {
		p->printf(F("Physical Drive #:%2d | Logical Drive #: %2u | Volume Label: %11s | valid: %u | "),
                                                           drvIdx[i].driveNumber, 
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
  Serial.printf("Default Logical Drive: /%s/ (%d:)\r\n",drvIdx[currDrv].name,drvIdx[currDrv].ldNumber);
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

	setError(DISKIO_PASS); // Clear existing error code (if any)
	strcpy(str1,path);  // Isolate pointer to path from changes below.
	tempPath = str1;
	// If no drive or path spec, return current default logical drive number.
	if(tempPath[0] == '\0') {
		// Check default logical drive is still connected if no drive spec provided.
		if((volNum2drvNum[currDrv] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[currDrv]))) {
			setError(DEVICE_NOT_CONNECTED); //Failed, You must have unplugged it!!
			return -1; // Signal failure.
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
			if((volNum2drvNum[volume] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[volume]))) {
				setError(DEVICE_NOT_CONNECTED); // Error number 253.
				return -1; // Indicate failure. Device not connected.
			} else if(!drvIdx[volume].valid) {
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
			if((volNum2drvNum[volume] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[volume]))) {
				setError(DEVICE_NOT_CONNECTED); // Error number 253.
				return -1; // Indicate failure. Device not connected.
			} else if(!drvIdx[volume].valid) {
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
	if((volNum2drvNum[currDrv] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[currDrv]))) {
		setError(DEVICE_NOT_CONNECTED); // Error number 253.
		return -1; // Indicate failure. Device not connected.
	}
	return currDrv;	// Return default logical drive. (No Drive spec given) 
}

// Check for a valid drive spec ('/volume name/' or 'logical drive number:').
// If preservePath == true then return full path spec with drive spec.
// Else return path spec stripped of drive spec.
// Return -1 for no drive spec given or the logical drive number.
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
	char pathOut[256];
    if(!relPathToAbsPath(pathSpec, pathOut, 256)) return false;
	strcpy((char *)pathSpec,pathOut);
	return true;
}

//------------------------------------------------
// Change drive.
// param in: Drive spec. ("/logicalDriveName/...")
// param in: preservePath. (true or false).
//------------------------------------------------
int diskIO::changeDrive(char *driveSpec) {
	int rslt = isDriveSpec(driveSpec, false); // returns ds stripped of drive spec.
	if(rslt < 0) { // Returned -1, Not Found or invalid drive spec. 
		return rslt;
	}
	currDrv = rslt;       // Set the current working drive number.
	mp[rslt].chvol();     // Change the volume to this logical drive.
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
		mp[ldNum].chvol(); // Change to the new logical drive.
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

//------------------------------------------
// Get current working logical drive number.
//------------------------------------------
uint8_t diskIO::getCDN(void) {
		return currDrv;
}

//-----------------------------------------------------------------
// Get logical drive OS type.
// If no drive spec given then return current logical drive osType.
// Else return the osType for given drive spec.
//-----------------------------------------------------------------
int diskIO::getOsType(char *path) {
	if((isDriveSpec(path,true) < 0) && (mscError != DISKIO_PASS)) // If no drive spec given then
		return -1; // Error occured.
	else if((isDriveSpec(path,true) < 0) && (mscError == DISKIO_PASS))
			return 	drvIdx[currDrv].osType; //  use current drive osType.
	else
		return 	drvIdx[isDriveSpec(path,true)].osType; // else return drive spec osType.
}

//---------------------------------------------------------
// Get current working directory (with logical drive name).
//---------------------------------------------------------
char *diskIO::cwd(void) {
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
// List a directory from given path. Can include a volume name delimited with '/'
// '/name/' at the begining of path string.
bool diskIO::lsDir(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("lsDir %s...\r\n", dirPath);
#endif
	PFsFile dir; // SD, MSCFS.
	File	lfsDir; // LittleFS.
	
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

#if defined(ARDUINO_TEENSY41)
	// First check if we are using a LFS device.
	if(drvIdx[currDrv].osType == FILE_TYPE) {
		if(!(lfsDir = myfs.open(savePath))) {
			setError(INVALID_PATH_NAME);
			return false;
		}
		// If wildcards given just list files that match (no sub directories) else
		// List sub directories first then files.
		if(wildcards) {
			lsFiles(&lfsDir, pattern, wildcards);
		} else {
			lsSubDir(&lfsDir);
			lsFiles(&lfsDir, pattern, wildcards);
		}
		lfsDir.close();		// Done. Close directory base file.
		// Show free space left on this device.
		Serial.printf("\r\nFree Space: %llu bytes\r\n\r\n",myfs.totalSize()-myfs.usedSize());
	}
#endif
	// Using a SD, SDIO, MSC device.
	// Try to open the directory.
	if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
		if(!dir.open(savePath)) {
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
// Uncomment this to show free space left on SD, SDIO, MSC devices.
// Note: If volume is FAT32 formatted then there will be a delay before completion.
//		Serial.printf("\r\nFree Space: %llu bytes\r\n\r\n",totalSize(currDrv)-usedSize(currDrv));
		dir.close();		// Done. Close directory base file.
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return true;
}

// List directories only.
bool diskIO::lsSubDir(void *dir) {
#ifdef TalkToMe
  Serial.printf(F("lsSubDir()...\r\n"));
#endif
	bool rsltOpen;   // file open status
	char fname[MAX_FILENAME_LEN];

#if defined(ARDUINO_TEENSY41)
	if(drvIdx[currDrv].osType == FILE_TYPE) {
		File lfsDirEntry; // LittleFS.
		File *lfsDir = reinterpret_cast < File * > ( dir );
		lfsDir->rewindDirectory(); // Start at beginning of directory.
		// Find next available object to display in the specified directory.
		while ((lfsDirEntry = lfsDir->openNextFile(O_RDONLY))) {
			if(lfsDirEntry.isDirectory()) { // Only list sub directories.
				Serial.printf(F("%s/"),lfsDirEntry.name()); // Display SubDir filename + '/'.
				for(uint8_t i = 0; i <= (45-strlen(lfsDirEntry.name())); i++) Serial.print(" ");
				Serial.printf(F("<DIR>\r\n"));
			}
		}
		lfsDirEntry.close(); // Done. Close sub-entry file.
	}
#endif
	// SD, SDIO, MSC device.
	if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
		PFsFile dirEntry;
		PFsFile *Dir = reinterpret_cast < PFsFile * > ( dir );
		Dir->rewind(); // Start at beginning of directory.
		// Find next available object to display in the specified directory.
		while ((rsltOpen = dirEntry.openNext(Dir, O_RDONLY))) {
			if (dirEntry.getName(fname, sizeof(fname))) { // Get the filename.
				if(dirEntry.isSubDir()) { // Only list sub directories.
					Serial.printf(F("%s/"),fname); // Display SubDir filename + '/'.
					for(uint8_t i = 0; i <= (45-strlen(fname)); i++) Serial.print(" ");
					Serial.printf(F("<DIR>\r\n"));
				}
			}
		}
		dirEntry.close(); // Done. Close sub-entry file.
	}
	return true;
}

// List files only. Proccess wildcards if specified.
bool diskIO::lsFiles(void *dir, char *pattern, bool wc) {
#ifdef TalkToMe
  Serial.printf(F("lsFiles()...\r\n"));
#endif
	uint16_t date;
	uint16_t time;
	bool rsltOpen;         // file open status
	char fname[MAX_FILENAME_LEN];

#if defined(ARDUINO_TEENSY41)
	if(drvIdx[currDrv].osType == FILE_TYPE) {
		File lfsDirEntry; // LittleFS.
		File *lfsDir = reinterpret_cast < File * > ( dir );
		lfsDir->rewindDirectory(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
		while ((lfsDirEntry = lfsDir->openNextFile(O_RDONLY))) {
			if (wc && !wildcardMatch(pattern, lfsDirEntry.name())) continue;
			if(!lfsDirEntry.isDirectory()) {
				Serial.printf(F("%s "),lfsDirEntry.name()); // Display filename.
				for(uint8_t i = 0; i <= (40-strlen(lfsDirEntry.name())); i++) Serial.print(" ");
				Serial.printf(F("%10ld\r\n"),lfsDirEntry.size()); // Then filesize.
//				dirEntry.getModifyDateTime(&date, &time);
//				struct tm tx = decode_fattime (date, time);
//				for(uint8_t i = 0; i <= 10; i++) Serial.printf(F(" "));
//					Serial.printf(F("%4d-%02d-%02d %02d:%02d:%02d\r\n"), // Then date/time.
//								tx.tm_year,tx.tm_mon,tx.tm_mday, 
//								tx.tm_hour,tx.tm_min,tx.tm_sec);
			}
		}
		lfsDirEntry.close(); // Done. Close sub-entry file.
	}
#endif
	// SD, SDIO, MSC device.
	if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
		PFsFile dirEntry;
		PFsFile *Dir = reinterpret_cast < PFsFile * > ( dir );
		Dir->rewind(); // Start at beginning of directory.
		// Find next available object to display in the specified directory.
		while ((rsltOpen = dirEntry.openNext(Dir, O_RDONLY))) {
			if (dirEntry.getName(fname, sizeof(fname))) { // Get the filename.
				// If wildcards (wc and pattern) given and there is no match with
				// fname then continue while loop without processing fname.
				if (wc && !wildcardMatch(pattern, fname)) continue;
				if(dirEntry.isFile()) {
					Serial.printf(F("%s "),fname); // Display filename.
					for(uint8_t i = 0; i <= (40-strlen(fname)); i++) Serial.print(" ");
					Serial.printf(F("%10ld"),dirEntry.fileSize()); // Then filesize.
					dirEntry.getModifyDateTime(&date, &time);
					struct tm tx = decode_fattime (date, time);
					for(uint8_t i = 0; i <= 10; i++) Serial.printf(F(" "));
					Serial.printf(F("%4d-%02d-%02d %02d:%02d:%02d\r\n"), // Then date/time.
									tx.tm_year,tx.tm_mon,tx.tm_mday, 
									tx.tm_hour,tx.tm_min,tx.tm_sec);
				}
			}
			dirEntry.close(); // Done. Close sub-entry file.
		}
	}
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
	if(drvIdx[currDrv].osType == FILE_TYPE) {
		if(!myfs.exists(tempPath)) {
			setError(PATH_NOT_EXIST);
			return false;
		}
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
		return true;
	}
#endif
	if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
		if(mp[currDrv].chdir((const char *)tempPath)) {
			sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
			return true;
		}
	}
	return false;
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
#if defined(ARDUINO_TEENSY41)
		// First check if we are using a LFS device.
		if(drvIdx[currDrv].osType == FILE_TYPE) {
			if(!myfs.mkdir(savePath)) setError(MKDIR_ERROR);
		}
#endif
		// Sd,SDIO or MSC
		if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
			if(!mp[currDrv].mkdir(savePath, true)) setError(MKDIR_ERROR);
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
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
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
#if defined(ARDUINO_TEENSY41)
		// First check if we are using a LFS device.
		if(drvIdx[currDrv].ifaceType == LFS_TYPE) {
			if(!myfs.remove(savePath)) setError(RM_ERROR);
		}
#endif
		// Sd,SDIO or MSC
		if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
			if(!mp[currDrv].remove(savePath)) setError(RM_ERROR);
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
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
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {

#if defined(ARDUINO_TEENSY41)
		// First check if we are using a LFS device.
		if(drvIdx[currDrv].ifaceType == LFS_TYPE) {
			if(!myfs.rmdir(savePath)) setError(RMDIR_ERROR);
		}
#endif
		// Sd,SDIO or MSC
		if(drvIdx[currDrv].ifaceType == PFSFILE_TYPE) {
			if(!mp[currDrv].rmdir(savePath)) setError(RMDIR_ERROR);
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

#if defined(ARDUINO_TEENSY41)
//---------------------------------------------------------------------------
// Test for existance of file or directory. (LFS)
// Will fail if directory/file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::lfsExists(char *dirPath) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	bool rslt = false;
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
			rslt = myfs.exists(savePath);
			if(!rslt) {
				setError(PATH_NOT_EXIST);
			} else {
				setError(PATH_EXISTS);
			}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return rslt;
}

//---------------------------------------------------------------------------
// Open file or directory. (LFS)
//---------------------------------------------------------------------------
bool diskIO::lfsOpen(void *fp, char* dirPath, oflag_t oflag) {
#ifdef TalkToMe
	Serial.printf(F("lfsOpen()\r\n"));
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

    // Preserve original path spec. Should only change if chdir() is used.	
	char savePath[256];
	savePath[0] = 0;
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		// Check for missing path name.
		if((savePath[0] = '/') && (strlen(savePath) == 1)) {
			setError(INVALID_PATH_NAME);
		} else {
			// Setup file pointer. 
			File *Fp = reinterpret_cast < File * > ( fp );
			if(!(*Fp = myfs.open(savePath, oflag))) {
				setError(OPEN_FAILED);
			}
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// close file or directory. (LFS)
//---------------------------------------------------------------------------
bool diskIO::lfsClose(void *fp) {
#ifdef TalkToMe
	Serial.printf("lfsClose()\r\n");
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	File *Fp = reinterpret_cast < File * > ( fp ); 
	Fp->close(); // Does not return results !!!
	return true;
}

//---------------------------------------------------------------------------
// Read from an open file. (LFS)
//---------------------------------------------------------------------------
int diskIO::lfsRead(void *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("lfsRead()\r\n");
#endif
	int br = 0;
	File *Fp = reinterpret_cast < File * > ( fp );
	br = Fp->read(buf, count);
	return br;
}

//---------------------------------------------------------------------------
// Write to an open file. (LFS)
//---------------------------------------------------------------------------
size_t diskIO::lfsWrite(void *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("lfsWrite()\r\n");
#endif
	int bw = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	File *Fp = reinterpret_cast < File * > ( fp );
	bw = Fp->write(buf, count);
	if(bw != (int)count) {
		setError(WRITE_ERROR);
		return bw;
	}
	return bw;
}

//---------------------------------------------------------------------------
// Seek to an offset in an open file. (LFS)
//---------------------------------------------------------------------------
bool diskIO::lfsLseek(void *fp, off_t offset, int whence) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	File *Fp = reinterpret_cast < File * > ( fp );
	if (!Fp->seek(offset)) {
		setError(SEEK_ERROR);
		return false;
	}
	
/*
	switch (whence) {
		case SEEK_SET:
			if (!Fp->seekSet(offset)) {
				mscError = SEEK_ERROR;
				return -1;
			}
			return Fp->position();
			break;
		case SEEK_CUR:
			if (!Fp->seekCur(offset)) {
				mscError = SEEK_ERROR;
				return -1;
			}
			return Fp->position();
			break;
		case SEEK_END:
			if (!Fp->seekEnd(offset)) {
				mscError = SEEK_ERROR;
				return -1;
			}
			return Fp->position();
			break;
	}
*/
	return true;
		
}

//---------------------------------------------------------------------------
// Flush an open file. Sync all un-written data to an open file. (LFS)
//---------------------------------------------------------------------------
void diskIO::lfsFflush(void *fp) {
	File *Fp = reinterpret_cast < File * > ( fp );
	Fp->flush();
}

//---------------------------------------------------------------------------
// Return current file position. (LFS)
//---------------------------------------------------------------------------
int64_t diskIO::lfsFtell(void *fp) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	File *Fp = reinterpret_cast < File * > ( fp );
	uint64_t filePos = 0;
	filePos = Fp->position();
	if(filePos < 0 ) {
		setError(FTELL_ERROR);
		return -1;
	}
    return filePos;
}

#endif // LFS functions

//---------------------------------------------------------------------------
// Test for existance of file or directory.
// Will fail if directory/file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::exists(char *dirPath) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	bool rslt = false;
	char savePath[256];
	savePath[0] = 0;

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);
	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;
	
	if(!(rslt = mp[currDrv].exists(savePath))) {
		setError(PATH_NOT_EXIST);
	} else {
		setError(PATH_EXISTS);
	}

	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return rslt;
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
		// First check if we are using a LFS device.
#if defined(ARDUINO_TEENSY41)
		if(drvIdx[currDrv].osType == FILE_TYPE) {
			if(!myfs.rename(savePath, newpath))	setError(RENAME_ERROR);
		}
#endif
		// Sd,SDIO or MSC
		if(drvIdx[currDrv].osType == PFSFILE_TYPE) {
			if(!mp[currDrv].rename(savePath, newpath)) setError(RENAME_ERROR);
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// Open file or directory.
//---------------------------------------------------------------------------
bool diskIO::open(void *fp, char* dirPath, oflag_t oflag) {
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
		// Setup PFsFile file pointer. 
		PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );
		if(!Fp->open(&mp[currDrv], savePath, oflag)) {
			setError(OPEN_FAILED);
		}
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//---------------------------------------------------------------------------
// close file or directory.
//---------------------------------------------------------------------------
bool diskIO::close(void *fp) {
#ifdef TalkToMe
	Serial.printf("close()\r\n");
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
		PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp ); 
		if(!Fp->close()) {
			setError(CLOSE_FAILED);
			return false;
		}
	return true;
}

//---------------------------------------------------------------------------
// Read from an open file.
//---------------------------------------------------------------------------
int diskIO::read(void *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("read()\r\n");
#endif
	int br = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );
	br = Fp->read(buf, count);
	if(br <= 0) {
		setError(READ_ERROR);
		return br;
	}
	return br;
}

//---------------------------------------------------------------------------
// Write to an open file.
//---------------------------------------------------------------------------
size_t diskIO::write(void *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf("write()\r\n");
#endif
	int bw = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );
	bw = Fp->write(buf, count);
	if(bw != (int)count) {
		setError(WRITE_ERROR);
		return bw;
	}
	return bw;
}

//---------------------------------------------------------------------------
// Seek to an offset in an open file.
//---------------------------------------------------------------------------
off_t diskIO::lseek(void *fp, off_t offset, int whence) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );

	switch (whence) {
		case SEEK_SET:
			if (!Fp->seekSet(offset)) {
				setError(SEEK_ERROR);
				return -1;
			}
			return Fp->position();
			break;
		case SEEK_CUR:
			if (!Fp->seekCur(offset)) {
				setError(SEEK_ERROR);
				return -1;
			}
			return Fp->position();
			break;
		case SEEK_END:
			if (!Fp->seekEnd(offset)) {
				setError(SEEK_ERROR);
				return -1;
			}
			return Fp->position();
			break;
	}
	return -1;
		
}

//---------------------------------------------------------------------------
// Flush an open file. Sync all un-written data to an open file.
//---------------------------------------------------------------------------
void diskIO::fflush(void *fp) {
	PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );
	Fp->flush();
}

//---------------------------------------------------------------------------
// Return current file position.
//---------------------------------------------------------------------------
int64_t diskIO::ftell(void *fp) {
	setError(DISKIO_PASS); // Clear any existing error codes.
	PFsFile *Fp = reinterpret_cast < PFsFile * > ( fp );
	uint64_t filePos = 0;
	filePos = Fp->curPosition();
	if(filePos < 0 ) {
		setError(FTELL_ERROR);
		return -1;
	}
    return filePos;
}
