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

## Communicate with Ruuvi sensor devices via Bluetooth
### Bluetooth connection
#### Creating a connection
The Ruuvi advertises itself in connectable, scannable mode in 1 MBit / s PHY. The scan response contains full device name “Ruuvi XXXX” and UUID of Nordic UART Service (NUS).

The central must request for scan responses (active scan) and connect to a device with matching scan response. Advertising payload should be ignored.

The Ruuvi has 3 services: Nordic Buttonless DFU service (DFU), Device Information Service (DIS) and NUS. NUS is used for most of the application communication with the device.

In development versions of firmware there is 120 second delay from starting of the connection in which the central must complete the registration process and register to NUS TX Notfications. If the deadline is not met, tag assumes that there is a software lockup and reboots. In production versions this deadline is 12 seconds.

![image](ruuvi-connection-flow.svg)

## Read logged history - RuuviTag
https://docs.ruuvi.com/communication/bluetooth-connection/nordic-uart-service-nus/log-read
https://github.com/ruuvi/ruuvi.air.ble_nus
### Log read flow
The log read is initiated by central which requests for the type of data and time range of the logs. The log is read with command that has a header destination set to physical quantity being read, source to any and type as a log read. The payload is two 32-bit timestamps, seconds since unix epoch. First timestamp is current time, and second time is the lower bound of log data timestamps. For example if the timestamps are 1567047917 and 1566047917 data from time between 2019-08-13 13:18 and 2019-08-29 03:05 is sent.

The logs are sent in format where header destination is the source of read command, source is the physical quantity and type is log write. Payload is 4 bytes of timestamp and 4 bytes of value. Interpretation of value depends on the type of data, possible types are listed below.

When the log buffer is sent and no more data remains, a special message with the entire payload set to 0xFF is sent. It should be noted that there is no way to send only “missing sections” of data from the middle of the logs, logs are always retrieved to the end of the stored data.

## Example communication - read temperature

| Device | Header | Payload | Description |
| --- | -- | -- | -- |
| Central | 0x30 30 11 | 5D6740ED 5D57FEAD | “To: temperature. From: temperature. Action: read log data. Clock is 2019-08-29 03:05 now, start from 2019-08-13 13:18” |
| Peripheral | 0x30 30 10 | 5D57FEAD 000000098D | “To: temperature. From: temperature. Action: write log data. Temperature at 2019-08-13 13:18 24.45 C“ |
| . | . | . | Log entry |
| . | . | . | Log entry |
| Peripheral | 0x30 30 10 | 0x5D6740ED FFFFF8AC | “To: temperature. From: temperature. Action: write log data. Temperature at 2019-08-29 0305 -18.76 C” |
| Peripheral | 0x30 30 10 | 0xFFFFFFFF FFFFFFFF | “To: temperature. From: temperature. Action: write log data. No more logs” |

## Example communication - read all environmental data

| Device | Header | Payload | Description |
| --- | -- | -- | -- |
| Central | 0x3A 3A 11 | 0x5D6740ED 5D57FEAD | “To: environmental. From: environmental. Action: read log data. Clock is 2019-08-29 03:05 now, start from 2019-08-13 13:18” |
| Peripheral | 0x3A 30 10 | 5D57FEAD 000000098D | “To: environmental. From: temperature. Action: write log data. Temperature at 2019-08-13 13:18 24.45 C“ |
| Peripheral | 0x3A 31 10 | 5D57FEAD 000000098D | “To: environmental. From: humidity. Action: write log data. Humidity at 2019-08-13 13:18 24.45 RH-%“ |
| Peripheral | 0x3A 32 10 | 5D57FEAD 000000098D | “To: environmental. From: pressure. Action: write log data. Pressure at 2019-08-13 13:18 2445 Pa“ |
| . | . | . | Log entry |
| . | . | . | Log entry |
| . | . | . | Log entry |
| Peripheral | 0x3A 3A 10 | 0xFFFFFFFF FFFFFFFF | “To: environmental. From: environmental. Action: write log data. No more logs” |

## Data endpoints

| Endpoint byte | Value | Interpretation |
| --- | --- | --- |
| 0x30 | Temperature | int32_t, 0.01 C per LSB |
| 0x31 | Humidity | uint32_t, 0.01 RH-% per LSB |
| 0x32 | Air pressure | uint32_t, 1 Pa per LSB |
| 0x3A | All environmental values | Special value which can be used to query all environmental data at once. |	

## BLE Scanner RuuviTag top level 
![image](20261117_005235000_iOS.jpg)

## BLE Scanner RuuviTag Service Level
![image](20261117_005301000_iOS.jpg)

## BLE Scanner RuuviTag Characteristic level
![image](20261117_005404000_iOS.png)

## HCI Download Example output
```
[2026-11-18T13:06:28] 46 [DD:4C:E8:7A:11:6E] (Flags) 06 (Manu) 0499:050C674C4AC8230378FE4CFFC4B516454345DD4CE87A116E (Temp) 15.9°C (Humidity) 48.83% (Pressure) 1012.35 hPa (Battery) 72% (TXPower)  4 dBm (AccelerationX)  0.888 g (AccelerationY) -0.436 g (AccelerationZ) -0.060 g (MovementCounter) 69 (MeasurementSequenceNumber) 17221 (BluetoothAddress) DD:4C:E8:7A:11:6E (Ruuvi)
[2026-11-18T13:06:28] Scanning Stopped.
[                   ] BlueZ_HCI_ConnectAndDownload DD:4C:E8:7A:11:6E
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] hci_le_create_conn Return(0) handle (0040)
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] Connected L2CAP LE connection on ATT channel: 4 BDADDR_LE_RANDOM
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0001..0xffff
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] <== Handles: 0x0001..0x0009 UUID: 1800 (Generic Access)
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] <== Handles: 0x000a..0x000d UUID: 1801 (Generic Attribute)
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] <== Handles: 0x000e..0x0016 UUID: 180a (Device Information)
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0017..0xffff
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] <== Handles: 0x0017..0xffff UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
[2026-11-18T13:06:35] [DD:4C:E8:7A:11:6E] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x0000..0xffff
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_GRP_TYPE_REQ GATT_PRIM_SVC_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Include Declaration, Handles: 0x0001..0x0009
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0001..0x0009
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== Handles: 0x0002..0x0003 Characteristic Properties: 0x02 UUID: 2a00 (Device Name)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x09 Characteristic: 0x0003 UUID: 2a00 (Device Name)
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== Handles: 0x0004..0x0005 Characteristic Properties: 0x02 UUID: 2a01 (Appearance)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x09 Characteristic: 0x0005 UUID: 2a01 (Appearance)
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== Handles: 0x0006..0x0007 Characteristic Properties: 0x02 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x09 Characteristic: 0x0007 UUID: 2a04 (Peripheral Preferred Connection Parameters)
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0007..0x0009
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== Handles: 0x0008..0x0009 Characteristic Properties: 0x02 UUID: 2aa6
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x09 Characteristic: 0x0009 UUID: 2aa6
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0009..0x0009
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Include Declaration, Handles: 0x000a..0x000d
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:36] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x000a..0x000d
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== Handles: 0x000b..0x000c Characteristic Properties: 0x20 UUID: 2a05 (Service Changed)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x0d Characteristic: 0x000c UUID: 2a05 (Service Changed)
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x000c..0x000d
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Include Declaration, Handles: 0x000e..0x0016
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x000e..0x0016
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== Handles: 0x000f..0x0010 Characteristic Properties: 0x02 UUID: 2a29 (Manufacturer Name String)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x16 Characteristic: 0x0010 UUID: 2a29 (Manufacturer Name String)
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== Handles: 0x0011..0x0012 Characteristic Properties: 0x02 UUID: 2a24 (Model Number String)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x16 Characteristic: 0x0012 UUID: 2a24 (Model Number String)
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== Handles: 0x0013..0x0014 Characteristic Properties: 0x02 UUID: 2a27
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x16 Characteristic: 0x0014 UUID: 2a27
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0014..0x0016
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== Handles: 0x0015..0x0016 Characteristic Properties: 0x02 UUID: 2a26 (Firmware Revision String)
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0x16 Characteristic: 0x0016 UUID: 2a26 (Firmware Revision String)
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0016..0x0016
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Include Declaration, Handles: 0x0017..0xffff
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP
[2026-11-18T13:06:37] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0017..0xffff
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== Handles: 0x0018..0x0019 Characteristic Properties: 0x0c UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0xffff Characteristic: 0x0019 UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x0019..0xffff
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== Handles: 0x001a..0x001b Characteristic Properties: 0x10 UUID: 6e400003-b5a3-f393-e0a9-e50e24dcca9e
[                   ] [DD:4C:E8:7A:11:6E] <== Service: 0xffff Characteristic: 0x001b UUID: 6e400003-b5a3-f393-e0a9-e50e24dcca9e
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x001b..0xffff
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP
[-------------------] Service Handles: 0x0001..0x0009 UUID: 1800 (Generic Access)
[                   ] Characteristic Handles: 0x0002..0x0003 UUID: 2a00 (Device Name) Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0004..0x0005 UUID: 2a01 (Appearance) Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0006..0x0007 UUID: 2a04 (Peripheral Preferred Connection Parameters) Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0008..0x0009 UUID: 2aa6 Properties: 0x02 (Read)
[-------------------] Service Handles: 0x000a..0x000d UUID: 1801 (Generic Attribute)
[                   ] Characteristic Handles: 0x000b..0x000c UUID: 2a05 (Service Changed) Properties: 0x20 (Indicate)
[-------------------] Service Handles: 0x000e..0x0016 UUID: 180a (Device Information)
[                   ] Characteristic Handles: 0x000f..0x0010 UUID: 2a29 (Manufacturer Name String) Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0011..0x0012 UUID: 2a24 (Model Number String) Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0013..0x0014 UUID: 2a27 Properties: 0x02 (Read)
[                   ] Characteristic Handles: 0x0015..0x0016 UUID: 2a26 (Firmware Revision String) Properties: 0x02 (Read)
[-------------------] Service Handles: 0x0017..0xffff UUID: 6e400001-b5a3-f393-e0a9-e50e24dcca9e
[                   ] Characteristic Handles: 0x0018..0x0019 UUID: 6e400002-b5a3-f393-e0a9-e50e24dcca9e Properties: 0x0c (Write Without Response) (Write)
[                   ] Characteristic Handles: 0x001a..0x001b UUID: 6e400003-b5a3-f393-e0a9-e50e24dcca9e Properties: 0x10 (Notify)
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] ==> BT_ATT_OP_WRITE_REQ Handle: 001c Value: 0100 (EnableNotification)
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_WRITE_RSP
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_RSP Handle: 7552 Value: 75766920496e6e6f766174696f6e73204c74646e
[2026-11-18T13:06:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_RSP Handle: 7552 Value: 7576695461672042766174696f6e73204c74646e
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_RSP Handle: 6843 Value: 65636b2050434242766174696f6e73204c74646e
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_READ_RSP Handle: 7552 Value: 7576692046572076332e33312e312b6465666175
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] ==> BT_ATT_OP_WRITE_REQ      Handle: 0019 Value: 3a3a115dc14c5600000000000000000000000000
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_WRITE_RSP
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbff45a00000fbb312e312b6465666175
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbff45a00018c4a312e312b6465666175
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbff45a000007c9312e312b6465666175
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbff58700000f9a312e312b6465666175
[2026-11-18T13:06:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbff58700018c47312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbff587000007c7312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbff6b300000f99312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbff6b300018c43312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbff6b3000007c4312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbff7e000000f91312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbff7e000018c42312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbff7e0000007bf312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbff90d00000f9b312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbff90d00018c40312e312b6465666175
[2026-11-18T13:06:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbff90d000007bf312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbffa3900000f7c312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbffa3900018c39312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbffa39000007ba312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbffb6600000f84312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbffb6600018c38312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbffb66000007b8312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbffc9300000f70312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbffc9300018c2f312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbffc93000007b4312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbffdc000000f89312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbffdc000018c2f312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbffdc0000007b3312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dbffeec00000f84312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dbffeec00018c2f312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dbffeec000007b1312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0001900000f8d312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0001900018c29312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00019000007b1312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0014600000f82312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0014600018c24312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00146000007ae312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0027200000f9d312e312b6465666175
[2026-11-18T13:06:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0027200018c1f312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00272000007ac312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0039f00000f81312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0039f00018c11312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0039f000007ae312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc004cc00000f8f312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc004cc00018c13312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc004cc000007ad312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc005f800000f57312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc005f800018c0d312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc005f8000007ab312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0072500000f63312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0072500018c0e312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00725000007ab312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0085200000f71312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0085200018c06312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00852000007ab312e312b6465666175
[2026-11-18T13:06:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0097e00000f60312e312b6465666175
[2026-11-18T13:06:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0097e00018c06312e312b6465666175
[2026-11-18T13:06:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0097e000007aa312e312b6465666175
[2026-11-18T13:06:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc00aab00000f7e312e312b6465666175
[2026-11-18T13:06:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc00aab00018c05312e312b6465666175
[2026-11-18T13:06:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00aab000007aa312e312b6465666175
[2026-11-18T13:06:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc00bd800000f74312e312b6465666175
[2026-11-18T13:06:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc00bd800018c06312e312b6465666175
[2026-11-18T13:06:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00bd8000007a9312e312b6465666175
[2026-11-18T13:06:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc00d0400000f65312e312b6465666175
[2026-11-18T13:06:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc00d0400018c09312e312b6465666175
[2026-11-18T13:06:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00d04000007aa312e312b6465666175
[2026-11-18T13:06:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc00e3100000f68312e312b6465666175
[2026-11-18T13:06:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc00e3100018c06312e312b6465666175
[2026-11-18T13:06:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00e31000007a9312e312b6465666175
[2026-11-18T13:06:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc00f5e00000f7a312e312b6465666175
[2026-11-18T13:06:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc00f5e00018c02312e312b6465666175
[2026-11-18T13:06:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc00f5e000007a7312e312b6465666175
[2026-11-18T13:06:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0108b00000f82312e312b6465666175
[2026-11-18T13:06:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0108b00018c06312e312b6465666175
[2026-11-18T13:06:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0108b000007a7312e312b6465666175
[2026-11-18T13:06:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc011b700000f7d312e312b6465666175
[2026-11-18T13:06:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc011b700018c04312e312b6465666175
[2026-11-18T13:06:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc011b7000007a7312e312b6465666175
[2026-11-18T13:06:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc012e400000f83312e312b6465666175
[2026-11-18T13:06:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc012e400018c02312e312b6465666175
[2026-11-18T13:06:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc012e4000007a7312e312b6465666175
^C***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************
[2026-11-18T13:06:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0141100000f93312e312b6465666175
[2026-11-18T13:06:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0141100018c02312e312b6465666175
[2026-11-18T13:06:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01411000007a9312e312b6465666175
[2026-11-18T13:06:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0153d00000f91312e312b6465666175
[2026-11-18T13:06:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0153d00018bf8312e312b6465666175
[2026-11-18T13:06:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0153d000007a7312e312b6465666175
[2026-11-18T13:06:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0166a00000fa1312e312b6465666175
[2026-11-18T13:06:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0166a00018bf9312e312b6465666175
[2026-11-18T13:06:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0166a000007a9312e312b6465666175
[2026-11-18T13:06:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0179700000fb5312e312b6465666175
[2026-11-18T13:06:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0179700018bf6312e312b6465666175
[2026-11-18T13:06:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01797000007a8312e312b6465666175
[2026-11-18T13:06:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc018c300000faa312e312b6465666175
[2026-11-18T13:06:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc018c300018bf5312e312b6465666175
[2026-11-18T13:06:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc018c3000007a9312e312b6465666175
[2026-11-18T13:06:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc019f000000f9a312e312b6465666175
[2026-11-18T13:06:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc019f000018bfb312e312b6465666175
[2026-11-18T13:06:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc019f0000007a9312e312b6465666175
[2026-11-18T13:07:03] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc01b1d00000fa1312e312b6465666175
[2026-11-18T13:07:03] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc01b1d00018bf5312e312b6465666175
[2026-11-18T13:07:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01b1d000007a8312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc01c4900000f95312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc01c4900018bf4312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01c49000007a8312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc01d7600000f9e312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc01d7600018bf5312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01d76000007a8312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc01ea300000fbf312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc01ea300018bee312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01ea3000007a7312e312b6465666175
[2026-11-18T13:07:06] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc01fd000000fb7312e312b6465666175
[2026-11-18T13:07:07] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc01fd000018bee312e312b6465666175
[2026-11-18T13:07:07] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc01fd0000007a9312e312b6465666175
[2026-11-18T13:07:07] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc020fc00000fb1312e312b6465666175
[2026-11-18T13:07:07] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc020fc00018be6312e312b6465666175
[2026-11-18T13:07:08] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc020fc000007a9312e312b6465666175
[2026-11-18T13:07:08] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0222900000fbc312e312b6465666175
[2026-11-18T13:07:09] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0222900018be4312e312b6465666175
[2026-11-18T13:07:09] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02229000007a7312e312b6465666175
[2026-11-18T13:07:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0235600000fc9312e312b6465666175
[2026-11-18T13:07:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0235600018be7312e312b6465666175
[2026-11-18T13:07:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02356000007a6312e312b6465666175
[2026-11-18T13:07:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0248200000fd7312e312b6465666175
[2026-11-18T13:07:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0248200018be7312e312b6465666175
[2026-11-18T13:07:11] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02482000007a5312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc025af00000ff0312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc025af00018beb312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc025af000007a5312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc026dc0000100a312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc026dc00018be7312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc026dc000007a3312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0280800001022312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0280800018be6312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02808000007a3312e312b6465666175
[2026-11-18T13:07:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0293500001034312e312b6465666175
[2026-11-18T13:07:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0293500018bee312e312b6465666175
[2026-11-18T13:07:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02935000007a3312e312b6465666175
[2026-11-18T13:07:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc02a6200001045312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc02a6200018bee312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02a62000007a1312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc02b8e00001050312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc02b8e00018be5312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02b8e000007a0312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc02cbb00001057312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc02cbb00018be7312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02cbb0000079d312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc02de800001066312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc02de800018be3312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02de80000079c312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc02f1500001067312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc02f1500018bed312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc02f150000079b312e312b6465666175
[2026-11-18T13:07:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0304100001064312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0304100018bfc312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0304100000797312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0316e0000106a312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0316e00018bfa312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0316e00000794312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0329b00001077312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0329b00018bf8312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0329b0000078f312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc033c70000107d312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc033c700018c02312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc033c70000078e312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc034f400001085312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc034f400018bfc312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc034f40000078a312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0362100001092312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0362100018bfc312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0362100000786312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0374d0000109e312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0374d00018bf8312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0374d00000782312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0387a000010a8312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0387a00018bf4312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0387a0000077f312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc039a7000010b4312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc039a700018bf9312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc039a70000077a312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc03ad3000010b9312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc03ad300018bfb312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc03ad300000777312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc03c00000010ba312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc03c0000018bfa312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc03c0000000773312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc03d2d000010c8312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc03d2d00018bf5312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc03d2d0000076f312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc03e5a000010cb312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc03e5a00018bf2312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc03e5a0000076b312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc03f86000010cc312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc03f8600018bf0312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc03f8600000767312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc040b3000010d1312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc040b300018bef312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc040b300000761312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc041e0000010d2312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc041e000018bec312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc041e00000075d312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0430c000010d5312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0430c00018bea312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0430c00000759312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04439000010d9312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0443900018be1312e312b6465666175
[2026-11-18T13:07:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0443900000755312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04566000010dc312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0456600018be5312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc045660000074f312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04692000010de312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0469200018bef312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc046920000074a312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc047bf000010dd312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc047bf00018bfe312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc047bf00000743312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc048ec000010e4312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc048ec00018bfc312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc048ec00000740312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04a18000010ea312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04a1800018bf6312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04a180000073b312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04b45000010ef312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04b4500018bf2312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04b4500000735312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04c72000010f5312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04c7200018bf3312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04c7200000731312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04d9e000010f7312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04d9e00018be8312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04d9e0000072a312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04ecb000010fc312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04ecb00018beb312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04ecb00000725312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc04ff8000010fc312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc04ff800018be5312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc04ff800000720312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05125000010fc312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0512500018bd6312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc051250000071a312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0525100001102312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0525100018bce312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0525100000715312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0537e00001108312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0537e00018bca312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0537e00000711312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc054ab00001105312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc054ab00018bc8312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc054ab0000070c312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc055d700001106312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc055d700018bca312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc055d700000708312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc057040000110d312e312b6465666175
[2026-11-18T13:07:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0570400018bc9312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0570400000701312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0583100001112312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0583100018bc3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05831000006fc312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0595d0000111a312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0595d00018bc3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0595d000006f8312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05a8a0000111a312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc05a8a00018bb7312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05a8a000006f3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05bb70000111d312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc05bb700018bb7312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05bb7000006ee312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05ce30000111e312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc05ce300018bab312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05ce3000006ea312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05e100000111d312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc05e1000018baf312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05e10000006e4312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc05f3d00001123312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc05f3d00018ba8312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc05f3d000006e0312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0606a0000112a312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0606a00018ba2312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0606a000006db312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0619600001126312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0619600018ba5312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06196000006d8312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc062c300001127312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc062c300018ba7312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc062c3000006d3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc063f00000112b312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc063f000018ba2312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc063f0000006d0312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0651c00001130312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0651c00018b9f312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0651c000006ca312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0664900001139312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0664900018b97312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06649000006c6312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0677600001141312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0677600018b9a312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06776000006c3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc068a200001147312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc068a200018b95312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc068a2000006bd312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc069cf00001151312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc069cf00018b91312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc069cf000006bb312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc06afc00001155312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc06afc00018b89312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06afc000006b6312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc06c280000115b312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc06c2800018b81312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06c28000006b3312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc06d550000115d312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc06d5500018b77312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06d55000006ae312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc06e820000115f312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc06e8200018b76312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06e82000006ab312e312b6465666175
[2026-11-18T13:07:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc06faf00001169312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc06faf00018b74312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc06faf000006a8312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc070db0000117f312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc070db00018b75312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc070db000006a3312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0720800001186312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0720800018b71312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07208000006a1312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0733500001188312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0733500018b71312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc073350000069b312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0746100001196312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0746100018b78312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0746100000697312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0758e00001191312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0758e00018b70312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0758e00000694312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc076bb0000118e312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc076bb00018b65312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc076bb0000068d312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc077e700001191312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc077e700018b65312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc077e700000689312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0791400001192312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0791400018b66312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0791400000686312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc07a4100001194312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc07a4100018b66312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07a4100000681312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc07b6d0000119c312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc07b6d00018b6c312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07b6d0000067c312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc07c9a000011a7312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc07c9a00018b69312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07c9a0000067b312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc07dc7000011a9312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc07dc700018b62312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07dc700000677312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc07ef4000011a6312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc07ef400018b5c312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc07ef400000674312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08020000011b9312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0802000018b57312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc080200000066f312e312b6465666175
[2026-11-18T13:07:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0814d000011ac312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0814d00018b5a312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0814d0000066c312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0827a000011b8312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0827a00018b4f312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0827a00000668312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc083a6000011af312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc083a600018b46312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc083a600000664312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc084d3000011b6312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc084d300018b38312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc084d300000661312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08600000011be312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0860000018b3c312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc086000000065e312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0872c000011b7312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0872c00018b43312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0872c00000659312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08859000011ba312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0885900018b44312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0885900000656312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08986000011b9312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0898600018b40312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0898600000652312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08ab2000011bc312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc08ab200018b39312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc08ab20000064f312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08bdf000011be312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc08bdf00018b36312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc08bdf0000064c312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08d0c000011bf312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc08d0c00018b38312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc08d0c00000648312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08e38000011bc312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc08e3800018b30312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc08e3800000645312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc08f65000011bd312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc08f6500018b2b312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc08f6500000641312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09092000011bf312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0909200018b2f312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc090920000063f312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc091bf000011c1312e312b6465666175
[2026-11-18T13:07:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc091bf00018b30312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc091bf0000063c312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc092eb000011bf312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc092eb00018b29312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc092eb00000639312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09418000011c1312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0941800018b31312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0941800000635312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09545000011c1312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0954500018b31312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0954500000634312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09671000011c3312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0967100018b3e312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0967100000631312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0979e000011c3312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0979e00018b48312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0979e0000062e312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc098cb000011c2312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc098cb00018b41312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc098cb0000062c312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc099f7000011be312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc099f700018b33312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc099f700000629312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09b24000011c0312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc09b2400018b31312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc09b2400000626312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09c51000011bb312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc09c5100018b20312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc09c5100000625312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09d7d000011ba312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc09d7d00018b1f312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc09d7d00000621312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09eaa000011ba312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc09eaa00018b21312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc09eaa0000061f312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc09fd7000011b9312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc09fd700018b1a312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc09fd70000061e312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a104000011b9312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a10400018b1a312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a1040000061d312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a230000011b6312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a23000018b16312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a23000000619312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a35d000011b6312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a35d00018b18312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a35d00000616312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a48a000011b3312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a48a00018b16312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a48a00000615312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a5b6000011b6312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a5b600018b12312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a5b600000613312e312b6465666175
[2026-11-18T13:07:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a6e3000011b0312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a6e300018b18312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a6e300000610312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a810000011b2312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a81000018b19312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a8100000060c312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0a93c000011b6312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0a93c00018b0e312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0a93c00000607312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0aa69000011ba312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0aa6900018b02312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0aa6900000604312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ab96000011b9312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ab9600018aed312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ab9600000602312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0acc2000011b3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0acc200018ae3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0acc2000005ff312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0adef000011b0312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0adef00018ade312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0adef000005fd312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0af1c000011ac312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0af1c00018ae4312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0af1c000005fb312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b049000011a9312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b04900018ae3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b049000005f7312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b175000011a5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b17500018ad9312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b175000005f5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b2a2000011a5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b2a200018ad3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b2a2000005f3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b3cf000011a5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b3cf00018ac8312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b3cf000005f1312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b4fb000011a5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b4fb00018ad3312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b4fb000005ef312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b628000011a9312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b62800018acd312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b628000005ec312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b755000011b1312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b75500018ace312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b755000005ea312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b881000011b4312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b88100018add312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b881000005e9312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0b9ae000011ba312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0b9ae00018ae0312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0b9ae000005e6312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0badb000011bd312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0badb00018ae1312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0badb000005e4312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0bc07000011bb312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0bc0700018ae0312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0bc07000005e1312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0bd34000011bd312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0bd3400018ae5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0bd34000005de312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0be61000011b6312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0be6100018adb312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0be61000005de312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0bf8e000011b7312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0bf8e00018adb312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0bf8e000005da312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c0ba000011bf312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c0ba00018ad5312e312b6465666175
[2026-11-18T13:07:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c0ba000005d8312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c1e7000011c4312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c1e700018ace312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c1e7000005d4312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c314000011c6312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c31400018abb312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c314000005d4312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c440000011cb312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c44000018ac1312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c440000005d3312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c56d000011d7312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c56d00018ad7312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c56d000005d0312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c69a000011de312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c69a00018ad6312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c69a000005d0312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c7c6000011e8312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c7c600018ad4312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c7c6000005cd312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0c8f3000011ec312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0c8f300018acf312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0c8f3000005cb312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ca20000011f3312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ca2000018ad2312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ca20000005c9312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0cb4c000011fb312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0cb4c00018ae5312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0cb4c000005c8312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0cc79000011fd312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0cc7900018ad4312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0cc79000005c7312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0cda60000120a312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0cda600018ac8312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0cda6000005c5312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ced200001212312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ced200018ac7312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ced2000005c2312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0cfff0000121f312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0cfff00018ad5312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0cfff000005c5312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d12c0000122b312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d12c00018aca312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d12c000005c2312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d25900001233312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d25900018ac0312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d259000005c1312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d3850000123b312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d38500018ac8312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d385000005c2312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d4b20000123f312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d4b200018ac3312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d4b2000005c1312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d5df00001242312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d5df00018ac9312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d5df000005bf312e312b6465666175
[2026-11-18T13:07:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d70b00001249312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d70b00018acf312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d70b000005bf312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d8380000124d312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d83800018ad2312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d838000005bf312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0d96500001251312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0d96500018ad2312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0d965000005bf312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0da9100001251312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0da9100018acc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0da91000005bd312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0dbbe00001253312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0dbbe00018acc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0dbbe000005be312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0dceb00001256312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0dceb00018ac5312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0dceb000005bc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0de170000125a312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0de1700018ac5312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0de17000005bc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0df4400001261312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0df4400018ad1312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0df44000005bc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e07100001266312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e07100018ac8312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e071000005bd312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e19e00001269312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e19e00018ad5312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e19e000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e2ca0000126b312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e2ca00018ad5312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e2ca000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e3f70000126f312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e3f700018ad3312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e3f7000005bb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e52400001275312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e52400018ad0312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e524000005bb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e6500000127f312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e65000018adb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e650000005bb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e77d00001282312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e77d00018adb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e77d000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e8aa0000128d312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e8aa00018adb312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e8aa000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0e9d600001292312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0e9d600018ad6312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0e9d6000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0eb0300001290312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0eb0300018adc312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0eb03000005b9312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ec3000001292312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ec3000018aed312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ec30000005b9312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ed5c00001296312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ed5c00018ae3312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ed5c000005ba312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0ee8900001298312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0ee8900018ae2312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0ee89000005b8312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0efb600001298312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0efb600018ae1312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0efb6000005b9312e312b6465666175
[2026-11-18T13:07:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f0e300001299312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f0e300018ad7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f0e3000005ba312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f20f0000129c312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f20f00018ad1312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f20f000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f33c0000129d312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f33c00018ad7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f33c000005b8312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f4690000129e312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f46900018adb312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f469000005b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f595000012a0312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f59500018af2312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f595000005b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f6c2000012a4312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f6c200018b02312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f6c2000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f7ef000012a4312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f7ef00018afe312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f7ef000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0f91b000012a7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0f91b00018b02312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0f91b000005b6312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0fa48000012ac312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0fa4800018b04312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0fa48000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0fb75000012af312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0fb7500018b0e312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0fb75000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0fca1000012b1312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0fca100018b13312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0fca1000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0fdce000012b6312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0fdce00018b18312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0fdce000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc0fefb000012b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc0fefb00018b13312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc0fefb000005b6312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10028000012b8312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1002800018b18312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10028000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10154000012bb312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1015400018b1a312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10154000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10281000012c0312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1028100018b19312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10281000005b8312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc103ae000012c2312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc103ae00018b1b312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc103ae000005b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc104da000012c2312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc104da00018b1d312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc104da000005b8312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10607000012ca312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1060700018b2b312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10607000005b7312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10734000012d0312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1073400018b27312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10734000005b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10860000012d2312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1086000018b2e312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10860000005b9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc1098d000012d4312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1098d00018b2f312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc1098d000005bb312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10aba000012d6312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc10aba00018b30312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10aba000005bc312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10be6000012d9312e312b6465666175
[2026-11-18T13:07:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc10be600018b2b312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10be6000005bd312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10d13000012d9312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc10d1300018b2e312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10d13000005be312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10e40000012de312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc10e4000018b31312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10e40000005c0312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc10f6c000012db312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc10f6c00018b39312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc10f6c000005c0312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc11099000012db312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1109900018b3b312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc11099000005c3312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc111c6000012da312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc111c600018b35312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc111c6000005c4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc112f3000012db312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc112f300018b39312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc112f3000005c5312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc1141f000012e0312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1141f00018b35312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc1141f000005c6312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc1154c000012e4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1154c00018b35312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc1154c000005c8312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc11679000012e4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc1167900018b32312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc11679000005cb312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc117a5000012ea312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc117a500018b38312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc117a5000005cb312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105dc118d2000012ea312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105dc118d200018b36312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105dc118d2000005cc312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db10afd000014db312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db10afd00018e56312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db10afd000006a8312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db10c29000014d4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db10c2900018e64312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db10c29000006a7312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db10d56000014cf312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db10d5600018e77312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db10d56000006a6312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db10e83000014d4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db10e8300018e75312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db10e83000006a6312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db10faf000014d5312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db10faf00018e80312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db10faf000006a6312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db110dc000014cf312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db110dc00018e7b312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db110dc000006a5312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11209000014ca312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1120900018e77312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11209000006a5312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11336000014bd312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1133600018e6b312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11336000006a3312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11462000014b4312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1146200018e5a312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11462000006a2312e312b6465666175
[2026-11-18T13:07:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1158f000014a1312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1158f00018e5d312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1158f000006a2312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db116bc00001493312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db116bc00018e5b312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db116bc000006a1312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db117e800001490312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db117e800018e57312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db117e8000006a0312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db119150000148c312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1191500018e4c312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11915000006a0312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11a4200001488312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db11a4200018e65312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11a420000069f312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11b6e00001486312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db11b6e00018e5b312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11b6e0000069e312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11c9b00001489312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db11c9b00018e4a312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11c9b0000069c312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11dc8000014a2312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db11dc800018e42312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11dc80000069d312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db11ef4000014b1312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db11ef400018e38312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db11ef40000069f312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12021000014b4312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1202100018e37312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db120210000069c312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1214e000014bd312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1214e00018e43312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1214e0000069b312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1227b000014bb312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1227b00018e42312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1227b0000069c312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db123a7000014bb312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db123a700018e3e312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db123a70000069b312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db124d4000014bc312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db124d400018e4a312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db124d40000069a312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12601000014ba312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1260100018e4b312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db126010000069a312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1272d000014af312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1272d00018e2f312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1272d00000699312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1285a000014b7312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1285a00018e29312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1285a00000698312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12987000014ad312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1298700018e1e312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1298700000697312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12ab3000014b0312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db12ab300018e22312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db12ab300000694312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12be0000014bc312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db12be000018e31312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db12be000000691312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12d0d000014c8312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db12d0d00018e41312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db12d0d0000068f312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12e39000014cc312e312b6465666175
[2026-11-18T13:07:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db12e3900018e5b312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db12e390000068d312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db12f66000014d0312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db12f6600018e7b312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db12f660000068b312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13093000014cf312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1309300018e66312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1309300000689312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db131c0000014d4312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db131c000018e2f312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db131c000000689312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db132ec000014df312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db132ec00018e2a312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db132ec00000688312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13419000014df312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1341900018e36312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1341900000685312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13546000014e2312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1354600018e42312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1354600000683312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1367200001501312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1367200018e56312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db136720000067f312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1379f0000152b312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1379f00018e47312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1379f0000067c312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db138cc0000153d312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db138cc00018e46312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db138cc0000067b312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db139f800001544312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db139f800018e58312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db139f800000678312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13b2500001548312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db13b2500018e56312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db13b2500000674312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13c5200001549312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db13c5200018e46312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db13c5200000674312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13d7e00001548312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db13d7e00018e3c312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db13d7e00000673312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13eab0000154e312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db13eab00018e02312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db13eab00000672312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db13fd800001548312e312b6465666175
[2026-11-18T13:07:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db13fd800018dde312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db13fd800000670312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1410400001543312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1410400018ddf312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db141040000066d312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db142310000153a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1423100018ddd312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db142310000066b312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1435e00001532312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1435e00018dd4312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1435e0000066a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1448b00001527312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1448b00018dc7312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1448b0000066a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db145b70000151b312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db145b700018dc2312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db145b70000066a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db146e4000014fd312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db146e400018dc9312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db146e40000066a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14811000014ec312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1481100018dc2312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1481100000667312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1493d000014e7312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1493d00018daf312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1493d00000666312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14a6a000014e0312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db14a6a00018dc2312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db14a6a00000666312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14b97000014dd312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db14b9700018dc2312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db14b9700000662312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14cc3000014eb312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db14cc300018dbe312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db14cc300000664312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14df0000014f2312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db14df000018dbf312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db14df000000662312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db14f1d00001500312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db14f1d00018db9312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db14f1d00000663312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db150490000150a312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1504900018dc6312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1504900000664312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db151760000151b312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1517600018dc5312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1517600000666312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db152a300001526312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db152a300018db3312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db152a300000666312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db153d000001531312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db153d000018dbd312e312b6465666175
[2026-11-18T13:07:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db153d000000667312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db154fc00001541312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db154fc00018dc6312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db154fc00000669312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db156290000156e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1562900018dc9312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db156290000066a312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db157560000157c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1575600018dad312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db157560000066c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15882000015a8312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1588200018d9e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db158820000066d312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db159af000015d5312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db159af00018daa312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db159af0000066e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15adc000015eb312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db15adc00018da4312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db15adc0000066e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15c08000015f3312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db15c0800018dcb312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db15c0800000670312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15d3500001601312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db15d3500018db5312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db15d3500000673312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15e620000161a312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db15e6200018d91312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db15e6200000675312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db15f8e00001628312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db15f8e00018da7312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db15f8e00000677312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db160bb0000162f312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db160bb00018d90312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db160bb0000067a312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db161e80000163f312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db161e800018da5312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db161e80000067b312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1631500001648312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1631500018d9c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db163150000067d312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1644100001651312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1644100018dae312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db164410000067f312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1656e00001660312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1656e00018da6312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1656e00000680312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1669b00001662312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1669b00018d9c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1669b00000682312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db167c700001660312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db167c700018daa312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db167c700000686312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db168f400001663312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db168f400018da5312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db168f400000687312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db16a2100001677312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db16a2100018d9f312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db16a2100000689312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db16b4d00001686312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db16b4d00018d84312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db16b4d0000068c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db16c7a0000168c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db16c7a00018d6d312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db16c7a0000068c312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db16da7000016a6312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db16da700018d84312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db16da70000068d312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db16ed3000016b3312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db16ed300018d86312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db16ed30000068e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17000000016ba312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1700000018d83312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db170000000068f312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1712d000016b7312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1712d00018d79312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1712d00000690312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1725a000016bf312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1725a00018d6e312e312b6465666175
[2026-11-18T13:07:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1725a00000692312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17386000016c3312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1738600018d63312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1738600000693312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db174b3000016cc312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db174b300018d53312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db174b300000695312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db175e0000016bf312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db175e000018d5d312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db175e000000698312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1770c000016a8312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1770c00018d6d312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1770c0000069b312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17839000016aa312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1783900018d75312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db178390000069c312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17966000016b4312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1796600018d89312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db179660000069f312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17a92000016c1312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db17a9200018daa312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db17a92000006a0312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17bbf000016cf312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db17bbf00018da9312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db17bbf000006a1312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17cec000016c1312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db17cec00018d87312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db17cec000006a5312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17e18000016dd312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db17e1800018d9d312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db17e18000006a6312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db17f45000016e7312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db17f4500018daf312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db17f45000006a6312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18072000016eb312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1807200018db5312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18072000006a9312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1819e000016ef312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1819e00018db1312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1819e000006ab312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db182cb000016dc312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db182cb00018d99312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db182cb000006ac312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db183f8000016f3312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db183f800018d7c312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db183f8000006ae312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1852500001728312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1852500018d7d312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18525000006ad312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1865100001735312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1865100018d80312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18651000006af312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1877e0000174a312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1877e00018d73312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1877e000006b1312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db188ab00001767312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db188ab00018d70312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db188ab000006b0312e312b6465666175
[2026-11-18T13:07:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db189d70000177a312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db189d700018d78312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db189d7000006ad312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18b04000017a1312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db18b0400018d90312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18b04000006ab312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18c310000179e312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db18c3100018da1312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18c31000006ab312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18d5d000017b2312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db18d5d00018db1312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18d5d000006a9312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18e8a000017b1312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db18e8a00018db0312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18e8a000006a8312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db18fb7000017b7312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db18fb700018da0312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db18fb7000006aa312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db190e3000017ae312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db190e300018d91312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db190e3000006aa312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19210000017bc312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1921000018da1312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19210000006ab312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1933d000017d4312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1933d00018da6312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1933d000006ab312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1946a000017e2312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1946a00018d99312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1946a000006ab312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1959600001807312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1959600018d86312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19596000006a9312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db196c300001809312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db196c300018d62312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db196c3000006a9312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db197f000001827312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db197f000018d7f312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db197f0000006a6312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1991c0000183d312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1991c00018d97312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1991c000006a2312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19a490000184b312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db19a4900018dac312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19a49000006a0312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19b760000184f312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db19b7600018db5312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19b760000069d312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19ca20000185f312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db19ca200018db5312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19ca20000069d312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19dcf0000186e312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db19dcf00018dad312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19dcf00000699312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db19efc0000187c312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db19efc00018dba312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db19efc00000697312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a0280000188c312e312b6465666175
[2026-11-18T13:07:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a02800018da9312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a02800000693312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a15500001892312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a15500018da5312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a15500000692312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a2820000189f312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a28200018d9a312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a2820000068f312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a3af000018ad312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a3af00018d81312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a3af0000068d312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a4db000018be312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a4db00018d7a312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a4db0000068a312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a608000018cc312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a60800018d91312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a60800000686312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a735000018de312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a73500018d9a312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a73500000684312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a861000018ea312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a86100018d98312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a86100000682312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1a98e000018f7312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1a98e00018d79312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1a98e00000681312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1aabb00001914312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1aabb00018d78312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1aabb0000067e312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1abe700001927312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1abe700018d9d312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1abe70000067b312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ad1400001939312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ad1400018da1312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ad140000067b312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ae4100001940312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ae4100018d99312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ae4100000679312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1af6d00001949312e312b6465666175
[2026-11-18T13:07:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1af6d00018d96312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1af6d00000677312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b09a00001953312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b09a00018da7312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b09a00000678312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b1c70000195f312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b1c700018dab312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b1c700000678312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b2f30000196b312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b2f300018dae312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b2f30000067a312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b42000001980312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b42000018daa312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b42000000679312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b54d00001982312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b54d00018daf312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b54d00000676312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b67a00001993312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b67a00018dbc312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b67a00000677312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b7a60000199d312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b7a600018dcc312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b7a600000675312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1b8d3000019ab312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1b8d300018dd2312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1b8d300000674312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ba00000019b9312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ba0000018dde312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ba0000000672312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1bb2c000019c2312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1bb2c00018e07312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1bb2c00000671312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1bc59000019d3312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1bc5900018dfe312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1bc590000066f312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1bd86000019da312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1bd8600018def312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1bd8600000671312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1beb2000019d9312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1beb200018dea312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1beb200000670312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1bfdf000019e0312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1bfdf00018e07312e312b6465666175
[2026-11-18T13:07:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1bfdf0000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c10c000019dd312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c10c00018dfe312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c10c0000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c238000019dd312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c23800018e22312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c2380000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c365000019dc312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c36500018e16312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c3650000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c492000019d9312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c49200018e18312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c4920000066e312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c5bf000019d5312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c5bf00018e14312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c5bf0000066e312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c6eb000019d1312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c6eb00018e25312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c6eb0000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c818000019d0312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c81800018e0d312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c8180000066f312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1c945000019cb312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1c94500018e0b312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1c94500000670312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ca71000019d1312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ca7100018e25312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ca7100000670312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1cb9e000019c8312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1cb9e00018e21312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1cb9e00000673312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1cccb000019cd312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1cccb00018e28312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1cccb00000672312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1cdf7000019ce312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1cdf700018e3a312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1cdf700000673312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1cf24000019d3312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1cf2400018e47312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1cf2400000674312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d051000019d2312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d05100018e63312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d05100000676312e312b6465666175
[2026-11-18T13:07:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d17d000019d0312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d17d00018e55312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d17d00000674312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d2aa000019cd312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d2aa00018e5d312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d2aa00000676312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d3d7000019d0312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d3d700018e5c312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d3d700000676312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d504000019ce312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d50400018e65312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d50400000677312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d630000019d4312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d63000018e69312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d63000000676312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d75d000019cc312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d75d00018e68312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d75d00000678312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d88a000019c6312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d88a00018e7e312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d88a00000679312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1d9b6000019c6312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1d9b600018e8c312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1d9b60000067b312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1dae3000019c2312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1dae300018ec6312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1dae30000067d312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1dc10000019be312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1dc1000018ecb312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1dc100000067f312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1dd3c000019b8312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1dd3c00018ece312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1dd3c00000680312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1de69000019b0312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1de6900018ed0312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1de6900000682312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1df96000019ac312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1df9600018ece312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1df9600000686312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e0c2000019ab312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e0c200018ee9312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e0c200000688312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e1ef000019aa312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e1ef00018efc312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e1ef00000687312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e31c000019aa312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e31c00018ef8312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e31c00000689312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e449000019a7312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e44900018ef8312e312b6465666175
[2026-11-18T13:07:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e4490000068a312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e575000019a5312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e57500018f09312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e5750000068c312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e6a2000019a5312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e6a200018f11312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e6a20000068d312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e7cf0000199d312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e7cf00018f1a312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e7cf00000690312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1e8fb0000199b312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1e8fb00018f20312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1e8fb00000691312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ea2800001998312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ea2800018f29312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ea2800000693312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1eb5500001992312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1eb5500018f33312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1eb5500000698312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ec810000198f312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ec8100018f2c312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ec810000069a312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1edae0000198c312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1edae00018f2a312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1edae0000069b312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1eedb00001988312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1eedb00018f20312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1eedb0000069f312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f00700001982312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f00700018f28312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f0070000069f312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f13400001984312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f13400018f32312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f134000006a3312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f2610000197d312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f26100018f34312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f261000006a2312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f38d0000197c312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f38d00018f39312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f38d000006a4312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f4ba0000197c312e312b6465666175
[2026-11-18T13:07:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f4ba00018f39312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f4ba000006a3312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f5e70000197f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f5e700018f47312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f5e7000006a5312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f71400001980312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f71400018f43312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f714000006a3312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f84000001976312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f84000018f3e312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f840000006a6312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1f96d0000196f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1f96d00018f4c312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1f96d000006a8312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1fa9a0000196f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1fa9a00018f49312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1fa9a000006a8312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1fbc60000196d312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1fbc600018f48312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1fbc6000006a9312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1fcf300001965312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1fcf300018f47312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1fcf3000006ac312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1fe2000001958312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1fe2000018f42312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1fe20000006b0312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db1ff4c0000194e312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db1ff4c00018f4e312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db1ff4c000006b1312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2007900001944312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2007900018f59312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20079000006b6312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db201a600001939312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db201a600018f58312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db201a6000006b8312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db202d200001930312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db202d200018f54312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db202d2000006ba312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db203ff0000192e312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db203ff00018f58312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db203ff000006bd312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2052c00001928312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2052c00018f64312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2052c000006c0312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2065900001915312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2065900018f7f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20659000006c2312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2078500001906312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2078500018f8e312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20785000006c5312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db208b20000191a312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db208b200018f97312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db208b2000006c7312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db209df00001928312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db209df00018fa1312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db209df000006c8312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db20b0b0000191f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db20b0b00018faa312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20b0b000006c9312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db20c3800001918312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db20c3800018f8f312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20c38000006ca312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db20d6500001903312e312b6465666175
[2026-11-18T13:07:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db20d6500018f8e312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20d65000006cb312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db20e9100001901312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db20e9100018f89312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20e91000006cd312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db20fbe000018f9312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db20fbe00018f87312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db20fbe000006cf312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db210eb000018ea312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db210eb00018f92312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db210eb000006d0312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21217000018dd312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2121700018f75312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21217000006d2312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21344000018ba312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2134400018f63312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21344000006d2312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21471000018a9312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2147100018f55312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21471000006d4312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2159e00001877312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2159e00018f67312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2159e000006d6312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db216ca00001866312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db216ca00018f51312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db216ca000006d7312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db217f70000184b312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db217f700018f2e312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db217f7000006d8312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2192400001833312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2192400018f3a312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21924000006d8312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21a500000182a312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db21a5000018f40312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21a50000006db312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21b7d0000180a312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db21b7d00018f34312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21b7d000006dc312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21caa000017f6312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db21caa00018f3d312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21caa000006dd312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21dd6000017dc312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db21dd600018f1e312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21dd6000006de312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db21f03000017cc312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db21f0300018f39312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db21f03000006de312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22030000017c0312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2203000018f3f312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22030000006de312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2215c000017ad312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2215c00018f34312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2215c000006df312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22289000017a2312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2228900018f2c312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22289000006df312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db223b600001792312e312b6465666175
[2026-11-18T13:07:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db223b600018f1f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db223b6000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db224e300001786312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db224e300018f11312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db224e3000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2260f00001775312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2260f00018f0b312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2260f000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2273c00001769312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2273c00018f07312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2273c000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2286900001769312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2286900018efd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22869000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2299500001765312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2299500018ee5312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22995000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22ac200001766312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db22ac200018eeb312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22ac2000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22bef00001760312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db22bef00018edb312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22bef000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22d1b00001759312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db22d1b00018ef5312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22d1b000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22e480000174d312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db22e4800018ee3312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22e48000006dc312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db22f7500001746312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db22f7500018edb312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db22f75000006dc312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db230a100001744312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db230a100018ed4312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db230a1000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db231ce00001747312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db231ce00018ed4312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db231ce000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db232fb00001757312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db232fb00018ee6312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db232fb000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2342700001742312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2342700018ed0312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23427000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2355400001740312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2355400018ebf312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23554000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2368100001730312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2368100018eb1312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23681000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db237ae00001738312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db237ae00018eb4312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db237ae000006dc312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db238da00001744312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db238da00018eaa312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db238da000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23a070000174e312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23a0700018eb1312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23a07000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23b3400001749312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23b3400018ea8312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23b34000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23c6000001742312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23c6000018eaa312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23c60000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23d8d0000173f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23d8d00018e9d312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23d8d000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23eba0000174b312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23eba00018e8a312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23eba000006dd312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db23fe60000174b312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db23fe600018e7e312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db23fe6000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2411300001750312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2411300018e80312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24113000006df312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db242400000174f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2424000018e79312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24240000006de312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2436c00001752312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2436c00018e76312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2436c000006e0312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db244990000174b312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2449900018e6f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24499000006e0312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db245c60000174f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db245c600018e6d312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db245c6000006e1312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db246f30000173f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db246f300018e65312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db246f3000006e0312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2481f00001731312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2481f00018e69312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2481f000006e2312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2494c0000172c312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2494c00018e66312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2494c000006e2312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db24a790000171c312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db24a7900018e50312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24a79000006e2312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db24ba50000170f312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db24ba500018e41312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24ba5000006e3312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db24cd20000170a312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db24cd200018e4b312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24cd2000006e3312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db24dff000016fe312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db24dff00018e21312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24dff000006e3312e312b6465666175
[2026-11-18T13:07:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db24f2b0000170b312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db24f2b00018e34312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db24f2b000006e4312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25058000016f3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2505800018e37312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25058000006e5312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25185000016ec312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2518500018e35312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25185000006e4312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db252b1000016e4312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db252b100018e30312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db252b1000006e4312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db253de000016da312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db253de00018e17312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db253de000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2550b000016d3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2550b00018e0f312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2550b000006e4312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25638000016cb312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2563800018dfe312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25638000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25764000016c1312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2576400018dfd312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25764000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25891000016b3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2589100018df5312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25891000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db259be000016ae312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db259be00018df8312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db259be000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25aea000016a3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db25aea00018df7312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25aea000006e3312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25c1700001698312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db25c1700018dfe312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25c17000006e0312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25d440000168d312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db25d4400018df0312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25d44000006e2312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25e700000167d312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db25e7000018def312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25e70000006e0312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db25f9d00001672312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db25f9d00018ddb312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db25f9d000006df312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db260ca0000166c312e312b6465666175
[2026-11-18T13:07:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db260ca00018dd7312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db260ca000006de312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db261f600001668312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db261f600018dc3312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db261f6000006dd312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2632300001660312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2632300018dc2312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26323000006dd312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2645000001658312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2645000018dc4312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26450000006db312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2657d0000164a312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2657d00018dad312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2657d000006d9312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db266a900001647312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db266a900018da6312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db266a9000006da312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db267d60000163c312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db267d600018d9b312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db267d6000006d8312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2690300001632312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2690300018d96312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26903000006d6312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db26a2f00001635312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db26a2f00018d99312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26a2f000006d4312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db26b5c0000162a312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db26b5c00018d94312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26b5c000006d4312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db26c8900001624312e312b6465666175
[2026-11-18T13:07:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db26c8900018d8d312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26c89000006d3312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db26db50000161b312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db26db500018d8e312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26db5000006d1312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db26ee200001627312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db26ee200018d96312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db26ee2000006ce312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2700f00001618312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2700f00018d95312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2700f000006cd312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2713b0000160d312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2713b00018d7c312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2713b000006cb312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2726800001608312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2726800018d7c312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27268000006ca312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2739500001603312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2739500018d74312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27395000006c8312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db274c1000015fd312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db274c100018d64312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db274c1000006c6312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db275ee000015fe312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db275ee00018d6a312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db275ee000006c5312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2771b00001601312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2771b00018d63312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2771b000006c4312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2784800001601312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2784800018d5d312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27848000006c2312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27974000015fd312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2797400018d5a312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27974000006c0312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27aa10000160e312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db27aa100018d67312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27aa1000006bf312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27bce0000160b312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db27bce00018d65312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27bce000006be312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27cfa0000160a312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db27cfa00018d5d312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27cfa000006bd312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27e270000160f312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db27e2700018d59312e312b6465666175
[2026-11-18T13:07:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27e27000006bb312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db27f540000160a312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db27f5400018d45312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db27f54000006b8312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2808000001617312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2808000018d4f312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28080000006b8312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db281ad00001626312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db281ad00018d52312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db281ad000006b8312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db282da00001624312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db282da00018d52312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db282da000006b6312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db284060000162c312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2840600018d4f312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28406000006b6312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2853300001629312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2853300018d34312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28533000006b5312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db286600000162d312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2866000018d2b312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28660000006b4312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2878d00001643312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2878d00018d1f312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2878d000006b4312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db288b90000163e312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db288b900018d19312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db288b9000006b4312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db289e60000163a312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db289e600018d12312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db289e6000006b4312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db28b130000164e312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db28b1300018d03312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28b13000006b6312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db28c3f00001659312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db28c3f00018cef312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28c3f000006b5312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db28d6c00001668312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db28d6c00018cf7312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28d6c000006b8312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db28e990000168e312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db28e9900018cef312e312b6465666175
[2026-11-18T13:07:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28e99000006b8312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db28fc500001690312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db28fc500018cdd312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db28fc5000006b8312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db290f200001691312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db290f200018ccf312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db290f2000006ba312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2921f00001694312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2921f00018cc8312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2921f000006b9312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2934b000016a4312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2934b00018cb6312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2934b000006bb312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29478000016ad312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2947800018cb5312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29478000006bb312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db295a5000016e3312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db295a500018ca4312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db295a5000006bc312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db296d2000016c1312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db296d200018c9c312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db296d2000006bf312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db297fe000016b9312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db297fe00018c96312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db297fe000006be312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2992b0000169d312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2992b00018c97312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2992b000006be312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29a58000016b3312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db29a5800018c8c312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29a58000006c0312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29b84000016b3312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db29b8400018c82312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29b84000006c1312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29cb1000016b5312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db29cb100018c86312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29cb1000006c3312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29dde000016b3312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db29dde00018c71312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29dde000006c6312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db29f0a000016ac312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db29f0a00018c65312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db29f0a000006c8312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a037000016b1312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a03700018c6b312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a037000006c8312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a164000016ca312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a16400018c69312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a164000006ca312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a290000016eb312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a29000018c62312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a290000006cd312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a3bd0000171d312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a3bd00018c5f312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a3bd000006cf312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a4ea0000171e312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a4ea00018c56312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a4ea000006d1312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a6170000173b312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a61700018c55312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a617000006d4312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a74300001737312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a74300018c4e312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a743000006d6312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a87000001744312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a87000018c53312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a870000006da312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2a99d00001749312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2a99d00018c49312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2a99d000006dd312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2aac90000174c312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2aac900018c4f312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2aac9000006df312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2abf600001737312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2abf600018c47312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2abf6000006e1312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ad230000172c312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ad2300018c36312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ad23000006e5312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ae4f00001727312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ae4f00018c28312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ae4f000006e6312e312b6465666175
[2026-11-18T13:07:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2af7c00001723312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2af7c00018c1c312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2af7c000006e9312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b0a900001721312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b0a900018c14312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b0a9000006ed312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b1d500001715312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b1d500018bfd312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b1d5000006f0312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b30200001719312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b30200018bf2312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b302000006f2312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b42f00001722312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b42f00018be9312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b42f000006f5312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b55b0000172e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b55b00018be4312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b55b000006f8312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b68800001731312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b68800018bda312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b688000006fc312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b7b500001738312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b7b500018be4312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b7b5000006ff312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2b8e20000173e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2b8e200018bce312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2b8e200000700312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ba0e00001722312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ba0e00018bb8312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ba0e00000703312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2bb3b00001718312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2bb3b00018ba4312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2bb3b00000706312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2bc680000170e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2bc6800018b93312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2bc680000070a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2bd9400001749312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2bd9400018b79312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2bd940000070b312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2bec100001747312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2bec100018b60312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2bec10000070d312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2bfee0000174d312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2bfee00018b5b312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2bfee0000070e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c11a00001752312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c11a00018b52312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c11a00000711312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c24700001758312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c24700018b45312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c24700000713312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c37400001750312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c37400018b33312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c37400000717312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c4a000001747312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c4a000018b2b312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c4a000000717312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c5cd00001747312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c5cd00018b22312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c5cd0000071a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c6fa0000174a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c6fa00018b24312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c6fa0000071c312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c8270000173e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c82700018b0f312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c8270000071e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2c95300001732312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2c95300018b08312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2c9530000071f312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ca8000001729312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ca8000018af7312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ca8000000723312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2cbad0000172a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2cbad00018ae5312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2cbad00000724312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ccd90000172d312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ccd900018adc312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ccd900000726312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ce060000172f312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ce0600018ad2312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ce060000072a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2cf3300001734312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2cf3300018ac7312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2cf330000072a312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d05f00001739312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d05f00018abf312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d05f0000072e312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d18c00001745312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d18c00018aba312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d18c00000732312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d2b900001748312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d2b900018abb312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d2b900000735312e312b6465666175
[2026-11-18T13:07:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d3e500001721312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d3e500018ab1312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d3e500000736312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d51200001718312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d51200018aa2312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d51200000738312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d63f00001711312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d63f00018a9f312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d63f0000073a312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d76c000016fd312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d76c00018a9d312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d76c0000073c312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d898000016ee312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d89800018a96312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d8980000073f312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2d9c5000016f0312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2d9c500018a87312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2d9c500000740312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2daf2000016fe312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2daf200018a82312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2daf200000742312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2dc1e00001705312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2dc1e00018a7b312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2dc1e00000745312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2dd4b00001708312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2dd4b00018a70312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2dd4b00000747312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2de780000170c312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2de7800018a61312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2de780000074b312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2dfa400001705312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2dfa400018a4d312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2dfa40000074e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e0d1000016e5312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e0d100018a48312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e0d10000074e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e1fe000016c7312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e1fe00018a3e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e1fe0000074e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e32a000016ab312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e32a00018a39312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e32a00000751312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e45700001684312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e45700018a3f312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e45700000751312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e58400001660312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e58400018a47312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e58400000753312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e6b100001661312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e6b100018a3e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e6b100000753312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e7dd00001664312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e7dd00018a39312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e7dd00000753312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2e90a00001668312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2e90a00018a2e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2e90a00000754312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ea3700001671312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ea3700018a1d312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ea3700000752312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2eb630000167b312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2eb6300018a17312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2eb6300000753312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ec900000168b312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ec9000018a0d312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ec9000000755312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2edbd0000169e312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2edbd00018a08312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2edbd00000756312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2eee9000016a9312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2eee9000189fc312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2eee900000756312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f016000016b0312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f01600018a01312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f01600000755312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f143000016b4312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f143000189f8312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f14300000753312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f26f000016b2312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f26f000189fe312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f26f00000751312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f39c000016b1312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f39c000189fc312e312b6465666175
[2026-11-18T13:07:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f39c00000751312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f4c9000016b8312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f4c9000189fc312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f4c900000750312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f5f5000016b7312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f5f500018a02312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f5f500000750312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f722000016c6312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f722000189f9312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f72200000750312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f84f000016d2312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f84f000189ec312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f84f0000074c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2f97c000016df312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2f97c000189e3312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2f97c0000074e312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2faa8000016fd312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2faa8000189e8312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2faa80000074d312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2fbd500001700312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2fbd5000189e8312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2fbd50000074c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2fd0200001715312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2fd02000189e2312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2fd020000074e312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2fe2e00001736312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2fe2e000189d3312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2fe2e0000074d312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db2ff5b0000174c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db2ff5b000189de312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db2ff5b0000074a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db300880000175f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30088000189d2312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db300880000074a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db301b400001763312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db301b4000189d2312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db301b400000747312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db302e10000176f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db302e1000189de312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db302e100000746312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3040e00001773312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3040e000189da312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3040e00000742312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3053a00001780312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3053a000189c6312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3053a00000743312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3066700001784312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30667000189ba312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3066700000740312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3079400001786312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30794000189b4312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db307940000073f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db308c100001788312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db308c1000189b1312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db308c10000073f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db309ed0000178f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db309ed000189b2312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db309ed0000073c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db30b1a00001798312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30b1a000189a7312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db30b1a0000073c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db30c4700001799312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30c470001898c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db30c470000073a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db30d730000179a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30d7300018982312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db30d7300000739312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db30ea00000179a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30ea00001897c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db30ea000000738312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db30fcd00001794312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db30fcd00018976312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db30fcd00000738312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db310f900001791312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db310f900018965312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db310f900000737312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3122600001789312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3122600018965312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3122600000738312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db313530000177c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3135300018952312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3135300000738312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3147f00001763312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3147f0001894a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3147f00000738312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db315ac0000174d312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db315ac00018940312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db315ac00000739312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db316d900001737312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db316d90001892f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db316d90000073a312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3180600001728312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db318060001891d312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db318060000073c312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3193200001717312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3193200018906312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db319320000073f312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db31a5f00001718312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db31a5f000188fc312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db31a5f0000073e312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db31b8c00001708312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db31b8c000188ed312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db31b8c00000740312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db31cb800001700312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db31cb8000188e9312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db31cb800000743312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db31de5000016f1312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db31de5000188d9312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db31de500000746312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db31f12000016e0312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db31f12000188bd312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db31f1200000747312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3203e000016b9312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3203e000188bd312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3203e00000747312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3216b000016ad312e312b6465666175
[2026-11-18T13:07:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3216b000188b7312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3216b00000749312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3229800001697312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32298000188bd312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3229800000747312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db323c40000167b312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db323c4000188bc312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db323c400000748312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db324f100001677312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db324f1000188bc312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db324f100000747312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3261e0000165c312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3261e000188bb312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3261e00000745312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3274b00001640312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3274b000188b9312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3274b00000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db328770000162d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32877000188a4312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3287700000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db329a400001648312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db329a4000188a0312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db329a400000740312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db32ad10000166c312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32ad100018897312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db32ad10000073e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db32bfd0000167b312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32bfd000188aa312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db32bfd00000740312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db32d2a0000168f312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32d2a000188b1312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db32d2a00000741312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db32e57000016aa312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32e57000188c6312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db32e570000073f312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db32f83000016ba312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db32f83000188d9312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db32f8300000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db330b0000016df312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db330b0000188fe312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db330b000000742312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db331dd00001720312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db331dd00018908312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db331dd00000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3330900001776312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3330900018927312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3330900000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33436000017d2312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db334360001895a312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3343600000745312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db335630000183c312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db335630001894a312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3356300000743312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3368f00001864312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3368f00018966312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3368f00000740312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db337bc00001872312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db337bc00018995312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db337bc0000073e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db338e900001889312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db338e9000189b1312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db338e90000073a312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33a16000018a3312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33a16000189ad312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33a1600000739312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33b42000018b2312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33b42000189b0312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33b420000073a312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33c6f000018c6312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33c6f000189e3312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33c6f00000738312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33d9c000018b8312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33d9c000189ae312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33d9c00000738312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33ec80000189e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33ec8000189d1312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33ec800000736312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db33ff50000187f312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db33ff5000189e7312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db33ff500000734312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db341220000186d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34122000189f6312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3412200000731312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3424e00001868312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3424e000189fe312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3424e00000731312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3437b00001862312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3437b00018a06312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3437b0000072e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db344a80000185c312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db344a800018a03312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db344a80000072d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db345d400001856312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db345d4000189ec312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db345d40000072d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3470100001856312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3470100018a0e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db347010000072e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3482e00001858312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3482e00018a04312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3482e0000072e312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3495b00001858312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3495b00018a1b312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3495b0000072c312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db34a870000185d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34a8700018a3f312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db34a870000072d312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db34bb400001859312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34bb400018a48312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db34bb400000729312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db34ce100001857312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34ce100018a43312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db34ce100000729312e312b6465666175
[2026-11-18T13:07:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db34e0d00001851312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34e0d00018a34312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db34e0d00000726312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db34f3a0000183f312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db34f3a00018a60312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db34f3a00000723312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3506700001833312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3506700018a61312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3506700000720312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3519300001839312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3519300018a8c312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db351930000071e312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db352c000001840312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db352c000018aa0312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db352c00000071e312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db353ed00001842312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db353ed00018aa4312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db353ed00000718312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3551900001847312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3551900018a8d312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3551900000715312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db356460000183f312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3564600018ab2312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3564600000711312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db357730000183d312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3577300018abe312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db357730000070e312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db358a000001838312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db358a000018a99312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db358a00000070d312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db359cc00001833312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db359cc00018abb312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db359cc0000070a312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db35af900001829312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db35af900018ab5312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db35af900000709312e312b6465666175
[2026-11-18T13:07:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db35c2600001820312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db35c2600018ab4312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db35c2600000707312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db35d520000181e312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db35d5200018ab4312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db35d5200000704312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db35e7f00001811312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db35e7f00018abc312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db35e7f00000701312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db35fac0000180e312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db35fac00018abc312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db35fac000006ff312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db360d800001804312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db360d800018abd312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db360d8000006fc312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36205000017fb312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3620500018ac3312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36205000006f9312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36332000017f9312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3633200018abb312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36332000006f6312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3645e000017f5312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3645e00018ad3312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3645e000006f4312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3658b000017e7312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3658b00018ade312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3658b000006f1312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db366b8000017d4312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db366b800018ae9312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db366b8000006ef312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db367e4000017c9312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db367e400018af7312e312b6465666175
[2026-11-18T13:07:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db367e4000006eb312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36911000017c3312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3691100018af4312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36911000006e8312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36a3e000017bd312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db36a3e00018afd312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36a3e000006e4312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36b6b000017b3312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db36b6b00018b02312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36b6b000006e1312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36c97000017a5312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db36c9700018b0d312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36c97000006de312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36dc4000017a3312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db36dc400018b00312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36dc4000006dc312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db36ef10000178d312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db36ef100018b04312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db36ef1000006d7312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3701d00001780312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3701d00018af8312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3701d000006d5312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3714a00001777312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3714a00018b0b312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3714a000006d0312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3727700001774312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3727700018af5312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37277000006cc312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db373a300001779312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db373a300018af8312e312b6465666175
[2026-11-18T13:07:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db373a3000006ca312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db374d000001774312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db374d000018aeb312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db374d0000006c6312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db375fd0000176d312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db375fd00018afa312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db375fd000006c4312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db377290000177b312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3772900018aec312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37729000006c2312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db378560000178a312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3785600018af0312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37856000006be312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db379830000178d312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3798300018ae1312e312b6465666175
[2026-11-18T13:07:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37983000006bc312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db37ab000001783312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db37ab000018aec312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37ab0000006ba312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db37bdc0000177e312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db37bdc00018adb312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37bdc000006b7312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db37d090000177a312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db37d0900018ae0312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37d09000006b4312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db37e3600001780312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db37e3600018ae5312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37e36000006b1312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db37f620000177c312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db37f6200018ae2312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db37f62000006af312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3808f0000176b312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3808f00018acb312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3808f000006ac312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db381bc00001757312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db381bc00018ace312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db381bc000006aa312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db382e800001742312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db382e800018adb312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db382e8000006a8312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3841500001732312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3841500018ae9312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38415000006a6312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3854200001724312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3854200018ae9312e312b6465666175
[2026-11-18T13:07:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38542000006a3312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3866e00001725312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3866e00018af4312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3866e000006a2312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3879b0000173b312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3879b00018af4312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3879b0000069f312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db388c80000174f312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db388c800018b15312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db388c80000069a312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db389f500001773312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db389f500018b37312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db389f50000069a312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db38b210000179b312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db38b2100018b5b312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38b2100000697312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db38c4e000017c0312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db38c4e00018b5b312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38c4e00000694312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db38d7b000017d4312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db38d7b00018b66312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38d7b00000694312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db38ea7000017f1312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db38ea700018b70312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38ea700000699312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db38fd4000017cd312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db38fd400018b72312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db38fd4000006e7312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3910100001717312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3910100018b85312e312b6465666175
[2026-11-18T13:07:55] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db391010000076f312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3922d0000161d312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3922d00018b95312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3922d00000816312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3935a000014dd312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3935a00018ba0312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3935a000008e8312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db394870000139a312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3948700018ba8312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db394870000098f312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db395b3000012ca312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db395b300018bb8312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db395b3000009fe312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db396e00000122b312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db396e000018bc8312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db396e000000a4f312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3980d000011b6312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3980d00018bca312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3980d00000a9a312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3993a00001138312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3993a00018bb3312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3993a00000adb312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db39a66000010c3312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db39a6600018bcc312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db39a6600000b16312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db39b9300001055312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db39b9300018be0312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db39b9300000b47312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db39cc000000fbd312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db39cc000018bf6312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db39cc000000b74312e312b6465666175
[2026-11-18T13:07:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db39dec00000f44312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db39dec00018c15312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db39dec00000b9c312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db39f1900000efc312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db39f1900018c19312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db39f1900000bc1312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a04600000ec8312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a04600018c13312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a04600000be5312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a17200000e9b312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a17200018c1c312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a17200000c05312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a29f00000e7f312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a29f00018c20312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a29f00000c24312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a3cc00000e5a312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a3cc00018c27312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a3cc00000c44312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a4f800000e33312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a4f800018c33312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a4f800000c64312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a62500000e06312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a62500018c3c312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a62500000c82312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a75200000ddc312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a75200018c4f312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a75200000c9e312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a87e00000dc0312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a87e00018c64312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a87e00000cb7312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3a9ab00000d9e312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3a9ab00018c6b312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3a9ab00000cd0312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3aad800000d79312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3aad800018c6e312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3aad800000ceb312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ac0500000d4f312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ac0500018c7d312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ac0500000d00312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ad3100000d30312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ad3100018c8c312e312b6465666175
[2026-11-18T13:07:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ad3100000d13312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ae5e00000d19312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ae5e00018c9b312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ae5e00000d20312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3af8b00000cfd312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3af8b00018ca5312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3af8b00000d2e312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b0b700000ce0312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b0b700018cb3312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b0b700000d40312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b1e400000cc7312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b1e400018cc7312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b1e400000d50312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b31100000cad312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b31100018cdc312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b31100000d63312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b43d00000c90312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b43d00018cdc312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b43d00000d74312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b56a00000c77312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b56a00018cec312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b56a00000d82312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b69700000c5a312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b69700018cf9312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b69700000d95312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b7c300000c3f312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b7c300018d18312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b7c300000da7312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3b8f000000c24312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3b8f000018d0d312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3b8f000000db7312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ba1d00000c11312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ba1d00018d17312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ba1d00000dc4312e312b6465666175
[2026-11-18T13:07:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3bb4a00000bf3312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3bb4a00018d22312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3bb4a00000dd4312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3bc7600000bd8312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3bc7600018d2b312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3bc7600000de4312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3bda300000bc6312e312b6465666175
[2026-11-18T13:07:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3bda300018d3e312e312b6465666175
[2026-11-18T13:08:01] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3bda300000df1312e312b6465666175
[2026-11-18T13:08:01] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3bed000000bb3312e312b6465666175
[2026-11-18T13:08:02] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3bed000018d46312e312b6465666175
[2026-11-18T13:08:02] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3bed000000dfc312e312b6465666175
[2026-11-18T13:08:02] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3bffc00000b9a312e312b6465666175
[2026-11-18T13:08:02] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3bffc00018d44312e312b6465666175
[2026-11-18T13:08:03] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3bffc00000e0b312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c12900000b89312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c12900018d43312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c12900000e17312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c25600000b72312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c25600018d42312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c25600000e20312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c38200000b63312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c38200018d4c312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c38200000e2b312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c4af00000b57312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c4af00018d59312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c4af00000e32312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c5dc00000b4c312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c5dc00018d5a312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c5dc00000e37312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c70800000b43312e312b6465666175
[2026-11-18T13:08:04] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c70800018d58312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c70800000e3d312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c83500000b3a312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c83500018d65312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c83500000e3f312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3c96200000b2d312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3c96200018d7d312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3c96200000e3d312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ca8f00000b2b312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ca8f00018d74312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ca8f00000e44312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3cbbb00000b20312e312b6465666175
[2026-11-18T13:08:05] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3cbbb00018d7e312e312b6465666175
[2026-11-18T13:08:08] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3cbbb00000e4a312e312b6465666175
[2026-11-18T13:08:09] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3cce800000b16312e312b6465666175
[2026-11-18T13:08:10] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3cce800018d94312e312b6465666175
[2026-11-18T13:08:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3cce800000e50312e312b6465666175
[2026-11-18T13:08:12] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ce1500000b0f312e312b6465666175
[2026-11-18T13:08:13] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ce1500018d99312e312b6465666175
[2026-11-18T13:08:13] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ce1500000e58312e312b6465666175
[2026-11-18T13:08:13] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3cf4100000afb312e312b6465666175
[2026-11-18T13:08:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3cf4100018da0312e312b6465666175
[2026-11-18T13:08:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3cf4100000e5f312e312b6465666175
[2026-11-18T13:08:14] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d06e00000af2312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d06e00018da5312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d06e00000e64312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d19b00000aee312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d19b00018da7312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d19b00000e68312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d2c700000ae5312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d2c700018da8312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d2c700000e66312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d3f400000ad7312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d3f400018dbb312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d3f400000e6b312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d52100000ac8312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d52100018daf312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d52100000e6d312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d64d00000abb312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d64d00018dba312e312b6465666175
[2026-11-18T13:08:15] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d64d00000e6b312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d77a00000ab1312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d77a00018dc7312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d77a00000e67312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d8a700000ab0312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d8a700018dd3312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d8a700000e66312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3d9d400000aa6312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3d9d400018de5312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3d9d400000e5f312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3db0000000aa9312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3db0000018dea312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3db0000000e5c312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3dc2d00000aa3312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3dc2d00018df6312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3dc2d00000e59312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3dd5a00000a99312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3dd5a00018dfa312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3dd5a00000e5a312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3de8600000a94312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3de8600018dfe312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3de8600000e5e312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3dfb300000a99312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3dfb300018e08312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3dfb300000e5d312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e0e000000a97312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e0e000018e18312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e0e000000e5f312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e20c00000a96312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e20c00018e23312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e20c00000e62312e312b6465666175
[2026-11-18T13:08:16] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e33900000a98312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e33900018e35312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e33900000e66312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e46600000a93312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e46600018e3f312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e46600000e68312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e59200000a92312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e59200018e4b312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e59200000e6f312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e6bf00000a8f312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e6bf00018e44312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e6bf00000e76312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e7ec00000a89312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e7ec00018e45312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e7ec00000e7a312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3e91800000a85312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3e91800018e48312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3e91800000e77312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ea4500000a81312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ea4500018e47312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ea4500000e78312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3eb7200000a7f312e312b6465666175
[2026-11-18T13:08:17] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3eb7200018e49312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3eb7200000e77312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ec9f00000a78312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ec9f00018e49312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ec9f00000e72312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3edcb00000a75312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3edcb00018e4c312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3edcb00000e71312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3eef800000a6e312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3eef800018e5c312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3eef800000e74312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f02500000a5c312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f02500018e69312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f02500000e75312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f15100000a5c312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f15100018e61312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f15100000e76312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f27e00000a56312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f27e00018e67312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f27e00000e77312e312b6465666175
[2026-11-18T13:08:18] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f3ab00000a51312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f3ab00018e66312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f3ab00000e79312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f4d700000a4d312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f4d700018e56312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f4d700000e78312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f60400000a4f312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f60400018e55312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f60400000e7a312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f73100000a49312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f73100018e5b312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f73100000e7a312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f85d00000a52312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f85d00018e71312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f85d00000e78312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3f98a00000a56312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3f98a00018e82312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3f98a00000e7d312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3fab700000a4f312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3fab700018e85312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3fab700000e86312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3fbe400000a4a312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3fbe400018e95312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3fbe400000e8b312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3fd1000000a40312e312b6465666175
[2026-11-18T13:08:19] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3fd1000018e95312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3fd1000000e91312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3fe3d00000a3a312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3fe3d00018e97312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3fe3d00000e96312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db3ff6a00000a33312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db3ff6a00018ea7312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db3ff6a00000e99312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4009600000a2a312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4009600018ea8312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4009600000ea2312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db401c300000a23312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db401c300018ea6312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db401c300000eaa312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db402f000000a0f312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db402f000018eaa312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db402f000000eb2312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4041c00000a0b312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4041c00018ea8312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4041c00000ebf312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40549000009fc312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4054900018eaa312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4054900000ecc312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40676000009ee312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4067600018e9f312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4067600000ed9312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db407a2000009e4312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db407a200018ea7312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db407a200000ee1312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db408cf000009da312e312b6465666175
[2026-11-18T13:08:20] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db408cf00018ea3312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db408cf00000eee312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db409fc000009d2312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db409fc00018ead312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db409fc00000ef5312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40b29000009c9312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db40b2900018ea6312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db40b2900000f00312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40c55000009bc312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db40c5500018ea9312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db40c5500000f08312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40d82000009b4312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db40d8200018ea8312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db40d8200000f0f312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40eaf000009b3312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db40eaf00018eb3312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db40eaf00000f18312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db40fdb000009ac312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db40fdb00018ead312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db40fdb00000f1f312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4110800000999312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4110800018eb2312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4110800000f2a312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db412350000098b312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4123500018ebd312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4123500000f2e312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db413610000097b312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4136100018ebb312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4136100000f35312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4148e00000976312e312b6465666175
[2026-11-18T13:08:21] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4148e00018eb7312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4148e00000f39312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db415bb00000963312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db415bb00018eb7312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db415bb00000f41312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db416e700000967312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db416e700018eb4312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db416e700000f49312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db418140000095f312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4181400018eb8312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4181400000f57312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db419410000094a312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4194100018eb4312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4194100000f62312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db41a6e00000941312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db41a6e00018ec5312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db41a6e00000f6d312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db41b9a00000937312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db41b9a00018ec3312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db41b9a00000f79312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db41cc700000928312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db41cc700018ec4312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db41cc700000f7e312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db41df400000919312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db41df400018eb9312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db41df400000f80312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db41f2000000917312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db41f2000018eb9312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db41f2000000f85312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4204d00000903312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4204d00018eb6312e312b6465666175
[2026-11-18T13:08:22] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4204d00000f8a312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4217a000008f4312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4217a00018eae312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4217a00000f8d312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db422a6000008f2312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db422a600018eb2312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db422a600000f8f312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db423d3000008ed312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db423d300018eb0312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db423d300000f90312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42500000008ec312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4250000018eb2312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4250000000f95312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4262c000008e4312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4262c00018eb7312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4262c00000fa2312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42759000008cf312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4275900018ec1312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4275900000fae312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42886000008b0312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4288600018ec4312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4288600000fb7312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db429b2000008c9312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db429b200018ec4312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db429b200000fc0312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42adf000008ac312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db42adf00018ec9312e312b6465666175
[2026-11-18T13:08:23] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db42adf00000fcb312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42c0c000008b3312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db42c0c00018ed4312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db42c0c00000fd3312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42d39000008a4312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db42d3900018ed6312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db42d3900000fd8312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42e65000008ba312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db42e6500018ed5312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db42e6500000fdf312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db42f92000008b4312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db42f9200018ed9312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db42f9200000fe6312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db430bf000008a1312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db430bf00018ed6312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db430bf00000fec312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db431eb00000895312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db431eb00018ed7312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db431eb00000fed312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4331800000880312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4331800018ede312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4331800000fed312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4344500000885312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4344500018ee4312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4344500000fee312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4357100000884312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4357100018ee6312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4357100000ff2312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4369e0000086b312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4369e00018eee312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4369e00000ff4312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db437cb00000867312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db437cb00018ef3312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db437cb00000ff2312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db438f700000868312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db438f700018efb312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db438f700000ff1312e312b6465666175
[2026-11-18T13:08:24] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db43a240000086b312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db43a2400018efa312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db43a2400000ff7312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db43b5100000877312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db43b5100018efb312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db43b5100000ffe312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db43c7e00000862312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db43c7e00018efc312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db43c7e00001001312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db43daa00000871312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db43daa00018ef9312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db43daa00000ffd312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db43ed700000879312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db43ed700018f00312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db43ed700000fdc312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4400400000897312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4400400018f0a312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4400400000f83312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44130000008ca312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4413000018f08312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4413000000f3e312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4425d000008e0312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4425d00018f04312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4425d00000f03312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4438a00000911312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4438a00018f09312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4438a00000ed4312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db444b600000936312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db444b600018f0f312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db444b600000eaa312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db445e300000959312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db445e300018f12312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db445e300000e81312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db447100000097a312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4471000018f11312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4471000000e57312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4483c00000993312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4483c00018f11312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4483c00000e31312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44969000009b1312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4496900018f1a312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4496900000e0b312e312b6465666175
[2026-11-18T13:08:25] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44a96000009d2312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db44a9600018f16312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db44a9600000dea312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44bc3000009e9312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db44bc300018f17312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db44bc300000dc6312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44cef00000a02312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db44cef00018f15312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db44cef00000da4312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44e1c00000a1d312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db44e1c00018f11312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db44e1c00000d86312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db44f4900000a33312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db44f4900018eff312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db44f4900000d63312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4507500000a49312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4507500018efe312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4507500000d41312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db451a200000a64312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db451a200018f03312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db451a200000d22312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db452cf00000a7e312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db452cf00018f02312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db452cf00000d05312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db453fb00000a9a312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db453fb00018f0a312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db453fb00000ceb312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4552800000aad312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4552800018f19312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4552800000cd2312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4565500000ac2312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4565500018f19312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4565500000cb7312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4578100000adf312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4578100018f0c312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4578100000c9e312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db458ae00000af3312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db458ae00018f0e312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db458ae00000c87312e312b6465666175
[2026-11-18T13:08:26] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db459db00000b0d312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db459db00018f11312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db459db00000c6c312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db45b0800000b27312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db45b0800018f15312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db45b0800000c4c312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db45c3400000b47312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db45c3400018f14312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db45c3400000c26312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db45d6100000b70312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db45d6100018f14312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db45d6100000c00312e312b6465666175
[2026-11-18T13:08:27] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db45e8e00000b9d312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db45e8e00018f18312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db45e8e00000bd9312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db45fba00000bbf312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db45fba00018f14312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db45fba00000bb0312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db460e700000beb312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db460e700018f1b312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db460e700000b88312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4621400000c17312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4621400018f1a312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4621400000b61312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4634000000c3c312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4634000018f1b312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4634000000b43312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4646d00000c54312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4646d00018f1a312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4646d00000b41312e312b6465666175
[2026-11-18T13:08:28] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4659a00000c64312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4659a00018f20312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4659a00000b42312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db466c600000c6e312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db466c600018f1c312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db466c600000b39312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db467f300000c7f312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db467f300018f1f312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db467f300000b2d312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4692000000c8f312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4692000018f2b312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4692000000b20312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db46a4c00000ca2312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db46a4c00018f23312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db46a4c00000b11312e312b6465666175
[2026-11-18T13:08:29] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db46b7900000cb0312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db46b7900018f25312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db46b7900000b07312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db46ca600000cbb312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db46ca600018f25312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db46ca600000b03312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db46dd300000cbe312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db46dd300018f1d312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db46dd300000b00312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db46eff00000cc6312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db46eff00018f26312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db46eff00000af2312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4702c00000cd6312e312b6465666175
[2026-11-18T13:08:30] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4702c00018f27312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4702c00000ae4312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4715900000ce1312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4715900018f1a312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4715900000ad6312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4728500000cee312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4728500018f26312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4728500000acf312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db473b200000cf5312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db473b200018f28312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db473b200000aca312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db474df00000cfa312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db474df00018f26312e312b6465666175
[2026-11-18T13:08:31] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db474df00000ac0312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4760b00000d04312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4760b00018f2d312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4760b00000ab5312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4773800000d10312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4773800018f29312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4773800000aa9312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4786500000d1a312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4786500018f2c312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4786500000a9f312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4799100000d28312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4799100018f28312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4799100000a92312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db47abe00000d33312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db47abe00018f20312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db47abe00000a88312e312b6465666175
[2026-11-18T13:08:32] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db47beb00000d3d312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db47beb00018f23312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db47beb00000a80312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db47d1800000d44312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db47d1800018f2d312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db47d1800000a75312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db47e4400000d50312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db47e4400018f2b312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db47e4400000a6e312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db47f7100000d59312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db47f7100018f28312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db47f7100000a64312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4809e00000d64312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4809e00018f30312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4809e00000a58312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db481ca00000d6d312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db481ca00018f29312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db481ca00000a51312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db482f700000d75312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db482f700018f28312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db482f700000a46312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4842400000d7d312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4842400018f31312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4842400000a3d312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4855000000d88312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4855000018f38312e312b6465666175
[2026-11-18T13:08:33] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4855000000a35312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4867d00000d93312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4867d00018f37312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4867d00000a29312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db487aa00000da0312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db487aa00018f3d312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db487aa00000a22312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db488d600000da8312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db488d600018f44312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db488d600000a12312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48a0300000db8312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48a0300018f43312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48a03000009ff312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48b3000000dcf312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48b3000018f47312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48b30000009e7312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48c5d00000de4312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48c5d00018f47312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48c5d000009cf312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48d8900000e01312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48d8900018f46312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48d89000009b7312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48eb600000e21312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48eb600018f47312e312b6465666175
[2026-11-18T13:08:34] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48eb60000099f312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db48fe300000e35312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db48fe300018f48312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db48fe300000986312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4910f00000e50312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4910f00018f47312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4910f00000971312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4923c00000e68312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4923c00018f40312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4923c0000095c312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4936900000e80312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4936900018f3c312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4936900000949312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4949500000e9b312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4949500018f32312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4949500000936312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db495c200000eb2312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db495c200018f31312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db495c200000922312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db496ef00000ecb312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db496ef00018f38312e312b6465666175
[2026-11-18T13:08:35] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db496ef00000911312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4981b00000edf312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4981b00018f35312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4981b00000901312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4994800000ef5312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4994800018f2e312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49948000008f0312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db49a7500000f04312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db49a7500018f29312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49a75000008e1312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db49ba200000f18312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db49ba200018f22312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49ba2000008d3312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db49cce00000f2b312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db49cce00018f21312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49cce000008c5312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db49dfb00000f38312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db49dfb00018f25312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49dfb000008b3312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db49f2800000f4c312e312b6465666175
[2026-11-18T13:08:36] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db49f2800018f1d312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db49f28000008a8312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a05400000f5d312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a05400018f24312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a0540000089c312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a18100000f6e312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a18100018f26312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a1810000088d312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a2ae00000f84312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a2ae00018f15312e312b6465666175
[2026-11-18T13:08:37] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a2ae00000881312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a3da00000f93312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a3da00018f15312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a3da00000876312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a50700000fa9312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a50700018f0e312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a5070000086a312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a63400000fbc312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a63400018f06312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a63400000861312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a76000000fcb312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a76000018efe312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a76000000855312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a88d00000fdd312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a88d00018ef1312e312b6465666175
[2026-11-18T13:08:38] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a88d0000084d312e312b6465666175
[2026-11-18T13:08:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4a9ba00000fec312e312b6465666175
[2026-11-18T13:08:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4a9ba00018eda312e312b6465666175
[2026-11-18T13:08:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4a9ba00000841312e312b6465666175
[2026-11-18T13:08:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4aae600000ffa312e312b6465666175
[2026-11-18T13:08:39] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4aae600018ecc312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4aae60000083a312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ac130000100d312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ac1300018ec9312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ac1300000830312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ad400000101e312e312b6465666175
[2026-11-18T13:08:40] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ad4000018ec9312e312b6465666175
[2026-11-18T13:08:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ad4000000828312e312b6465666175
[2026-11-18T13:08:41] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ae6d0000102a312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ae6d00018eb9312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ae6d0000081d312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4af9900001037312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4af9900018eab312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4af9900000815312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b0c60000104b312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b0c600018ea1312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b0c60000080d312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b1f30000105b312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b1f300018e9d312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b1f300000804312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b31f0000106c312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b31f00018e98312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b31f000007fc312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b44c0000107f312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b44c00018e97312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b44c000007f4312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b5790000108a312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b57900018e92312e312b6465666175
[2026-11-18T13:08:42] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b579000007eb312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b6a500001095312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b6a500018e8d312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b6a5000007e2312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b7d20000109b312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b7d200018e90312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b7d2000007dc312e312b6465666175
[2026-11-18T13:08:43] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4b8ff000010af312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4b8ff00018e8f312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4b8ff000007d3312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ba2b000010ba312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ba2b00018e93312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ba2b000007ce312e312b6465666175
[2026-11-18T13:08:44] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4bb58000010cd312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4bb5800018e8f312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4bb58000007c7312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4bc85000010d2312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4bc8500018e98312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4bc85000007c3312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4bdb2000010db312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4bdb200018ea1312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4bdb2000007bc312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4bede000010e4312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4bede00018e9b312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4bede000007b5312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c00b000010ec312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c00b00018e98312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c00b000007af312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c138000010f9312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c13800018e95312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c138000007ab312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c26400001102312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c26400018e95312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c264000007a5312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c3910000110c312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c39100018e89312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c391000007a0312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c4be00001118312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c4be00018e7e312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c4be0000079c312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c5ea0000111f312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c5ea00018e87312e312b6465666175
[2026-11-18T13:08:45] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c5ea00000797312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c7170000112d312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c71700018e85312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c71700000791312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c84400001132312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c84400018e7d312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c8440000078b312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4c97000001136312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4c97000018e7e312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4c97000000787312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ca9d0000113e312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ca9d00018e7d312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ca9d00000781312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4cbca00001147312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4cbca00018e79312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4cbca0000077c312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ccf70000114a312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ccf700018e72312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ccf700000777312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ce2300001151312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ce2300018e6a312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ce2300000773312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4cf500000115b312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4cf5000018e62312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4cf5000000770312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d07d00001162312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d07d00018e60312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d07d0000076a312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d1a900001174312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d1a900018e63312e312b6465666175
[2026-11-18T13:08:46] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d1a900000767312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d2d600001181312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d2d600018e5e312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d2d600000763312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d4030000118b312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d40300018e5c312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d4030000075f312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d52f00001199312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d52f00018e58312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d52f0000075b312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d65c000011a9312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d65c00018e56312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d65c00000758312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d789000011b8312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d78900018e53312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d78900000753312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d8b5000011c4312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d8b500018e48312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d8b50000074f312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4d9e2000011cc312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4d9e200018e42312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4d9e20000074b312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4db0f000011ce312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4db0f00018e32312e312b6465666175
[2026-11-18T13:08:47] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4db0f00000746312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4dc3b000011d7312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4dc3b00018e2d312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4dc3b00000743312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4dd68000011e5312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4dd6800018e28312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4dd680000073f312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4de95000011fc312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4de9500018e21312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4de950000073d312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4dfc200001200312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4dfc200018e0f312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4dfc200000739312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e0ee00001205312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e0ee00018e05312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e0ee00000736312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e21b0000121d312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e21b00018e04312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e21b00000731312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e3480000122a312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e34800018dfc312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e3480000072e312e312b6465666175
[2026-11-18T13:08:48] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e47400001228312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e47400018df4312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e4740000072b312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e5a100001223312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e5a100018de5312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e5a10000072a312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e6ce0000122b312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e6ce00018de1312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e6ce00000726312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e7fa0000122f312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e7fa00018dd9312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e7fa00000723312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4e9270000122f312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4e92700018ddc312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4e92700000721312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ea540000123c312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ea5400018ddd312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ea540000071f312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4eb800000123c312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4eb8000018dd7312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4eb800000071c312e312b6465666175
[2026-11-18T13:08:49] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ecad0000123f312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ecad00018dd3312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ecad0000071a312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4edda0000123a312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4edda00018dd1312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4edda00000715312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ef0700001238312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ef0700018dc1312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ef0700000716312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f03300001234312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f03300018dc0312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f03300000714312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f16000001236312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f16000018db5312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f16000000711312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f28d0000123d312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f28d00018dae312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f28d00000710312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f3b90000123d312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f3b900018da9312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f3b90000070f312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f4e60000123b312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f4e600018d9e312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f4e60000070b312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f61300001239312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f61300018d82312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f61300000707312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f73f00001238312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f73f00018d71312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f73f00000707312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f86c0000123d312e312b6465666175
[2026-11-18T13:08:50] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f86c00018d71312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f86c00000705312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4f9990000123d312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4f99900018d76312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4f99900000703312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4fac500001239312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4fac500018d7d312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4fac500000700312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4fbf200001238312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4fbf200018d78312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4fbf2000006ff312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4fd1f00001237312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4fd1f00018d6d312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4fd1f000006fd312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4fe4c00001232312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4fe4c00018d61312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4fe4c000006fb312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db4ff7800001233312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db4ff7800018d64312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db4ff78000006f9312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db500a500001235312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db500a500018d6a312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db500a5000006f8312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db501d20000122b312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db501d200018d54312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db501d2000006f6312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db502fe0000122f312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db502fe00018d4f312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db502fe000006f4312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5042b0000122e312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5042b00018d5a312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5042b000006f3312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5055800001231312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5055800018d56312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50558000006f0312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5068400001234312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5068400018d5a312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50684000006ef312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db507b100001237312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db507b100018d62312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db507b1000006ed312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db508de00001239312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db508de00018d5c312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db508de000006ec312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50a0a0000123a312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50a0a00018d58312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50a0a000006ea312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50b3700001241312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50b3700018d6a312e312b6465666175
[2026-11-18T13:08:51] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50b37000006e7312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50c6400001247312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50c6400018d61312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50c64000006e4312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50d9100001250312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50d9100018d6a312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50d91000006e3312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50ebd00001252312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50ebd00018d5e312e312b6465666175
[2026-11-18T13:08:52] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50ebd000006e0312e312b6465666175
[2026-11-18T13:08:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db50fea00001257312e312b6465666175
[2026-11-18T13:08:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db50fea00018d67312e312b6465666175
[2026-11-18T13:08:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db50fea000006df312e312b6465666175
[2026-11-18T13:08:53] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5111700001258312e312b6465666175
[2026-11-18T13:08:54] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5111700018d6b312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51117000006dc312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db512430000125d312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5124300018d67312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51243000006d9312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db513700000125e312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5137000018d5d312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51370000006d8312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5149d0000125e312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5149d00018d59312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5149d000006d6312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db515c90000125f312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db515c900018d4b312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db515c9000006d6312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db516f600001263312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db516f600018d4a312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db516f6000006d4312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5182300001265312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5182300018d3b312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51823000006d1312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5194f00001267312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5194f00018d33312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5194f000006d0312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db51a7c0000126e312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db51a7c00018d31312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51a7c000006ce312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db51ba90000126f312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db51ba900018d35312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51ba9000006cc312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db51cd500001274312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db51cd500018d38312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51cd5000006ca312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db51e0200001277312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db51e0200018d39312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51e02000006c8312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db51f2f00001278312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db51f2f00018d45312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db51f2f000006c8312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5205c0000127b312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5205c00018d3d312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5205c000006c6312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db521880000127f312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5218800018d42312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52188000006c4312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db522b50000127b312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db522b500018d45312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db522b5000006c2312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db523e200001280312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db523e200018d44312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db523e2000006c0312e312b6465666175
[2026-11-18T13:08:56] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5250e00001280312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5250e00018d4d312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5250e000006be312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5263b00001288312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5263b00018d53312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5263b000006bf312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5276800001290312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5276800018d4c312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52768000006bb312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5289400001297312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5289400018d41312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52894000006b9312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db529c1000012ad312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db529c100018d4f312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db529c1000006b8312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db52aee000012b8312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db52aee00018d44312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52aee000006b7312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db52c1a000012b7312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db52c1a00018d42312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52c1a000006b5312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db52d47000012c0312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db52d4700018d41312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52d47000006b4312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db52e74000012c5312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db52e7400018d42312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52e74000006b4312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db52fa1000012d0312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db52fa100018d4e312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db52fa1000006b3312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db530cd000012d5312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db530cd00018d4f312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db530cd000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db531fa000012e2312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db531fa00018d50312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db531fa000006b2312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53327000012fe312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5332700018d50312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53327000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db534530000130d312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5345300018d43312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53453000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5358000001318312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5358000018d35312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53580000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db536ad0000132b312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db536ad00018d2b312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db536ad000006b0312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db537d900001347312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db537d900018d22312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db537d9000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5390600001349312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5390600018d23312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53906000006af312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53a3300001347312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db53a3300018d23312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53a33000006af312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53b5f0000133d312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db53b5f00018d13312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53b5f000006af312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53c8c00001338312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db53c8c00018d0e312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53c8c000006af312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53db90000133c312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db53db900018d0c312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53db9000006af312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db53ee600001343312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db53ee600018d06312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db53ee6000006b1312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5401200001345312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5401200018cfd312e312b6465666175
[2026-11-18T13:08:57] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54012000006b2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5413f0000133f312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5413f00018cfb312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5413f000006b4312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5426c0000133a312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5426c00018cfd312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5426c000006b7312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db543980000132e312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5439800018cfc312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54398000006b9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db544c500001314312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db544c500018cf2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db544c5000006bb312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db545f20000130e312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db545f200018cf7312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db545f2000006bf312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5471e0000130c312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5471e00018cf5312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5471e000006bf312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5484b000012f4312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5484b00018d00312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5484b000006c2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54978000012ee312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5497800018cfb312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54978000006c5312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54aa4000012e2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db54aa400018cf9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54aa4000006c9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54bd1000012d7312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db54bd100018cfa312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54bd1000006ca312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54cfe000012c1312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db54cfe00018cee312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54cfe000006cf312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54e2b0000128a312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db54e2b00018ceb312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54e2b000006d1312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db54f5700001264312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db54f5700018cea312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db54f57000006d3312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5508400001263312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5508400018ce2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55084000006d8312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db551b10000124d312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db551b100018cdd312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db551b1000006db312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db552dd00001265312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db552dd00018cd0312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db552dd000006df312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5540a00001269312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5540a00018cd2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5540a000006e3312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5553700001289312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5553700018cca312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55537000006e9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5566300001294312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5566300018cbc312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55663000006ee312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55790000012b9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5579000018cb2312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55790000006f4312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db558bd000012a6312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db558bd00018cb8312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db558bd000006fb312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db559e9000012a9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db559e900018cab312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db559e900000700312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55b16000012ac312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db55b1600018ca9312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55b1600000703312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55c43000012c0312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db55c4300018ca3312e312b6465666175
[2026-11-18T13:08:58] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55c430000070a312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55d6f000012af312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db55d6f00018c99312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55d6f00000710312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55e9c000012a8312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db55e9c00018c9a312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55e9c00000713312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db55fc90000128f312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db55fc900018c96312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db55fc900000717312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db560f60000127f312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db560f600018c94312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db560f60000071d312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5622200001241312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5622200018c8b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5622200000721312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5634f0000123a312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5634f00018c88312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5634f00000723312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5647c00001227312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5647c00018c8b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5647c00000724312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db565a800001233312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db565a800018c87312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db565a800000726312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db566d50000124c312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db566d500018c79312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db566d500000729312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db568020000126b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5680200018c68312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db568020000072a312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5692e0000126c312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5692e00018c62312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5692e0000072c312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db56a5b0000126b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db56a5b00018c55312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db56a5b0000072e312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db56b880000126c312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db56b8800018c50312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db56b8800000731312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db56cb400001275312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db56cb400018c46312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db56cb400000733312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db56de100001275312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db56de100018c36312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db56de100000733312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db56f0e0000127c312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db56f0e00018c26312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db56f0e00000734312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5703b00001279312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5703b00018c18312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5703b00000736312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5716700001272312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5716700018c13312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5716700000738312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5729400001263312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5729400018c02312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5729400000738312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db573c10000126b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db573c100018bfe312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db573c100000738312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db574ed0000127a312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db574ed00018be7312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db574ed00000739312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5761a00001286312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5761a00018bdd312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5761a00000738312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5774700001290312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5774700018bd7312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db577470000073b312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57873000012a0312e312b6465666175
[2026-11-18T13:08:59] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5787300018bd3312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5787300000739312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db579a0000012af312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db579a000018bcf312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db579a00000073c312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57acd000012c0312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db57acd00018bc8312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db57acd0000073b312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57bf9000012de312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db57bf900018bc0312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db57bf900000739312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57d26000012f5312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db57d2600018bc1312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db57d260000073a312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57e5300001310312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db57e5300018baf312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db57e530000073c312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db57f800000131d312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db57f8000018b98312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db57f800000073d312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db580ac00001326312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db580ac00018b96312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db580ac0000073f312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db581d900001337312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db581d900018b8b312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db581d90000073f312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5830600001348312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5830600018b88312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5830600000740312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5843200001359312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a32105db5843200018b80312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a30105db5843200000742312e312b6465666175
[2026-11-18T13:09:00] [DD:4C:E8:7A:11:6E] <== BT_ATT_OP_HANDLE_VAL_NOT Handle: 001b Value: 3a31105db5855f00001364312e312b6465666175
Killed

```