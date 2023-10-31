# DiskIO

## This repository attempts to integrate all of the various filesystems for the T3.6/T4.0/T4.1/MicroMod.

## This is work in progress and is strictly experimentation and/or proof of concept. 

### Required Libraries:
 
 #### https://github.com/wwatson4506/USBHost_t36/tree/ext4FS_additions
 
 #### https://github.com/wwatson4506/TeensyEXT4/tree/TeensyEXT4V3
 
 #### https://github.com/wwatson4506/Arduino-Teensy-Codec-lib (If playing music files enabled)

 
Built using Arduino 1.8.19 and Teensyduino 1.57/1.59B3 versions.

The main goal is to be able to unify all of the different access methods of SdFat, LittleFs, MSC and EXT4 filesystems into one API using FS abstraction methods. This is done by using an indexed list of device descriptors. One for each logical device (partition). You do not need to know what type of filesystem you are accessing. All methods work the same no matter what the partition type is thanks to FS. This is done by using a drive specification which can be a logical drive number "0:"-"38:" or a volume label "/volume name/".

**SPI SD not supported yet.**

#### Examples of device names and numbers:
 * **cp** /QPINAND/test.txt 1:test.txt
 * **cp** test.txt test1.txt
 * **cp** /32GSDEXT4/MusicFile.wav /128GEXFAT/MusicFile.wav
 * **ls** 5:../src/*.cpp
 * **rename** /QSPIFLASH/test.dat test1.dat

### Updates:

 - Removed Teensy MTP support.

#### Objectives:

- Support up to 4 USB Mass Storage devices (2 supported at the moment to minimize memory usage) the native SDIO SD and LittleFS devices.
- Allow for 4 partitions per Mass Storage device except LittleFS and SDIO cards. Total of 32 logical drives are supported plus most types of LittleFS devices are defined (28:-34:).
- Use a volume name for access to each logical drive or use an index number for array of mounted logical drives. LittleFS will use defined device names.
- Be able to set a default drive (change drive). The first valid drive is set to default drive on boot up.
- Be able to parse a full path spec including drive spec, relative path specs and wildcard processing.
- Use a more standard directory listing including time and dates stamps using the Teensy builtin RTC.
- Properly process hot plugging including switching default drive to next available drive if default drive is unplugged.
- Support auto mounting and supplying an un-mount method for use with SDFat and ext4FS. (Warnings given for not un-mounting devices cleanly).
- Add extra support for additional EXT4 functionality outside of SDFat and littleFS. (hard links,symbolic links, permissions etc...)
- Play music files from any logical drive. Cannot use the same device if playing a music file from it. Other devices can be accessed as it is non-blocking. 

#### DiskIO Examples:
- DiskIOTesting.ino: A simple test of some DiskIO functions.
- DiskIOMB.ino: A simple cli for testing most diskIO functions and demonstrating unified access to different types of Mass Storage devices on the Teensy. (SdFat, LittleFS and EXT4). DiskIOMB easily allows adding commands that can be executed from the command line. 

#### Playing Music Files:
- Supported types are WAV, MP3, AAC, FLAC and RAW. In "DiskIOMB.h" line #14 un-comment the define "#define AUDIOPLAY 1" to enable playing music files and you will need this library as well:
#### https://github.com/wwatson4506/Arduino-Teensy-Codec-lib


#### Notes:

 - DiskIOMB is a modified version of microBox used for testing and found here: http://sebastian-duell.de/en/microbox/index.html
 - When you plug in a USB drive it will be auto mounted. Supported file systems are SdFat, LittleFS and EXT4. Care must be taken to unmount the device before unplugging it. If not unmounted, you will get a warning about it. If the device is not removed it will be automatically remounted. The EXT4 filesystem writes current information to the drive and clears the cache before it unmounts and unregisters it. Hot plugging SD cards is not supported at this time. Not sure if it is practical to do this as most Teensys would be put in enclosures. Using USB drives seem more practical.
 - Only 4 EXT4 partitions are supported right now. EXT4 device 4 is fixed for an SD card and is logical device "20:". 1 to 3 can be spread across two USB drives.
 - Multiple partiions can be a mix of EXT4, FAT32 and EXFAT. 4 partitions per physical device. SD cards support 1 partition only.

#### TODO:
 - Recursive copy and delete commands with use of wildcards.
 - Change "umount" command to "eject" and add seperate "mount" and "umount" commands to work with individual partitions instead of complete physical USB device.
 - Finish adding support for EXT4 hard links,symbolic links, permissions, directory indexing etc...

In DiskIOMB type "help" at the command line to see the commands that were modified and commands that were added for use with Teensy.

So far most of this is working well but still needs a lot more work. Really don't know if this is something that might be useful but it is fun to play with:)
