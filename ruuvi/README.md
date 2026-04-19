# Ruuvi Tag Information
I purchased a RuuviTag Pro Sensor and have been working with it because my Victron gateway will recognize and record data from it. 
They are significantly more expensive than the Govee tags and are well documented. The Ruuvi Tag is a popular bluetooth temperature and humidity sensor. It has a different bluetooth protocol than the Govee devices. 
I have added code to decode the Ruuvi Tag data and write it to the log files in a similar format to the Govee devices. 

https://ruuvi.com/

## RuuviTag Pro Sensor – (4in1) temperature, humidity, pressure, motion
![image](tuote-ruuvitag-pro-4in1-en-300x300.webp)

## RuuviTag Pro Sensor – (3in1) temperature, humidity, motion
![image](tuote-ruuvitag-pro-3in1-en-300x300.webp)

## RuuviTag Pro Sensor – (2in1) temperature, motion. Waterproof.
![image](tuote-ruuvitag-pro-2in1-en-300x300.webp)

## Data format 5 (RAWv2)
https://docs.ruuvi.com/communication/bluetooth-advertisements/data-format-5-rawv2

The data is decoded from "Manufacturer Specific Data" -field, for more details please check Bluetooth Advertisements section. 
Manufacturer ID is `0x0499`, which is transmitted as `0x9904` in raw data. The actual data payload is:

| Offset | Allowed values | Description |
| --- | --- | --- |
| 0 | 5 | Data format (8bit) |
| 1-2 | -32767 ... 32767 | Temperature in 0.005 degrees |
| 3-4 | 0 ... 40 000 | Humidity (16bit unsigned) in 0.0025% (0-163.83% range, though realistically 0-100%) |
| 5-6 | 0 ... 65534 | Pressure (16bit unsigned) in 1 Pa units, with offset of -50 000 Pa |
| 7-8 | -32767 ... 32767 | Acceleration-X (Most Significant Byte first) |
| 9-10 | -32767 ... 32767 | Acceleration-Y (Most Significant Byte first) |
| 11-12 | -32767 ... 32767 | Acceleration-Z (Most Significant Byte first) |
| 13-14 | 0 ... 2046, 0 ... 30 | Power info (11+5bit unsigned), first 11 bits is the battery voltage above 1.6V, in millivolts (1.6V to 3.646V range). Last 5 bits unsigned are the TX power above -40dBm, in 2dBm steps. (-40dBm to +20dBm range) |
| 15 | 0 ... 254 | Movement counter (8 bit unsigned), incremented by motion detection interrupts from accelerometer |
| 16-17 | 0 ... 65534 | Measurement sequence number (16 bit unsigned), each time a measurement is taken, this is incremented by one, used for measurement de-duplication. Depending on the transmit interval, multiple packets with the same measurements can be sent, and there may be measurements that never were sent. |
| 18-23 | Any valid mac | 48bit MAC address. |

Not available is signified by largest presentable number for unsigned values, smallest presentable number for signed values 
and all bits set for mac. All fields are MSB first 2-complement, i.e. `0xFC18` is read as `-1000` and `0x03E8` is read as `1000`. 
If original data overflows the data format, data is clipped to closests value that can be represented. For example 
temperature 170.00 C becomes 163.835 C and acceleration -40.000 G becomes -32.767 G.

## Data field descriptions
### Temperature
Values supported: (-163.835 °C to +163.835 °C in 0.005 °C increments.

Example

| Value | Measurement |
| --- | --- |
| 0x0000 | 0 °C |
| 0x01C3 | +2.255 °C |
| 0xFE3D | -2.255 °C |
| 0x8000 | Invalid / not available |


### Humidity
Values supported: 0.0 % to 100 % in 0.0025 % increments. Higher values than 100 % are possible, but they generally indicate a faulty or miscalibrated sensor.

Example

| Value | Measurement |
| --- | --- |
| 0 | 0% |
| 10010 | 25.025% |
| 40000 | 100.0% |
| 65535 | Invalid / not available |

### Atmospheric Pressure
Values supported: 50000 Pa to 115536 Pa in 1 Pa increments.

Example

| Value | Measurement |
| --- | --- |
| 00000 | 50000 Pa |
| 51325 | 101325 Pa (average sea-level pressure) |
| 65534 | 115534 Pa |
| 65535 | Invalid / not available |

### Acceleration
Values supported: -32767 to 32767 (mG), however the sensor on RuuviTag supports only 16 G max (2 G in default configuration). Values are 2-complement int16_t, MSB first. All channels are identical.

Example

| Value | Measurement |
| --- | --- |
| 0xFC18 | -1000 mG |
| 0x03E8 | 1000 mG |
| 0x8000 | Invalid / not available |

### Battery voltage
Values supported: 1600 mV to 3647 mV in 1 mV increments, practically 1800 ... 3600 mV.

Example

| Value | Measurement |
| --- | --- |
| 0000 | 1600 mV |
| 1400 | 3000 mV |
| 2047 | Invalid / not available |


### Tx Power
Values supported: -40 dBm to +22 dBm in 2 dBm increments.

Example

| Value | Measurement |
| --- | --- |
| 00 | -40 dBm |
| 22 | +4 dBm |
| 31 | Invalid / not available |


### Movement counter
Movement counter is one-byte counter which gets triggered when LIS2DH12 gives "activity interrupt". Sensitivity depends on the firmware, by default the sensitivity is a movement over 64 mG. The counter will roll over. Movement is deduced by "rate of change". Please note that the highest valid value is 254, and 255 is reserved for the "not available".

Example

| Value | Measurement |
| --- | --- |
| 00 | 0 counts |
| 100 | 100 counts |
| 255 | Invalid / not available |

### Measurement sequence number
Measurement sequence number gets incremented by one for every measurement. It can be used to gauge signal quality and packet loss as well as to deduplicate data entries. You should note that the measurement sequence refers to data rather than transmission, so you might receive many transmissions with the same measurement sequence number. Please note that the highest valid value is 65534, and 65535 is reserved for the "not available".

Example

| Value | Measurement |
| --- | --- |
| 0000 | 0 counts |
| 1000 | 1000 counts |
| 65535 | Invalid / not available |

## HCI Example output
```
wim@WimPi5:~ $ ../visualstudio/projects/GoveeBTTempLogger/bin/ARM64/Debug/GoveeBTTempLogger.out --HCI --controller 2C:CF:67:0B:78:71 --verbose 3 --only DD:4C:E8:7A:11:6E
[2026-04-17T12:05:35] GoveeBTTempLogger Version (non-CMake) Built on: Apr 17 2026 at 12:05:17
[                   ]  verbose: 3
[                   ]      log: ""
[                   ]    cache: ""
[                   ]      svg: ""
[                   ]  battery: 0
[                   ]   minmax: 0
[                   ]  celsius: false
[                   ] titlemap: ""
[                   ]     time: 60
[                   ]  average: 5
[                   ] download: 0 (days betwen data download)
[                   ]  passive: false
[                   ] no-bluetooth: false
[                   ]      HCI: true
[                   ] only listening to: [DD:4C:E8:7A:11:6E]
[2026-04-17T12:05:35] /dev/rfkill 0 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 2 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 3 Wireless LAN, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 11 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 0 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 2 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 3 Wireless LAN, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:05:35] /dev/rfkill 11 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[                   ] Host Controller Address: 04:7F:0E:00:FD:5C BlueTooth Device ID: 0 HCI Name: hci0
[                   ] Host Controller Address: F4:4E:FC:A0:F7:DA BlueTooth Device ID: 1 HCI Name: hci1
[                   ] Host Controller Address: 2C:CF:67:0B:78:71 BlueTooth Device ID: 2 HCI Name: hci2
[                   ] BlueToothDevice_ID: 2
[                   ] Reset device: hci2. Success(0)
[                   ] DOWN device: hci2. Success(0)
[                   ] UP device: hci2. Success(0)
[2026-04-17T12:05:35] Using Controller Address: 2C:CF:67:0B:78:71
[2026-04-17T12:05:35] LocalName: WimPi5 #3
[2026-04-17T12:05:35] BlueTooth Address Filter: [DD:4C:E8:7A:11:6E]
[2026-04-17T12:05:35] Scanning Stopped.
[2026-04-17T12:05:35] BlueTooth Address Filter: [DD:4C:E8:7A:11:6E]
[2026-04-17T12:05:35] Scanning Started. ScanInterval(11.25 msec) ScanWindow(11.25 msec) ScanType(1)
[2026-04-17T12:05:36] 46 [DD:4C:E8:7A:11:6E] (Flags) 06 (Manu) 0499:051C012806CE230390FE34FFD0C0765AF615DD4CE87A116E (Temp) 35.8°C (Humidity) 25.62% (Pressure) 1027.71 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.912 g (AccelerationY) -0.460 g (AccelerationZ) -0.048 g (MovementCounter) 90 (MeasurementSequenceNumber) 62997 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:05:45] 46 [DD:4C:E8:7A:11:6E] (Flags) 06 (Manu) 0499:051C0227FBCE20038CFE38FFC8C0765AF618DD4CE87A116E (Temp) 35.9°C (Humidity) 25.59% (Pressure) 1027.68 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.908 g (AccelerationY) -0.456 g (AccelerationZ) -0.056 g (MovementCounter) 90 (MeasurementSequenceNumber) 63000 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:05:47] 46 [DD:4C:E8:7A:11:6E] (Flags) 06 (Manu) 0499:051C0227F0CE20038CFE34FFCCC0765AF619DD4CE87A116E (Temp) 35.9°C (Humidity) 25.56% (Pressure) 1027.68 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.908 g (AccelerationY) -0.460 g (AccelerationZ) -0.052 g (MovementCounter) 90 (MeasurementSequenceNumber) 63001 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:05:52] 46 [DD:4C:E8:7A:11:6E] (Flags) 06 (Manu) 0499:051C0227EECE200388FE38FFC0C0765AF61BDD4CE87A116E (Temp) 35.9°C (Humidity) 25.55% (Pressure) 1027.68 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.904 g (AccelerationY) -0.456 g (AccelerationZ) -0.064 g (MovementCounter) 90 (MeasurementSequenceNumber) 63003 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
^C***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************
[2026-04-17T12:05:56] Scanning Stopped.
GoveeBTTempLogger Version (non-CMake) Built on: Apr 17 2026 at 12:05:17 (exiting)
```

## dBus Example output
```
wim@WimPi5:~ $ ../visualstudio/projects/GoveeBTTempLogger/bin/ARM64/Debug/GoveeBTTempLogger.out --controller 2C:CF:67:0B:78:71 --verbose 3 --only DD:4C:E8:7A:11:6E
[2026-04-17T12:17:42] GoveeBTTempLogger Version (non-CMake) Built on: Apr 17 2026 at 12:05:17
[                   ]  verbose: 3
[                   ]      log: ""
[                   ]    cache: ""
[                   ]      svg: ""
[                   ]  battery: 0
[                   ]   minmax: 0
[                   ]  celsius: false
[                   ] titlemap: ""
[                   ]     time: 60
[                   ]  average: 5
[                   ] download: 0 (days betwen data download)
[                   ]  passive: false
[                   ] no-bluetooth: false
[                   ]      HCI: false
[                   ] only listening to: [DD:4C:E8:7A:11:6E]
[2026-04-17T12:17:42] /dev/rfkill 0 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 2 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 3 Wireless LAN, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 11 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 0 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 2 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 3 Wireless LAN, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] /dev/rfkill 11 Bluetooth, Blocked: NO (Soft: no, Hard: no)
[2026-04-17T12:17:42] D-Bus connection ":1.834" Opened
[                   ] bluez_find_adapters ":1.834"
[                   ] /: org.freedesktop.DBus.ObjectManager: GetManagedObjects
[                   ]   Message Type: method_return
[                   ]      Signature: a{oa{sa{sv}}}
[                   ]    Destination: :1.834
[                   ]         Sender: :1.2
[                   ]        Object Path: /org/bluez
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.AgentManager1
[                   ]             String: org.bluez.ProfileManager1
[                   ]             String: org.bluez.HealthManager1
[                   ]        Object Path: /org/bluez/hci1
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Adapter1
[                   ]                Address: F4:4E:FC:A0:F7:DA
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]             String: org.bluez.BatteryProviderManager1
[                   ]             String: org.bluez.GattManager1
[                   ]             String: org.bluez.Media1
[                   ]             String: org.bluez.NetworkServer1
[                   ]             String: org.bluez.SimAccess1
[                   ]             String: org.bluez.LEAdvertisingManager1
[                   ]        Object Path: /org/bluez/hci1/dev_70_6F_C8_23_FA_5B
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_FD_B6_2F_B9_DF_0D
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_C0_FB_7E_F8_BF_5D
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_C4_E8_05_0B_8B_30
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E0_AA_D9_8E_F9_49
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_C0_0E_D3_45_4A_63
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_61_2D_2D_20_C5_09
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_04_44_66_F5_62_0A
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_DC_CC_3D
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E3_5E_CC_21_5C_0F
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E3_8E_C8_C1_98_9A
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_0D_42_7B
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_05_C7_A1
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E3_60_59_21_80_65
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_D5_A3_3B
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_DD_4C_E8_7A_11_6E
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_37_BC_AE
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_A4_C1_38_0D_3B_10
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_D0_35_33_33_44_03
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_D3_21_C4_06_25_0D
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_50_04_49_74_9E_B2
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_C2_35_33_30_25_50
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_D2_AF_09_4A_F0_4E
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_D3_D1_90_54_EB_F0
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_CE_A5_D7_7B_CD_81
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_C3_36_35_30_61_77
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E3_60_59_23_14_7D
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_E3_9C_25_49_FC_68
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci1/dev_0B_A0_89_1A_42_A7
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Device1
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]        Object Path: /org/bluez/hci2
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Adapter1
[                   ]                Address: 2C:CF:67:0B:78:71
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]             String: org.bluez.BatteryProviderManager1
[                   ]             String: org.bluez.GattManager1
[                   ]             String: org.bluez.Media1
[                   ]             String: org.bluez.NetworkServer1
[                   ]             String: org.bluez.SimAccess1
[                   ]             String: org.bluez.LEAdvertisingManager1
[                   ]        Object Path: /org/bluez/hci0
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.Adapter1
[                   ]                Address: 04:7F:0E:00:FD:5C
[                   ]             String: org.freedesktop.DBus.Properties
[                   ]             String: org.bluez.BatteryProviderManager1
[                   ]             String: org.bluez.GattManager1
[                   ]             String: org.bluez.Media1
[                   ]             String: org.bluez.NetworkServer1
[                   ]             String: org.bluez.SimAccess1
[                   ]             String: org.bluez.LEAdvertisingManager1
[                   ]        Object Path: /org/bluez/test
[                   ]             String: org.freedesktop.DBus.Introspectable
[                   ]             String: org.bluez.SimAccessTest1
[                   ] Host Controller Address: 04:7F:0E:00:FD:5C BlueZ Adapter Path: /org/bluez/hci0
[                   ] Host Controller Address: 2C:CF:67:0B:78:71 BlueZ Adapter Path: /org/bluez/hci2
[                   ] Host Controller Address: F4:4E:FC:A0:F7:DA BlueZ Adapter Path: /org/bluez/hci1
[                   ] /org/bluez/hci2: org.freedesktop.DBus.Properties: SetPowered: true
[                   ] /org/bluez/hci2: org.bluez.Adapter1: SetDiscoveryFilter
[                   ] /: org.freedesktop.DBus.ObjectManager: GetManagedObjects
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_70_6F_C8_23_FA_5B
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_FD_B6_2F_B9_DF_0D
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_C0_FB_7E_F8_BF_5D
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_C4_E8_05_0B_8B_30
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E0_AA_D9_8E_F9_49
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_C0_0E_D3_45_4A_63
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_61_2D_2D_20_C5_09
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_04_44_66_F5_62_0A
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_DC_CC_3D
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E3_5E_CC_21_5C_0F
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E3_8E_C8_C1_98_9A
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_0D_42_7B
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_05_C7_A1
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E3_60_59_21_80_65
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_D5_A3_3B
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_DD_4C_E8_7A_11_6E
[2026-04-17T12:17:42] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c872747ce17038cfe34ffd0c0765af72edd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.59 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.908 g (AccelerationY) -0.460 g (AccelerationZ) -0.048 g (MovementCounter) 90 (MeasurementSequenceNumber) 63278 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_37_BC_AE
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_A4_C1_38_0D_3B_10
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_D0_35_33_33_44_03
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_D3_21_C4_06_25_0D
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_50_04_49_74_9E_B2
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_C2_35_33_30_25_50
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_D2_AF_09_4A_F0_4E
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_D3_D1_90_54_EB_F0
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_CE_A5_D7_7B_CD_81
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_C3_36_35_30_61_77
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E3_60_59_23_14_7D
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_E3_9C_25_49_FC_68
[2026-04-17T12:17:42]        Object Path: /org/bluez/hci1/dev_0B_A0_89_1A_42_A7
[                   ] /org/bluez/hci2: org.bluez.Adapter1: StartDiscovery
[                   ] Add Match Rule: "type='signal',sender='org.bluez',member='InterfacesAdded'"
[                   ] Add Match Rule: "type='signal',sender='org.bluez',member='PropertiesChanged'"
[2026-04-17T12:17:42] 60 seconds or more have passed. Writing LOG Files
[2026-04-17T12:17:44] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:44] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:44] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:44] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:44] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:45] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:45] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:45] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:45] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:45] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8a2748ce150380fe38ffc4c0765af730dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.57 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.896 g (AccelerationY) -0.456 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63280 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8c2746ce1b0388fe34ffc4c0765af731dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.63 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.904 g (AccelerationY) -0.460 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63281 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-04-17T12:17:46] [DD:4C:E8:7A:11:6E] ManufacturerData: 0499:051c8c2746ce1b0388fe34ffc4c0765af731dd4ce87a116e (Temp) 36.5°C (Humidity) 25.14% (Pressure) 1027.63 hPa (Battery) 3.139 V (TXPower)   4 dBm (AccelerationX)  0.904 g (AccelerationY) -0.460 g (AccelerationZ) -0.060 g (MovementCounter) 90 (MeasurementSequenceNumber) 63281 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
^C***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************
[                   ] Remove Match Rule: "type='signal',sender='org.bluez',member='InterfacesAdded'"
[                   ] Remove Match Rule: "type='signal',sender='org.bluez',member='PropertiesChanged'"
[                   ] /org/bluez/hci2: org.bluez.Adapter1: StopDiscovery
[2026-04-17T12:17:47] D-Bus connection ":1.834" Closed
GoveeBTTempLogger Version (non-CMake) Built on: Apr 17 2026 at 12:05:17 (exiting)
```