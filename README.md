# GoveeBTTempLogger
Govee H5074, H5075, H5174, and H5177 Bluetooth Low Energy Temperature and Humidity Logger, and Govee H5181, H5182 and H5183 Smart Meat Thermometers

Uses libbluetooth functionality from BlueZ on linux to open the default Bluetooth device and listen for low energy advertisments from Govee H5074, H5075, H5174, H5177, H5181, H5182, H5183 thermometers.

Each of these devices currently cost less than $15 on Amazon and use BLE for communication, so don't require setting up a manufacterer account to track the data.  

GoveeBTTempLogger was initially built using Microsoft Visual Studio 2017, targeting ARM processor running on Linux. I'm using a Raspberry Pi 4 as my linux host. I've verified the same code works on a Raspbery Pi ZeroW and a Raspberry Pi 3b.

GoveeBTTempLogger creates a log file, if specified by the -l or --log option, for each of the devices it receives broadcasted data from using a simple tab-separated format that's compatible with loading in Microsoft Excel. Each line in the log file has Date, Temperature, relative humidity, and battery percent. The log file naming format includes the unique Govee device name, the current year, and month. A new log file is created monthly.

### Minor update 2022-12-17
Added the option --index to create an html index file based on the existing log files.

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

#### Ubuntu/Debian/Raspbian

Note: be sure to clone this repository before running below commands: git clone https://github.com/wcbonner/GoveeBTTempLogger.git

```sh
sudo apt-get install bluetooth bluez libbluetooth-dev
make deb
sudo make install-deb
```

This will install a systemd unit `goveebttemplogger.service` which will automatically start GoveeBTTempLogger. The service can be configured using environment variables via
the `systemctl edit goveebttemplogger.service` command. By default, it writes logs to `/var/log/goveebttemplogger` and writes SVG files to `/var/www/html/goveebttemplogger`.

The following environment variables control the service:

* `VERBOSITY` controlls the verbosity level; default: `0`
* `LOGDIR` directory the TSV files are written to; default: `/var/log/goveebttemplogger`
* `TIME` Sets the frequency data is written to the logs; default: `60`
* `SVGARGS` controlls options for writing SVG files; default: `--svg /var/www/html/goveebttemplogger/ --battery 8 --minmax 8`
* `EXTRAARGS` can be used to pass extra arguments; default is unset (empty)

As an example, to disable SVG files, increase verbosity, and change the directory the TSV files are written to, use 
`sudo systemctl edit goveebttemplogger.service` and enter the following file in the editor:

```
[Service]
Environment="VERBOSITY=1"
Environment="LOGDIR=/opt/govee/data"
Environment="SVGARGS="
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
 * -d (--download) download the 20 days historical data from each device. This is still very much a work in progress.
 * -s (--svg) SVG output directory. Writes four SVG files per device to this directory every 5 minutes that can be used in standard web page. 
 * -i (--index) HTML index file for SVG files, must be paired with log directory. HTML file is a fully qualified name. This is meant as a one time run option just to create a simple index of all the SVG files.
 * -T (--titlemap) SVG-title fully-qualified-filename. A mapfile with bluetooth addresses as the beginning of each line, and a replacement title to be used in the SVG graph.
 * -c (--celsius) SVG output using degrees C
 * -b (--battery) Draw the battery status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly
 * -x (--minmax) Draw the minimum and maximum temperature and humidity status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly

 ## Log File Format

 The log file format has been stable for a long time as a simple tab-separated text file with a set number of columns: Date (UTC), Temperature (C), Humidity, Battery.

 With the addition of support for the meat thermometers multiple temperature readings, I've changed the format slightly in a way that should be backwards compatible with most programs reading existing logs. After the existing columns of Date, Temperature, Humidity, Battery I've added optional columns of Model, Temperature, Temperature, Temperature

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

## BTData directory contains a Data Dump
The file btsnoop_hci.log is a Bluetooth hci snoop log from a Google Nexus 7 device running Android and the Govee Home App. It can be loaded directly in Wireshark.

```sh
sudo apt install -y wireshark-qt
```
 
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

### Wireshark filter that limits to visible packets sent or recieved from a single H5075 device
bluetooth.src || bluetooth.dst == a4:c1:38:37:bc:ae

bluetooth.src == a4:c1:38:37:bc:ae || bluetooth.dst == a4:c1:38:37:bc:ae || bluetooth.src_str == "controller" || bluetooth.src_str == "host"

#### My H5074 (Outside)
bluetooth.src == e3:5e:cc:21:5c:0f || bluetooth.dst == e3:5e:cc:21:5c:0f || bluetooth.src_str == "controller" || bluetooth.src_str == "host"

#### My H5174 (3 AA Batteries)
bluetooth.src == A4:C1:38:DC:CC:3D || bluetooth.dst == A4:C1:38:DC:CC:3D

# What I've learned from decoding H5074 2023-02-15
 * open and connect lcap socket
 * send struct { uint8_t opcode; uint16_t starting_handle; uint16_t ending_handle; uint16_t UUID; } primary_service_declaration_1 = {BT_ATT_OP_READ_BY_GRP_TYPE_REQ,  * send: 218	31.238678	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0001..0xffff
 * recieve: 221	31.413696	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	29	Rcvd Read By Group Type Response, Attribute List Length: 3, Generic Access Profile, Generic Attribute Profile, Device Information
 * because packet can't be bigger than 32 bytes, we only recived 3 handles, the biggest grout end handle is 0x0016, so we now need to modify our initial request to start at 0x0017
 * send: 222	31.431977	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0017..0xffff
 * recieve: 224	31.510590	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	17	Rcvd Read By Group Type Response, Attribute List Length: 1, Dialog Semiconductor GmbH
 * the group end handle is 0x002a so we send another request starting at 0x002b
 * send: 225	31.520508	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x002b..0xffff
 * recieve: 227	31.657318	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	31	Rcvd Read By Group Type Response, Attribute List Length: 1, Unknown
 * which has a group end handle of 0x003b
 * send: 229	31.679199	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x003c..0xffff
 * recieve: 231	31.720642	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	14	Rcvd Error Response - Attribute Not Found, Handle: 0xffff (Unknown: Unknown)
 * and that was our first error response. So now we are going to move on to reading by type
0x0001, 0xffff, 0x2800 };
```
[2023-02-16T03:06:59] GoveeBTTempLogger Version 2.20230215-1 Built on: Feb 15 2023 at 16:49:54
[2023-02-16T03:06:59] LocalName: WimPi4-Dev
[2023-02-16T03:06:59] BlueTooth Address Filter: [E3:5E:CC:21:5C:0F]
[2023-02-16T03:06:59] Scanning...
[2023-02-16T03:07:24] 44 [E3:5E:CC:21:5C:0F] (Flags) 06 (UUID) 0A18F5FE88EC (Name) Govee_H5074_5C0F
[2023-02-16T03:07:24] Scanning Stopped
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] hci_le_create_conn Return(0) handle (0040)
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] Connected L2CAP LE connection on ATT channel: 4
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0001..0xFFFF
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] <== Handles: 0x0001..0x0005 UUID: 1800
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] <== Handles: 0x0006..0x0009 UUID: 1801
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] <== Handles: 0x000A..0x0016 UUID: 180A
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0017..0xFFFF
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] <== Handles: 0x0017..0x002A UUID: FEF5
[2023-02-16T03:07:26] [E3:5E:CC:21:5C:0F] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x002B..0xFFFF
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== Handles: 0x002B..0x003B UUID: 57485f53-4b43-4f52-5f49-4c4c45544e49
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x003C..0xFFFF
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_GRP_TYPE_REQ GATT_PRIM_SVC_UUID BT_ATT_OP_ERROR_RSP
```
This gets a list of services, each defined by a UUID and a pair of handles.
Next, for each service, we attempt to get details using the pair of handles. For each service, we try GATT Include Declaration and then GATT Characteristic Declaration, looping on the results as needed.
 * send struct { uint8_t opcode; uint16_t starting_handle; uint16_t ending_handle; uint16_t UUID; } gatt_include_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, 0x0001, 0x0005, 0x2802 };
 * send: 232	31.730683	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Type Request, GATT Include Declaration, Handles: 0x0001..0x0005
 * I'm not sure why the maximum handle was only 0x0005 on that one, but we got an error, so will move on to the next query
 * recieve: 234	31.750763	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	14	Rcvd Error Response - Attribute Not Found, Handle: 0x0006 (Generic Attribute Profile)
 * send struct { uint8_t opcode; uint16_t starting_handle; uint16_t ending_handle; uint16_t UUID; } gatt_characteristic_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, 0x0001, 0x0005, 0x2803 };
 * send: 235	31.763550	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Type Request, GATT Characteristic Declaration, Handles: 0x0001..0x0005
 * recieve: 237	31.782593	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	25	Rcvd Read By Type Response, Attribute List Length: 2, Device Name, Appearance
 * once again, maximum handle returned was 0x0005 sp we'll try again starting with 0x005.
 * send: 238	31.803650	ASUSTekC_30:4e:ef (Nexus 7)	e3:5e:cc:21:5c:0f ()	ATT	16	Sent Read By Type Request, GATT Characteristic Declaration, Handles: 0x0005..0x0005
 * recieve: 240	31.825531	e3:5e:cc:21:5c:0f ()	ASUSTekC_30:4e:ef (Nexus 7)	ATT	14	Rcvd Error Response - Attribute Not Found, Handle: 0x0006 (Generic Attribute Profile)
```
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Include Declaration, Handles: 0x0001..0x0005
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0001..0x0005
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== Handles: 0x0002..0x0003 Characteristic Properties: 0x02 UUID: 2A00
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== Handles: 0x0004..0x0005 Characteristic Properties: 0x02 UUID: 2A01
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0005..0x0005
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:27] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Include Declaration, Handles: 0x0006..0x0009
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0006..0x0009
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== Handles: 0x0007..0x0008 Characteristic Properties: 0x22 UUID: 2A05
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0008..0x0009
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Include Declaration, Handles: 0x000A..0x0016
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x000A..0x0016
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== Handles: 0x000B..0x000C Characteristic Properties: 0x02 UUID: 2A29
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== Handles: 0x000D..0x000E Characteristic Properties: 0x02 UUID: 2A24
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] <== Handles: 0x000F..0x0010 Characteristic Properties: 0x02 UUID: 2A26
[2023-02-16T03:07:28] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0010..0x0016
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== Handles: 0x0011..0x0012 Characteristic Properties: 0x02 UUID: 2A28
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== Handles: 0x0013..0x0014 Characteristic Properties: 0x02 UUID: 2A23
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== Handles: 0x0015..0x0016 Characteristic Properties: 0x02 UUID: 2A50
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0016..0x0016
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Include Declaration, Handles: 0x0017..0x002A
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0017..0x002A
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] <== Handles: 0x0018..0x0019 Characteristic Properties: 0x0A UUID: 34cc54b9-f956-c691-2140-a641a8ca8280
[2023-02-16T03:07:29] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0019..0x002A
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] <== Handles: 0x001A..0x001B Characteristic Properties: 0x0A UUID: 5186f05a-3442-0488-5f4b-c35ef0494272
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x001B..0x002A
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] <== Handles: 0x001C..0x001D Characteristic Properties: 0x02 UUID: d44f33fb-927c-22a0-fe45-a14725db536c
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x001D..0x002A
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] <== Handles: 0x001E..0x001F Characteristic Properties: 0x0A UUID: 31da3f67-5b85-8391-d849-0c00a3b9849d
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x001F..0x002A
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] <== Handles: 0x0020..0x0021 Characteristic Properties: 0x0E UUID: b29c7bb1-d057-1691-a14c-16d5e8717845
[2023-02-16T03:07:30] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0021..0x002A
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== Handles: 0x0022..0x0023 Characteristic Properties: 0x12 UUID: 885c066a-ebb3-0a99-f546-8c7994df785f
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0023..0x002A
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== Handles: 0x0025..0x0026 Characteristic Properties: 0x02 UUID: 3a913bdb-c8ac-1da2-1b40-e50db5e8b464
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0026..0x002A
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== Handles: 0x0027..0x0028 Characteristic Properties: 0x02 UUID: 3bfb6752-878f-5484-9c4d-be77dddfc342
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0028..0x002A
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== Handles: 0x0029..0x002A Characteristic Properties: 0x02 UUID: 3ce2fc3d-90c4-afa3-bb43-3d82ea1edeb7
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x002A..0x002A
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Include Declaration, Handles: 0x002B..0x003B
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2023-02-16T03:07:31] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x002B..0x003B
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] <== Handles: 0x002C..0x002D Characteristic Properties: 0x1A UUID: 12205f53-4b43-4f52-5f49-4c4c45544e49
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x002D..0x003B
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] <== Handles: 0x0030..0x0031 Characteristic Properties: 0x12 UUID: 13205f53-4b43-4f52-5f49-4c4c45544e49
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0031..0x003B
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] <== Handles: 0x0034..0x0035 Characteristic Properties: 0x1A UUID: 11205f53-4b43-4f52-5f49-4c4c45544e49
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0035..0x003B
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] <== Handles: 0x0038..0x0039 Characteristic Properties: 0x1A UUID: 14205f53-4b43-4f52-5f49-4c4c45544e49
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0039..0x003B
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
```
Now I've cycled through all of the Characteristics Declarations and Properties. After this, I am sending some find information request commands that I've directly copied from the govee app trace in wireshark. I don't know where the handles are coming from.

I'm also writing a value to an unknown handle, in an attempt to get data returned from the device. It's not working.
```
[2023-02-16T03:07:32] [E3:5E:CC:21:5C:0F] ==> Find Information Request, Handles: 0x002E..0x002F
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x002E UUID: 2902
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x002F UUID: 2901
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] ==> Find Information Request, Handles: 0x0032..0x0033
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x0032 UUID: 2902
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x0033 UUID: 2901
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] ==> Find Information Request, Handles: 0x0036..0x0037
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x0036 UUID: 2902
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x0037 UUID: 2901
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] ==> Find Information Request, Handles: 0x003A..0x003B
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x003A UUID: 2902
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] <== Handle: 0x003B UUID: 2901
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] BT_ATT_OP_WRITE_REQ Handle: 002D Value: 3301278100020000000000000000000000000096
[2023-02-16T03:07:33] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 32
[2023-02-16T03:07:45] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 31
[2023-02-16T03:07:45] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 30
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2F
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2E
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2D
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2C
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2B
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2A
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 29
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 28
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 27
[2023-02-16T03:07:46] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 26
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 25
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 24
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 23
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 22
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 21
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 20
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1F
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1E
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1D
[2023-02-16T03:07:47] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1C
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1B
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1A
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 19
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 18
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 17
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 16
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 15
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 14
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 13
[2023-02-16T03:07:48] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 12
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 11
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 10
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = F
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = E
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = D
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = C
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = B
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = A
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 9
[2023-02-16T03:07:49] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 8
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 7
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 6
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 5
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 4
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 3
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 2
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 1
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Reading from device. RetryCount = 0
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] Closing l2cap_socket
[2023-02-16T03:07:50] [E3:5E:CC:21:5C:0F] [E3:5E:CC:21:5C:0F] hci_disconnect
[2023-02-16T03:07:50] Scanning...
GoveeBTTempLogger Version 2.20230215-1 Built on: Feb 15 2023 at 16:49:54 (exiting)
```
 * UUID's are really important in the handshaking. [Service UUID: 494e54454c4c495f524f434b535f4857] and [UUID: 494e54454c4c495f524f434b535f2013] are associated with all of the data packets returned on Handle: 0x0031 that appear to be the historical data.

The Following two frames are the response that gets the UUID
```
Frame 225: 16 bytes on wire (128 bits), 16 bytes captured (128 bits)
Bluetooth
    [Source: ASUSTekC_30:4e:ef (d8:50:e6:30:4e:ef)]
    [Destination: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)]
Bluetooth HCI H4
Bluetooth HCI ACL Packet
Bluetooth L2CAP Protocol
    Length: 7
    CID: Attribute Protocol (0x0004)
Bluetooth Attribute Protocol
    Opcode: Read By Group Type Request (0x10)
        0... .... = Authentication Signature: False
        .0.. .... = Command: False
        ..01 0000 = Method: Read By Group Type Request (0x10)
    Starting Handle: 0x002b
    Ending Handle: 0xffff
    UUID: GATT Primary Service Declaration (0x2800)
    0000   02 02 00 0b 00 07 00 04 00 10 2b 00 ff ff 00 28   ..........+....(

  Frame 227: 31 bytes on wire (248 bits), 31 bytes captured (248 bits)
Bluetooth
    [Source: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)]
    [Destination: ASUSTekC_30:4e:ef (d8:50:e6:30:4e:ef)]
Bluetooth HCI H4
Bluetooth HCI ACL Packet
Bluetooth L2CAP Protocol
    Length: 22
    CID: Attribute Protocol (0x0004)
Bluetooth Attribute Protocol
    Opcode: Read By Group Type Response (0x11)
        0... .... = Authentication Signature: False
        .0.. .... = Command: False
        ..01 0001 = Method: Read By Group Type Response (0x11)
    Length: 20
    Attribute Data, Handle: 0x002b, Group End Handle: 0x003b, UUID128: Unknown
        Handle: 0x002b (Unknown)
            [UUID: 494e54454c4c495f524f434b535f4857]
        Group End Handle: 0x003b
        UUID: 57485f534b434f525f494c4c45544e49
    [UUID: GATT Primary Service Declaration (0x2800)]
    [Request in Frame: 225]
0000   02 02 20 1a 00 16 00 04 00 11 14 2b 00 3b 00 57   .. ........+.;.W
0010   48 5f 53 4b 43 4f 52 5f 49 4c 4c 45 54 4e 49      H_SKCOR_ILLETNI
```

Then a lot of frames later I find the other UUID 

```
Frame 307: 16 bytes on wire (128 bits), 16 bytes captured (128 bits)
Bluetooth
    [Source: ASUSTekC_30:4e:ef (d8:50:e6:30:4e:ef)]
    [Destination: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)]
Bluetooth HCI H4
Bluetooth HCI ACL Packet
Bluetooth L2CAP Protocol
    Length: 7
    CID: Attribute Protocol (0x0004)
Bluetooth Attribute Protocol
    Opcode: Read By Type Request (0x08)
        0... .... = Authentication Signature: False
        .0.. .... = Command: False
        ..00 1000 = Method: Read By Type Request (0x08)
    Starting Handle: 0x002d
    Ending Handle: 0x003b
    UUID: GATT Characteristic Declaration (0x2803)
0000   02 02 00 0b 00 07 00 04 00 08 2d 00 3b 00 03 28   ..........-.;..(

Frame 309: 32 bytes on wire (256 bits), 32 bytes captured (256 bits)
Bluetooth
    [Source: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)]
    [Destination: ASUSTekC_30:4e:ef (d8:50:e6:30:4e:ef)]
Bluetooth HCI H4
Bluetooth HCI ACL Packet
Bluetooth L2CAP Protocol
    Length: 23
    CID: Attribute Protocol (0x0004)
Bluetooth Attribute Protocol
    Opcode: Read By Type Response (0x09)
        0... .... = Authentication Signature: False
        .0.. .... = Command: False
        ..00 1001 = Method: Read By Type Response (0x09)
    Length: 21
    Attribute Data, Handle: 0x0030, Characteristic Handle: 0x0031, UUID128: Unknown
        Handle: 0x0030 (Unknown: Unknown: GATT Characteristic Declaration)
            [Service UUID: 494e54454c4c495f524f434b535f4857]
            [Characteristic UUID: 494e54454c4c495f524f434b535f2012]
            [UUID: GATT Characteristic Declaration (0x2803)]
        Characteristic Properties: 0x12, Notify, Read
        Characteristic Value Handle: 0x0031 (Unknown: Unknown)
            [Service UUID: 494e54454c4c495f524f434b535f4857]
            [UUID: 494e54454c4c495f524f434b535f2013]
        UUID: 13205f534b434f525f494c4c45544e49
    [UUID: GATT Characteristic Declaration (0x2803)]
    [Request in Frame: 307]
0000   02 02 20 1b 00 17 00 04 00 09 15 30 00 12 31 00   .. ........0..1.
0010   13 20 5f 53 4b 43 4f 52 5f 49 4c 4c 45 54 4e 49   . _SKCOR_ILLETNI
```
