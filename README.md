# DiskIO
This is a repository that uses and tests UsbMscFat-FS_DATES (KurtE's branch) on the T3.6/T4.0/T4.1. MicroMod has not beeen tested yet.

Required libraries:

UsbMscFat
https://github.com/wwatson4506/UsbMscFat

FrankB's T4_PowerButton
https://github.com/FrankBoesing/T4_PowerButton (T4.0/T4.1 freeMem functions)

Built using Arduino 1.8.15 and Teensyduino 1.54 release version.

The main goal is to be able to unify all of the different access methods of USBFat, SdFat and LittleFs into one. LittleFS has been a bit of a challenge but is working. So far just QPINAND has beeen tested. I want to add the rest of the LittleFS devices later. 

This is work in progress and is strictly experimentation and/or proof of concept. 

The objectives are:

- Support up to 4 USB Mass Storage devices, the native SDIO SD card and a SPI SD card and LittleFS devices.
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
