// diskIO.h

#ifndef diskIO_h
#define diskIO_h
#include <type_traits>
#include "mscFS.h"
#include "LittleFS.h"

#define CNT_MSDRIVES 4
#define CNT_PARITIONS 32 

#define LOGICAL_DRIVE_SDIO  4
#define LOGICAL_DRIVE_SDSPI 5
#define LOGICAL_DRIVE_LFS   6

#define SD_SPI_CS 10
#define SPI_SPEED SD_SCK_MHZ(40)  // adjust to sd card 
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#define SD_SPICONFIG SdioConfig(FIFO_SDIO)

// LFS Testing (Just QPINAND for now)
#define QPINAND_CS 3
const char memDrvName[] {"QPINAND"};

// Path spec defines.
#define DIRECTORY_SEPARATOR "/"
#define MAX_FILENAME_LEN   256
#define MAX_SUB_DEPTH	256

// Types of device hardware interfaces.
#define USB_TYPE	0
#define SDIO_TYPE	1
#define SPI_TYPE	2
#define LFS_TYPE	5

// Types of OS
#define PFSFILE_TYPE 0
#define FILE_TYPE    1

// Four partition slot per physical device
#define SLOT_OFFSET 4

// Error codes
#define DISKIO_PASS			0
#define INVALID_PATH_NAME	1
#define PATH_EXISTS			2
#define PATH_NOT_EXIST		3
#define OPEN_FAILED			4
#define CLOSE_FAILED		5
#define SEEK_ERROR			6
#define READ_ERROR			7
#define WRITE_ERROR			8
#define FTELL_ERROR			9
#define RENAME_ERROR		10
#define MKDIR_ERROR			11
#define RMDIR_ERROR			12
#define RM_ERROR			13
#define DISK_FULL_ERROR		14
#define OSTYPE_ERROR		15
#define LDRIVE_NOT_FOUND	252	
#define DEVICE_NOT_CONNECTED 253

#define ifLower(c) ((c) >= 'a' && (c) <= 'z')

//#define TalkToMe  1 // Uncomment this for debug

// Logical drive device descriptor struct based on partitions.
// Some of the entries are probably redundant a this point.
typedef struct {
	char	name[32];        // Volume name as a drive name.
	char	currentPath[256];    // Current default path spec.
	char	fullPath[256];	 // Full path name. Includes Logical drive name.
	bool	valid = false;   // If true device is connected and mounted.
	uint8_t	driveNumber = 0; // Physical drive number.
	uint8_t	ldNumber = 0;    // Logical drive number.
	uint8_t	driveType = 0;       // USB, SDHC, SDHX or littleFS
	uint8_t	devAddress = 0;      // MSC device address
	uint8_t	fatType = 0;     // FAT32 or ExFat
	uint8_t	ifaceType = 0;	 // Interface type USB, SDHC, SPI.
	uint8_t osType = 0;		// Type of OS: 0 = PFsFile or 1 = File
} deviceDecriptorEntry_t;

// diskIO class
class diskIO : public PFsVolume
{
public:
	PFsVolume mp[CNT_PARITIONS]; // Setup an array of global mount points.
	deviceDecriptorEntry_t drvIdx[CNT_PARITIONS]; // An array of device descriptors.
	uint8_t error(void);
	void setError(uint8_t error);
	uint64_t usedSize(uint8_t drive_number);
	uint64_t totalSize(uint8_t drive_number);
	bool init();
	void checkDrivesConnected(void);
	int  getLogicalDriveNumber(char *path);	
	int  isDriveSpec(char *driveSpec, bool preservePath);
	int  changeDrive(char *driveSpec);
	bool chdir(char *dirPath);
	bool mkdir(char *path);
	bool rmdir(char *dirPath);
	bool rm(char *dirPath);
	bool exists(char *dirPath);
	bool lfsExists(char *dirPath);
	bool rename(char *oldpath, char *newpath);
	bool open(void *fp, char* dirPath, oflag_t oflag = O_RDONLY);
	bool lfsOpen(void *fp, char* dirPath, oflag_t oflag = O_RDONLY);
	bool close(void *fp);
	bool lfsClose(void *fp);
	int  read(void *fp, void *buf, size_t count);
	int  lfsRead(void *fp, void *buf, size_t count);
	size_t  write(void *fp, void *buf, size_t count);
	size_t  lfsWrite(void *fp, void *buf, size_t count);
	off_t  lseek(void *fp, off_t offset, int whence);
	bool  lfsLseek(void *fp, off_t offset, int whence);
	void fflush(void *fp);	
	void lfsFflush(void *fp);	
	int64_t ftell(void *fp);
	int64_t lfsFtell(void *fp);
	
	void processMSDrive(uint8_t drive_number, msController &msDrive, UsbFs &msc);
	void processSDDrive(uint8_t drive_number);
	void ProcessSPISD(uint8_t drive_number);
	void ProcessLFS(uint8_t drive_number);
	bool processPathSpec(char *path);
	bool isConnected(uint8_t deviceNumber);
	uint8_t getVolumeCount(void);
	void listAvailableDrives(print_t* p);
	bool relPathToAbsPath(const char *path_in, char * path_out, int outLen);
	bool parsePathSpec(const char *pathSpec);
	bool getWildCard(char *specs, char *pattern);
	bool wildcardMatch(const char *pattern, const char *filename);
	bool lsDir(char *);
	bool lsSubDir(void *dir);
	bool lsFiles(void *dir, char *pattern, bool wc);
	uint8_t getCDN(void);
	uint8_t getOsType(char *path);
	char *cwd(void);
	diskIO *dio() { return m_diskio; };
private:
	uint8_t count_mp = 0;
	uint8_t currDrv = 0;
	uint8_t m_error = 0;
	SdFs sd;
	SdFs sdSPI;
	UsbFs msc[CNT_MSDRIVES];
	LittleFS_QPINAND myfs; // This will become an array of LFS devices.
	diskIO *m_diskio = this;
};

#endif
