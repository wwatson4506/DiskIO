// diskIO.h

#ifndef diskIO_h
#define diskIO_h

#include "SD.h"
#include "USBHost_t36.h"
#include "ext4FS.h"

#if defined(ARDUINO_TEENSY41)
#include "LittleFS.h" // T4.1 only
#endif

//***********************************************
// un-comment to use TFT library.
//#define USE_TFT
//***********************************************

//***********************************************
// un-comment to use VGA_4bit_T4 library.
//#define USE_VGA
//***********************************************

//***********************************************
//#define TalkToMe  1 // Uncomment this for debug
//***********************************************

#if defined(ARDUINO_TEENSY41)
#define CNT_PARITIONS 34 // Up to 33 partitions including LFS devices. 
#else
#define CNT_PARITIONS 27 // Up to 27 partitions w/o lfs devices
#endif

//************************************************
// Define fixed block device index numbers.
//************************************************
#define LOGICAL_DRIVE_SDIO  20 // SDIO
#define LOGICAL_DRIVE_SDSPI 24 // SDSPI (not used by default)
// LFS fixed block device index numbers.
#define LFS_DRIVE_QPINAND   28
#define LFS_DRIVE_QSPIFLASH 29
#define LFS_DRIVE_QPINOR5   30
#define LFS_DRIVE_QPINOR6   31
#define LFS_DRIVE_SPINAND3  32
#define LFS_DRIVE_SPINAND4  33
//************************************************
// Define LFS CS numbers.
//************************************************
#define LFS_NAND3_CS 3 
#define LFS_NAND4_CS 4 
#define LFS_NOR5_CS  5 
#define LFS_NOR6_CS  6 

//************************************************
// Define SDIO EXT4 block device number and mount
// point index.
// ***********************************************
#define SDIO_BD	 3  // SDIO ext4 block device
#define SDIO_MP 12  // SDIO ext4 mount point 

//************************************************
// Define SDIO EXT4 block device number and mount
// point index.
// ***********************************************
#define SDSPI_BD 3   // SDIO ext4 block device
#define SDSPI_MP 16  // SDIO ext4 mount point 

//************************************************
// Defines for builtin and external SD cards.
//************************************************
#define CS_SD BUILTIN_SDCARD
#define SD_SPI_CS 10
#define SPI_SPEED SD_SCK_MHZ(40)  // adjust to sd card 
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_SPICONFIG SdSpiConfig(SD_SPI_CS, DEDICATED_SPI, SPI_SPEED)

// Arbitrary USB drive device type.
#define DEVICE_TYPE_USB 4

// LFS defines.
#if defined(ARDUINO_TEENSY41)
// Set which Chip Select pin for SPI usage.
const int FlashChipSelect = 6; // PJRC AUDIO BOARD is 10 // Tested NOR 64MB on #5, #6 : NAND 1Gb on #3, 2GB on #4
#endif

// Path spec defines.
#define DIRECTORY_SEPARATOR "/"
//#define MAX_FILENAME_LEN	256
#define MAX_SUB_DEPTH		256
#define MAX_PARTITIONS		4
#define NUMSPACES			40

// Types of device hardware interfaces.
#define USB_TYPE	0
#define SDIO_TYPE	1
#define SPI_TYPE	2
#if defined(ARDUINO_TEENSY41)
#define LFS_TYPE	5
#endif

// Four partition slots per physical device
#define SLOT_OFFSET 4

// Error codes
#define DISKIO_PASS		0
#define INVALID_PATH_NAME	1
#define PATH_EXISTS		2
#define PATH_NOT_EXIST		3
#define OPEN_FAILED		4
#define CLOSE_FAILED		5
#define SEEK_ERROR		6
#define READERROR		7
#define WRITEERROR		8
#define FTELL_ERROR		9
#define RENAME_ERROR		10
#define MKDIR_ERROR		11
#define RMDIR_ERROR		12
#define RM_ERROR		13
#define DISK_FULL_ERROR		14
#define FORMAT_ERROR		15
#define AUDIO_WAV_PLAY_ERR	16
#define UMOUNT_FAILED		17
#define INVALID_VOLUME_NAME	18
#define UNREGISTER_FAILED	19
#define LDRIVE_NOT_FOUND	252	
#define DEVICE_NOT_CONNECTED 253

#define ifLower(c) ((c) >= 'a' && (c) <= 'z')

//#ifdef USE_TFT
//#undef USE_TFT	//undef to use without RA8876 TFT. 
//#endif

// Logical drive device descriptor struct based on partitions.
// Some of the entries are probably redundant a this point.
typedef struct {
	FS		*fstype = nullptr;
	char	currentPath[256];    // Current default path spec.
	char	fullPath[256];	 // Full path name. Includes Logical drive name.
	char	name[32];        // Volume name as a drive name.
	bool	valid = false;   // If true device is connected and mounted.
	uint8_t	ldNumber = 0;    // Logical drive number.
	int     driveID  = -1;	 // Drive ID (0-3).
	uint8_t	driveType = 0;   // USB, SDHC, SDHX or littleFS
	uint8_t	fatType = 0;     // FAT32 or ExFat or EXT4.
	uint8_t	ifaceType = 0;	 // Interface type USB, SDHC, SPI or LFS
} deviceDecriptorEntry_t;

// diskIO class
class diskIO // : public USBFilesystem
{
public:
//	diskIO(USBHost &host);
//	diskIO(USBHost *host);

	FS *fs[CNT_PARITIONS];		 // FS file abstraction.
	deviceDecriptorEntry_t drvIdx[CNT_PARITIONS]; // An array of device descriptors.

	uint8_t error(void);
	void setError(uint8_t error);
	bool mkfs(char *path, int fat_type);
	bool init();
	void findNextDrive(void);
	void connectedMSCDrives(void);
	void initSDDrive(uint8_t sdDrive);
	int  getLogicalDriveNumber(const char *path);	
	int  isDriveSpec(char *driveSpec, bool preservePath);
	int  changeDrive(char *driveSpec);
	void changeVolume(uint8_t volume);
	bool chdir(const char *dirPath);
	bool mkdir(const char *path);
	bool rmdir(const char *dirPath);
	bool rm(const char *dirPath);
	bool exists(const char *dirPath);
	bool rename(const char *oldpath, const char *newpath);
	bool open(File *fp, const char* dirPath, oflag_t oflag = O_RDONLY);
	bool close(File *fp);
	int  read(File *fp, void *buf, size_t count);
	size_t  write(File *fp, void *buf, size_t count);
	off_t  lseek(File *fp, off_t offset, int whence);
	void fflush(File *fp);	
	int64_t ftell(File *fp);
	bool processSDDrive(void);
	bool ProcessLFS(uint8_t drive_number, const char *name);
	bool processPathSpec(char *path);
	uint8_t getVolumeCount(void);
	void listAvailableDrives(print_t* p);
	bool relPathToAbsPath(const char *path_in, char * path_out, int outLen);
	int getExt4PartIndex(const char *vName);
	int getExt4DrvID(const char *vName);	
	int getDrvIdx(USBDrive *device);
	bool mountExt4Part(uint8_t partNum);
	bool mountFATPart(uint8_t partNum);
	bool umountFS(const char *device);
//	char *dirName(const char *path);
//	char *baseName(const char *path);

	bool parsePathSpec(const char *pathSpec);
	bool getWildCard(char *specs, char *pattern);
	bool wildcardMatch(const char *pattern, const char *filename);
	void displayDateTime(uint16_t date, uint16_t time);
	bool lsDir(const char *);
	bool lsSubDir(void *dir);
	bool openDir(const char *pathSpec);
	bool readDir(File *entry, char *dirEntry);
	void closeDir(File *entry);
	void printSpaces(int num);
	bool lsFiles(void *dir, const char *pattern, bool wc);
	uint8_t getCDN(void);
	void setCDN(uint8_t drive);
	char *cwd(void);
	void page(void);
	
	void dumpFilesystemList(uint8_t count);
	
	diskIO *dio() { return m_diskio; };
private:
	char savePath[256];
	uint8_t count_mp = 0;
	uint8_t currDrv = 4;
	uint8_t m_error = 0;
	SdFs sd;
	SdFs sdSPI;

	SDClass sdSDIO;
	SDClass spi;
#if defined(ARDUINO_TEENSY41)
	LittleFS_QPINAND   QPINandFS;
	LittleFS_QSPIFlash QSpiFlashFS;
	LittleFS_Program   ProgFS;
	LittleFS_SPIFlash  SPIFlashFS[2];
	LittleFS_SPIFram   SPIRamFS;
	LittleFS_SPINAND   SPINandFS[2];
#endif
	diskIO *m_diskio = this;
};

extern diskIO dio;
#endif
