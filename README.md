# GoveeBTTempLogger
Govee H5074, H5075, H5100, H5101, H5104, H5105, H5174, H5177, and H5179 Bluetooth Low Energy Temperature and Humidity Logger, and Govee H5181, H5182 and H5183 Smart Meat Thermometers

Each of these devices currently cost less than $15 on Amazon and use BLE for communication, so don't require setting up a manufacterer account to track the data.  

GoveeBTTempLogger was initially built using Microsoft Visual Studio 2017, targeting ARM processor running on Linux. I'm using a Raspberry Pi 4 as my linux host. I've verified the same code works on a Raspbery Pi ZeroW, Raspberry Pi Zero2W, Raspberry Pi 3b, and a Raspberry Pi 5.

GoveeBTTempLogger creates a log file, if specified by the -l or --log option, for each of the devices it receives broadcasted data from using a simple tab-separated format that's compatible with loading in Microsoft Excel. Each line in the log file has Date (recorded in UTC), Temperature, relative humidity, and battery percent. The log file naming format includes the unique Govee device name, the current year, and month. A new log file is created monthly.

### Trixie Release Information 2025-10-07
Raspberry released the update to Trixie this week https://www.raspberrypi.com/news/trixie-the-new-version-of-raspberry-pi-os/ and while GoveeBTTempLogger works on the updated system, the built in bluetooth is blocked on many systems. I had two issues open related to Trixie, https://github.com/wcbonner/GoveeBTTempLogger/issues/89 and https://github.com/wcbonner/GoveeBTTempLogger/issues/91 with the second finding the solution, which is to run the command `rfkill unblock bluetooth` 
```
wim@WimPiZeroW-Sola:~ $ rfkill
ID TYPE      DEVICE      SOFT      HARD
 0 bluetooth hci0     blocked unblocked
 1 wlan      phy0   unblocked unblocked
```

### Minor update 2022-12-17
Added the option --index to create an html index file based on the existing log files. This option creates an index file and exits without running any of the bluetooth code. It can be run without affecting a running instance of the program listening to Bluetooth advertisments. Example command to create index: 

```sh
sudo /usr/local/bin/goveebttemplogger --log /var/log/goveebttemplogger/ --index index.html
```

## Major update to version 3.
Conversion to Bluetooth using BlueZ over DBus!
DBus is the approved method of Bluetooth communication. 
It seems to use more CPU than the pure HCI code.
When I tried building this on a machine running Raspbian GNU/Linux 10 (buster) the system builds but the BlueZ DBus routines to find the bluetooth adapter fail. 
For this reason, I've left the old HCI commands in the code and fallback to running HCI if DBus fails. 

I've added an --HCI option to allow the user to force it to run the HCI commands instead of using the DBus interface.

When running DBus, there is no way to run in passive scanning mode. The --passive option is ignored.

When running HCI mode, the whitelist created with the --only option is sent to the bluetooth hardware and only those devices are sent from the hardware to the software.
In DBus mode whitelisting does not appear to be available.
In DBus mode I'm filtering the output based on the whitelist.

### 2024-10-10 HCI Code in #ifdef sections
The code has been rearranged slightly for clarity, moving all of the HCI access code into #ifdef blocks. 
The CMakeLists.txt file defines \_BLUEZ_HCI\_ to keep the code in the application. 
Removing or commenting out the line add_compile_definitions(_BLUEZ_HCI_) will compile without the Bluetooth HCI libraries.
I should also be able to ignore the files att-types.h, uuid.c, and uuid.h. I'm proficient at CMake to do this yet.

HCI code uses libbluetooth functionality from BlueZ on linux to open the default Bluetooth device and listen for low energy advertisments from Govee thermometers.

### 2024-10-01 Run As goveebttemplogger
Updated the postinst debian install script to add a user goveebttemplogger and make changes to the permissions on the default directories appropriately.
Changed the service file to specify running the program as user goveebttemplogger. This is possible because accessing BlueZ via DBus does not require root access.

### 2025-02-18 Added --download to default service command
The download functionality is now working with the DBus code base as well as being more robust using the HCI code. As such I've added it to the default running configuration. 
It does not attempt to download data more than every two weeks, to reduce battery usage of the bluetooth device.
It retrieves the oldest data first, and if the transfer is stopped midway, will attempt to start from the new "oldest" data downloaded.

## Major update to version 2.
Added the SVG output function, directly creating SVG graphs from internal data in a specified directory. The causes the program to take longer to start up as it will attempt to read all of the old logged data into an internal memory structure as it starts. Once the program has entered the normal running state it writes four SVG files per device to the specified directory every five minutes.

Here is an example filename: gvh-E35ECC215C0F-day.svg

![Image](./gvh-E35ECC215C0F-day.svg)

The most recent temperature and humidity are displayed in the vertical scale on the left. The temperature scale is displayed on the left side of the graph, the humidity scale on the right. The most recent time data is displayed in the top right, with a title on the top left of the graph.

Minimum and maximum temperature and humidity data, at the granularity of the graph, may be displayed. This is most useful in yearly graphs, where the granularity is one day. Here is the corresponding yearly graph for the previous daily graph: gvh-E35ECC215C0F-year.svg

![Image](./gvh-E35ECC215C0F-year.svg)

Humidity, and the humidity scale on the right, are automatically omitted if the current data reports a humidity of zero. The meat thermometer reports its current temperature and its alarm set temperature but no humidity measurement.

A simple text file mapping Bluetooth addresses to titles will be read from the filename gvh-titlemap.txt in the svg output directory.  Each line in the file should consist of the bluetooth address (in hexadecimal format with (`:`) between octets), whitespace, and the title. See [gvh-titlemap.txt](./gvh-titlemap.txt) for an example. If no title mapping exists, the Bluetooth address is used for the graph title.

If the --svg option is not added to the command line, the program should continue to operate exactly the same as it did before.

## Verbosity has been significantly changed since the intial release.

 * -v 0 no output to stdout. Errors still sent to stderr.
 * -v 1 prints all advertisments that have been decoded from Govee H5075, H5074, H5174, and H5177 thermometers to stdout.
 * -v 2 prints all advertisments recieved and categorized
 * -v levels higher than 2 print way too much debugging information, but can be interesting to look at.

## Prerequisites

### Linux

 * Kernel version 3.6 or above
 * ```libbluetooth-dev```
 * ```libdbus-1-dev```
 
#### Ubuntu/Debian/Raspbian

##### Build Process changed from using make with a makefile to cmake 2023-09-18
This seems to better build the debian package with the correct installed size, dependencies, and md5sums details. I'm still learning CMake so there may be regular updates for a while.

```sh
sudo apt install build-essential cmake git libbluetooth-dev libdbus-1-dev
git clone https://github.com/wcbonner/GoveeBTTempLogger.git
cmake -S GoveeBTTempLogger -B GoveeBTTempLogger/build
cmake --build GoveeBTTempLogger/build
pushd GoveeBTTempLogger/build && cpack . && popd
```

The install package will creates a systemd unit `goveebttemplogger.service` which will automatically start GoveeBTTempLogger. The service can be configured via
the `systemctl edit goveebttemplogger.service` command. By default, it writes logs to `/var/log/goveebttemplogger` and writes SVG files to `/var/www/html/goveebttemplogger`.


The [postinst](https://github.com/wcbonner/GoveeBTTempLogger/blob/master/postinst) install routine creates a user and three directories.
It also will change the permissions on those dirctories to be owned by and writable by the newly created user.
```
adduser --system --ingroup www-data goveebttemplogger
mkdir --verbose --mode 0755 --parents /var/log/goveebttemplogger /var/cache/goveebttemplogger /var/www/html/goveebttemplogger
chown --changes --recursive goveebttemplogger:www-data /var/log/goveebttemplogger /var/cache/goveebttemplogger /var/www/html/goveebttemplogger
chmod --changes --recursive 0644 /var/log/goveebttemplogger/* /var/cache/goveebttemplogger/* /var/www/html/goveebttemplogger/*
sudo setcap 'cap_net_raw,cap_net_admin+eip' /usr/local/bin/goveebttemplogger
rfkill --output-all
rfkill unblock bluetooth
```

##### Debian 13 (Trixie) 2025-12-08
The Raspberry Pi seems to configure the built in bluetooth support with rfkill to be soft blocked. The two rfkill commands were
added to the [postinst](https://github.com/wcbonner/GoveeBTTempLogger/blob/master/postinst) file to display the initial settings
and then unblock bluetooth. This is the `rfkill --output-all` output on a fresh installation.
```
ID TYPE      DEVICE TYPE-DESC         SOFT      HARD
 0 bluetooth hci0   Bluetooth      blocked unblocked
 1 wlan      phy0   Wireless LAN unblocked unblocked
```

The systemd unit file section `ExecStart` to start the service has been broken into several lines for clarity.

```
[Service]
Type=simple
Restart=always
RestartSec=30
User=goveebttemplogger
Group=www-data
ExecStart=/usr/local/bin/goveebttemplogger \
    --verbose 0 \
    --log /var/log/goveebttemplogger \
    --time 60 \
    --download 7 \
    --restart 3 \
    --svg /var/www/html/goveebttemplogger --battery 8 --minmax 8 \
    --cache /var/cache/goveebttemplogger
KillSignal=SIGINT
```

As an example, to disable SVG files, increase verbosity, and change the directory the log files are written to, use 
`sudo systemctl edit --full goveebttemplogger.service` and enter the following file in the editor:
```
[Service]
Type=simple
Restart=always
RestartSec=5
ExecStartPre=/bin/mkdir -p /var/log/gvh
ExecStart=/usr/local/bin/goveebttemplogger \
    --verbose 1 \
    --log /var/log/gvh \
    --time 60 \
    --download
KillSignal=SIGINT
```

Then use `sudo systemctl restart goveebttemplogger` to restart GoveeBTTempLogger.

#### Windows Subsystem for Linux (WSL) Cross Compile requirements (Debian)

The first two commands below set up the required environment for Visual Studio 2022 to build the project. The third command added the required libraries to build bluetooth projects.

```sh
sudo apt-get update
sudo apt install g++ gdb make ninja-build rsync zip -y
sudo apt install bluetooth bluez libbluetooth-dev -y
```

## Command Line Options
 * -h (--help) Prints supported options and exits.
 * -l (--log) Sets the log directory. If not specified, no logs are written.
 * -t (--time) Sets the frequency data is written to the logs. The time frequency of the data in the logs is based on bluetooth announcement time. This is how often the logfile is written.
 * -v (--verbose) Sets output verbosity.
 * -m (--mrtg) Takes a bluetooth address as parameter, returns data for that particular address in the format MRTG can interpret.
 * -o (--only) Takes a bluetooth address as parameter and only reports on that address.
 * -C (--controller) Takes a bluetooth address as parameter to specify the controller to listen with.
 * -a (--average) Affects MRTG output. The parameter is a number of minutes. 0 simply returns the last value in the log file. Any number more than zero will average the entries over that number of minutes. If no entries were logged in that time period, no results are returned. MRTG graphing is then determined by the setting of the unknaszero option in the MRTG.conf file.
 * -s (--svg) SVG output directory. Writes four SVG files per device to this directory every 5 minutes that can be used in standard web page. 
 * -i (--index) HTML index file for SVG files, must be paired with log directory. HTML file is a fully qualified name. This is meant as a one time run option just to create a simple index of all the SVG files. The program will exit after creating the index file.
 * -T (--titlemap) SVG-title fully-qualified-filename. A mapfile with bluetooth addresses as the beginning of each line, and a replacement title to be used in the SVG graph.
 * -c (--celsius) SVG output using degrees C
 * -b (--battery) Draw the battery status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly
 * -x (--minmax) Draw the minimum and maximum temperature and humidity status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly
 * -d (--download) Sets the number of days between attempts to connect and download stored data
 * -n (--no-bluetooth) Monitor Logging Directory and process logs without Bluetooth Scanning
 * -M (--monitor) Monitor Logged Data for updated data
 * -R (--restart) Maximum minutes without bluetooth advertisments before attempting to restart
 * -H (--HCI) Prefer deprecated BlueZ HCI interface over modern DBus communication
 * -p (--passive) Bluetooth LE Passive Scanning

 ## Log File Format

 The log file format has been stable for a long time as a simple tab-separated text file with a set number of columns: Date (UTC), Temperature (C), Humidity, Battery.

 With the addition of support for the meat thermometers multiple temperature readings, I've changed the format slightly in a way that should be backwards compatible with most programs reading existing logs. After the existing columns of Date, Temperature, Humidity, Battery I've added optional columns of Model, Temperature, Temperature, Temperature
 
 ### Minor update 2023-04-03
 I changed the default log filename to start with `gvh-` instead of `gvh507x_`. The code will still read the old log files, and will rename the current months log file to the new format. I used the linux shell command `for f in gvh507x_*.txt; do sudo mv "${f}" "${f//gvh507x_/gvh-}"; done` in the log file directory to rename all of the old files to the new format on my machine.

 ## Bluetooth UUID details
  * (UUID) 88EC (Name) Govee_H5074_C7A1
  * (Name) GVH5075_AE36 (UUID) 88EC
  * (Name) GVH5174_CC3D (UUID) 88EC
  * (Name) GVH5177_3B10 (UUID) 88EC
  * (UUID) 5182
  * (UUID) 5183

The 5074, 5075, 5174, and 5177 units all broadcast a UUID of 88EC. Unfortunately, the 5074 does not include the UUID in the same advertisment as the temperatures. 

The H5181, 5182 and 5183 units broadcast UUID of 5182 and 5183 respectivly in each of their broadcast messages including the temperatures.
```
(Flags) 06 (UUID) 5182 (Manu) 3013270100010164018007D0FFFF860708FFFF (Temp) 20°C (Temp) -0.01°C (Temp) 18°C (Temp) -0.01°C (Battery) 0%
(UUID) 5183 (Flags) 05 (Manu) 5DA1B401000101E40186076C2F660000 (Temp) 19°C (Temp) 121.34°C (Battery) 0% (Other: 00)  (Other: 00)  (Other: 00)  (Other: 00)  (Other: 00)  (Other: CB)
```

## Download Details
It has taken me a long time to get around to downloading data from the devices directly instead of purely listening for advertisments. The direct download method is nice because it can retrieve data accumulated while the listener was offline. 

#### 2024-01-14 H5105 support
It appears that the H5105 broadcast data is automatically recognized as a Govee thermometer and data stored, but downloading is not working. The H5105 has a pairing button on the top of the device. I've noticed that the H5100 device does not seem to be downloading historical data either. They may use the same protocol, different from the older thermometers.

##### 2024-02-03 H5100 and H5105 Device Support
I've made a couple of hacks to get the two devices I have at my location to work. The H5100 device I have has a bluetooth address that starts with a C, the H5105 device starts with a D. The realy big issue is that to communicate with them, the protocol must declare that it's talking with LE_RANDOM_ADDRESS as opposed to LE_PUBLIC_ADDRESS that the other devices I've used require. This also comes into play if a bluetooth filter is configured to only listen to certain devices. I don't understand this setup as according to what I've read, if the most significant bits of the 48 bit bluetooth address are set to one, that defines the address as RANDOM, which would mean that a C, D, E, or F in the leading digit of the address should all require RANDOM. 

I have run into problems recognizing advertisments. This has led to too much time experimenting with the method of scanning for bluetooth advertisments, primarily with the settings of ScanWindow and ScanInterval, but also with the difference between active scanning and passive scanning. 
### Passive Scanning
In this mode that program is doing excactly what you'd expect, listening for advertisments.
### Active Scanning
In this mode the bluetooth stack itself will attempt to connect to devices it recives advertisments from and retrieve more information. 
### Scan Window vs Scan Interval
For the longest time I've had fixed values set in my code for Scan Window and Scan Interval quickly being set to bt_ScanInterval(0x0012) bt_ScanWindow(0x0012) followed by bt_ScanInterval(0x1f40) bt_ScanWindow(0x1f40).
The values are in increments of 0.625 msec. The First value was 11.25 msec, and the second value was  (5000 msec).
Doing a bunch of reading I came across recommendations to use 40 msec and 30 msec, so I've tried bt_ScanInterval(64) and bt_ScanWindow(48). When it's set this way, I seem to get advertisments, but I'm not able to connect and download.

## --only ff:ff:ff:ff:ff:ff Hack
I included a hack some time ago related to the bluetooth filtering to easily filter on devices that have already been logged. If a filter is specified with all the bits set, the program will submit a filter of known addresses to the stack when scanning is started. This disables new device discovery, but may improve performance in some situations.

Connections on bluetooth devices are all based on handles and UUIDs. There are some defined UUIDs that every bluetooth device is required to support, and then there are custom UUIDs. This listing came from a GVH5177. 
```
[-------------------] Service Handles: 0x0001..0x0007 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 Properties: 0x12 UUID: 2a00 (Device Name)
[                   ] Characteristic Handles: 0x0004..0x0005 Properties: 0x02 UUID: 2a01 (Appearance)
[                   ] Characteristic Handles: 0x0006..0x0007 Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[-------------------] Service Handles: 0x0008..0x000b UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x0009..0x000a Properties: 0x20 UUID: 2a05 (Service Changed)
[-------------------] Service Handles: 0x000c..0x000e UUID: 180a (Device Information)
[                   ] Characteristic Handles: 0x000d..0x000e Properties: 0x02 UUID: 2a50 (PnP ID)
[-------------------] Service Handles: 0x000f..0x001b UUID: 494e5445-4c4c-495f-524f-434b535f4857
[                   ] Characteristic Handles: 0x0010..0x0011 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2011
[                   ] Characteristic Handles: 0x0014..0x0015 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2012
[                   ] Characteristic Handles: 0x0018..0x0019 Properties: 0x12 UUID: 494e5445-4c4c-495f-524f-434b535f2013
[-------------------] Service Handles: 0x001c..0x001f UUID: 00010203-0405-0607-0809-0a0b0c0d1912
[                   ] Characteristic Handles: 0x001d..0x001e Properties: 0x06 UUID: 00010203-0405-0607-0809-0a0b0c0d2b12
```
##### 2025-02-18 128 bit UUID output
My old code didn't properly display the UUID when it was a 128 bit UUID, I standardized on the proper output and have corrected this readme with updated respresentation. Thanks to [govee-h5075-thermo-hygrometer](https://github.com/Heckie75/govee-h5075-thermo-hygrometer/blob/main/API.md) I understand the three primary UUIDs much better.

**494e5445-4c4c-495f-524f-434b535f4857** is the custom 128 bit UUID that all of the Govee thermometers seem to use for their primary service. If printed as an ascii string, it looks like this text **INTELLI_ROCKS_HW**.

**494e5445-4c4c-495f-524f-434b535f2011** is the 128 bit UUID used for command and responses. There commands are well documented at the previous link.

**494e5445-4c4c-495f-524f-434b535f2012** is the 128 bit UUID of the service characteristic that I write to request download of data. It looks like the primary UUID except that the first two bytes are different. **INTELLI_ROCKS_**.

**494e5445-4c4c-495f-524f-434b535f2013** is the 128 bit UUID that will return requested historical data.

Most of the devices hold 20 days of history. The GVH5177 devices hold a month of data. In my output below, the GVH5105 and GVH5179 devices have had their batteries replaced within the last month and I was not able to determine the historical data size.

```
Download from device: [E3:5E:CC:21:5C:0F] 2025-02-10 20:53:00 2025-03-02 20:57:00 (28804) (GVH5074)
Download from device: [E3:60:59:21:80:65] 2025-02-10 20:48:00 2025-03-02 20:50:00 (28802) (GVH5074)
Download from device: [A4:C1:38:D5:A3:3B] 2025-02-10 20:55:00 2025-03-02 20:57:00 (28802) (GVH5074)
Download from device: [A4:C1:38:05:C7:A1] 2025-02-10 21:03:00 2025-03-02 21:05:00 (28802) (GVH5074)
Download from device: [E3:8E:C8:C1:98:9A] 2025-02-10 20:34:00 2025-03-02 20:39:00 (28805) (GVH5074)
Download from device: [E3:60:59:23:14:7D] 2025-02-10 20:36:00 2025-03-02 20:40:00 (28804) (GVH5074)
Download from device: [A4:C1:38:37:BC:AE] 2025-02-10 21:03:00 2025-03-02 21:06:00 (28803) (GVH5075)
Download from device: [A4:C1:38:0D:42:7B] 2025-02-10 20:49:00 2025-03-02 20:49:00 (28800) (GVH5075)
Download from device: [C2:35:33:30:25:50] 2025-02-10 20:55:00 2025-03-02 20:55:00 (28800) (GVH5100)
Download from device: [C3:36:35:30:61:77] 2025-02-10 20:51:00 2025-03-02 20:51:00 (28800) (GVH5104)
Download from device: [D0:35:33:33:44:03] 2025-02-19 16:13:00 2025-03-02 21:05:00 (16132) (GVH5105)
Download from device: [A4:C1:38:DC:CC:3D] 2025-02-10 21:02:00 2025-03-02 21:06:00 (28804) (GVH5174)
Download from device: [A4:C1:38:0D:3B:10] 2025-01-31 20:55:00 2025-03-02 20:56:00 (43201) (GVH5177)
Download from device: [D3:21:C4:06:25:0D] 2025-02-15 14:51:00 2025-03-02 20:44:00 (21953) (GVH5179)
```

```
[2025-02-18T09:17:35] 46 [C2:35:33:30:25:50] (Flags) 06 (Name) GVH5100_2550 (UUID) ec88 (Manu) 0001:010102B1903D (Temp) 17.7°C (Humidity)  52.8% (Battery)  61% (GVH5100)
[-------------------] Service Handles: 0x0001..0x0009 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 Properties: 0x0a UUID: 2a00 (Device Name)
[                   ] Characteristic Handles: 0x0004..0x0005 Properties: 0x0a UUID: 2a01 (Appearance)
[                   ] Characteristic Handles: 0x0006..0x0007 Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[                   ] Characteristic Handles: 0x0008..0x0009 Properties: 0x02 UUID: 2ac9
[-------------------] Service Handles: 0x000a..0x000d UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x000b..0x000c Properties: 0x22 UUID: 2a05 (Service Changed)
[-------------------] Service Handles: 0x000e..0x001a UUID: 494e5445-4c4c-495f-524f-434b535f4857
[                   ] Characteristic Handles: 0x000f..0x0010 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2011
[                   ] Characteristic Handles: 0x0013..0x0014 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2012
[                   ] Characteristic Handles: 0x0017..0x0018 Properties: 0x12 UUID: 494e5445-4c4c-495f-524f-434b535f2013
[-------------------] Service Handles: 0x001b..0x0025 UUID: 02f00000-0000-0000-0000-00000000fe00
[                   ] Characteristic Handles: 0x001c..0x001d Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff03
[                   ] Characteristic Handles: 0x001e..0x001f Properties: 0x12 UUID: 02f00000-0000-0000-0000-00000000ff02
[                   ] Characteristic Handles: 0x0022..0x0023 Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff00
[                   ] Characteristic Handles: 0x0024..0x0025 Properties: 0x0c UUID: 02f00000-0000-0000-0000-00000000ff01
[2025-02-18T09:18:45] [C2:35:33:30:25:50] Download from device: [C2:35:33:30:25:50] 2025-01-29 09:17:00 2025-02-18 09:17:00 (28800) (GVH5100)

[2024-02-04T04:00:25] 46 [D0:35:33:33:44:03] (Flags) 06 (Name) GVH5105_4403 (UUID) 88EC (Manu) 0100010102868262 (Temp) 16.5506°C (Humidity) 50.6% (Battery) 98% (GVH5105)
[-------------------] Service Handles: 0x0001..0x0009 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 Properties: 0x0a UUID: 2a00 (Device Name)
[                   ] Characteristic Handles: 0x0004..0x0005 Properties: 0x0a UUID: 2a01 (Appearance)
[                   ] Characteristic Handles: 0x0006..0x0007 Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[                   ] Characteristic Handles: 0x0008..0x0009 Properties: 0x02 UUID: 2ac9
[-------------------] Service Handles: 0x000a..0x000d UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x000b..0x000c Properties: 0x22 UUID: 2a05 (Service Changed)
[-------------------] Service Handles: 0x000e..0x001a UUID: 494e5445-4c4c-495f-524f-434b535f4857
[                   ] Characteristic Handles: 0x000f..0x0010 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2011
[                   ] Characteristic Handles: 0x0013..0x0014 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2012
[                   ] Characteristic Handles: 0x0017..0x0018 Properties: 0x12 UUID: 494e5445-4c4c-495f-524f-434b535f2013
[-------------------] Service Handles: 0x001b..0x0025 UUID: 02f00000-0000-0000-0000-00000000fe00
[                   ] Characteristic Handles: 0x001c..0x001d Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff03
[                   ] Characteristic Handles: 0x001e..0x001f Properties: 0x12 UUID: 02f00000-0000-0000-0000-00000000ff02
[                   ] Characteristic Handles: 0x0022..0x0023 Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff00
[                   ] Characteristic Handles: 0x0024..0x0025 Properties: 0x0c UUID: 02f00000-0000-0000-0000-00000000ff01
[2024-02-04T04:01:31] [D0:35:33:33:44:03] Download from device. 2024-01-14 20:00:00 2024-02-03 20:00:00 (28800)

[2025-02-18T09:28:42] 45 [D3:21:C4:06:25:0D] (Flags) 06 (Name) GV5179_250D (UUID) ec88 (Manu) 0001:010102A1F064 (Temp) 17.3°C (Humidity)  52.8% (Battery) 100% (GVH5179)
[-------------------] Service Handles: 0x0001..0x0009 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 Properties: 0x0a UUID: 2a00 (Device Name)
[                   ] Characteristic Handles: 0x0004..0x0005 Properties: 0x0a UUID: 2a01 (Appearance)
[                   ] Characteristic Handles: 0x0006..0x0007 Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[                   ] Characteristic Handles: 0x0008..0x0009 Properties: 0x02 UUID: 2ac9
[-------------------] Service Handles: 0x000a..0x000d UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x000b..0x000c Properties: 0x22 UUID: 2a05 (Service Changed)
[-------------------] Service Handles: 0x000e..0x001a UUID: 494e5445-4c4c-495f-524f-434b535f4857
[                   ] Characteristic Handles: 0x000f..0x0010 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2011
[                   ] Characteristic Handles: 0x0013..0x0014 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2012
[                   ] Characteristic Handles: 0x0017..0x0018 Properties: 0x12 UUID: 494e5445-4c4c-495f-524f-434b535f2013
[-------------------] Service Handles: 0x001b..0x0025 UUID: 02f00000-0000-0000-0000-00000000fe00
[                   ] Characteristic Handles: 0x001c..0x001d Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff03
[                   ] Characteristic Handles: 0x001e..0x001f Properties: 0x12 UUID: 02f00000-0000-0000-0000-00000000ff02
[                   ] Characteristic Handles: 0x0022..0x0023 Properties: 0x02 UUID: 02f00000-0000-0000-0000-00000000ff00
[                   ] Characteristic Handles: 0x0024..0x0025 Properties: 0x0c UUID: 02f00000-0000-0000-0000-00000000ff01
[2025-02-18T09:28:58] [D3:21:C4:06:25:0D] Download from device: [D3:21:C4:06:25:0D] 2025-02-15 14:47:00 2025-02-18 09:28:00 (4001) (GVH5179)

[2025-02-18T09:52:43] 46 [A4:C1:38:0D:42:7B] (Name) GVH5075_427B (UUID) ec88 (Flags) 05 (Manu) EC88:0002542F5100 (Temp) 15.2°C (Humidity)  62.3% (Battery)  81% (GVH5075)
[-------------------] Service Handles: 0x0001..0x0007 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 Properties: 0x12 UUID: 2a00 (Device Name)
[                   ] Characteristic Handles: 0x0004..0x0005 Properties: 0x02 UUID: 2a01 (Appearance)
[                   ] Characteristic Handles: 0x0006..0x0007 Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[-------------------] Service Handles: 0x0008..0x000b UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x0009..0x000a Properties: 0x20 UUID: 2a05 (Service Changed)
[-------------------] Service Handles: 0x000c..0x000e UUID: 180a (Device Information)
[                   ] Characteristic Handles: 0x000d..0x000e Properties: 0x02 UUID: 2a50 (PnP ID)
[-------------------] Service Handles: 0x000f..0x001b UUID: 494e5445-4c4c-495f-524f-434b535f4857
[                   ] Characteristic Handles: 0x0010..0x0011 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2011
[                   ] Characteristic Handles: 0x0014..0x0015 Properties: 0x1a UUID: 494e5445-4c4c-495f-524f-434b535f2012
[                   ] Characteristic Handles: 0x0018..0x0019 Properties: 0x12 UUID: 494e5445-4c4c-495f-524f-434b535f2013
[-------------------] Service Handles: 0x001c..0x001f UUID: 00010203-0405-0607-0809-0a0b0c0d1912
[                   ] Characteristic Handles: 0x001d..0x001e Properties: 0x06 UUID: 00010203-0405-0607-0809-0a0b0c0d2b12
[2025-02-18T09:53:10] [A4:C1:38:0D:42:7B] Download from device: [A4:C1:38:0D:42:7B] 2025-01-29 09:51:00 2025-02-18 09:52:00 (28801) (GVH5075)
```

## BTData directory contains Data Dumps
The file btsnoop_hci.log is a Bluetooth hci snoop log from a Google Nexus 7 device running Android and the Govee Home App.
