//io.c
#include <Arduino.h>
#include <stdio.h>
#include <fcntl.h>
#include <reent.h>
#include <malloc.h>
#include <sys/errno.h>
#include <string.h>
#include <errno.h>
//#include "syscalls.h"
#include "USBHost_t36.h"
#include "devices.h"
#include "ioctl.h"
#include "io.h"

#ifdef __cplusplus
extern "C" {
#endif
/*
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHub hub3(myusb);
USBHub hub4(myusb);
*/
extern DEVICE_TABLE_ARRAY device_table_list [MaxDeviceDescriptors];
void printFilePointerHiLow(FILE *file);
void IO_init(void) {
//	myusb.begin();
}

int dos_read_r(int fd, _PTR buf, size_t cnt)
{
	int	idx;
		// zero read always returns 0.
	if (cnt == 0) return cnt;
		// look thru device table for this device.
	if ((idx = find_device(DEVICE_TYPE(fd))) == -1) return -1;
		// call function if defined.
	if (device_table_list [idx].item->read) {
		return device_table_list [idx].item->read(fd, buf, cnt);
	}
	errno = EBADF;
	return -1;
}

#define STDIN_EOF_CHAR 0x04   /* a ctl-d character */

int dos_write_r (int fd, const void * buf, size_t cnt)
{
int	idx;
		// zero write always returns 0.
	if (cnt == 0) return cnt;
		// look thru device table for this device.
	if ((idx = find_device (DEVICE_TYPE(fd))) == -1) return -1;
		// call function if defined.
	if (device_table_list [idx].item->write) {
		return device_table_list [idx].item->write(fd, buf, cnt);
	}
	errno = EBADF;
	return -1;
}

int kyb_r(char *buf, int nbytes)
{
//	while((!USBKeyboard_available()));
//	*buf = (int)(USBKeyboard_read() & 0xff);
	return 1;
}

int usbSerial_w(char *buf, int nbytes)
{
	int i;
	for(i = 0; i < nbytes; i++)
	{
		usb_serial_putchar(*(buf+i));
//		if (*(buf+i) == '\n')
//			usb_serial_putchar('\r');
	}
	usb_serial_flush_output();
	return i;

//  for (i = 0; i < nbytes; i++) {
//    if (*(buf + i) == '\n') {
//		VT100Putc('\r');
//		tft_print('\r');
//    }
//	VT100Putc(*(buf + i));
//	tft_print(*(buf + i));
//  }
  return (nbytes);
}

int usbSerial_r(char *buf, int nbytes)
{
	int i;
	while(!usb_serial_available());
	for (i = 0; i < nbytes; i++) {
		*buf = usb_serial_getchar();
		if ((*buf == '\n') || (*buf == '\r')) {
			*buf = 0;
			break;
		}
	}
	return nbytes;
}

__attribute__((weak)) 
int _read(int file, char *ptr, int len)
{
//	int i = 0;
	if(DEVICE_TYPE(file) == 3)
		return dos_read_r(file,ptr,len);
	return kyb_r(ptr,len);

//printf("file = %d, len = %d\r\n", file, len);
//		for (i = 0; i < len; i++) {
//	while((!USBKeyboard_available()));
//	*ptr = (int)(USBKeyboard_read() & 0xff);

	return usbSerial_r(ptr,len);
}

__attribute__((weak)) 
int _write(int file, char *ptr, int len)
{
	if(DEVICE_TYPE(file) == 3)
		return dos_write_r(file, ptr, len);
	else
		return usbSerial_w(ptr, len);
}

__attribute__((weak)) 
int _open(const char *filename, int oflag) //, int pmode)
{

	int	i, len;
		// all devices must start with a name and name is bracketed with '/'.
	
	if (filename[0] == '/') {
			// we might have a valid device name, search the table list.
		for (i=0; device_table_list [i].item; i++) {
			len = strlen(device_table_list[i].item->name);
//printf("filename = %s\n", filename);

			if (strncmp (device_table_list [i].item->name, &filename[1], len) == 0) {
					// ensure trailing '/'.
				if (filename[len+1] != '/') continue;
					// found it, now call the open.
					// device is called with device name removed.
				if (device_table_list[i].item->open) {
						// a function address appears to be there...
					return device_table_list [i].item->open(&filename[len+2], oflag);//, pmode);
				}
			}
		}
	}
		// otherwise we get nasty about it..
	errno = ENODEV;
	return -1;

}

__attribute__((weak)) 
int _close(int fd)
{
	int result;
	int	idx;
		// look thru device table for this device.
	if ((idx = find_device (DEVICE_TYPE(fd))) == -1) return -1;
		// call function if defined.
	if (device_table_list [idx].item->close) {
		result = device_table_list [idx].item->close(fd);
	} else {
		errno = EBADF;
		return -1;
	}
	return result;
}

#include <sys/stat.h>

__attribute__((weak)) 
void _getpid(int status)
{
	status = 1;
}

__attribute__((weak)) 
void _kill(int status)
{
	status = 0;
}

__attribute__((weak)) 
int _link(const char *old, const char *new1)
{
	int	len, idx;
	struct IOCTLRENAMESTRUCT {
		const char * oldname;
		const char * newname;
	} IOCTL_RENAME;
		// all devices must start with a name and name is bracketed with '/'.
	if (old[0] == '/') {
			// we might have a valid device name, search the table list.
		for (idx=0; device_table_list [idx].item; idx++) {
			len = strlen(device_table_list[idx].item->name);
			if (strncmp (device_table_list [idx].item->name, &old[1], len) == 0) {
					// ensure trailing '/'.
				if (old[len+1] != '/') continue;
					// found it, now call the open.
					// device is called with device name removed.
				if (device_table_list [idx].item->ioctl) {
					IOCTL_RENAME.oldname = &old[len+2];
					IOCTL_RENAME.newname = new1;
					return device_table_list [idx].item->ioctl (0, IOCTL_MMC_RENAME,&IOCTL_RENAME);
				}
			}
		}
	}
		// otherwise we get nasty about it..
	errno = EINVAL;
	return -1;
}

__attribute__((weak)) 
int _unlink(const char * path)
{
	int	i, len;
	long	ptr;
		// unlink is from some device name, so find that named device.
		// all devices must start with a name and name is bracketed with '/'.
	if (path[0] == '/') {
			// we might have a valid device name, search the table list.
		for (i=0; device_table_list [i].item; i++) {
			len = strlen(device_table_list[i].item->name);
			if (strncmp (device_table_list [i].item->name, &path[1], len) == 0) {
					// ensure trailing '/'.
				if (path[len+1] != '/') continue;
					// found it, now call the unlink.
					// device is called with device name removed.
				if (device_table_list[i].item->ioctl) {
					ptr = ((long)&path[len+2]);
					return device_table_list[i].item->ioctl(0, IOCTL_MMC_UNLINK, (long *)&ptr);
				}
			}
		}
	}
		// otherwise we get nasty about it..
	errno = ENODEV;
	return -1;
}

__attribute__((weak))
int _stat (const char *fname, struct stat *st)
{
  int file;

  /* The best we can do is try to open the file readonly.  If it exists,
     then we can guess a few things about it.  */
  if ((file = _open (fname, O_RDONLY)) < 0)
    return -1;

  memset (st, 0, sizeof (* st));
  st->st_mode = S_IFREG | S_IREAD;
  st->st_blksize = 1024;
  _close (file); /* Not interested in the error.  */
  return 0;
}

__attribute__((weak)) 
int _fstat(int fd, struct stat *st)
{
//	FILINFO fno;
//	FRESULT result;
	st->st_dev = 0; //fcbs[(fd & 0xff)].drive;		/* mmc 0 */
	st->st_ino = 0; //fcbs[(fd & 0xff)].file.first_cluster;
	st->st_mode = S_IFCHR;						/* Always pretend to be a tty */
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = DEVICE_TYPE(fd);				/* ide is device 3 */
	st->st_size = 0; //fcbs[(fd & 0xff)].file.size;
	st->st_atime = 0;
	st->st_mtime = 0;
	st->st_ctime = 0;
	st->st_blocks = 0; //(st->st_size / RDCF_SECTOR_SIZE)+1;
	st->st_blksize = 512; //RDCF_SECTOR_SIZE;
	
	return 0;
}

__attribute__((weak)) 
int _lseek(int fd, off_t offset, int whence)
{
	struct IOCTLSEEKSTRUCT {
		off_t	* offset;
		int		* whence;
	} IOCTL_SEEKER;

	int	idx;
		// look thru device table for this device.
	if ((idx = find_device (DEVICE_TYPE(fd))) == -1) return -1;
		// call function if defined.
	if (device_table_list [idx].item->ioctl) {
		IOCTL_SEEKER.offset = &offset;
		IOCTL_SEEKER.whence = &whence;
		return device_table_list [idx].item->ioctl (fd, IOCTL_MMC_SEEK, &IOCTL_SEEKER);
	}
	errno = EBADF;
	return -1;
}

__attribute__((weak)) 
double _gettimeofday(void) {
//	return sys_timer();
	return 0;
}

#ifdef __cplusplus
}
#endif
