# GoveeBTTempLogger
Govee H5074 and H5075 Bluetooth Low Energy Temperature and Humidity Logger

Uses libbluetooth functionality from BlueZ on linux to open the default Bluetooth device and listen for low energy advertisments from Govee H5074 and H5075 thermometers. 

Each of these devices currently cost less than $15 on Amazon and use BLE for communication, so don't require setting up a manufacterer account to track the data.  

GoveeBTTempLogger was initially built using Microsoft Visual Studio 2017, targeting ARM processor running on Linux. I'm using a Raspberry Pi 4 as my linux host. I've verified the same code works on a Raspbery Pi ZeroW and a Raspberry Pi 3b.

GoveeBTTempLogger creates a log file for each of the devices it receives broadcasted data from using a simple tab seperated format that's compatible with loading in Microsoft Excel. Each line in the log file has Date, Temperature, relative humidity, and battery percent. The log file naming format includes the unique Govee device name, the current year, and month. A new log file is created monthly.

## Verbosity has been significantly changed since the intial release.

 * -v 1 prints all advertisments that have been decoded from Govee H5075 and H5074 thermometers to stdout.
 * -v 2 prints all advertisments recieved and categorized
 * levels higher than 2 print way too much debugging information, but can be interesting to look at.

## Prerequisites

### Linux

 * Kernel version 3.6 or above
 * ```libbluetooth-dev```

#### Ubuntu/Debian/Raspbian

```sh
sudo apt-get install bluetooth bluez libbluetooth-dev
```
