# GoveeBTTempLogger
Govee H5074 and H5075 Bluetooth Low Energy Temperature and Humidity Logger

Uses libbluetooth functionality from BlueZ on linux to open the default Bluetooth device and listen for low energy advertisments from Govee H5074 and H5075 thermometers. 

Each of these devices currently cost less than $15 on Amazon and use BLE for communication, so don't require setting up a manufacterer account to track the data.  

GoveeBTTempLogger was initially built using Microsoft Visual Studio 2017, targeting ARM processor running on Linux. I'm using a Raspberry Pi 4 as my linux host. I've verified the same code works on a Raspbery Pi ZeroW and a Raspberry Pi 3b.

GoveeBTTempLogger creates a log file for each of the devices it receives broadcasted data from using a simple tab seperated format that's compatible with loading in Microsoft Excel. Each line in the log file has Date, Temperature, relative humidity, and battery percent. The log file naming format includes the unique Govee device name, the current year, and month. A new log file is created monthly.

## Verbosity has been significantly changed since the intial release.

 * -v 0 no output to stdout. Errors still sent to stderr.
 * -v 1 prints all advertisments that have been decoded from Govee H5075 and H5074 thermometers to stdout.
 * -v 2 prints all advertisments recieved and categorized
 * -v levels higher than 2 print way too much debugging information, but can be interesting to look at.

## Prerequisites

### Linux

 * Kernel version 3.6 or above
 * ```libbluetooth-dev```

#### Ubuntu/Debian/Raspbian

```sh
sudo apt-get install bluetooth bluez libbluetooth-dev
make deb
sudo apt-get install ./GoveeBTTempLogger.deb
```

## Command Line Options
 * -h (--help) Prints supported options and exits.
 * -l (--log) Sets the log directory
 * -t (--time) Sets the frequency data is written to the logs
 * -v (--verbose) Sets output verbosity.
 * -m (--mrtg) Takes a bluetooth address as parameter, returns data for that particular address in the format MRTG can interpret.
 * -a (--average) Affects MRTG output. The parameter is a number of minutes. 0 simply returns the last value in the log file. Any number more than zero will average the entries over that number of minutes. If no entries were logged in that time period, no results are returned. MRTG graphing is then determined by the setting of the unknaszero option in the MRTG.conf file.

## BTData directory contains a Data Dump
The file btsnoop_hci.log is a Bluetooth hci snoop log from a Google Nexus 7 device running Android and the Govee Home App. It can be loaded directly in Wireshark.
 
In frames 260, 261, 313, 320, 11126, 11402, and 11403 you can see advertisements from my H5074 device. (e3:5e:cc:21:5c:0f)

Using the Govee Home App, I add a connection to Govee_H5074_5C0F and download it's historical data.
In frames 5718, 5719, 5728, and 11450 you can see advertisements from one of my H5075 devices. (a4:c1:38:37:bc:ae)
Interesting frames start around 543 in response to [UUID: 494e54454c4c495f524f434b535f2013].

Two sequential values are:
 * 708003611c0365010365000368ea0368ea0368ec
 * 707a0368eb036cd3036cd1036cd3036cd3036cd0

Looking at the data I believe that the first two bytes are an offset into the total data, and then there are six repeating three byte datasets. 
7080 03611c 036501 036500 0368ea 0368ea 0368ec

Using the same math that decodes the BT LE Advertisements gets very reasonable values. (71.86424, 46.8) (72.0437, 46.5) (72.04352, 46.4) (72.22388, 46.6) (72.22388, 46.6) (72.22424, 46.8)

If I zoom all the way to frame 5414, it appears to bt the last response to [UUID: 494e54454c4c495f524f434b535f2013] and has a value of 
0002 030c6b 030c70 ffffffffffffffffffffffff

Using the Govee Home App, I add a connection to GVH5075_BCAE and download it's historical data. 

The frames received from the thermometer start to look especially interesting around 5924 when they are returning consistent length data (20 bytes) similar to 7080031322031325031324031325031326031328 in response to [UUID: 494e54454c4c495f524f434b535f2013]
 * 7080031322031325031324031325031326031328
 * 707a031327031329031329031329031329031329

 7080 031322 031325 031324 031325 031326 031328 

 The last frame from [UUID: 494e54454c4c495f524f434b535f2013] (16687) has Value: 000603658f036590036590036590036590036590
 0006 03658f 036590 036590 036590 036590 036590

 Off the top of my head each device is storing 0x7080 time/humidity values. That's 28,800. 20 days * 24 hours * 60 minutes = 28,800 entries.

