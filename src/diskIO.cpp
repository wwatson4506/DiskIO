// diskIO.cpp

#include <stdio.h>
#include <fcntl.h>

#include "Arduino.h"
#include "diskIO.h"
#include "Arduino.h"
#include "TimeLib.h"
#include "diskIO.h"

#ifdef USE_TFT
#include <RA8876_t41_p.h>
extern RA8876_t41_p tft;
#endif

#ifdef USE_VGA
#include "VGA_4bit_T4.h"
#endif

// Setup USBHost_t36 and as many HUB ports as needed.
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);

// MSC objects.
USBDrive drive1(myusb);
USBDrive drive2(myusb);
USBDrive drive3(myusb);
USBDrive drive4(myusb);

DMAMEM USBFilesystem msFS1(myusb);
DMAMEM USBFilesystem msFS2(myusb);
DMAMEM USBFilesystem msFS3(myusb);
DMAMEM USBFilesystem msFS4(myusb);
DMAMEM USBFilesystem msFS5(myusb);
DMAMEM USBFilesystem msFS6(myusb);
DMAMEM USBFilesystem msFS7(myusb);
DMAMEM USBFilesystem msFS8(myusb);
//DMAMEM USBFilesystem msFS9(myusb);
//DMAMEM USBFilesystem msFS10(myusb);
//DMAMEM USBFilesystem msFS11(myusb);
//DMAMEM USBFilesystem msFS12(myusb);
//DMAMEM USBFilesystem msFS13(myusb);
//DMAMEM USBFilesystem msFS14(myusb);
//DMAMEM USBFilesystem msFS15(myusb);
//DMAMEM USBFilesystem msFS16(myusb);

DMAMEM ext4FS ext4fsi;
//**********************************************************************
// Setup four instances of ext4FS (four mountable partitions).
//**********************************************************************
DMAMEM ext4FS ext4fs1;
DMAMEM ext4FS ext4fs2;
DMAMEM ext4FS ext4fs3;
DMAMEM ext4FS ext4fs4;
ext4FS *ext4fsp[] = {&ext4fs1, &ext4fs2, &ext4fs3, &ext4fs4};

static uint8_t lncnt = 0;
static int ext4_mount_cnt = 0;

// Quick and dirty
USBFilesystem *filesystem_list[] = {&msFS1, &msFS2, &msFS3, &msFS4, &msFS5, &msFS6, &msFS7, &msFS8}; //,
//						 &msFS9, &msFS10, &msFS11, &msFS12 };//, &msFS13, &msFS14, &msFS15, &msFS16};
#define CNT_MSC  (sizeof(filesystem_list)/sizeof(filesystem_list[0]))

USBDrive *drive_list[] = {&drive1, &drive2}; //, &drive3, &drive4};
#define CNT_DRIVES  (sizeof(drive_list)/sizeof(drive_list[0]))

block_device_t *bdl = ext4fsi.get_bd_list();
bd_mounts_t *ml = ext4fsi.get_mount_list();

typedef struct {
  uint8_t csPin;
  const char *name;
  SDClass sd;

} SDList_t;

SDList_t DMAMEM sdfs[] = {
  {CS_SD, "SDIO"},
  {SD_SPI_CS, "SDSPI"}
};

SdCardFactory cardFactory;
// EXT4 usage with an SD SDIO card.
SdCard *extsd = cardFactory.newCard(SD_CONFIG);

// EXT4 usage with an SD SPI card.
//SdCardFactory cardSpiFactory;
//SdCard *extspisd = cardSpiFactory.newCard(SD_CONFIG);

deviceDecriptorEntry_t drvIdx[CNT_PARITIONS] DMAMEM; // An array of device descriptors.
//FS *fs[CNT_PARITIONS];		 // FS file abstraction.

static uint8_t mscError = DISKIO_PASS;
File root;

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
// Return error for last operation.
// -------------------------------------
uint8_t diskIO::error(void) {
	uint8_t error = mscError;
	mscError = DISKIO_PASS; // Clear error.
	return error;
}

// -----------------------------------
// Set a DISK IO error code.
// -----------------------------------
void diskIO::setError(uint8_t error) {
	mscError = error; // Set error code.
}

// -----------------------------------
// Find next available drive.
// -----------------------------------
void diskIO::findNextDrive(void) {
	for(uint8_t i = 0;  i < CNT_PARITIONS-1; i++) {
		if(drvIdx[i].valid == true) {
			currDrv = i;
			changeVolume(currDrv);     // Change the volume to this logical drive.
//			sprintf(drvIdx[currDrv].fullPath,"/%s%s", drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
			return;
		}
	}
}

//--------------------------------------------------------------
// Display info on available mounted devices and logical drives.
//--------------------------------------------------------------
void diskIO::listAvailableDrives(print_t* p) {
	count_mp = 0;
	p->printf(F("\r\nLogical Drive Information For Attached Drives\r\n"));
	connectedMSCDrives();
	
    for(uint8_t i = 0; i < CNT_PARITIONS; i++) {
	if(drvIdx[i].valid) {
		count_mp++;
		p->printf(F("Logical Drive #: %2u: | Volume Label: %16s | valid: %u | "),
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
			case DEVICE_TYPE_USB:
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
  Serial.printf(F("%d Logical Drives Found\r\n"),count_mp);
  Serial.printf(F("Default Logical Drive: /%s/ (%d:)\r\n"),drvIdx[currDrv].name,drvIdx[currDrv].ldNumber);
}

int diskIO::getDrvIdx(USBDrive *device) {
  for (uint8_t drive_index = 0; drive_index < (CNT_DRIVES); drive_index++) {
    USBDrive *pdrive = drive_list[drive_index];
	if(pdrive == device) return drive_index;
  }
  return -1; // Not found.
}

void diskIO::dumpFilesystemList(uint8_t count){
	for(uint8_t i = 0; i < count; i++) {
		Serial.printf("filesystem_list[%d]->device = %x\n",i,filesystem_list[i]->device);
		Serial.printf("filesystem_list[%d]->partition = %x\n",i,filesystem_list[i]->partition);
		Serial.printf("filesystem_list[%d]->partitionType = %x\n",i,filesystem_list[i]->partitionType);
	}
}

bool diskIO::mountExt4Part(uint8_t partNum) {
#ifdef TalkToMe
  Serial.printf(F("mountExt4Part(uint8_t partNum %d)\r\n"), partNum);
#endif
	char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.

	int driveIndex = ml[partNum].parent_bd.dev_id;
	if(!ext4fsp[ext4_mount_cnt]->begin(partNum)) {
		Serial.printf("ext4fsp[%d]->begin(%d): Failed\n",driveIndex, partNum);
		return false;
	}
	drvIdx[partNum].fstype = (FS *)ext4fsp[ext4_mount_cnt];
	strcpy(drvIdx[partNum].name, (const char *)ml[partNum].volName);
	drvIdx[partNum].ldNumber = partNum;
	
	// Used to avoid [-Wrestrict] warning.
	sprintf(tmpStr,"%s/%s/", drvIdx[partNum].fullPath,drvIdx[partNum].name);
	strcpy(drvIdx[partNum].fullPath,tmpStr);
	// and repaced this...
//	sprintf(drvIdx[partNum].fullPath ,"/%s/", drvIdx[partNum].name);

	drvIdx[partNum].currentPath[0] = '\0';
	drvIdx[partNum].fatType = EXT4_TYPE;
	drvIdx[partNum].driveType = DEVICE_TYPE_USB;
	drvIdx[partNum].ifaceType = EXT4_TYPE;
	drvIdx[partNum].valid = true;
	ext4_mount_cnt++;
	return true;
}

bool diskIO::mountFATPart(uint8_t partNum) {
#ifdef TalkToMe
  Serial.printf(F("mountFATPart(uint8_t partNum %d)\r\n"), partNum);
#endif
	char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.

	int drvidx = getDrvIdx(filesystem_list[partNum]->device); 
	uint8_t idx = (filesystem_list[partNum]->partition-1)+(uint8_t)(drvidx*4);
	if(drvIdx[idx].valid == true) return true; // Already mounted.
	filesystem_list[partNum]->mscfs.getVolumeLabel(drvIdx[idx].name, sizeof(drvIdx[idx].name));

	// Used to avoid [-Wrestrict] warning.
	sprintf(tmpStr,"%s/%s/", drvIdx[idx].fullPath,drvIdx[idx].name);
	strcpy(drvIdx[idx].fullPath,tmpStr);
	// and repaced this...
//    sprintf(drvIdx[idx].fullPath ,"/%s/", drvIdx[idx].name);

	drvIdx[idx].ldNumber = idx;
	drvIdx[idx].fstype = filesystem_list[partNum];
	drvIdx[idx].currentPath[0] = '\0';
	drvIdx[idx].fatType = filesystem_list[partNum]->mscfs.fatType();
	drvIdx[idx].driveType = DEVICE_TYPE_USB;
	drvIdx[idx].ifaceType = USB_TYPE;
	drvIdx[idx].valid = true;
	return true;
}

//------------------------------------------------------
// Check for disconnected/newly connected MSC drives.
// If a drive has been disconnected find the next valid
// drive and set as default drive including SD and LFS.
//------------------------------------------------------
void diskIO::connectedMSCDrives(void) {
#ifdef TalkToMe
  Serial.printf(F("connectedMSCDrives()...\r\n"));
#endif

  // check and init all available USB drives for possible ext4 partitions.
  for (uint16_t drive_index = 0; drive_index < (CNT_DRIVES); drive_index++) {
	USBDrive *pdrive = drive_list[drive_index];
	ext4fsp[drive_index]->init_block_device(pdrive, drive_index);  
  }
  for (uint8_t i = 0; i < CNT_MSC; i++) {
	// If partition type is EXT4 type (0x83) then setup EXT4 partition
	// else setup windows type partitions.
	if((ml[i].pt == EXT4_TYPE) &&
	   (!ml[i].mounted) &&
	   (ext4_mount_cnt < CONFIG_EXT4_BLOCKDEVS_COUNT)) {
      // Setup EXT4 Partition.
	  mountExt4Part(i);
	} else if (*filesystem_list[i] &&
			  (filesystem_list[i]->mscfs.fatType() != EXT4_TYPE) &&
			  (drive_list[i/4]->msDriveInfo.connected) && // Is this needed ??
	          (filesystem_list[i]->mscfs.fatType() != 0x00)) {
      // Setup FAT32/EXFAT Partition.
      mountFATPart(i);
    }
    // Check if USB device is still connected and Partition(s) were previously cleanly unmounted.
    if ((!drive_list[i/4]->msDriveInfo.connected) && (drvIdx[i].valid == true)) {
	  // If not cleanly unmounted, complain, unmount and unregister EXT4 partition.
	  if((ext4_mount_cnt > 0) &&
	     (ml[i].mounted == true) &&
	     (ml[i].pt == EXT4_TYPE)) { // No...
		// EXT4 filesystem...
		Serial.printf(F("\n**************** A BAD THING JUST HAPPENED!!! ****************\n"));
		Serial.printf(F("When unmounting an EXT4 drive data is written back to the drive.\n"));
		Serial.printf(F("To avoid losing cached data, unmount drive before removing device.\n"));
		Serial.printf(F("Unmounting and removing USB drive (%d) filesystem (%d) from drive list. \n"),i/4, i);
		Serial.printf(F("*****************************************************************\n\n"));
		// Manualy unmount and unregister EXT4 drive. Presumably was not done before removed.
		// Reset device descriptor.
		int r = ext4_umount(ml[i].pname);
		if(r != EOK) Serial.printf("Failed to unmount %d: code: %d\n",i, r);
		ext4_device_unregister(ml[i].pname);
		memset((uint8_t *)&drvIdx[i], 0, sizeof(drvIdx[i])); // Clear device descriptor entry.
		memset((uint8_t *)&ml[i], 0, sizeof(bd_mounts_t));   // Clear EXT4 mount list entry.
		memset((uint8_t *)&bdl[i/4], 0, sizeof(block_device_t)); // Clear block device list entry.
		ext4_mount_cnt--; // Reduce EXT4 device count;
		if(i == currDrv) findNextDrive();   // If this was the current drive find the next one and
											// make it the default drive.
      }
	  // If not cleanly unmounted, complain and  unmount and unregister FAT partition.
      if((drvIdx[i].valid == true)) {	// FAT/EXFAT filesystem...
 		// Manualy unmount FAT/EXFAT drive. Presumably was not done before removed.
		// Reset device descriptor.
		Serial.printf(F("\n**************** A BAD THING JUST HAPPENED!!! ****************\n"));
		Serial.printf(F("FAT32/EXFAT partition not cleanly unmounted.\n"));
		Serial.printf(F("To avoid losing cached data, unmount drive before removing device\n"));
		Serial.printf(F("Unmounting and removing USB drive (%d) filesystem (%d) from drive list. \n"),i/4, i);
		Serial.printf(F("*****************************************************************\n\n"));
		filesystem_list[i]->mscfs.end();
		memset((uint8_t *)&drvIdx[i], 0, sizeof(drvIdx[i]));
		// Find and setup next avaiable device.
		if(i == currDrv) findNextDrive();   // If this was the current drive find the next one and
											// make it the default drive.
      }
    }
  }
}

//----------------------------------------------------------------------
// Initialize (0)SDIO/(1)SDSPI Drive. SDSPI does not suppoert EXT4.
//----------------------------------------------------------------------
void diskIO::initSDDrive(uint8_t sdDrive) {
#ifdef TalkToMe
  Serial.printf(F("initSDDrive(uint8_t sdDrive) = %d\r\n"),sdDrive);
#endif
	char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.
	uint8_t slot = LOGICAL_DRIVE_SDIO;

	if(sdDrive == 1) slot = LOGICAL_DRIVE_SDSPI;
	drvIdx[slot].ldNumber = slot;

	// Used to avoid [-Wrestrict] warning.
	strcpy(drvIdx[slot].name, sdfs[sdDrive].name);
	sprintf(tmpStr,"%s/%s/", drvIdx[slot].fullPath,drvIdx[slot].name);
	// and repaced this...
//    sprintf(drvIdx[slot].fullPath ,"/%s/", drvIdx[slot].name);

	strcpy(drvIdx[slot].fullPath,tmpStr);
	drvIdx[slot].fstype = &sdfs[sdDrive].sd;
	drvIdx[slot].currentPath[0] = '\0';
	drvIdx[slot].fatType = sdfs[sdDrive].sd.sdfs.fatType();
	drvIdx[slot].driveType = sdfs[sdDrive].sd.sdfs.card()->type();
	if(sdDrive == 1)
		drvIdx[slot].ifaceType = SPI_TYPE;
	else
		drvIdx[slot].ifaceType = SDIO_TYPE;
	drvIdx[slot].valid = true;
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
	setSyncProvider((getExternalTime)rtc_get);	// the function to get the time from the RTC
	FsDateTime::setCallback(dateTime);		// Set callback

//	pinMode(READ_PIN, OUTPUT); // Init disk read activity indicator.
//	pinMode(WRITE_PIN, OUTPUT); // Init disk write activity indicator.

    // clear device descriptor array.
	for(int ii = 0; ii < CNT_PARITIONS; ii++)
		memset((uint8_t *)&drvIdx[ii], 0, sizeof(drvIdx[ii]));

	// Initialize USBHost_t36
	myusb.begin();
    myusb.Task();

	delay(1500); // Not sure why this delay needs to be this large?
	// Process MSC drives (4 MAX).
	connectedMSCDrives(); // Modified version of KurtE's version.

#if defined(ARDUINO_TEENSY41)
	ProcessLFS(LFS_DRIVE_QPINAND, (const char *)"QPINAND");
	ProcessLFS(LFS_DRIVE_QSPIFLASH, (const char *)"QSPIFLASH");
#if !defined(USE_VGA)
	ProcessLFS(LFS_DRIVE_QPINOR5, (const char *)"SPIFLASH5");
	ProcessLFS(LFS_DRIVE_QPINOR6, (const char *)"SPIFLASH6");
	ProcessLFS(LFS_DRIVE_SPINAND3, (const char *)"SPINAND3");
	ProcessLFS(LFS_DRIVE_SPINAND4, (const char *)"SPINAND4");
#endif
#endif	

	// Process SDIO drive.
	processSDDrive();

//***************************************************************
// Initialize SD drive (SPI). 
// Note: uses pins 10,11,12,13 (SPI). Does not support EXT4.
// Uncomment next line to use SPI SD card.
//	if(sdfs[1].sd.begin(sdfs[1].csPin)) initSDDrive(1); // SDIO.
//***************************************************************

	findNextDrive(); // Find first available drive.
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
  char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.
  uint8_t slot = LOGICAL_DRIVE_SDIO;

  // Init SD card (Block device 3) fixed.
  if(ext4fsp[SDIO_BD]->init_block_device(extsd, SDIO_BD) != EOK) {  
	// Device disconnected reset device descriptor.
	if(ml[SDIO_MP].mounted) {
		ext4fsp[SDIO_BD]->lwext_umount(SDIO_MP);
		if((ext4_mount_cnt > 0) && (ml[SDIO_MP].pt == EXT4_TYPE)) ext4_mount_cnt--;
		memset((uint8_t *)&drvIdx[slot], 0, sizeof(drvIdx[slot])); // Clear device descriptor entry.
		memset((uint8_t *)&ml[SDIO_MP], 0, sizeof(bd_mounts_t));   // Clear EXT4 mount list entry.
		memset((uint8_t *)&bdl[SDIO_MP/4], 0, sizeof(block_device_t)); // Clear block device list entry.
	}
	// Else find and setup next avaiable device.
	if(currDrv == slot) findNextDrive();
    return false;
  }
  if(ml[SDIO_MP].pt == EXT4_TYPE) { //EXT4 type.
    if(!ext4fsp[SDIO_BD]->begin(SDIO_MP)) {
	  return false;
    }
    drvIdx[slot].ldNumber = slot;
    strcpy(drvIdx[slot].name, (const char *)ext4fsp[SDIO_BD]->getVolumeLabel());
	sprintf(tmpStr,"/%s/", drvIdx[slot].name);
	strcpy(drvIdx[slot].fullPath,tmpStr);
    drvIdx[slot].fstype = ext4fsp[SDIO_BD];
    drvIdx[slot].currentPath[0] = '\0';
    drvIdx[slot].fatType = EXT4_TYPE;
    drvIdx[slot].driveType = SDIO_TYPE;
    drvIdx[slot].fatType = EXT4_TYPE;
    drvIdx[slot].ifaceType = EXT4_TYPE;
    drvIdx[slot].valid = true;
	ext4_mount_cnt++;
  } else { // FAT type.
    if(sdfs[0].sd.begin(sdfs[0].csPin)) initSDDrive(0);
  }
  return true;
}

#if defined(ARDUINO_TEENSY41)
// -------------------------------------------------------------
// Mount available LFS devices.
// -------------------------------------------------------------
bool diskIO::ProcessLFS(uint8_t drive_number, const char *name) {
#ifdef TalkToMe
    Serial.printf(F("Initialize LFS device %d...\n"),drive_number);
#endif
  char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.
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
		pinMode(LFS_NOR5_CS,OUTPUT);
		digitalWriteFast(LFS_NOR5_CS,HIGH);
		if (!SPIFlashFS[0].begin(LFS_NOR5_CS,SPI)) return false;
		drvIdx[slot].fstype = &SPIFlashFS[0];
		break;
	case LFS_DRIVE_QPINOR6:
		pinMode(LFS_NOR6_CS,OUTPUT);
		digitalWriteFast(LFS_NOR6_CS,HIGH);
		if (!SPIFlashFS[1].begin(LFS_NOR6_CS,SPI)) return false;
		drvIdx[slot].fstype = &SPIFlashFS[1];
		break;
	case LFS_DRIVE_SPINAND3:
		pinMode(LFS_NAND3_CS,OUTPUT);
		digitalWriteFast(LFS_NAND3_CS,HIGH);
		if (!SPINandFS[0].begin(LFS_NAND3_CS,SPI)) return false;
		drvIdx[slot].fstype = &SPINandFS[0];
		break;
	case LFS_DRIVE_SPINAND4:
		pinMode(LFS_NAND4_CS,OUTPUT);
		digitalWriteFast(LFS_NAND4_CS,HIGH);
		if (!SPINandFS[1].begin(LFS_NAND4_CS,SPI)) return false;
		drvIdx[slot].fstype = &SPINandFS[1];
		break;
	default:
		return false;
  }
  strcpy(drvIdx[slot].name, name);
  sprintf(tmpStr,"%s/%s/", drvIdx[slot].fullPath, drvIdx[slot].name);
  strcpy(drvIdx[slot].fullPath, tmpStr);
  drvIdx[slot].currentPath[0] = '\0';
  drvIdx[slot].fatType = LFS_TYPE;
  drvIdx[slot].driveType = LFS_TYPE;
  drvIdx[slot].ifaceType = LFS_TYPE;
  drvIdx[slot].valid = true;
  return true;
}
#endif

//--------------------------------------------------
// Return index number for a partition by volume name
// or -1 if not found.
//--------------------------------------------------
int diskIO::getExt4PartIndex(const char *vName) {
#ifdef TalkToMe
  Serial.printf("getExt4PartIndex(%s)\r\n", vName);
#endif
	for(int i=0; i < MAX_MOUNT_POINTS; i++) {
		if(!strcmp(vName,ml[i].volName)) return i;
	}
	return -1;
}

//--------------------------------------------------
// Return physical drive index number by volume name
// or -1 if not found.
//--------------------------------------------------
int diskIO::getExt4DrvID(const char *vName) {
//#ifdef TalkToMe
  Serial.printf("getExt4DrvID(%s)\r\n", vName);
//#endif
	for(int i=0; i < MAX_MOUNT_POINTS; i++) {
		if(!strcmp(vName,ml[i].volName)) {
			return ml[i].parent_bd.dev_id;
		}
	}
	return -1;
}

//--------------------------------------------------
// UnMount a partition by name or number.
// device name: /some device/ or 'x:'
//--------------------------------------------------
bool diskIO::umountFS(const char * device) {
#ifdef TalkToMe
  Serial.printf("umountFS(%s)\r\n", device);
#endif
	int drv = 0;
	int idx = 0;
	
	setError(DISKIO_PASS); // Clear any existing error codes.

	// Find logical drive index by volume name or number.
	int ldNum = isDriveSpec((char *)device, false);
	if(ldNum < 0) {
		setError(INVALID_PATH_NAME); // Invalid logical drive number or name.
		return false;
	}
	// Get EXT4 mount list index (by volume name).
	if(drvIdx[ldNum].ifaceType == EXT4_TYPE) {
		idx = getExt4PartIndex(drvIdx[ldNum].name);
	} else { // Get index to FAT/EXTFAT partition.
		idx = ldNum;
	}
	if(idx < 0) {
		setError(INVALID_VOLUME_NAME); // Volume name not found.
		return false;
	} else { 
		// From that we need the physical drive number (0 to 3).
		drv = ml[idx].parent_bd.dev_id;
		for(int i = 0; i < 4; i++) {
			if(ml[(drv*4)+i].pt == EXT4_TYPE && ml[(drv*4)+i].mounted) {
				if(ext4fsp[drv]->lwext_umount((drv*4)+i)) {
					setError(UMOUNT_FAILED);
					return false;
				}
				if(ext4_device_unregister(ml[(drv*4)+i].pname) != ENOENT) {
					setError(UNREGISTER_FAILED);
					return false;
				}
				if(ext4_mount_cnt > 0) ext4_mount_cnt--;
				Serial.printf("Unmounting partition %s.\n", ml[(drv*4)+i].pname);
			} else {
				if(drvIdx[(drv*4)+i].fatType != EXT4_TYPE && drvIdx[(drv*4)+i].valid) {
					filesystem_list[(drv*4)+i]->mscfs.end();
					Serial.printf("Unmounting FAT32/EXFAT partition.\n");
				}
			}
			// Clear Device descriptor and mount list entries.
			memset((uint8_t *)&drvIdx[(drv*4)+i], 0, sizeof(drvIdx[(drv*4)+i]));
			memset((uint8_t *)&ml[(drv*4)+i], 0, sizeof(ml[(drv*4)+i]));
		}
		Serial.printf("Drive /mp/%s can be safely removed now.\n", bdl[drv].name);
		ext4fsp[drv]->clr_BDL_entry(drv); // Clear block device list entry.
	}
	findNextDrive();
	return true;
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

// ----------------------------------------------------------------------------
// Find a device and return the logical drive index number for that device and
// the path spec with volume label stripped off of the path spec.
// param in:  full path.
// param out: device index number or (-1 not found). 'path' name stripped of
// volume label. Processes both '/volume name/' and 'n:' drive specs.
// If an invalid path spec or none existent drive then set mscError code.
// ----------------------------------------------------------------------------
int diskIO::getLogicalDriveNumber(const char *path) {
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
			sprintf((char *)path, "%s", ldNumber+cntDigits);	// Strip off the drive number.
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
			strcpy((char *)path, tempPath); // Strip off the logical drive name (leave last '/').
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
	// Make sure current device is still connected.
	if(drvIdx[currDrv].fstype == nullptr) {
		setError(DEVICE_NOT_CONNECTED); // Error number 253.
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
// Parse out the the directory chain.
//------------------------------------------------------
/*
char *diskIO::dirName(const char *path) {
	char dirpath[256];
	char *dirchain = NULL;
	char *basename = NULL;
	
	strcpy(dirpath,path);
	basename = strrchr(path, '/');
	dirchain = strndup(dirpath, strlen(dirpath) - (strlen(basename)));
	return dirchain;
}
*/

//------------------------------------------------------
// Parse out the filename.
//------------------------------------------------------
/*
char *diskIO::baseName(const char *path) {
	char dirpath[256];
	char *basename = NULL; 

	strcpy(dirpath,path);
	basename = strrchr(dirpath, '/');
	return basename+1; // Strip leading '/'.
}
*/

//------------------------------------------------------
// Parse the path spec. Handle ".", ".." and "../" OP's
//------------------------------------------------------
bool diskIO::parsePathSpec(const char *pathSpec) {
#ifdef TalkToMe
    Serial.printf(F("parsePathSpec(%s)\r\n"),pathSpec);
#endif
	char pathOut[256] = {""};
    if(!relPathToAbsPath(pathSpec, pathOut, 256)) return false;
	strcpy((char *)pathSpec,pathOut);
	return true;
}

//------------------------------------------------------
// Change to volume. This is for the sake of SdFat only.
// This method is not used with LFS or EXT4 drives.
//------------------------------------------------------
void diskIO::changeVolume(uint8_t volume) {
#ifdef TalkToMe
    Serial.printf(F("changeVolume(%d)\r\n"),volume);
#endif
	switch(drvIdx[volume].ifaceType) {
		case USB_TYPE:
			filesystem_list[volume]->mscfs.chvol(); // Change the volume logical USB drive.
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
	tempPath[0] = 0;
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

//---------------------------------------------------------------------
// Change directory.
// param in: "path name". Also processes drive spec and changes drives.
// LFS and EXT4 handled differently.
//---------------------------------------------------------------------
bool diskIO::chdir(const char *dirPath) {
#ifdef TalkToMe
  Serial.printf("chdir(%s)\r\n", dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	char tempPath[512];
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
// TODO: Change ifaceType to the type of LFS device being used instead of 'LFS_TYPE'
//       in all functions using it.
	if(drvIdx[currDrv].ifaceType == LFS_TYPE && currDrv == drvIdx[currDrv].ldNumber) {
		if(!QPINandFS.exists(tempPath)) {
			if(!QSpiFlashFS.exists(tempPath)) {
				setError(PATH_NOT_EXIST);
				return false;
			}
		}
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
		return true;
	}
#endif
	// Ditto for ext4FS.
	if(drvIdx[currDrv].ifaceType == EXT4_TYPE && currDrv == drvIdx[currDrv].ldNumber) {
		if(!drvIdx[currDrv].fstype->exists(tempPath)) {
			setError(PATH_NOT_EXIST);
			return false;
		}
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
		return true;
	}
	switch(drvIdx[currDrv].ifaceType) {
		case USB_TYPE:
			if(!drvIdx[currDrv].fstype->exists((const char *)tempPath)) {
				setError(PATH_NOT_EXIST);
				return false;
			}
			break;
		case SDIO_TYPE:
			if(!sdfs[0].sd.sdfs.chdir((const char *)tempPath)) {
				setError(PATH_NOT_EXIST);
				return false;
			}
			break;
		case SPI_TYPE:
			if(!sdfs[1].sd.sdfs.chdir((const char *)tempPath)) {
				setError(PATH_NOT_EXIST);
				return false;
			}
			break;
		default:
			return false;
	}
		sprintf(drvIdx[currDrv].currentPath, "%s", tempPath);
	return true;
}

//------------------------------------------
// Get current working logical drive number.
//------------------------------------------
uint8_t diskIO::getCDN(void) {
#ifdef TalkToMe
  Serial.printf("getCDN()\r\n");
#endif
	connectedMSCDrives(); // Check for changes.
	return currDrv; // Return current default drive number.
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
// Copy it to 
//---------------------------------------------------------
char *diskIO::cwd(void) {
#ifdef TalkToMe
  Serial.printf("cwd()\r\n");
#endif
	char tmpStr[256] = {};  // Used to avoid [-Wrestrict] warning.
	// Handle special case for ext4. We need to add a '/' to currentPath.
	if(drvIdx[currDrv].fatType == EXT4_TYPE) {
		sprintf(tmpStr,"/%s/%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
	} else {
		sprintf(tmpStr,"/%s%s",drvIdx[currDrv].name, drvIdx[currDrv].currentPath);
	}
	strcpy(drvIdx[currDrv].fullPath, tmpStr);
	return drvIdx[currDrv].fullPath;
}

//------------------------------------------------------------
// Make an absolute path string from a relative path string.
// Process ".", ".." and "../". Returns absolute path string
// in path_out.
// This function found on internet as an example. Needs to be
// optimized. Modified for use with diskIO.
//------------------------------------------------------------
bool diskIO::relPathToAbsPath(const char *path_in, char * path_out, int outLen) {
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
// diskIO.
// Found and taken from SparkFun's OpenLog library.
// ---------------------------------------------------------
// Check for filename match to wildcard pattern.
// ---------------------------------------------------------
bool diskIO::wildcardMatch(const char *pattern, const char *filename) {
#ifdef TalkToMe
  Serial.printf(F("wildcardMatch(%s,%s)\r\n"),pattern, filename);
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
  Serial.printf(F("getWildCard(%s,%s)\r\n"), specs, pattern);
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

const char *months[12] PROGMEM = {
	"January","February","March","April","May","June",
	"July","August","September","October","November","December"
};

//-------------------------------------------------------------------
// Directory Listing functions.
//-------------------------------------------------------------------
// Display file date and time. (SD/MSC)
void diskIO::displayDateTime(uint16_t date, uint16_t time) {
#ifdef TalkToMe
  Serial.printf(F("displayDateTime(%d,%d)\r\n"), date, time);
#endif
    DateTimeFields tm;
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

//-------------------------------------------------------------------------------
// List a directory from given path. Can include a volume name delimited with '/'
// '/name/' at the begining of path string.
//-------------------------------------------------------------------------------
bool diskIO::lsDir(const char *dirPath) {
#ifdef TalkToMe
  Serial.printf(F("lsDir %s...\r\n"), dirPath);
#endif
	File dir; // SD, MSCFS.

	savePath[0] = 0;
	char pattern[256];
	pattern[0] = 0;
	lncnt = 0;

	bool wildcards = false;
	setError(DISKIO_PASS); // Clear any existing error codes.

	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.

    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,dirPath);

	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;

	// Show current logical drive name.
	Serial.printf(F("Volume Label: %s\r\n"), drvIdx[currDrv].name);
	page(); // Display one page at a time. Prompt to continue.
	// Show full path name (with logical drive name).
	// If no path spec given then show current path.
	if(dirPath == (const char *)"")
		Serial.printf(F("Full Path: %s\r\n"), cwd());
	else
		Serial.printf(F("Full Path: %s\r\n"), dirPath);
	page(); // Display one page at a time. Prompt to continue.

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
			Serial.printf(F("Free Space: %llu\r\n"),
			filesystem_list[currDrv]->totalSize()-filesystem_list[currDrv]->usedSize());
			break;
		case SDIO_TYPE:
			Serial.printf(F("Free Space: %llu\r\n"),
			sdfs[0].sd.totalSize()-sdfs[0].sd.usedSize());
			break;
		case SPI_TYPE:
			Serial.printf("Free Space: %llu\r\n",
			sdfs[1].sd.totalSize()-sdfs[1].sd.usedSize());
			break;
#if defined(ARDUINO_TEENSY41)
		case LFS_TYPE:
			Serial.printf(F("Free Space: %llu\r\n"),
			drvIdx[currDrv].fstype->totalSize()-drvIdx[currDrv].fstype->usedSize());
			break;
#endif
		case EXT4_TYPE:
			Serial.printf(F("Free Space: %llu\r\n"),
			drvIdx[currDrv].fstype->totalSize()-drvIdx[currDrv].fstype->usedSize());
			break;
	}
	page(); // Display one page at a time. Prompt to continue.
	if(currDrv != drive) {
		changeVolume(drive);  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return true;
}

//------------------------------------------------------------------
// List directories only.
//------------------------------------------------------------------
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
			for(uint8_t i = 0; i <= (NUMSPACES-strlen(fsDirEntry.name())); i++) Serial.printf(F(" "));
			Serial.printf(F("<DIR>\r\n"));
			page(); // Display one page at a time. Prompt to continue.
		}
	}
	fsDirEntry.close(); // Done. Close sub-entry file.
	return true;
}

//-----------------------------------------------------------------
// List files only. Proccess wildcards if specified.
//-----------------------------------------------------------------
bool diskIO::lsFiles(void *dir, const char *pattern, bool wc) {
#ifdef TalkToMe
  Serial.printf(F("lsFiles()...\r\n"));
#endif
    DateTimeFields tm;
	File fsDirEntry;
	File *fsDir = reinterpret_cast < File * > ( dir );
	uint8_t spacing = NUMSPACES;
	 	
	fsDir->rewindDirectory(); // Start at beginning of directory.
	// Find next available object to display in the specified directory.
	while ((fsDirEntry = fsDir->openNextFile(O_RDONLY))) {
		if (wc && !wildcardMatch(pattern, fsDirEntry.name())) continue;
		if(!fsDirEntry.isDirectory()) {
			Serial.printf(F("%s"),fsDirEntry.name()); // Display filename.
			uint8_t fnlen = strlen(fsDirEntry.name());
			if(fnlen > spacing) // Check for filename bigger the NUMSPACES.
				spacing += (fnlen - spacing); // Correct for buffer overun.
			else
				spacing = NUMSPACES;
			for(int i = 0; i < (spacing-fnlen); i++)
				Serial.printf("%c",' ');
			Serial.printf(F("%10ld"),fsDirEntry.size()); // Then filesize.
			fsDirEntry.getModifyTime(tm);
			for(uint8_t i = 0; i <= 1; i++) Serial.printf(F(" "));
			Serial.printf(F("%9s %02d %02d %02d:%02d:%02d\r\n"), // Show date/time.
							months[tm.mon],tm.mday,tm.year + 1900, 
							tm.hour,tm.min,tm.sec);
			page(); // Display one page at a time. Prompt to continue.
		}
	}
	fsDirEntry.close(); // Done. Close sub-entry file.
	return true;
}

//----------------------------------------------------------------
// Wait for input after lncnt lines have been displayed.
// Used with listDir().
//----------------------------------------------------------------
void diskIO::page(void) {
#ifdef TalkToMe
  Serial.printf(F("page()...\r\n"));
  Serial.printf(F("lncnt = %d\r\n"),lncnt);
//  Serial.printf(F("tft.bottommarg()-1 = %d\r\n"),tft.bottommarg()-1);
#endif
  char chx;
  lncnt++;
#ifdef USE_TFT
  if (lncnt < tft.bottommarg()-2)
#endif
#ifdef USE_VGA
  if(lncnt < vga4bit.getTheight()-1)
#else
  if (lncnt < 35 )
#endif
  return;
  Serial.printf("more>");	

#if defined(USE_TFT) || defined(USE_VGA)
  chx = getchar();
#else
  while(!Serial.available());
  chx = Serial.read();
#endif
  if (chx == '\n')
    chx = ' ';
  Serial.printf("\n");
  lncnt = 0;
}

//---------------------------------------------------------------
// Open a directory and set a File pointer to it.
//---------------------------------------------------------------
bool diskIO::openDir(const char *pathSpec) {
#ifdef TalkToMe
  Serial.printf(F("openDir(%s)...\r\n"),pathSpec);
#endif
	savePath[0] = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,pathSpec);
	// Process path spec.  Return false if failed (-1).
	if(!processPathSpec(savePath)) return false;
	if(!(root = drvIdx[currDrv].fstype->open(savePath))) {
		setError(INVALID_PATH_NAME);
		return false;
	}
	root.rewindDirectory();
	if(currDrv != drive) {
		changeVolume(drive);  // Change back to original logical drive. If changed.
		currDrv = drive;    // Ditto with the drive index.
	}
	return true;
}

//---------------------------------------------------------------
// Read a directory entry.
//---------------------------------------------------------------
bool diskIO::readDir(File *entry, char *dirEntry) {
#ifdef TalkToMe
  Serial.printf(F("readDir(endtry,%s)...\r\n"), dirEntry);
#endif
	*entry = root.openNextFile(O_RDONLY);
	if(!*entry) {
		return false;
	}
	strcpy(dirEntry, entry->name());
	return true;
}

//---------------------------------------------------------------
// close a directory entry. Close root entry as well.
//---------------------------------------------------------------
void diskIO::closeDir(File *entry) {
#ifdef TalkToMe
  Serial.printf(F("closeDir(entry)...\r\n"));
#endif
	entry->close();
	root.close();
// I guess we just have to assume close() worked...
}

//---------------------------------------------------------------------------
// Test for existance of file or directory.
// Will fail if directory/file does not exist or is an invald path spec.
//---------------------------------------------------------------------------
bool diskIO::exists(const char *dirPath) {
#ifdef TalkToMe
  Serial.printf("exists(%s)\r\n", dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
	bool rslt = false;
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
bool diskIO::mkdir(const char *path) {
#ifdef TalkToMe
    Serial.printf(F("mkdir(%s)\r\n"),path);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
 	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
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
bool diskIO::rmdir(const char *dirPath) {
#ifdef TalkToMe
    Serial.printf(F("rmdir(%s)\r\n"),dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
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
bool diskIO::rm(const char *dirPath) {
#ifdef TalkToMe
    Serial.printf(F("rm(%s)\r\n"),dirPath);
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
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

//----------------------------------------------------------------------
// Rename file or directory.
// Will fail if directory/file does not exist or is an invald path spec.
//----------------------------------------------------------------------
bool diskIO::rename(const char *oldpath, const char *newpath) {
#ifdef TalkToMe
	Serial.printf(F("rename(%s,%s)\r\n"),oldpath, newpath);
#endif
	savePath[0] = 0;
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
    // Preserve original path spec. Should only change if chdir() is used.	
	strcpy(savePath,oldpath);
	// Process path spec.  Return false if failed (-1).
	if(processPathSpec(savePath)) {
		if(!drvIdx[currDrv].fstype->rename(savePath, newpath))
			setError(RENAME_ERROR);
	}
	if(currDrv != drive) {
		changeVolume(currDrv);
		currDrv = drive;    // Ditto with the drive index.
	}
	if(mscError == DISKIO_PASS)
		return true;
	return false;
}

//-----------------------------------------------------------------
// Open file or directory.
//-----------------------------------------------------------------
bool diskIO::open(File *fp, const char* dirPath, oflag_t oflag) {
#ifdef TalkToMe
	Serial.printf(F("open()\r\n"));
#endif
	setError(DISKIO_PASS); // Clear any existing error codes.
	uint8_t drive = getCDN(); // Get current logical drive index number. Save it.
 
    // Preserve original path spec. Should only change if chdir() is used.	
	savePath[0] = 0;
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

//----------------------------------------------------------------
// close file or directory.
//----------------------------------------------------------------
bool diskIO::close(File *fp) {
#ifdef TalkToMe
	Serial.printf("close()\r\n");
#endif
	fp->close(); // Does not return results !!!
	return true;
}

//-----------------------------------------------------------------
// Read from an open file.
//-----------------------------------------------------------------
int diskIO::read(File *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf(F("read()\r\n"));
#endif
	int32_t br = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	br = fp->read(buf, count);
	if(br <= 0) {
		setError(READERROR);
		return br;
	}
	return br;
}

//----------------------------------------------------------------
// Write to an open file.
//----------------------------------------------------------------
size_t diskIO::write(File *fp, void *buf, size_t count) {
#ifdef TalkToMe
	Serial.printf(F("write()\r\n"));
#endif
	int32_t bw = 0;

	setError(DISKIO_PASS); // Clear any existing error codes.
	bw = fp->write(buf, count);
	if(bw != (int)count) {
		setError(WRITEERROR);
		return bw;
	}
	return bw;
}

//----------------------------------------------------------------
// Seek to an offset in an open file.
//----------------------------------------------------------------
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

//----------------------------------------------------------------
// Flush an open file. Sync all un-written data to an open file.
//----------------------------------------------------------------
void diskIO::fflush(File *fp) {
#ifdef TalkToMe
    Serial.printf(F("fflush(%lu)\r\n"),*fp);
#endif
	fp->flush();
}

//---------------------------------------------------------------
// Return current file position.
//---------------------------------------------------------------
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
