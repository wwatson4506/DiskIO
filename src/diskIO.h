// diskIO.h

#ifndef diskIO_h
#define diskIO_h

#include "mscFS.h"

#define CNT_MSDRIVES 4
#define CNT_PARITIONS 24 

#define LOGICAL_DRIVE_SDIO  4
#define LOGICAL_DRIVE_SDSPI 5
#define SD_SPI_CS 10
#define SPI_SPEED SD_SCK_MHZ(40)  // adjust to sd card 
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_SPICONFIG SdioConfig(FIFO_SDIO)

// Path spec defines.
#define DIRECTORY_SEPARATOR "/"
#define MAX_FILENAME_LEN   256
#define MAX_SUB_DEPTH	256

// Types of device hardware interfaces.
#define USB_TYPE	0
#define SDIO_TYPE	1
#define SPI_TYPE	2

// Four partition slot per physical device
#define SLOT_OFFSET 4

// Error codes
#define DISKIO_PASS			0
#define INVALID_PATH_NAME	1
#define PATH_EXISTS			2
#define PATH_NOT_EXIST		3
#define OPEN_FAILED_USB		4
#define OPEN_FAILED_SDIO	5
#define OPEN_FAILED_SPI		6
#define CLOSE_FAILED_USB	7
#define CLOSE_FAILED_SD		8
#define SEEK_ERROR_USB		9
#define SEEK_ERROR_SD		10
#define READ_ERROR_USB		11
#define READ_ERROR_SD		12
#define WRITE_ERROR_USB		13
#define WRITE_ERROR_SD		14
#define FTELL_ERROR			15
#define LDRIVE_NOT_FOUND	252	
#define ifLower(c) ((c) >= 'a' && (c) <= 'z')

//#define TalkToMe  1 // Uncomment this for debug

// Logical drive device descriptor struct based on partitions.
typedef struct {
	msController *thisDrive = nullptr;
	char	name[32];        // Volume name as a drive name.
	char	currentPath[256];    // Current default path spec.
	char	fullPath[256];	 // Full path name. Includes Logical drive name.
	bool	valid = false;   // If true device is connected and mounted.
	uint8_t	driveNumber = 0; // Physical drive number.
	uint8_t	ldNumber = 0;    // Logical drive number.
	uint8_t	driveType;       // USB, SDHC or SDHX
	uint8_t	devAddress;      // MSC device address
	uint8_t	fatType = 0;     // FAT32 or ExFat
	uint8_t	ifaceType = 0;	 // Interface type USB, SDHC, SPI.
//	uint8_t	lastError = 0;
} deviceDecriptorEntry_t;

class diskIO : public PFsVolume
{
public:
	PFsVolume mp[CNT_PARITIONS]; // Setup an array of global mount points.
	deviceDecriptorEntry_t drvIdx[CNT_PARITIONS]; // An array of device descriptors.

	uint8_t error(void);
	bool init();
	int  getLogicalDeviceNumber(char *path);	
	int  isDriveSpec(char *driveSpec);
	int  changeDrive(char *driveSpec);
	bool chdir(char *path);
	bool mkdir(const char *path);
	bool rmdir(const char *path);
	bool exists(const char *path);
	bool rename(const char *oldpath, const char *newpath);
	bool open(void *fp, const char* path, oflag_t oflag = O_RDONLY);
	bool close(void *fp);
	int  read(void *fp, char *buf, size_t count);
	size_t  write(void *fp, char *buf, size_t count);
	off_t  lseek(void *fp, off_t offset, int whence);
	void fflush(void *fp);	
	int64_t ftell(void *fp);
	
	void processMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc);
	void processSDDrive(uint8_t drive_number);
	void ProcessSPISD(uint8_t drive_number);
	bool isConnected(uint8_t deviceNumber);
	uint8_t getVolumeCount(void);
	void listAvailableDrives(print_t* p);
	bool relPathToAbsPath(const char *path_in, char * path_out, int outLen);
	bool parsePathSpec(const char *pathSpec);
	bool getWildCard(char *specs, char *pattern);
	bool wildcardMatch(const char *pattern, const char *filename);
	bool lsDir(char *);
	bool lsSubDir(PFsFile *dir);
	bool lsFiles(PFsFile *dir, char *pattern, bool wc);
	uint8_t getCDN(void);
	char *cwd(void);
	diskIO *dio() { return m_diskio; };
private:
	uint8_t count_mp = 0;
	uint8_t currDrv = 0;
	uint8_t m_error = 0;
	UsbFs msc[CNT_MSDRIVES];
	SdFs sd;
	SdFs sdSPI;
	diskIO *m_diskio = this;
};

#endif
