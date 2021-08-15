# DiskIO
This is a repository that uses and tests UsbMscFat-FS_DATES (KurtE's branch) on the T3.6/T4.0/T4.1 and MM.

KurtE's UsbMscFat-FS_DATES:
https://github.com/KurtE/UsbMscFat/tree/FS_DATES

The main goal is to be able to unify all of the different access methods of USBFat, SdFat and LittleFs into one. LittleFS has been a bit of a challenge but is working. So far just QPINAND has beeen tested. I want to add the rest of the LittleFS devices later. 

This is work in progress and is strictly experimentation and/or proof of concept. 

The objectives are:

- Support up to 4 USB Mass Storage devices, the native SDIO SD card and a SPI SD card and LittleFS devices.
- Allow for 4 partitions per Mass Storage device. Total of 24 logical drives and all types of LittleFS devices.
- Use a volume name for access to each logical drive or use an index number for array of mounted logical drives.
- Be able to set a default drive (change drive).
- Be able to parse a full path spec including drive spec, relative path specs and wildcard processing.
- Use a more standard directory listing including time and dates stamps using the Teensy builtin RTC.
- Properly process hot plugging.
- Keep all of this compatible with SD and FS and LittleFS.

Examples:
- DiskIOTesting.ino: A simple test of some Diskio functions.
- DiskIOMB.ino: A simple cli for testing most diskIO functions and demonstrating unified access to different types of Mass Storage devices on the Teensy. (SdFat, UsbFat and LittleFS).

DiskIOMB is a partialy modified version of microBox found here:
http://sebastian-duell.de/en/microbox/index.html

In DiskIOMB type help at the command line to see the commands I modified and commands I added for use with Teensy.

So far most of this is working well but still needs a lot more work. Really don't know if this is something that might be useful but it is fun to play with:)
