# DiskIOV3

## This repository attempts to integrate all of the various filesystems for the T3.6/T4.0/T4.1/MicroMod.

## This is work in progress and is strictly experimentation and/or proof of concept. 

### Required libraries:
 
 #### https://github.com/wwatson4506/USBHost_t36/tree/ext4FS_addition
 
 #### https://github.com/wwatson4506/TeensyEXT4/tree/TeensyEXT4V3
 
 #### https://github.com/wwatson4506/Arduino-Teensy-Codec-lib (If playing music files enabled)

 
Built using Arduino 1.8.19 and Teensyduino 1.57/1.58B2 versions.

The main goal is to be able to unify all of the different access methods of SdFat, LittleFs, MSC and EXT4 filesystems into one API using FS abstraction methods. This is done by using an indexed list of device descriptors. One for each logical device (partition). You do not need to know what type of filesystem you are accessing. All methods work the same no matter what the partition type is thanks to FS. This is done by using a drive specification which can be a logical drive number "0:"-"38:" or a volume label "/volume name/".

Examples:
 * cp /QPINAND/test.txt 1:test.txt
 * cp test.txt test1.txt
 * cp /32GSDEXT4/MusicFile.wav /128GEXFAT/MusicFile.wav
 * ls 5:../src/*.cpp
 * rename /QSPIFLASH/test.dat test1.dat

The objectives are:

- Support up to 4 USB Mass Storage devices (2 supported at the moment to minimize memory usage) the native SDIO SD and LittleFS devices.
- Allow for 4 partitions per Mass Storage device Except LittleFS. Total of 32 logical drives with all types of LittleFS devices.
- Use a volume name for access to each logical drive or use an index number for array of mounted logical drives. LittleFS will   use defined device names.
- Be able to set a default drive (change drive).
- Be able to parse a full path spec including drive spec, relative path specs and wildcard processing.
- Use a more standard directory listing including time and dates stamps using the Teensy builtin RTC.
- Properly process hot plugging including swithching default drive to next available drive if default drive is unplugged.
- Keep all of this as compatible with SD and FS and LittleFS as possible.
- Play wave files from any logical drive. Cannot use the same device if playing a wave file from it. Other devices can be accessed as it is non-blocking. Two files from FrankB's Teensy-WavePlayer https://github.com/FrankBoesing/Teensy-WavePlayer were modified for use with diskIO and are in the src directory. 'play_wav.cpp' and 'play_wav.h'.

Examples:
- DiskIOTesting.ino: A simple test of some Diskio functions.
- DiskIOMB.ino: A simple cli for testing most diskIO functions and demonstrating unified access to different types of Mass Storage devices on the Teensy. (SdFat, UsbFat and LittleFS). Being able to add commands so easly is really helpfull for debugging.
- Hot plugging now supports unplugging the default device and switching to the next available device. This is not recommended if the device is in use.
- DiskIOMB is a partialy modified version of microBox for testing found here:
http://sebastian-duell.de/en/microbox/index.html

In DiskIOMB type help at the command line to see the commands that were modified and commands that were added for use with Teensy.

So far most of this is working well but still needs a lot more work. Really don't know if this is something that might be useful but it is fun to play with:)
