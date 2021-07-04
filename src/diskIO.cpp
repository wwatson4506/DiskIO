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

static uint8_t mscError = MS_INIT_PASS;
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

// -----------------------------------
// Return last error for this device.
// -----------------------------------
uint8_t diskIO::error(void) {
	uint8_t error = mscError;
	mscError = MS_INIT_PASS; // Clear error.
	return error;
}

// -------------------------------------------------------------------------
// Initialize diskIO system.
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
	chdir((char *)"0:");
	currDrv = 0;	      // Set default drive to 0
	mscError = 0;		  // Clear errors
	mp[currDrv].chvol();  // Change the volume to this logical drive.
	return true;
}

// ---------------------------------------
// Check drive connected/initialized
// ---------------------------------------
bool diskIO::isConnected(uint8_t deviceNumber) {
#ifdef TalkToMe
  Serial.printf("isConnected(%d)...\r\n", deviceNumber);
#endif
	//TODO: SDIO and SPI SD cards.
	
	// Invalidate any partitions for device (MSC drives).
	if((mscError = msDrives[deviceNumber].checkConnectedInitialized()) != 0) {
		for(uint8_t i = 0; i < CNT_PARITIONS; i++) { 
			if(drvIdx[i].driveNumber == deviceNumber && drvIdx[i].valid == true) {
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
		if(count_mp >= SLOT_OFFSET)
			count_mp -= SLOT_OFFSET; // Reduce partition count by 4.
		// If current device is valid stay with it and return tested device as false.
		if(drvIdx[currDrv].valid == true) return false; //
		// Else find and setup next avaiable device.
		for(uint8_t i = 0;  i < CNT_PARITIONS; i++) {
			if(drvIdx[i].valid == true) {
				currDrv = i;
				mp[i].chvol();     // Change the volume to this logical drive.
				chdir((char *)"/");
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
//	  drvIdx[i].driveNumber = drive_number;
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].ldNumber = i;
      drvIdx[i].driveType = msc.usbDrive()->usbType();
      drvIdx[i].devAddress = drvIdx[i].thisDrive->getDeviceAddress();
      drvIdx[i].thisDrive = &msDrive;
      drvIdx[i].ifaceType = USB_TYPE;
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
//      drvIdx[i].driveNumber = LOGICAL_DRIVE_SDIO;
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].ldNumber = i;
      drvIdx[i].driveType = sd.card()->type();
      drvIdx[i].ifaceType = SDIO_TYPE;
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
    Serial.println(F("External SD card initialization failed."));
    return;
  }
  for (i = slot; i < (slot + SLOT_OFFSET); i++) {
  if (count_mp >= CNT_PARITIONS) return; // don't overrun
    if (mp[i].begin(sdSPI.card(), true, (i - slot) + 1)) {
//      drvIdx[i].driveNumber = LOGICAL_DRIVE_SDSPI;
      mp[i].getVolumeLabel(drvIdx[i].name, sizeof(drvIdx[i].name));
	  sprintf(drvIdx[i].fullPath ,"/%s/", drvIdx[i].name);
	  drvIdx[i].currentPath[0] = '\0';
      drvIdx[i].fatType = mp[i].fatType();
      drvIdx[i].ldNumber = i;
      drvIdx[i].driveType = sdSPI.card()->type();
      drvIdx[i].ifaceType = SPI_TYPE;
      drvIdx[i].valid = true;
      count_mp++;
    }
  }
  // Move index to next set of 4 entries for next drive.
  while((count_mp % SLOT_OFFSET)) count_mp++;
}

//--------------------------------------------------
// Return number of volumes found and mounted.
//--------------------------------------------------
uint8_t diskIO:: getVolumeCount(void) {
	uint8_t validDrvCount = 0;
	
	for(int i = 0; i < CNT_PARITIONS; i++)
		if(drvIdx[i].valid) validDrvCount++;
	return validDrvCount;
}

void diskIO::checkDrivesConnected(void) {
	for(uint8_t i = 0; i < CNT_MSDRIVES; i++) {
		isConnected(i);
	}
}

//--------------------------------------------------
// Display info on available mounted logical drives.
//--------------------------------------------------
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
// param out: device index number. Path name stripped of volume label.
// ----------------------------------------------------------------------------
int diskIO::getLogicalDeviceNumber(char *path) {
#ifdef TalkToMe
  Serial.printf("getLogicalDeviceNumber(%s)\r\n", path);
#endif
	char str1[256];
	char *tempPath;
	char tempChar, ldNumber[256];
	const char *strPtr;
	char pathChar;
	int i = 0, volume = -1;
	uint8_t cntDigits = 0;
	mscError = DISKIO_PASS; // Clear exsisting error code (if any)

	strcpy(str1,path);  // Isolate pointer to path from changes below.
	tempPath = str1;
//	if (!tempPath) return -1;	// Invalid path name?
	// If no drive or path spec, return current default logical drive number.
	if(tempPath[0] == '\0') {
	// Check default logical drive is still connected if no drive spec provided.
	if((volNum2drvNum[currDrv] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[currDrv]))) {
		mscError = DEVICE_NOT_CONNECTED; //Failed, You must have unplugged it!!
		return -1; // Signal failure.
	}
		 return currDrv; // If no drive or path spec, return current 
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
		if (i < CNT_PARITIONS) {	// Don't overrun array.
			volume = i;		// Drive number
			ldNumber[cntDigits] = '/';  // Change ':' to '/'.
			sprintf(path, "%s", ldNumber+cntDigits);	// Snip off the drive number.
			if((volNum2drvNum[volume] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[volume]))) {
				mscError = DEVICE_NOT_CONNECTED;
				return -1;
			} else if(!drvIdx[volume].valid) {
				mscError = LDRIVE_NOT_FOUND;
				return -1;
			}
			return volume;
		}
	}
    // Look for logical device name (/volume name/).
	if (*tempPath == '/') { // Look for first '/'
		do {
			strPtr = drvIdx[i].name;	// Volume label.
			tempPath = str1;			// Path to search.
			// Compare the volume label with path name.
			do {
				// Get a character to compare (with inc).
				pathChar = *strPtr++; tempChar = *(++tempPath);
				if (ifLower(pathChar)) pathChar -= 0x20; // Make upper case.
				if (ifLower(tempChar)) tempChar -= 0x20; // Ditto.
			} while (pathChar && (char)pathChar == tempChar);
		  // Repeat for each label until there is a pattern match.
		} while ((pathChar || (tempChar != '/')) && ++i < CNT_PARITIONS);
		// If a volume label is found, get the drive number and strip label from path.
		if (i < CNT_PARITIONS) {
			volume = i;		// Volume number.
			strcpy(path, tempPath); // Strip off the logical drive name (leave last '/').
			if((volNum2drvNum[volume] < CNT_MSDRIVES) && (!isConnected(volNum2drvNum[volume]))) {
				mscError = DEVICE_NOT_CONNECTED;
				return -1;
			} else if(!drvIdx[volume].valid) {
				mscError = LDRIVE_NOT_FOUND;
				return -1;
			}
			return volume;
		}
	} 
	return currDrv;	// Return default logical drive. (No Drive spec given) 
}
// Check for a valid drive spec (/volume name/).
// Return possible path spec stripped of drive spec.
int diskIO::isDriveSpec(char *driveSpec) {
	const char *ds = driveSpec;
	int  rslt = getLogicalDeviceNumber(driveSpec);
	if((rslt < 0) && (mscError != DISKIO_PASS)) { // Returned -1, Missing or invalid drive spec. 
		return rslt;
	}
	strcpy((char *)driveSpec,ds); // Save file path name stripped of drive spec.
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

//-------------------------------------
// Change drive.
// param in: Drive spec. ("/logicalDriveName/...")
int diskIO::changeDrive(char *driveSpec) {
	int rslt = isDriveSpec(driveSpec); // returns ds stripped of drive spec.
	if(rslt < 0) { // Returned -1, Not Found or invalid drive spec. 
		return rslt;
	}
	currDrv = rslt;       // Set the current working drive number.
	mp[rslt].chvol();     // Change the volume to this logical drive.
	return rslt;
}

// Get current working drive number.
uint8_t diskIO::getCDN(void) {
	return currDrv;
}

// Get current working directory (with logical drive name).
char *diskIO::cwd(void) {
//	tempPath[256];
	sprintf(drvIdx[currDrv].fullPath,"/%s%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
	return drvIdx[currDrv].fullPath;
}

//------------------------------------------------------------
// Make an absolute path string from a relative pathe string.
// Process ".", ".." and "../".
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

//Wildcard string compare.
//Written by Jack Handy - jakkhandy@hotmail.com
//http://www.codeproject.com/KB/string/wildcmp.aspx
// -------------------------------------------------------------------
// Based on 'wildcmp()' (original) and highly modified for use with
// Teensy by Warren Watson.
// Found in SparkFun's OpenLog library.
// -------------------------------------------------------------------
// Check for filename match to wildcard pattern.
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

//-----------------------------
// Directory Listing functions.
//-----------------------------
// List a directory from given path. Can include a volume name delimeted with
// '/name/' at the begining of path string.
bool diskIO::lsDir(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("lsDir %s...\r\n", dirPath);
#endif
	PFsFile dir;
	char pattern[256];
	char path[256];
	char tempPath[256];
	bool wildcards = false;
	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

	path[0] = 0;
	pattern[0] = 0;
	
	strcpy(path,dirPath); // Isolate original path pointer from changes below.

	// Check for a drive spec (/volume name/).
	newDrv = isDriveSpec(path);
	if((newDrv < 0) && (mscError != DISKIO_PASS)) {
		return false; // Returns drive index else -1.
	} else if((currDrv != newDrv) && (mscError == DISKIO_PASS)) {
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	}
//mp[newDrv].chvol(); // Change to the new logical drive.

	// Get current path spec and add '/' + given path spec to it.
	if(strlen(path) != 1)
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	else
		strcpy(tempPath,path);

	// Check for '.', '..', '../'.
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false;	// Invalid path name.
	}

	// Show current logical drive name.
	Serial.printf(F("Volume Label: %s\r\n"), drvIdx[currDrv].name);
	// Show full path name (with logical drive name).
	Serial.printf(F("Full Path: %s\r\n"), cwd());

	// wildcards = true if any wildcards used else false.
	wildcards = getWildCard((char *)tempPath,pattern);  

	// Try to open the directory.
	if(!dir.open(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false;  // Invalid path name.
	}

	// If wildcards given just list files that match (no sub directories) else
	// List sub directories first then files.
	if(wildcards) {
		lsFiles(&dir, pattern, wildcards);
	} else {
		lsSubDir(&dir);
		lsFiles(&dir, pattern, wildcards);
	}
	// If a logical drive was specified then switch back to default drive.
	mp[drive].chvol();  // Change back to original logical drive. If changed.
	currDrv = drive;    // Ditto with the drive index.
	dir.close();		// Done. Close directory base file.
	return true;
}

// List directories only.
bool diskIO::lsSubDir(PFsFile *dir) {
#ifdef TalkToMe
  Serial.printf(F("lsSubDir()...\r\n"));
#endif
	bool rsltOpen;   // file open status
	char fname[MAX_FILENAME_LEN];
    PFsFile dirEntry;

	dir->rewind(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
	while ((rsltOpen = dirEntry.openNext(dir, O_RDONLY))) {
		if (dirEntry.getName(fname, sizeof(fname))) { // Get the filename.
			if(dirEntry.isSubDir()) { // Only list sub directories.
				Serial.printf(F("%s/"),fname); // Display SubDir filename + '/'.
				for(uint8_t i = 0; i <= (45-strlen(fname)); i++) Serial.print(" ");
				Serial.printf(F("<DIR>\r\n"));
			}
		}
	}
    dirEntry.close(); // Done. Close sub-entry file.
	return true;
}

// List files only. Proccess wildcards if specified.
bool diskIO::lsFiles(PFsFile *dir, char *pattern, bool wc) {
#ifdef TalkToMe
  Serial.printf(F("lsFiles()...\r\n"));
#endif
	uint16_t date;
	uint16_t time;
	bool rsltOpen;         // file open status
	char fname[MAX_FILENAME_LEN];
    PFsFile dirEntry;

	dir->rewind(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
	while ((rsltOpen = dirEntry.openNext(dir, O_RDONLY))) {
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
	}
    dirEntry.close(); // Done. Close sub-entry file.
	return true;
}


//---------------------------------------------------------------------------
// Change directory.
// param in: "path name". Also processes drive spec and changes drives.
//---------------------------------------------------------------------------
bool diskIO::chdir(char *dirPath) {
#ifdef TalkToMe
  Serial.printf("chdir %s...\r\n", path);
#endif
	char tempPath[256];
	char path[256];

	strcpy(path,dirPath); // Isolate original path pointer from changes below.

	// Check for a logical drive change.
	if(changeDrive(path) < 0 && mscError != DISKIO_PASS) {
		return false;
	}
	// Get current path spec and add '/' + given path spec to it.
	if(strlen(path) != 1)
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	else
    strcpy(tempPath,path);

	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}

	if(mp[currDrv].chdir((const char *)tempPath)) {
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// Create directory from path spec.
// Will fail if directory exsits or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::mkdir(const char *path) {
	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

	char tempPath[256];
	strcpy(tempPath,path); // Isolate original path pointer from changes below.

	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(tempPath)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}

	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	if(mp[currDrv].exists(tempPath)) {
		mscError = PATH_EXISTS;
		return false;
	}
	if(mp[currDrv].mkdir(tempPath)) {
		if(currDrv != drive) {
			mp[drive].chvol();  // Change back to original logical drive. If changed.
			currDrv = drive;    // Ditto with the drive index.
		}
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// Remove directory from path spec.
// Will fail if directory does not exsit or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::rmdir(const char *path) {
	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

	char tempPath[256];
	strcpy(tempPath,path); // Isolate original path pointer from changes below.
	
	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(tempPath)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}

	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	if(!mp[currDrv].exists(tempPath)) {
		mscError = PATH_NOT_EXIST;
		return false;
	}
	if(mp[currDrv].rmdir(tempPath)) {
		if(currDrv != drive) {
			mp[drive].chvol();  // Change back to original logical drive. If changed.
			currDrv = drive;    // Ditto with the drive index.
		}
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// Test forr exsistance of file or directory.
// Will fail if directory/file does not exsit or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::exists(char *dirPath) {
	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	bool rslt = false;
	char tempPath[256];
	char path[256];

	strcpy(path,dirPath); // Isolate original path pointer from changes below.
	
	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(path)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}
	// Get current path spec and add '/' + given path spec to it.
	if(strlen(path) != 1)
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	else
		strcpy(tempPath,path);

	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	rslt = mp[currDrv].exists(tempPath);
	if(!rslt) mscError = PATH_NOT_EXIST;
	mp[drive].chvol();  // Change back to original logical drive. If changed.
	currDrv = drive;    // Ditto with the drive index.

	return rslt;
}

//---------------------------------------------------------------------------
// Test for exsistance of file or directory.
// Will fail if directory/file does not exsit or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::rename(const char *oldpath, const char *newpath) {
#ifdef TalkToMe
	Serial.printf(F("rename()\r\n"));
#endif

	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

	char tempPathOld[256];
	strcpy(tempPathOld,oldpath); // Isolate original path pointer from changes below.
	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(tempPathOld)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}

	char tempPathNew[256];
	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(tempPathNew)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}

	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPathOld)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPathNew)) { // This one may be silly...
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	if(mp[currDrv].rename(tempPathOld, tempPathNew)) {
		if(currDrv != drive) {
			mp[drive].chvol();  // Change back to original logical drive. If changed.
			currDrv = drive;    // Ditto with the drive index.
		}
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
// Open file or directory.
//---------------------------------------------------------------------------
bool diskIO::open(void *fp, char* dirPath, oflag_t oflag) {
	int newDrv = 0;
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

	char tempPath[256];
	char path[256];

	// Setup two types. One for MSC and one for SDIO/SPISD. 
	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );

	strcpy(path,dirPath); // Isolate original path pointer from changes below.

	// Check for a drive spec (/volume name/).
	if((newDrv = isDriveSpec(path)) >= 0) { // Returns drive index else -1.
		currDrv = (uint8_t)newDrv;	// Set new logical drive index.
		mp[newDrv].chvol(); // Change to the new logical drive.
	} else {
		mscError = LDRIVE_NOT_FOUND;
		return false; // Invalid logical drive.
	}
	// Get current path spec and add '/' + given path spec to it.
	if(strlen(path) != 1)
		sprintf(tempPath, "%s/%s", drvIdx[currDrv].currentPath, path);
	else
		strcpy(tempPath,path);
	
	// Check for ".", ".." and "../"
	if(!parsePathSpec(tempPath)) {
		mscError = INVALID_PATH_NAME;
		return false; // Invalid path spec.
	}
	uint8_t iface = drvIdx[currDrv].ifaceType;
	switch(iface) {
		case USB_TYPE:
			if(!mscFtype->open(&mp[currDrv], tempPath, oflag)) {
				mscError = OPEN_FAILED_USB;
				return false;
			}
			break;
		case SDIO_TYPE:
		if(!sdFtype->open(reinterpret_cast < FsVolume * > (&mp[currDrv]), tempPath, oflag)) {
				mscError = OPEN_FAILED_SDIO;
				return false;
			}
			break;
		case SPI_TYPE:
			if(!sdFtype->open(reinterpret_cast < FsVolume * > (&mp[currDrv]), tempPath, oflag)) {
				mscError = OPEN_FAILED_SPI;
				return false;
			}
			break;
		default:
			break;
	}
	if(currDrv != drive) {
		mp[drive].chvol();  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return true;
	
}

//---------------------------------------------------------------------------
// close file or directory.
//---------------------------------------------------------------------------
bool diskIO::close(void *fp) {

	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp ); 
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;

	switch(iface) {
		case USB_TYPE:
			if(!mscFtype->close()) {
				mscError = CLOSE_FAILED_USB;
				return false;
			}
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			if(!sdFtype->close()) {
				mscError = CLOSE_FAILED_SD;
				return false;
			}
			break;
		default:
			break;
	}
	return true;
}

//---------------------------------------------------------------------------
// Read from an open file.
//---------------------------------------------------------------------------
int diskIO::read(void *fp, char *buf, size_t count) {

	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;
	int br = 0;
	
	switch(iface) {
		case USB_TYPE:
			br = mscFtype->read(buf, count);
			if(br <= 0) {
				mscError = READ_ERROR_USB;
				return br;
			}
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			br = sdFtype->read(buf, count);
			if(br <= 0) {
				mscError = READ_ERROR_SD;
				return br;
			}
			break;
		default:
			break;
	}
	return br;
		
}

//---------------------------------------------------------------------------
// Write to an open file.
//---------------------------------------------------------------------------
size_t diskIO::write(void *fp, char *buf, size_t count) {

	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;
	int bw = 0;
	
	switch(iface) {
		case USB_TYPE:
			bw = mscFtype->write(buf, count);
			if(bw != (int)count) {
				mscError = WRITE_ERROR_USB;
				return bw;
			}
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			bw = sdFtype->write(buf, count);
			if(bw != (int)count) {
				mscError = WRITE_ERROR_SD;
				return bw;
			}
			break;
		default:
			break;
	}
	return bw;
		
}

//---------------------------------------------------------------------------
// Seek to an offset in an open file.
//---------------------------------------------------------------------------
off_t diskIO::lseek(void *fp, off_t offset, int whence) {

	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;
	
	switch(iface) {
		case USB_TYPE:
			switch (whence) {
				case SEEK_SET:
					if (!mscFtype->seekSet(offset)) {
						mscError = SEEK_ERROR_USB;
						return -1;
					}
					return mscFtype->position();
				break;
				case SEEK_CUR:
					if (!mscFtype->seekCur(offset)) {
						mscError = SEEK_ERROR_USB;
						return -1;
					}
					return mscFtype->position();
				break;
				case SEEK_END:
					if (!mscFtype->seekEnd(offset)) {
						mscError = SEEK_ERROR_USB;
						return -1;
					}
					return mscFtype->position();
				break;
			}
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			switch (whence) {
				case SEEK_SET:
					if (!sdFtype->seekSet(offset)) {
						mscError = SEEK_ERROR_SD;
						return -1;
					}
					return sdFtype->position();
				break;
				case SEEK_CUR:
					if (!sdFtype->seekCur(offset)) {
						mscError = SEEK_ERROR_SD;
						return -1;
					}
					return sdFtype->position();
				break;
				case SEEK_END:
					if (!sdFtype->seekEnd(offset)) {
						mscError = SEEK_ERROR_SD;
						return -1;
					}
					return sdFtype->position();
				break;

		}
			break;
		default:
			break;
	}
	return -1;
		
}

//---------------------------------------------------------------------------
// Flush an open file. Sync all un-written data to an open file.
//---------------------------------------------------------------------------
void diskIO::fflush(void *fp) {
	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;

	switch(iface) {
		case USB_TYPE:
			mscFtype->flush();
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			sdFtype->flush();
			break;
		default:
			break;
	}
}

//---------------------------------------------------------------------------
// Return current file position.
//---------------------------------------------------------------------------
int64_t diskIO::ftell(void *fp) {
	PFsFile *mscFtype = reinterpret_cast < PFsFile * > ( fp );
	FsFile *sdFtype = reinterpret_cast < FsFile * > ( fp );
	uint8_t iface = drvIdx[currDrv].ifaceType;
	uint64_t filePos = 0;
	switch(iface) {
		case USB_TYPE:
			filePos = mscFtype->curPosition();
			break;
		case SDIO_TYPE:
		case SPI_TYPE:
			filePos = sdFtype->curPosition();
			break;
		default:
			break;
	}
	if(filePos < 0 ) {
		mscError = FTELL_ERROR;
		return -1;
	}
	else
	    return filePos;
}
