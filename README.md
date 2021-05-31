# DiskIO
This is a repository that uses and tests UsbMscFat-FS_DATES (KurtE's branch) on the T3.6/T4.0/T4.1.

KurtE's UsbMscFat-FS_DATES:
https://github.com/KurtE/UsbMscFat/tree/FS_DATES

The main goal is to be able to unify all of the different access methods to USBFat, SdFat and LittleFs into one. What has not been done yet is LittleFs. That will probably be a challenge.

This is work in progress and is strictly experimentation and/or proof of concept. 

The objectives are:

- Support up to 4 USB Mass Storage device, the native SDIO SD card and a SPI SD card.
- Allow for 4 partitions per Mass Storage device. Total of 24 logical drives.
- Use a volume name for access to each logical drive or use an index number for array of mounted drives.
- Be able to set a default drive (change drive).
- Be able to parse a full path spec including relative path specs and wildcard processing.
- Use a more standard directory listing including time and dates stamps using the Teensy builtin RTC.
- Properly process hot plugging.
- Keep all of this compatible with SD and FS.

So far most of this is working well but still needs a lot more work. Really don't know if this is something that might be useful but it is fun to play with:)
