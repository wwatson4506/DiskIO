/********************************************************************
 *
 * This is the device interface for the MMC drive.
 *
********************************************************************/
#include <stdio.h>
#include <fcntl.h>
#include <reent.h>
#include <errno.h>
#include <string.h>
#include <sys/errno.h>
#include "devices.h"
#include "ioctl.h"
#include <stdint.h>

//extern struct DRIVE_DESCRIPTION Drive;

// note, last file buffer is reserved
// for special ops such as delete, rename
// dir listings etc.

//FIL fcbs[MaxFileBuffers+1];

DEVICE_TABLE_ARRAY device_table_list [MaxDeviceDescriptors];

DEVICE_TABLE_ENTRY mmc_driver;

DEVICE_TABLE_ARRAY device_table_list[MaxDeviceDescriptors] = {
	{ &mmc_driver },
//	{ &usb_driver },
	{ NULL }
};

DEVICE_TABLE_LIST device_table = device_table_list;

//int fatfs_to_errno ( FRESULT Result );

/// @brief Convert FafFs error result to POSIX errno.
///
/// - man page errno (3).
///
/// @param[in] Result: FatFs Result code.
///
/// @return POSIX errno.
/// @return EBADMSG if no conversion possible.
//int fatfs_to_errno( FRESULT Result )
//{
//    switch( Result )
//   {
//        case FR_OK:              /* FatFS (0) Succeeded */
//            return (0);          /* POSIX OK */
//        case FR_DISK_ERR:        /* FatFS (1) A hard error occurred in the low level disk I/O layer */
//            return (EIO);        /* POSIX Input/output error (POSIX.1) */

//        case FR_INT_ERR:         /* FatFS (2) Assertion failed */
//            return (EPERM);      /* POSIX Operation not permitted (POSIX.1) */
//
//        case FR_NOT_READY:       /* FatFS (3) The physical drive cannot work */
//            return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

//        case FR_NO_FILE:         /* FatFS (4) Could not find the file */
//            return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

//        case FR_NO_PATH:         /* FatFS (5) Could not find the path */
//            return (ENOENT);     /* POSIX No such file or directory (POSIX.1) */

//        case FR_INVALID_NAME:    /* FatFS (6) The path name format is invalid */
//            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

//        case FR_DENIED:          /* FatFS (7) Access denied due to prohibited access or directory full */
//            return (EACCES);     /* POSIX Permission denied (POSIX.1) */

//        case FR_EXIST:           /* FatFS (8) Access denied due to prohibited access */
//            return (EACCES);     /* POSIX Permission denied (POSIX.1) */

//        case FR_INVALID_OBJECT:  /* FatFS (9) The file/directory object is invalid */
//            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

//        case FR_WRITE_PROTECTED: /* FatFS (10) The physical drive is write protected */
//            return(EROFS);       /* POSIX Read-only filesystem (POSIX.1) */

//        case FR_INVALID_DRIVE:   /* FatFS (11) The logical drive number is invalid */
//            return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

//        case FR_NOT_ENABLED:     /* FatFS (12) The volume has no work area */
//            return (ENOSPC);     /* POSIX No space left on device (POSIX.1) */

//        case FR_NO_FILESYSTEM:   /* FatFS (13) There is no valid FAT volume */
//            return(ENXIO);       /* POSIX No such device or address (POSIX.1) */

//        case FR_MKFS_ABORTED:    /* FatFS (14) The f_mkfs() aborted due to any parameter error */
//            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

//        case FR_TIMEOUT:         /* FatFS (15) Could not get a grant to access the volume within defined period */
//            return (EBUSY);      /* POSIX Device or resource busy (POSIX.1) */

//       case FR_LOCKED:          /* FatFS (16) The operation is rejected according to the file sharing policy */
//            return (EBUSY);		 /* POSIX Device or resource busy (POSIX.1) */


//        case FR_NOT_ENOUGH_CORE: /* FatFS (17) LFN working buffer could not be allocated */
//            return (ENOMEM);     /* POSIX Not enough space (POSIX.1) */

//        case FR_TOO_MANY_OPEN_FILES:/* FatFS (18) Number of open files > _FS_SHARE */
//            return (EMFILE);     /* POSIX Too many open files (POSIX.1) */

//        case FR_INVALID_PARAMETER:/* FatFS (19) Given parameter is invalid */
//            return (EINVAL);     /* POSIX Invalid argument (POSIX.1) */

//    }
//	return (EBADMSG);            /* POSIX Bad message (POSIX.1) */
//}

static unsigned char allocate_fcb (void)
{// find free fcb and return it or -1 if none available.
 // start at slot 3. slots 0 to 2 are reserved for std*.	
int	i;
	for (i=3; i<MaxFileBuffers; i++)
	{
//		if (fcbs[i].obj.fs) continue;
		return i;
	}
	return -1;
}

static int openFileOnDrive (const char *name, int flags)//, int always666)
{
//	FRESULT	result;
	uint8_t	handle;
    int fatfs_modes = 0;

    errno = 0;
//printf("flags = %2.2x\n", flags);
	// Convert flags to fatfs flags
//    if((flags & O_ACCMODE) == O_RDWR)
//        fatfs_modes = FA_READ | FA_WRITE;
//    else if((flags & O_ACCMODE) == O_RDONLY)
//        fatfs_modes = FA_READ;
//    else
//        fatfs_modes = FA_WRITE;

//    if(flags & O_CREAT)
//    {
//        if(flags & O_TRUNC)
//            fatfs_modes |= FA_CREATE_ALWAYS;
//        else
//            fatfs_modes |= FA_OPEN_ALWAYS;
//printf("fatfs_modes = %d\n", fatfs_modes & 0xff);
//    }

		// find a buffer to use.
	if ((handle = allocate_fcb()) == -1)
	{
		errno = ENOBUFS;
		return -1;
	}
//	result = f_open(&fcbs[handle], name, (uint8_t) (fatfs_modes & 0xff));
//printf("result = %d\n", result);
//	if (result != FR_OK)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}

	if(flags & O_APPEND)
    {
//  Seek to end of the file
//        result = f_lseek(&fcbs[handle], fcbs[handle].obj.objsize);
//        if (result != FR_OK)
        {
//            errno = fatfs_to_errno(result);
//            f_close(&fcbs[handle]);
            return(-1);
        }
    }

	return DEVICE(DEVICE_MMC) | handle;
}

static int closeFileOnDrive (int file)
{
//	FRESULT result;
//	result = f_close (&fcbs[file & 0xff]);
//	if (result)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}
	return 0;
}

static _ssize_t readFromDrive (int file, void *ptr, size_t len)
{
//	FRESULT	result;
	uint32_t 	bytes = len;
	uint32_t	br;
		// is a drive still there?
//	if (!DriveDesc.IsValid)
//	{
//		errno = ENODEV;
//		return -1;
//	}
//	result = f_read(&fcbs[file & 0xff], ptr, bytes, &br);
//	if (result != FR_OK)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}
	return ((ssize_t)br);
}

static _ssize_t writeToDrive (int file, const void *ptr, size_t len)
{
//	FRESULT result;
	uint32_t	bw;
		// is a drive still there?
//	if (!DriveDesc.IsValid)
//	{
//		errno = ENODEV;
//		return -1;
//	}
//printf("file = %x\n",file);
//printf("fcbs[file & 0xff].file.spec = %s\n",fcbs[file & 0xff].file.spec);
//pause();
//	result = f_write (&fcbs[file & 0xff], ptr, len, &bw);
//	if (result != FR_OK)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}
	return bw;
}

static int ioctl_dos_seek(int file, _off_t pos, int whence)
{
//	FRESULT	result;
	switch (whence) {
		case SEEK_SET:
//			result = f_lseek(&fcbs[file & 0xff], pos);
//			if (result != FR_OK)
			{
//				errno = fatfs_to_errno(result);
				return -1;
			}
//			return fcbs[file & 0xff].fptr;
			break;
		case SEEK_CUR:
//					result = f_lseek(&fcbs[file & 0xff],
//					(fcbs[file & 0xff].fptr + pos));
//			if (result != FR_OK)
			{
//				errno = fatfs_to_errno(result);
				return -1;
			}
//			return fcbs[file & 0xff].fptr;
			break;
		case SEEK_END:
//			result = f_lseek(&fcbs[file & 0xff],
//					(fcbs[file & 0xff].obj.objsize + pos));
//			if (result != FR_OK)
			{
//				errno = fatfs_to_errno(result);
				return -1;
			}
//			return fcbs[file & 0xff].fptr;
			break;
	}
	errno = EINVAL;
	return -1;
}

static int ioctl_dos_unlink (const char *name)
{
//	FRESULT	result;
//	result = f_unlink(name);
//	if (result != FR_OK)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}
	return 0;
}

static int ioctl_dos_rename (char * old, char * new)
{
//	FRESULT	result;
//	result = f_rename(old, new);
//	if (result != FR_OK)
	{
//		errno = fatfs_to_errno(result);
		return -1;
	}
	return 0;
}

static int ioctl_dos_flush_dir (int file)
{// flush dir entry associated with file.
//	FRESULT	result;
	if ((file & 0xff) < 0 || (file & 0xff) >= MaxFileBuffers) {
		return -1;
	}
//	result = f_sync(&fcbs[file & 0xff]);
//	if (result != FR_OK){
//		errno = fatfs_to_errno(result);
		return -1;
//	}
	return 0;
}

static int ioctlDrive (int file, int cmd, void *ptr)
{
		// is a drive still there?
//	if (!DriveDesc.IsValid) { errno = ENODEV; return -1; }
	switch (cmd) {
		case IOCTL_MMC_SEEK:
				// ptr is to a structure containing two pointers to pointers.
			return ioctl_dos_seek (file, *(_off_t *)((long *)ptr)[0], *(int *)((long *)ptr)[1]);
		case IOCTL_MMC_UNLINK:
				// ptr is the address of a pointer to filename string.
			return ioctl_dos_unlink ((char *)((long *)ptr)[0]);
		case IOCTL_MMC_RENAME:
			return ioctl_dos_rename ((char *)((long *)ptr)[0], (char *)((long *)ptr)[1]);
		case IOCTL_IDE_FLUSH_DIR:
			return ioctl_dos_flush_dir(file);
	}
		// anything not implemented is "INVALID".
	errno = EINVAL;
	return -1;
}

static int initTheDrive (void)
{
		// init the drive system & software.
//	return initIDEdrive ();
	return 0;
}

void initDevices (void)
{
int	i, retries;
	for (i=0; device_table_list[i].item; i++) {
		for (retries=MAX_RETRIES; retries; retries--) {
				// device will return TRUE if failed to init.
			if (device_table_list [i].item->init () == 0) break;
		}
		if (!retries) {
			device_table_list [i].item->init ();
		}
	}
}

DEVICE_TABLE_ENTRY	mmc_driver = {
	"mmc",	// do not change, this is a reserved device name.
	DEVICE_MMC,
	openFileOnDrive,
	closeFileOnDrive,
	readFromDrive,
	writeToDrive,
	initTheDrive,
	ioctlDrive
};

int fflushdir (FILE * fd)
{
	int	idx;
	short file;

	if (!fd) return -EBADF;
	fflush (fd);
	file = fd->_file;
		// look thru device table for this device.
	if ((idx = find_device (DEVICE_TYPE(file))) == -1) return -EBADF;
		// call function if defined.
	if (device_table_list[idx].item->ioctl) {
		return device_table_list[idx].item->ioctl(file, IOCTL_IDE_FLUSH_DIR, NULL);
	}
	return -EBADF;
}

/***********************************************************
 * find_device
 *   search device table and return idx to device.
 *   or return -1 on not found.
***********************************************************/
int find_device(uint8_t device_type)
{
int	i;
		// consider all devices.
	for (i=0; i<MaxDeviceDescriptors && device_table_list[i].item; i++) {
		if (device_table_list[i].item->device_type == device_type) break;
	}
		// look to see which device to call.
	if (i >= MaxDeviceDescriptors || !device_table_list[i].item) {
		errno = ENODEV;
		return -1;
	}
	return i;
}




