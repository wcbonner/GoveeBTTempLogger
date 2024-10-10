#include <algorithm>
#include <arpa/inet.h>
#include <bluetooth/bluetooth.h> // apt install libbluetooth-dev
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <cfloat>
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dbus/dbus.h> //  sudo apt install libdbus-1-dev
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>
#include <vector>
#include "wimiso8601.h"
#include "att-types.h"
#include "uuid.h"
#include "goveebttemplogger.h"
#include "goveebttemplogger-hci.h"

extern int ConsoleVerbosity;
extern std::string ProgramVersionString;
extern volatile bool bRun;
extern int DaysBetweenDataDownload;
extern int LogFileTime;
extern size_t DAY_SAMPLE;
extern std::filesystem::path LogDirectory;
extern std::filesystem::path SVGDirectory;
extern std::map<bdaddr_t, std::vector<Govee_Temp>> GoveeMRTGLogs; // memory map of BT addresses and vector structure similar to MRTG Log Files
extern std::map<bdaddr_t, std::queue<Govee_Temp>> GoveeTemperatures;
extern std::map<bdaddr_t, ThermometerType> GoveeThermometers;
extern std::map<bdaddr_t, time_t> GoveeLastDownload;
/////////////////////////////////////////////////////////////////////////////
#ifndef BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS
#define BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS		0x2041
int hci_le_set_ext_scan_parameters(int dd, uint8_t type, uint16_t interval, uint16_t window, uint8_t own_type, uint8_t filter, int to)
{
	struct bt_hci_cmd_le_set_ext_scan_params {
		uint8_t  own_addr_type;
		uint8_t  filter_policy;
		uint8_t  num_phys;
		uint8_t  type;
		uint16_t interval;
		uint16_t window;
	} __attribute__((packed)) param_cp;
	memset(&param_cp, 0, sizeof(param_cp));
	param_cp.type = type;
	param_cp.interval = interval;
	param_cp.window = window;
	param_cp.own_addr_type = own_type;
	param_cp.filter_policy = filter;
	param_cp.num_phys = 1;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS;
	rq.cparam = &param_cp;
	rq.clen = sizeof(bt_hci_cmd_le_set_ext_scan_params);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // !BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS
#ifndef BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE
#define BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE		0x2042
int hci_le_set_ext_scan_enable(int dd, uint8_t enable, uint8_t filter_dup, int to)
{
	struct bt_hci_cmd_le_set_ext_scan_enable {
		uint8_t  enable;
		uint8_t  filter_dup;
		uint16_t duration;
		uint16_t period;
	} __attribute__((packed)) scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = enable;
	scan_cp.filter_dup = filter_dup;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE;
	rq.cparam = &scan_cp;
	rq.clen = sizeof(scan_cp);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // !BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE
#ifndef BT_HCI_CMD_LE_SET_RANDOM_ADDRESS
// 2023-11-29 Added this function to fix problem with Raspberry Pi Zero 2 W Issue https://github.com/wcbonner/GoveeBTTempLogger/issues/50
int hci_le_set_random_address(int dd, int to)
{
	le_set_random_address_cp scan_cp{ 0 };
	std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());	// 2023-12-01 switch to c++ std library <random>
	for (auto& b : scan_cp.bdaddr.b)
		b = generator() % 256;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_RANDOM_ADDRESS;
	rq.cparam = &scan_cp;
	rq.clen = sizeof(scan_cp);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // BT_HCI_CMD_LE_SET_RANDOM_ADDRESS
/////////////////////////////////////////////////////////////////////////////
std::string iBeacon(const uint8_t* const data)
{
	std::ostringstream ssValue;
	const size_t data_len = data[0];
	if (data[1] == 0xFF) // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/ «Manufacturer Specific Data»
	{
		if ((data[2] == 0x4c) && (data[3] == 0x00))
		{
			ssValue << " (Apple)";
			if ((data[4] == 0x02) && (data[5] == 0x15)) // SubType: 0x02 (iBeacon) && SubType Length: 0x15
			{
				ssValue << " (UUID) ";
				for (auto index = 6; index < 22; index++)
					ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[index]);
				ssValue << " (Major) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[22]);
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[23]);
				ssValue << " (Minor) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[24]);
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[25]);
				ssValue << " (RSSI) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[26]);
				// https://en.wikipedia.org/wiki/IBeacon
				// https://scapy.readthedocs.io/en/latest/layers/bluetooth.html#apple-ibeacon-broadcast-frames
				// https://atadiat.com/en/e-bluetooth-low-energy-ble-101-tutorial-intensive-introduction/
				// https://deepai.org/publication/handoff-all-your-privacy-a-review-of-apple-s-bluetooth-low-energy-continuity-protocol
			}
			else
			{
				// 2 3  4  5  6 7 8 9 0 1 2 3 4 5 6 7 8 9 0  1 2  3 4  5
				// 4C00 02 15 494E54454C4C495F524F434B535F48 5750 740F 5CC2
				// 4C00 02 15494E54454C4C495F524F434B535F48 5750 75F2 FFC2
				ssValue << " ";
				for (size_t index = 4; index < data_len; index++)
					ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[index]);
				// Apple Company Code: 0x004C
				// UUID 16 bytes
				// Major 2 bytes
				// Minor 2 bytes
			}
		}
	}
	return(ssValue.str());
}
/////////////////////////////////////////////////////////////////////////////
const char* addr_type_name(const int dst_type)
{
	switch (dst_type)
	{
	case BDADDR_BREDR: return "BDADDR_BREDR";
	case BDADDR_LE_PUBLIC: return "BDADDR_LE_PUBLIC";
	case BDADDR_LE_RANDOM: return "BDADDR_LE_RANDOM";
	default: return NULL;
	}
}
#define ATT_CID 4
typedef struct __attribute__((__packed__)) { uint8_t opcode; uint16_t starting_handle; uint16_t ending_handle; uint16_t UUID; } GATT_DeclarationPacket;
typedef struct __attribute__((__packed__)) { uint8_t opcode; uint16_t handle; uint8_t buf[20]; } GATT_WritePacket;
class BlueToothServiceCharacteristic { public: uint16_t starting_handle; uint8_t properties; uint16_t ending_handle; bt_uuid_t theUUID; };
class BlueToothService { public: bt_uuid_t theUUID; uint16_t starting_handle; uint16_t ending_handle; std::vector<BlueToothServiceCharacteristic> characteristics; };
const int bt_TimeOut = 1000;
bool operator ==(const bt_uuid_t& a, const bt_uuid_t& b)
{
	if (a.type != b.type)
		return(false);
	else
		switch (a.type)
		{
		case 16:
			return (a.value.u16 == b.value.u16);
		case 32:
			return (a.value.u32 == b.value.u32);
		case 128:
		default:
			if (a.value.u128.data[0] != b.value.u128.data[0])
				return (false);
			if (a.value.u128.data[1] != b.value.u128.data[1])
				return (false);
			if (a.value.u128.data[2] != b.value.u128.data[2])
				return (false);
			if (a.value.u128.data[3] != b.value.u128.data[3])
				return (false);
			if (a.value.u128.data[4] != b.value.u128.data[4])
				return (false);
			if (a.value.u128.data[5] != b.value.u128.data[5])
				return (false);
			if (a.value.u128.data[6] != b.value.u128.data[6])
				return (false);
			if (a.value.u128.data[7] != b.value.u128.data[7])
				return (false);
			if (a.value.u128.data[8] != b.value.u128.data[8])
				return (false);
			if (a.value.u128.data[9] != b.value.u128.data[9])
				return (false);
			if (a.value.u128.data[10] != b.value.u128.data[10])
				return (false);
			if (a.value.u128.data[11] != b.value.u128.data[11])
				return (false);
			if (a.value.u128.data[12] != b.value.u128.data[12])
				return (false);
			if (a.value.u128.data[13] != b.value.u128.data[13])
				return (false);
			if (a.value.u128.data[14] != b.value.u128.data[14])
				return (false);
			if (a.value.u128.data[15] != b.value.u128.data[15])
				return (false);
			return (true);
		}
	return(false);
}
std::string bt_UUID_2_String(const bt_uuid_t* uuid)
{
	char local[37] = { 0 };
	bt_uuid_to_string(uuid, local, sizeof(local));
	std::string rVal(local);
	if (uuid->type == uuid->BT_UUID16)
	{
		std::stringstream ss;
		ss << std::hex << std::setw(4) << std::setfill('0') << uuid->value.u16;
		switch (uuid->value.u16)
		{
			// https://btprodspecificationrefs.blob.core.windows.net/assigned-numbers/Assigned%20Number%20Types/Assigned%20Numbers.pdf
		case 0x1800:
			ss << " (Generic Access)";
			break;
		case 0x1801:
			ss << " (Generic Attribute)";
			break;
		case 0x180A:
			ss << " (Device Information)";
			break;
		case 0xFEF5:
			ss << " (Dialog Semiconductor GmbH)";
			break;
		case 0x2A00:
			ss << " (Device Name)";
			break;
		case 0x2A01:
			ss << " (Appearance)";
			break;
		case 0x2A04:
			ss << " (Peripheral Preferred Connection Parameters)";
			break;
		case 0x2A05:
			ss << " (Service Changed)";
			break;
		case 0x2A29:
			ss << " (Manufacturer Name String)";
			break;
		case 0x2A24:
			ss << " (Model Number String)";
			break;
		case 0x2A26:
			ss << " (Firmware Revision String)";
			break;
		case 0x2A28:
			ss << " (Software Revision String)";
			break;
		case 0x2A23:
			ss << " (System ID)";
			break;
		case 0x2A50:
			ss << " (PnP ID)";
			break;
		case 0x2901:
			ss << " (Characteristic User Description)";
			break;
		case 0x2902:
			ss << " (Client Characteristic Configuration)";
		}
		rVal = ss.str();
	}
	return(rVal);
}
// My command to stop and start bluetooth scanning
int bt_LEScan(int BlueToothDevice_Handle, const bool enable, const std::set<bdaddr_t>& BT_WhiteList, const bool HCI_Passive_Scanning)
{
	uint8_t bt_ScanType(0x01);		// Scan Type: Active (0x01)
	// In passive scanning, the BLE module just listens to other node advertisements.
	// in active scanning the module will request more information once an advertisement is received, and the advertiser will answer with information like friendly name and supported profiles.
	if (HCI_Passive_Scanning)
		bt_ScanType = 0x00;
	// For a long time my code set bt_ScanInterval(0x0012) bt_ScanWindow(0x0012) followed by bt_ScanInterval(0x1f40) bt_ScanWindow(0x1f40)
	//const uint16_t bt_ScanInterval(18);	// Scan Interval: 18 (11.25 msec) (how long to wait between scans).
	//const uint16_t bt_ScanWindow(18);	// Scan Window: 18 (11.25 msec) (how long to scan)
	//const uint16_t bt_ScanInterval(8000);	// Scan Interval: 8000 (5000 msec) (how long to wait between scans).
	//const uint16_t bt_ScanWindow(800);	// Scan Window: 800 (500 msec) (how long to scan)
	//const uint16_t bt_ScanInterval(96);	// Scan Interval: 96 (60 msec) (how long to wait between scans).
	//const uint16_t bt_ScanWindow(48);	// Scan Window: 48 (30 msec) (how long to scan)
	// Interval and Window are in steps of 0.625ms
	// Apple's foreground mode of 30 ms scanWindow with 40 ms scanInterval means that for a base
	// advertising interval of 1022.5 ms that you see the device within 1 second about 3/4ths of
	// the time and always within 2 seconds, assuming no RF interference obscuring the advertising
	// packet. In background mode with a 30 ms scanWindow and 300 ms scanInterval, the median time
	// becomes 5 seconds and the usual maximum becomes 19 seconds though with very bad luck of the
	// random shifts it could be a little longer.
	//const uint16_t bt_ScanInterval(64);	// Scan Interval: 64 (40 msec) (how long to wait between scans).
	//const uint16_t bt_ScanWindow(48);	// Scan Window: 48 (30 msec) (how long to scan)
	// 2023-10-08 I'm still having problems recieving data from many of my h5074 devices, so am trying a set of scan parameters that I'll cycle through each time I enable scanning
	static std::vector<std::pair<uint16_t, uint16_t>> ScanParameterList;	// Pair corresponding to ScanInterval and ScanWindow
	if (ScanParameterList.empty())
	{
		ScanParameterList.push_back(std::make_pair(18, 18));
		ScanParameterList.push_back(std::make_pair(8000, 800));
		ScanParameterList.push_back(std::make_pair(8000, 8000));
		ScanParameterList.push_back(std::make_pair(8000, 3200));
		ScanParameterList.push_back(std::make_pair(64, 48));
		ScanParameterList.push_back(std::make_pair(96, 48));
	}
	const uint8_t bt_ScanFilterDuplicates(0x00);	// Set this once, to make sure I'm consistent through the file.
	// https://development.libelium.com/ble-networking-guide/scanning-ble-devices
	// https://electronics.stackexchange.com/questions/82098/ble-scan-interval-and-window
	// https://par.nsf.gov/servlets/purl/10275622
	// https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum/616269/cc2640r2f-q1-how-scan-interval-window-work-during-scanning-duration
	// https://www.scirp.org/journal/paperinformation.aspx?paperid=106311
	// https://microchipdeveloper.com/wireless:ble-link-layer-discovery has a nice description of Passive Scanning vs Active Scanning
	int btRVal = 0;
	uint8_t bt_ScanFilterPolicy = 0x00; // Scan Filter Policy: Accept all advertisements, except directed advertisements not addressed to this device (0x00)
	if (enable)
	{
		time_t TimeNow;
		time(&TimeNow);
		static auto ScanParameters = ScanParameterList.begin();
		auto bt_ScanInterval(ScanParameters->first);
		auto bt_ScanWindow(ScanParameters->second);

		static time_t LastScanEnableMessage = TimeNow;
		bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList, HCI_Passive_Scanning); // call this routine recursively to disable any existing scanning
		if (!BT_WhiteList.empty())
		{
			const bdaddr_t TestAddress = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }; // BDADDR_ALL;
			if (TestAddress == *BT_WhiteList.begin()) // if first element in whitelist is FFFFFFFFFF
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] BlueTooth Address Filter:";
				else
					if (difftime(TimeNow, LastScanEnableMessage) > (60 * 5)) // Reduce Spamming Syslog
						std::cerr << "BlueTooth Address Filter:";
				for (auto& iter : GoveeMRTGLogs)
				{
					const bdaddr_t FilterAddress(iter.first);
					bool bRandomAddress = (FilterAddress.b[5] >> 4 == 0xC || FilterAddress.b[5] >> 4 == 0xD); // If the two most significant bits of the address are set to 1, it is defined as a Random Static Address
					hci_le_add_white_list(BlueToothDevice_Handle, &FilterAddress, (bRandomAddress ? LE_RANDOM_ADDRESS : LE_PUBLIC_ADDRESS), bt_TimeOut);
					if (ConsoleVerbosity > 0)
						std::cout << " [" << ba2string(FilterAddress) << "]";
					else
						if (difftime(TimeNow, LastScanEnableMessage) > (60 * 5)) // Reduce Spamming Syslog
							std::cerr << " [" << ba2string(FilterAddress) << "]";
				}
				if (ConsoleVerbosity > 0)
					std::cout << std::endl;
				else
					if (difftime(TimeNow, LastScanEnableMessage) > (60 * 5)) // Reduce Spamming Syslog
						std::cerr << std::endl;
			}
			else
			{
				if (ConsoleVerbosity > 1)
					std::cout << "[" << getTimeISO8601(true) << "] BlueTooth Address Filter:";
				for (auto& iter : BT_WhiteList)
				{
					const bdaddr_t FilterAddress(iter);
					bool bRandomAddress = (FilterAddress.b[5] >> 4 == 0xC || FilterAddress.b[5] >> 4 == 0xD); // If the two most significant bits of the address are set to 1, it is defined as a Random Static Address
					hci_le_add_white_list(BlueToothDevice_Handle, &FilterAddress, (bRandomAddress ? LE_RANDOM_ADDRESS : LE_PUBLIC_ADDRESS), bt_TimeOut);
					if (ConsoleVerbosity > 1)
						std::cout << " [" << ba2string(FilterAddress) << "]";
				}
				if (ConsoleVerbosity > 1)
					std::cout << std::endl;
			}
			bt_ScanFilterPolicy = 0x01; // Scan Filter Policy: Accept only advertisements from devices in the White List. Ignore directed advertisements not addressed to this device (0x01)
		}
		btRVal = hci_le_set_scan_parameters(BlueToothDevice_Handle, bt_ScanType, htobs(bt_ScanInterval), htobs(bt_ScanWindow), LE_RANDOM_ADDRESS, bt_ScanFilterPolicy, bt_TimeOut);
		// It's been reported that on Linux version 5.19.0-28-generic (x86_64) the bluetooth scanning produces an error,
		// This custom code setting extended scan parameters is an attempt to work around the issue (2023-02-06)
		if (btRVal < 0)
			// If the standard scan parameters commands fails, try the extended command.
			btRVal = hci_le_set_ext_scan_parameters(BlueToothDevice_Handle, bt_ScanType, htobs(bt_ScanInterval), htobs(bt_ScanWindow), LE_RANDOM_ADDRESS, bt_ScanFilterPolicy, bt_TimeOut);
		if (btRVal < 0)
			std::cerr << "[                   ] Error: Failed to set scan parameters: " << strerror(errno) << std::endl;
		else
		{
			btRVal = hci_le_set_scan_enable(BlueToothDevice_Handle, 0x01, bt_ScanFilterDuplicates, bt_TimeOut);
			if (btRVal < 0)
				// If the standard scan enable commands fails, try the extended command.
				btRVal = hci_le_set_ext_scan_enable(BlueToothDevice_Handle, 0x01, bt_ScanFilterDuplicates, bt_TimeOut);
			if (btRVal < 0)
			{
				std::cerr << "[                   ] Error: Failed to enable scan: " << strerror(errno) << " (" << errno << ")" << std::endl;
				if (errno == EPERM)
				{
					std::cerr << "**********************************************************" << std::endl;
					std::cerr << " NOTE: This program lacks the permissions necessary for" << std::endl;
					std::cerr << "  manipulating the raw Bluetooth HCI socket, which" << std::endl;
					std::cerr << "  is required for scanning and for setting the minimum" << std::endl;
					std::cerr << "  connection inverval to speed up data transfer.\n" << std::endl << std::endl;
					std::cerr << "  To fix this, run it as root or, better yet, set the" << std::endl;
					std::cerr << "  following capabilities on the GoveeBTTempLogger executable:\n" << std::endl << std::endl;
					std::cerr << "  # sudo setcap 'cap_net_raw,cap_net_admin+eip' goveebttemplogger\n" << std::endl << std::endl;
					std::cerr << "**********************************************************" << std::endl;
				}
			}
			else
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Scanning Started. ScanInterval(" << double(bt_ScanInterval) * 0.625 << " msec) ScanWindow(" << double(bt_ScanWindow) * 0.625 << " msec) ScanType(" << uint(bt_ScanType) << ")" << std::endl;
				else
				{
					if (difftime(TimeNow, LastScanEnableMessage) > (60 * 5)) // Reduce Spamming Syslog
					{
						LastScanEnableMessage = TimeNow;
						std::cerr << ProgramVersionString << " (listening for Bluetooth Low Energy Advertisements) ScanInterval(" << double(bt_ScanInterval) * 0.625 << " msec) ScanWindow(" << double(bt_ScanWindow) * 0.625 << " msec) ScanType(" << uint(bt_ScanType) << ")" << std::endl;
					}
				}
			}
		}
		if (++ScanParameters == ScanParameterList.end())
			ScanParameters = ScanParameterList.begin();
	}
	else
	{
		btRVal = hci_le_set_scan_enable(BlueToothDevice_Handle, 0x00, bt_ScanFilterDuplicates, bt_TimeOut);
		if (btRVal < 0)
			// If the standard scan enable commands fails, try the extended command.
			btRVal = hci_le_set_ext_scan_enable(BlueToothDevice_Handle, 0x00, bt_ScanFilterDuplicates, bt_TimeOut);
		if (!BT_WhiteList.empty())
		{
			hci_le_clear_white_list(BlueToothDevice_Handle, bt_TimeOut);
			bt_ScanFilterPolicy = 0x00; // Scan Filter Policy: Accept all advertisements, except directed advertisements not addressed to this device (0x00)
		}
		//if (hci_le_set_scan_parameters(BlueToothDevice_Handle, bt_ScanType, htobs(18), htobs(18), LE_RANDOM_ADDRESS, bt_ScanFilterPolicy, bt_TimeOut) < 0)
		//	hci_le_set_ext_scan_parameters(BlueToothDevice_Handle, bt_ScanType, htobs(18), htobs(18), LE_RANDOM_ADDRESS, bt_ScanFilterPolicy, bt_TimeOut);
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] Scanning Stopped." << std::endl;
	}
	return(btRVal);
}
/////////////////////////////////////////////////////////////////////////////
void bt_ListDevices(void)
{
	// https://www.linumiz.com/bluetooth-list-available-controllers/
	// I used the blog post above to learn develop an HCI routine to list the bluetooth devices

	std::ostringstream ssOutput;
	std::vector<struct hci_dev_info> hci_devices;
	for (auto i = 0; i < HCI_MAX_DEV; i++)
	{
		struct hci_dev_info hci_device_info;
		if (hci_devinfo(i, &hci_device_info) == 0)
			hci_devices.push_back(hci_device_info);
	}
	for (auto& hci_device : hci_devices)
	{
		if (hci_test_bit(HCI_UP, &hci_device.flags))
			if (ConsoleVerbosity > 0)
				ssOutput << "[                   ] ";
		ssOutput << "Host Controller Address: " << ba2string(hci_device.bdaddr) << " BlueTooth Device ID: " << hci_device.dev_id << " HCI Name: " << hci_device.name << std::endl;
	}
	if (ConsoleVerbosity > 0)
		std::cout << ssOutput.str();
	else
		std::cerr << ssOutput.str();
}
/////////////////////////////////////////////////////////////////////////////
// Connect to a Govee Thermometer device over Bluetooth and download its historical data.
time_t ConnectAndDownload(int BlueToothDevice_Handle, const bdaddr_t GoveeBTAddress, const time_t GoveeLastReadTime = 0, const int BatteryToRecord = 0)
{
	time_t TimeDownloadStart(0);
	uint16_t DataPointsRecieved(0);
	uint16_t offset(0);
	// Save the current HCI filter (Host Controller Interface)
	struct hci_filter original_filter;
	socklen_t olen = sizeof(original_filter);
	if (0 == getsockopt(BlueToothDevice_Handle, SOL_HCI, HCI_FILTER, &original_filter, &olen))
	{
		// Bluetooth HCI Command - LE Create Connection (BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f))
		uint16_t handle = 0;
		bool bRandomAddress = (GoveeBTAddress.b[5] >> 4 == 0xC || GoveeBTAddress.b[5] >> 4 == 0xD); // If the two most significant bits of the address are set to 1, it is defined as a Random Static Address
		int iRet = hci_le_create_conn(
			BlueToothDevice_Handle,
			96, // interval, Scan Interval: 96 (60 msec)
			48, // window, Scan Window: 48 (30 msec)
			0x00, // initiator_filter, Initiator Filter Policy: Use Peer Address (0x00)
			(bRandomAddress ? LE_RANDOM_ADDRESS : LE_PUBLIC_ADDRESS), // peer_bdaddr_type, Peer Address Type: Public Device Address (0x00)
			GoveeBTAddress, // BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)
			LE_RANDOM_ADDRESS, // own_bdaddr_type, Own Address Type: Random Device Address (0x01)
			24, // min_interval, Connection Interval Min: 24 (30 msec)
			40, // max_interval, Connection Interval Max: 40 (50 msec)
			0, // latency, Connection Latency: 0 (number events)
			2000, // supervision_timeout, Supervision Timeout: 2000 (20 sec)
			0, // min_ce_length, Min CE Length: 0 (0 msec)
			0, // max_ce_length, Max CE Length: 0 (0 msec)
			&handle,
			15000);	// A 15 second timeout gives me a better chance of success
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] hci_le_create_conn Return(" << std::dec << iRet << ") handle (" << std::hex << std::setw(4) << std::setfill('0') << handle << ")" << std::endl;
#ifdef BT_READ_REMOTE_FEATURES
		if ((iRet == 0) && (handle != 0))
		{
			// Bluetooth HCI Command - LE Read Remote Features
			uint8_t features[8];
			if (hci_le_read_remote_features(BlueToothDevice_Handle, handle, features, 15000) != -1)
			{
				char* cp = lmp_featurestostr(features, "", 50);
				if (cp != NULL)
				{
					std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "]     Features: " << cp << std::endl;
					bt_free(cp);
				}
			}
		}
#endif
#ifdef BT_READ_REMOTE_VERSION
		if ((iRet == 0) && (handle != 0))
		{
			// Bluetooth HCI Command - Read Remote Version Information
			struct hci_version ver;
			iRet = hci_read_remote_version(BlueToothDevice_Handle, handle, &ver, 15000);
			if (iRet != -1)
			{
				std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "]      Version: " << lmp_vertostr(ver.lmp_ver) << std::endl;
				std::cout << "[-------------------] [" << ba2string(GoveeBTAddress) << "]   Subversion: " << std::hex << std::setw(2) << std::setfill('0') << ver.lmp_subver << std::endl;
				std::cout << "[-------------------] [" << ba2string(GoveeBTAddress) << "] Manufacturer: " << bt_compidtostr(ver.manufacturer) << std::endl;
			}
		}
#endif
#ifdef BT_CONN_UPDATE
		if ((iRet == 0) && (handle != 0))
		{
			iRet = hci_le_conn_update(
				BlueToothDevice_Handle,
				handle,
				6,		//Connection Interval Min : 6 (7.5 msec)
				6,		//Connection Interval Max : 6 (7.5 msec)
				0,		//Connection Latency : 0 (number events)
				2000,	//Supervision Timeout : 2000 (20 sec)
				bt_TimeOut);
		}
#endif
		if ((iRet == 0) && (handle != 0))
		{
			// allocate a socket
			int l2cap_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
			if (l2cap_socket < 0)
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Failed to create L2CAP socket: " << strerror(errno) << " (" << errno << ")" << std::endl;
			}
			else
			{
				/* Set up source address */
				struct sockaddr_l2 srcaddr;
				memset(&srcaddr, 0, sizeof(srcaddr));
				srcaddr.l2_family = AF_BLUETOOTH;
				srcaddr.l2_cid = htobs(ATT_CID);
				srcaddr.l2_bdaddr_type = BDADDR_LE_RANDOM;
				if (bind(l2cap_socket, (struct sockaddr*)&srcaddr, sizeof(srcaddr)) < 0)
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Failed to bind L2CAP socket: " << strerror(errno) << " (" << errno << ")" << std::endl;
					close(l2cap_socket);
				}
				else
				{
					/* Set up destination address */
					struct sockaddr_l2 dstaddr;
					memset(&dstaddr, 0, sizeof(dstaddr));
					dstaddr.l2_family = AF_BLUETOOTH;
					dstaddr.l2_cid = htobs(ATT_CID);
					dstaddr.l2_bdaddr_type = bRandomAddress ? BDADDR_LE_RANDOM : BDADDR_LE_PUBLIC;
					bacpy(&dstaddr.l2_bdaddr, &GoveeBTAddress);
					if (connect(l2cap_socket, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0)
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Failed to connect: " << strerror(errno) << " (" << errno << ")" << std::endl;
						close(l2cap_socket);
					}
					else
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Connected L2CAP LE connection on ATT channel: " << ATT_CID << std::endl;

						unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
						std::vector<BlueToothService> BTServices;
						// First we query the device to get the list of SERVICES.
						// What I end up with is a starting handle, ending handle, and either 16 bit or 128 bit UUID for the service.
						GATT_DeclarationPacket primary_service_declaration = { BT_ATT_OP_READ_BY_GRP_TYPE_REQ, 0x0001, 0xffff, GATT_PRIM_SVC_UUID };
						do {
							if (ConsoleVerbosity > 1)
								std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << primary_service_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << primary_service_declaration.ending_handle << std::endl;
							if (-1 == send(l2cap_socket, &primary_service_declaration, sizeof(primary_service_declaration), 0))
								buf[0] = BT_ATT_OP_ERROR_RSP;
							else
							{
								auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
								if (-1 == bufDataLen)
									buf[0] = BT_ATT_OP_ERROR_RSP;
								else
								{
									if (buf[0] == BT_ATT_OP_READ_BY_GRP_TYPE_RSP)
									{
										for (auto AttributeOffset = 2; AttributeOffset < bufDataLen; AttributeOffset += buf[1])
										{
											if (buf[1] == 6) // length of Handle/Value Pair
											{
												struct bt_attribute_data_uuid16 { uint16_t starting_handle; uint16_t ending_handle; uint16_t UUID; } *attribute_data = (bt_attribute_data_uuid16*)&(buf[AttributeOffset]);
												bt_uuid_t theUUID;
												bt_uuid16_create(&theUUID, attribute_data->UUID);
												if (ConsoleVerbosity > 1)
													std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->starting_handle << "..0x" << std::setw(4) << std::setfill('0') << attribute_data->ending_handle << " UUID: " << bt_UUID_2_String(&theUUID) << std::endl;
												primary_service_declaration.starting_handle = attribute_data->ending_handle + 1;
												BlueToothService bts = { theUUID, attribute_data->starting_handle, attribute_data->ending_handle };
												BTServices.push_back(bts);
											}
											else if (buf[1] == 20) // length of Handle/Value Pair
											{
												// UUID: 57485f534b434f525f494c4c45544e49 = WH_SKCOR_ILLETNI
												struct bt_attribute_data_uuid128 { uint16_t starting_handle; uint16_t ending_handle; uint128_t UUID; } *attribute_data = (bt_attribute_data_uuid128*)&(buf[AttributeOffset]);
												bt_uuid_t theUUID;
												bt_uuid128_create(&theUUID, attribute_data->UUID);
												if (ConsoleVerbosity > 1)
													std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->starting_handle << "..0x" << std::setw(4) << std::setfill('0') << attribute_data->ending_handle << " UUID: " << bt_UUID_2_String(&theUUID) << std::endl;
												primary_service_declaration.starting_handle = attribute_data->ending_handle + 1;
												BlueToothService bts = { theUUID, attribute_data->starting_handle, attribute_data->ending_handle };
												BTServices.push_back(bts);
											}
										}
									}
								}
							}
						} while (buf[0] != BT_ATT_OP_ERROR_RSP);
						if (ConsoleVerbosity > 1)
							std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_GRP_TYPE_REQ GATT_PRIM_SVC_UUID BT_ATT_OP_ERROR_RSP" << std::endl;

						// Next I go through my stored set of SERVICES requesting CHARACTERISTICS based on the combination of starting handle and ending handle
						for (auto bts = BTServices.begin(); bts != BTServices.end(); bts++)
						{
							GATT_DeclarationPacket gatt_include_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, bts->starting_handle, bts->ending_handle, GATT_INCLUDE_UUID };
							do {
								if (ConsoleVerbosity > 1)
									std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Type Request, GATT Include Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_include_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_include_declaration.ending_handle << std::endl;
								if (-1 == send(l2cap_socket, &gatt_include_declaration, sizeof(gatt_include_declaration), 0))
									buf[0] = BT_ATT_OP_ERROR_RSP;
								else
								{
									auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
									if (-1 == bufDataLen)
										buf[0] = BT_ATT_OP_ERROR_RSP;
									else
									{
										// TODO: Here I need to interpret the buffer, figure out what the maximum handle is, and increase the starting handle
										buf[0] = BT_ATT_OP_ERROR_RSP; // Since I'm not interpreting this response right now, I'm assigning the buffer that moves us to the next feature.
									}
								}
							} while (buf[0] != BT_ATT_OP_ERROR_RSP);
							if (ConsoleVerbosity > 1)
								std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP" << std::endl;

							GATT_DeclarationPacket gatt_characteristic_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, bts->starting_handle, bts->ending_handle, GATT_CHARAC_UUID };
							do {
								if (ConsoleVerbosity > 1)
									std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_characteristic_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_characteristic_declaration.ending_handle << std::endl;
								if (-1 == send(l2cap_socket, &gatt_characteristic_declaration, sizeof(gatt_characteristic_declaration), 0))
									buf[0] = BT_ATT_OP_ERROR_RSP;
								else
								{
									auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
									if (-1 == bufDataLen)
										buf[0] = BT_ATT_OP_ERROR_RSP;
									else
									{
										if (buf[0] == BT_ATT_OP_READ_BY_TYPE_RSP)
										{
											for (auto AttributeOffset = 2; AttributeOffset < bufDataLen; AttributeOffset += buf[1])
											{
												BlueToothServiceCharacteristic Characteristic;
												if (buf[1] == 7) // length of Handle/Value Pair
												{
													struct __attribute__((__packed__)) bt_attribute_data { uint16_t starting_handle; uint8_t properties; uint16_t ending_handle; uint16_t UUID; } *attribute_data = (bt_attribute_data*)&(buf[AttributeOffset]);
													bt_uuid_t theUUID;
													bt_uuid16_create(&theUUID, attribute_data->UUID);
													Characteristic.starting_handle = attribute_data->starting_handle;
													Characteristic.ending_handle = attribute_data->ending_handle;
													Characteristic.properties = attribute_data->properties;
													Characteristic.theUUID = theUUID;
												}
												else if (buf[1] == 21) // length of Handle/Value Pair
												{
													// UUID: 34cc54b9-f956-c691-2140-a641a8ca8280
													// UUID: 5186f05a-3442-0488-5f4b-c35ef0494272
													// UUID: d44f33fb-927c-22a0-fe45-a14725db536c
													// UUID: 31da3f67-5b85-8391-d849-0c00a3b9849d
													// UUID: b29c7bb1-d057-1691-a14c-16d5e8717845
													// UUID: 885c066a-ebb3-0a99-f546-8c7994df785f
													// UUID: 3a913bdb-c8ac-1da2-1b40-e50db5e8b464
													// UUID: 3bfb6752-878f-5484-9c4d-be77dddfc342
													// UUID: 3ce2fc3d-90c4-afa3-bb43-3d82ea1edeb7
													// UUID: 12205f53-4b43-4f52-5f49-4c4c45544e49  _SKCOR_ILLETNI
													// UUID: 13205f53-4b43-4f52-5f49-4c4c45544e49  _SKCOR_ILLETNI
													// UUID: 11205f53-4b43-4f52-5f49-4c4c45544e49  _SKCOR_ILLETNI
													// UUID: 14205f53-4b43-4f52-5f49-4c4c45544e49  _SKCOR_ILLETNI
													struct __attribute__((__packed__)) bt_attribute_data { uint16_t starting_handle; uint8_t properties; uint16_t ending_handle; uint128_t UUID; } *attribute_data = (bt_attribute_data*)&(buf[AttributeOffset]);
													bt_uuid_t theUUID;
													bt_uuid128_create(&theUUID, attribute_data->UUID);
													Characteristic.starting_handle = attribute_data->starting_handle;
													Characteristic.ending_handle = attribute_data->ending_handle;
													Characteristic.properties = attribute_data->properties;
													Characteristic.theUUID = theUUID;
												}
												if (ConsoleVerbosity > 1)
												{
													std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << Characteristic.starting_handle;
													std::cout << "..0x" << std::setw(4) << std::setfill('0') << Characteristic.ending_handle;
													std::cout << " Characteristic Properties: 0x" << std::setw(2) << std::setfill('0') << unsigned(Characteristic.properties);
													std::cout << " UUID: " << bt_UUID_2_String(&Characteristic.theUUID) << std::endl;
												}
												gatt_characteristic_declaration.starting_handle = Characteristic.ending_handle;
												bts->characteristics.push_back(Characteristic);
											}
										}
									}
								}
							} while (buf[0] != BT_ATT_OP_ERROR_RSP);
							if (ConsoleVerbosity > 1)
								std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP" << std::endl;
						}

						if (ConsoleVerbosity > 0)
						{
							// List accumulated Services
							for (auto bts = BTServices.begin(); bts != BTServices.end(); bts++)
							{
								std::cout << "[-------------------] Service Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << bts->starting_handle;
								std::cout << "..0x" << std::setw(4) << std::setfill('0') << bts->ending_handle;
								std::cout << " UUID: " << std::hex << std::setw(4) << std::setfill('0') << bt_UUID_2_String(&(bts->theUUID)) << std::endl;
								for (auto btsc = bts->characteristics.begin(); btsc != bts->characteristics.end(); btsc++)
								{
									std::cout << "[                   ] Characteristic Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << btsc->starting_handle;
									std::cout << "..0x" << std::setw(4) << std::setfill('0') << btsc->ending_handle;
									// Characteristic Properties: 0x1a, Notify, Write, Read
									// Characteristic Properties: 0x12, Notify, Read
									std::cout << " Properties: 0x" << std::setw(2) << std::setfill('0') << unsigned(btsc->properties);
									std::cout << " UUID: " << bt_UUID_2_String(&btsc->theUUID) << std::endl;
								}
							}
						}

#ifdef BT_GET_INFORMATION
						// Loop Through Govee services, request information on the handles.
						// I'm not storing or doing anything with this information right now, but the android app does it, so I'm repeating the sequence.
						buf[0] = 0;
						for (auto bts = BTServices.begin(); (bts != BTServices.end() && (buf[0] != BT_ATT_OP_ERROR_RSP)); bts++)
						{
							bt_uuid_t INTELLI_ROCKS_HW;
							bt_uuid128_create(&INTELLI_ROCKS_HW, { 0x57, 0x48, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							if (bts->theUUID == INTELLI_ROCKS_HW)
								for (auto btsc = bts->characteristics.begin(); btsc != bts->characteristics.end(); btsc++)
								{
									struct __attribute__((__packed__)) { uint8_t opcode; uint16_t starting_handle; uint16_t ending_handle; } gatt_information = { BT_ATT_OP_FIND_INFO_REQ, btsc->ending_handle, btsc->ending_handle };
									gatt_information.starting_handle++;
									gatt_information.ending_handle++;
									gatt_information.ending_handle++;
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> Find Information Request, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_information.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_information.ending_handle << std::endl;
									if (-1 == send(l2cap_socket, &gatt_information, sizeof(gatt_information), 0))
										buf[0] = BT_ATT_OP_ERROR_RSP;
									else
									{
										auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
										if (-1 == bufDataLen)
											buf[0] = BT_ATT_OP_ERROR_RSP;
										else
										{
											if ((buf[0] == BT_ATT_OP_FIND_INFO_RSP) && (buf[1] == 0x01)) // UUID Format: 16-bit UUIDs (0x01)
											{
												for (auto AttributeOffset = 2; AttributeOffset < bufDataLen; AttributeOffset += 4)
												{
													// UUID: 2902 Client Characteristic Configuration
													// UUID: 2901 Characteristic User Description
													struct __attribute__((__packed__)) bt_attribute_data { uint16_t handle; uint16_t UUID; } *attribute_data = (bt_attribute_data*)&(buf[AttributeOffset]);
													bt_uuid_t theUUID;
													bt_uuid16_create(&theUUID, attribute_data->UUID);
													if (ConsoleVerbosity > 0)
													{
														std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== Handle: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->handle;
														std::cout << " UUID: " << bt_UUID_2_String(&theUUID);
														std::cout << std::endl;
													}
												}
											}
										}
									}
								}
						}
#endif // BT_GET_INFORMATION

						uint16_t bt_Handle_RequestData = 0;
						uint16_t bt_Handle_ReturnData = 0;
						// This loops through and enables notification on each of the Govee service handles
						buf[0] = 0;
						for (auto bts = BTServices.begin(); (bts != BTServices.end() && (buf[0] != BT_ATT_OP_ERROR_RSP)); bts++)
						{
							bt_uuid_t INTELLI_ROCKS_HW; bt_uuid128_create(&INTELLI_ROCKS_HW, { 0x57, 0x48, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							//bt_uuid_t INTELLI_ROCKS_11; bt_uuid128_create(&INTELLI_ROCKS_11, { 0x11, 0x20, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							bt_uuid_t INTELLI_ROCKS_12; bt_uuid128_create(&INTELLI_ROCKS_12, { 0x12, 0x20, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							bt_uuid_t INTELLI_ROCKS_13; bt_uuid128_create(&INTELLI_ROCKS_13, { 0x13, 0x20, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							//bt_uuid_t INTELLI_ROCKS_14; bt_uuid128_create(&INTELLI_ROCKS_14, { 0x14, 0x20, 0x5f, 0x53, 0x4b, 0x43, 0x4f, 0x52, 0x5f, 0x49, 0x4c, 0x4c, 0x45, 0x54, 0x4e, 0x49 });
							if (bts->theUUID == INTELLI_ROCKS_HW)
								for (auto btsc = bts->characteristics.begin(); btsc != bts->characteristics.end(); btsc++)
								{
									if (btsc->theUUID == INTELLI_ROCKS_12)
										bt_Handle_RequestData = btsc->ending_handle;
									if (btsc->theUUID == INTELLI_ROCKS_13)
										bt_Handle_ReturnData = btsc->ending_handle;
									struct __attribute__((__packed__)) { uint8_t opcode; uint16_t handle; uint8_t buf[2]; } pkt = { BT_ATT_OP_WRITE_REQ, btsc->ending_handle, {0x01 ,0x00} };
									pkt.handle++;
									if (ConsoleVerbosity > 1)
									{
										std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> BT_ATT_OP_WRITE_REQ Handle: ";
										std::cout << std::hex << std::setfill('0') << std::setw(4) << pkt.handle << " Value: ";
										for (auto index = 0; index < sizeof(pkt.buf) / sizeof(pkt.buf[0]); index++)
											std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(pkt.buf[index]);
										std::cout << std::endl;
									}
									if (-1 == send(l2cap_socket, &pkt, sizeof(pkt), 0))
										buf[0] = BT_ATT_OP_ERROR_RSP;
									else
									{
										auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
										if (-1 == bufDataLen)
											buf[0] = BT_ATT_OP_ERROR_RSP;
										else
										{
											if (buf[0] == BT_ATT_OP_WRITE_RSP)
											{
												if (ConsoleVerbosity > 1)
													std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_WRITE_RSP" << std::endl;
											}
											else if (buf[0] == BT_ATT_OP_ERROR_RSP)
											{
												struct __attribute__((__packed__)) bt_error { uint8_t opcode; uint8_t req_opcode; uint16_t handle; uint8_t errcode; } *result = (bt_error*)&(buf[0]);
												if (ConsoleVerbosity > 1)
												{
													std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_ERROR_RSP";
													std::cout << " Handle: " << std::hex << std::setw(4) << std::setfill('0') << result->handle;
													std::cout << " Error: " << std::dec << result->errcode;
													std::cout << std::endl;
												}
												buf[0] = 0; // this allows me to keep looping
											}
										}
									}
								}
						}

						std::queue<GATT_WritePacket> WritePacketQueue;
						GATT_WritePacket MyRequest({ BT_ATT_OP_WRITE_REQ, bt_Handle_RequestData, {0} });
						MyRequest.buf[0] = 0x33;
						MyRequest.buf[1] = 0x01;
						time(&TimeDownloadStart);
						TimeDownloadStart = (TimeDownloadStart / 60) * 60; // trick to align time on minute interval
						uint16_t DataPointsToRequest = 0xffff;
						if (((TimeDownloadStart - GoveeLastReadTime) / 60) < 0xffff)
							DataPointsToRequest = (TimeDownloadStart - GoveeLastReadTime) / 60;
#ifdef DEBUG
						DataPointsToRequest = 123; // this saves a huge amount of time
#endif // DEBUG
						MyRequest.buf[2] = DataPointsToRequest >> 8;
						MyRequest.buf[3] = DataPointsToRequest;
						MyRequest.buf[5] = 0x01;
						// Create a checksum in the last byte by XOR each of the buffer bytes.
						for (auto index = 0; index < sizeof(MyRequest.buf) / sizeof(MyRequest.buf[0]) - 1; index++)
							MyRequest.buf[(sizeof(MyRequest.buf) / sizeof(MyRequest.buf[0])) - 1] ^= MyRequest.buf[index];
						WritePacketQueue.push(MyRequest);

						//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, bt_Handle_RequestData, {0x33,0x01,0x3d,0xee,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0} });
						//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, bt_Handle_RequestData, {0x33,0x01,0x70,0x81,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc2} });
						// The following while loop attempts to read from the non-blocking socket.
						// As long as the read call simply times out, we sleep for 100 microseconds and try again.
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x0035, {0xaa,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa4} });
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x0035, {0xaa,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa4} });
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x0035, {0xaa,0x0d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa7} });
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x0035, {0x33,0x10,0x59,0x77,0xaa,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xa6} });
						// For each of the above, I got both a write response and a handle notification from handle 0x0035
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x002d, {0x33,0x02,0x00,0x81,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x31} });
							//WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x002d, {0x33,0x01,0x3d,0xee,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0} });
						// For each of the above, I got both a write response and a handle notification from handle 0x002d
						// then I get 89 notifications on handle 0x0031
						// The first three handle 0x0031 packets have values:
						// Value: 3dee021250020e67020e65020e65020e66020e66
						// Value: 3de8020e68020e66020e66020e66020e65020e66
						// Value: 3de2020e64020e6602124d02124f02124c020e5f
						// I'm guessing that the last packet send a command 0x3301 followed by a starting address of 0x3dee and a finishing address of 0x0001
						// I had data in my app from about 11 days ago, and if this data is stored in minute increments, 3dee = 15854, and 1584/60/24 = 11 days
						//	WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x002d, {0xaa,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xab} });
						// After that, I got 6 more notifications, then a write response, and then 113 notifications
						//	WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, 0x002d, {0xaa,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xab} });
						// After that, I got 5 more notifications, then a write response, and then 92 notifications
						// This patern kept up with notifications on handle 0x0031 until the value counted down to zero.
						// Here's the last three Value Notifications:
						// Value: 000e016377016377016377016377016377016377
						// Value: 0008016377016377016377016377016377016377
						// Value: 0002016377015f8fffffffffffffffffffffffff
						// Then there was a notification on handle 0x002d
						// Value: ee010a53000000000000000000000000000000b6
						// There were 2644 Notification packets on Handle 31,
						int RetryCount(4);
						int NotificationCount(0);
						bool bDownloadInProgress(true);
						while (bDownloadInProgress)
						{
							auto pkt = WritePacketQueue.front();
							if (!WritePacketQueue.empty())
							{
								if (ConsoleVerbosity > 1)
								{
									std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] ==> BT_ATT_OP_WRITE_REQ Handle: ";
									std::cout << std::hex << std::setfill('0') << std::setw(4) << pkt.handle << " Value: ";
									for (auto index = 0; index < sizeof(pkt.buf) / sizeof(pkt.buf[0]); index++)
										std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(pkt.buf[index]);
									std::cout << std::endl;
								}
								if (-1 == send(l2cap_socket, &pkt, sizeof(pkt), 0))
								{
									buf[0] = BT_ATT_OP_ERROR_RSP;
									bDownloadInProgress = false;
								}
								else
									WritePacketQueue.pop();
							}
							if ((buf[0] != BT_ATT_OP_ERROR_RSP) && bDownloadInProgress)
							{
								auto bufDataLen = recv(l2cap_socket, buf, sizeof(buf), 0);
								if (bufDataLen > 1)
								{
									RetryCount = 4; // if we got a response, reset the retry count
									if (buf[0] == BT_ATT_OP_WRITE_RSP)
									{
										if (ConsoleVerbosity > 1)
											std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_WRITE_RSP" << std::endl;
									}
									else if (buf[0] == BT_ATT_OP_HANDLE_VAL_NOT)
									{
										struct __attribute__((__packed__)) bt_handle_value { uint8_t opcode;  uint16_t handle; uint8_t value[20]; } *data = (bt_handle_value*)&(buf[0]);
										if (ConsoleVerbosity > 1)
										{
											std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_HANDLE_VAL_NOT";
											std::cout << " Handle: " << std::hex << std::setfill('0') << std::setw(4) << data->handle;
										}
										if (data->handle == bt_Handle_ReturnData)
										{
											NotificationCount++;
											offset = uint16_t(data->value[0]) << 8 | uint16_t(data->value[1]);
											if (offset < 7)	// If offset is 6 or less we are in the last bit of data, and as soon as we decode it we can close the connection.
												bDownloadInProgress = false;
											else if (NotificationCount > 75)
											{
												WritePacketQueue.push({ BT_ATT_OP_WRITE_REQ, bt_Handle_RequestData, {0xaa,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xab} });
												NotificationCount = 0;
											}
											if (ConsoleVerbosity > 1)
												std::cout << " offset: " << std::hex << std::setfill('0') << std::setw(4) << offset;
											for (auto index = 2; (index < (bufDataLen - sizeof(uint8_t) - sizeof(uint16_t))) && (offset > 0); index += 3)
											{
												int iTemp = int(data->value[index]) << 16 | int(data->value[index + 1]) << 8 | int(data->value[index + 2]);
												bool bNegative = iTemp & 0x800000;	// check sign bit
												iTemp = iTemp & 0x7ffff;			// mask off sign bit
												double Temperature = float(iTemp) / 10000.0;
												double Humidity = float(iTemp % 1000) / 10.0;
												if (bNegative)						// apply sign bit
													Temperature = -1.0 * Temperature;
												if (ConsoleVerbosity > 1)
												{
													std::cout << " " << std::dec << Temperature;
													std::cout << " " << std::dec << Humidity;
												}
												Govee_Temp localTemp(TimeDownloadStart - (60 * offset--), Temperature, Humidity, BatteryToRecord);
												std::queue<Govee_Temp> foo;
												auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(GoveeBTAddress, foo));
												ret.first->second.push(localTemp);
												DataPointsRecieved++;
											}
										}
										else
										{
											if (ConsoleVerbosity > 1)
											{
												std::cout << " Value: ";
												for (auto index = 0; index < sizeof(data->value) / sizeof(data->value[0]); index++)
													std::cout << std::hex << std::setfill('0') << std::setw(2) << unsigned(data->value[index]);
											}
										}
										if (ConsoleVerbosity > 1)
											std::cout << std::endl;
									}
								}
								else
								{
									if (ConsoleVerbosity > 1)
										std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Reading from device. RetryCount = " << std::dec << RetryCount << std::endl;
									usleep(100000); // 1,000,000 = 1 second.
									if (--RetryCount < 0)
										bDownloadInProgress = false;
								}
							}
						}
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Closing l2cap_socket" << std::endl;
						close(l2cap_socket);
					}
				}
			}
		}
		if (handle != 0)
		{
			hci_disconnect(BlueToothDevice_Handle, handle, HCI_OE_USER_ENDED_CONNECTION, 2000);
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] hci_disconnect" << std::endl;
		}
		if (setsockopt(BlueToothDevice_Handle, SOL_HCI, HCI_FILTER, &original_filter, sizeof(original_filter)) < 0)
			std::cerr << "[                   ] Error: Could not set socket options: " << strerror(errno) << std::endl;
	}
	if (DataPointsRecieved == 0)
		TimeDownloadStart = 0;
	if ((DataPointsRecieved > 0) && (TimeDownloadStart != 0))
	{
		auto TimeStart = TimeDownloadStart - static_cast<long>(60) * DataPointsRecieved;
		auto TimeStop = TimeDownloadStart;
		TimeStart -= static_cast<long>(60) * offset;
		TimeStop -= static_cast<long>(60) * offset;
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(GoveeBTAddress) << "] Download from device. " << timeToExcelLocal(TimeStart) << " " << timeToExcelLocal(TimeStop) << " (" << std::dec << DataPointsRecieved << ")" << std::endl;
		else
			std::cerr << "Download from device: [" << ba2string(GoveeBTAddress) << "] " << timeToExcelLocal(TimeStart) << " " << timeToExcelLocal(TimeStop) << " (" << std::dec << DataPointsRecieved << ")" << std::endl;
		TimeDownloadStart -= static_cast<long>(60) * offset;
	}
	return(TimeDownloadStart);
}
void HCI_BlueZ_MainLoop(std::string& ControllerAddress, std::set<bdaddr_t>& BT_WhiteList, int& ExitValue, const bool bMonitorLoggingDirectory, const bool HCI_Passive_Scanning)
{
	bt_ListDevices();
	int BlueToothDevice_ID;
	if (ControllerAddress.empty())
		BlueToothDevice_ID = hci_get_route(NULL);
	else
		BlueToothDevice_ID = hci_devid(ControllerAddress.c_str());
	if (BlueToothDevice_ID < 0)
		std::cerr << "[                   ] Error: Bluetooth device not found" << std::endl;
	else
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[                   ] BlueToothDevice_ID: " << BlueToothDevice_ID << std::endl;

		// 2022-12-26: I came across information tha signal() is bad and I should be using sigaction() instead
		// example of signal() https://www.gnu.org/software/libc/manual/html_node/Basic-Signal-Handling.html#Basic-Signal-Handling
		// example of sigaction() https://www.gnu.org/software/libc/manual/html_node/Sigaction-Function-Example.html
		//struct sigaction new_action, old_action;
		//new_action.sa_handler = SignalHandlerSIGINT;
		//sigemptyset(&new_action.sa_mask);
		//new_action.sa_flags = 0;
		//sigaction(SIGINT, NULL, &old_action);

		int BlueToothDevice_Handle = hci_open_dev(BlueToothDevice_ID);
		if (BlueToothDevice_Handle < 0)
			std::cerr << "[                   ] Error: Cannot open device: " << strerror(errno) << std::endl;
		else
		{
			// (2023-11-09) I'm resetting, downing, and upping the device in an attempt to have the device always in the same state as if I'd powered off the pi.
			// see this code for source https://kernel.googlesource.com/pub/scm/bluetooth/bluez/+/utils-2.3/tools/hciconfig.c
			// Reset HCI device
			if (ioctl(BlueToothDevice_Handle, HCIDEVRESET, BlueToothDevice_ID) < 0)
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[                   ] Error: Reset failed device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
				else
					std::cerr << "Error: Reset failed device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
			}
			else
				if (ConsoleVerbosity > 0)
					std::cout << "[                   ] Reset device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;

			// Stop HCI device
			if (ioctl(BlueToothDevice_Handle, HCIDEVDOWN, BlueToothDevice_ID) < 0)
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[                   ] Error: Cannot down device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
				else
					std::cerr << "Error: Cannot down device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
			}
			else
				if (ConsoleVerbosity > 0)
					std::cout << "[                   ] DOWN device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;

			// Start HCI device
			if (ioctl(BlueToothDevice_Handle, HCIDEVUP, BlueToothDevice_ID) < 0)
			{
				if (errno == EALREADY)
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[                   ] Already UP device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
					else
						std::cerr << "Already UP device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
				}
				else
					if (ConsoleVerbosity > 0)
						std::cout << "[                   ] Error: Cannot init device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
					else
						std::cerr << "Error: Cannot init device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;
			}
			else
				if (ConsoleVerbosity > 0)
					std::cout << "[                   ] UP device: hci" << BlueToothDevice_ID << ". " << strerror(errno) << "(" << errno << ")" << std::endl;

			int on = 1; // Nonblocking on = 1, off = 0;
			if (ioctl(BlueToothDevice_Handle, FIONBIO, (char*)&on) < 0)
				std::cerr << "[                   ] Error: Could set device to non-blocking: " << strerror(errno) << std::endl;
			else
			{
				hci_le_set_random_address(BlueToothDevice_Handle, bt_TimeOut);	// 2023-11-29 Added this command to fix problem with Raspberry Pi Zero 2 W Issue #50
				char LocalName[HCI_MAX_NAME_LENGTH] = { 0 };
				hci_read_local_name(BlueToothDevice_Handle, sizeof(LocalName), LocalName, bt_TimeOut);

				// TODO: get controller address and put it in the log. Useful for machines with multiple controllers to verify which is being used
				bdaddr_t TheLocalBlueToothAddress({ 0 });
				hci_read_bd_addr(BlueToothDevice_Handle, &TheLocalBlueToothAddress, bt_TimeOut);
				ControllerAddress = ba2string(TheLocalBlueToothAddress);

				if (ConsoleVerbosity > 0)
				{
					if (!ControllerAddress.empty())
						std::cout << "[" << getTimeISO8601(true) << "] Using Controller Address: " << ControllerAddress << std::endl;
					std::cout << "[" << getTimeISO8601(true) << "] LocalName: " << LocalName << std::endl;
					if (BT_WhiteList.empty())
						std::cout << "[" << getTimeISO8601(true) << "] No BlueTooth Address Filter" << std::endl;
					else
					{
						std::cout << "[" << getTimeISO8601(true) << "] BlueTooth Address Filter:";
						for (auto iter = BT_WhiteList.begin(); iter != BT_WhiteList.end(); iter++)
							std::cout << " [" << ba2string(*iter) << "]";
						std::cout << std::endl;
					}
				}
				else
					if (!ControllerAddress.empty())
						std::cerr << "Using Controller Address: " << ControllerAddress << std::endl;

				auto btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList, HCI_Passive_Scanning);
				if (btRVal < 0)
					ExitValue = EXIT_FAILURE;
				else
				{
					// Save the current HCI filter (Host Controller Interface)
					struct hci_filter original_filter;
					socklen_t olen = sizeof(original_filter);
					if (0 == getsockopt(BlueToothDevice_Handle, SOL_HCI, HCI_FILTER, &original_filter, &olen))
					{
						// Create and set the new filter
						struct hci_filter new_filter;
						hci_filter_clear(&new_filter);
						hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
						hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
						if (setsockopt(BlueToothDevice_Handle, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0)
							std::cerr << "[                   ] Error: Could not set socket options: " << strerror(errno) << std::endl;
						else
						{
							bRun = true;
							time_t TimeStart(0), TimeSVG(0), TimeAdvertisment(0);
							time(&TimeStart);
							while (bRun)
							{
								unsigned char buf[HCI_MAX_EVENT_SIZE];

								// This select() call coming up will sit and wait until until the socket read would return something that's not EAGAIN/EWOULDBLOCK
								// But first we need to set a timeout -- we need to do this every time before we call select()
								struct timeval select_timeout = { 60, 0 };	// 60 second timeout, 0 microseconds
								// and reset the value of check_set, since that's what will tell us what descriptors were ready
								// Set up the file descriptor set that select() will use
								fd_set check_set;
								FD_ZERO(&check_set);
								FD_SET(BlueToothDevice_Handle, &check_set);
								// This will block until either a read is ready (i.e. won’t return EWOULDBLOCK) -1 on error, 0 on timeout, otherwise number of FDs changed
								if (0 < select(BlueToothDevice_Handle + 1, &check_set, NULL, NULL, &select_timeout))	// returns number of handles ready to read. 0 or negative indicate other than good data to read.
								{
									// We got data ready to read, check and make sure it's the right descriptor, just as a sanity check (it shouldn't be possible ot get anything else)
									if (FD_ISSET(BlueToothDevice_Handle, &check_set))
									{
										// okay, if we made it this far, we can read our descriptor, and shouldn't get EAGAIN. Ideally, the right way to process this is 'read in a loop
										// until you get EAGAIN and then go back to select()', but worst case is that you don't read everything availableand select() immediately returns, so not
										// a *huge* deal just doing one read and then back to select, here.
										ssize_t bufDataLen = read(BlueToothDevice_Handle, buf, sizeof(buf));
										if (bufDataLen > HCI_MAX_EVENT_SIZE)
											std::cerr << "[                   ] Error: bufDataLen (" << bufDataLen << ") > HCI_MAX_EVENT_SIZE (" << HCI_MAX_EVENT_SIZE << ")" << std::endl;
										if (bufDataLen > (HCI_EVENT_HDR_SIZE + 1 + LE_ADVERTISING_INFO_SIZE))
										{
											if (ConsoleVerbosity > 3)
												std::cout << "[" << getTimeISO8601(true) << "] Read: " << std::dec << bufDataLen << " Bytes" << std::endl;
											std::ostringstream ConsoleOutLine;
											ConsoleOutLine << "[" << getTimeISO8601(true) << "]" << std::setw(3) << bufDataLen;

											// At this point I should have an HCI Event in buf (hci_event_hdr)
											evt_le_meta_event* meta = (evt_le_meta_event*)(buf + (HCI_EVENT_HDR_SIZE + 1));
											if (meta->subevent == EVT_LE_ADVERTISING_REPORT)
											{
												time(&TimeAdvertisment);
												const le_advertising_info* const info = (le_advertising_info*)(meta->data + 1);
												bool AddressInGoveeSet(GoveeTemperatures.end() != GoveeTemperatures.find(info->bdaddr));
												bool TemperatureInAdvertisment(false);
												char addr[19] = { 0 };
												ba2str(&info->bdaddr, addr);
												ConsoleOutLine << " [" << addr << "]";
												std::string localName;
												if (ConsoleVerbosity > 2)
												{
													ConsoleOutLine << " (bdaddr_type) " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(info->bdaddr_type);
													ConsoleOutLine << " (evt_type) " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(info->evt_type);
												}
												if (ConsoleVerbosity > 8)
												{
													std::cout << "[                   ]";
													for (auto index = 0; index < bufDataLen; index++)
														std::cout << " " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(buf[index]);
													std::cout << std::endl;
													std::cout << "[                   ] ^^ ^^ ^^ ^^ ^^ ^^ ^^ ^^                ^^--> le_advertising_info.length (bytes following)" << std::endl;
													std::cout << "[                   ] |  |  |  |  |  |  |  ^---------------------> le_advertising_info.bdaddr" << std::endl;
													std::cout << "[                   ] |  |  |  |  |  |  ^------------------------> le_advertising_info.bdaddr_type" << std::endl;
													std::cout << "[                   ] |  |  |  |  |  ^---------------------------> ??" << std::endl;
													std::cout << "[                   ] |  |  |  |  ^------------------------------> le_advertising_info.evt_type" << std::endl;
													std::cout << "[                   ] |  |  |  ^---------------------------------> evt_le_meta_event.subevent = EVT_LE_ADVERTISING_REPORT = 02" << std::endl;
													std::cout << "[                   ] |  |  ^------------------------------------> ?? length (bytes following)" << std::endl;
													std::cout << "[                   ] |  ^---------------------------------------> hci_event_hdr.plen = EVT_LE_META_EVENT = 3E" << std::endl;
													std::cout << "[                   ] ^------------------------------------------> hci_event_hdr.evt = HCI_EVENT_PKT = 04" << std::endl;
												}
												if (info->length > 0)
												{
													int current_offset = 0;
													bool data_error = false;
													Govee_Temp localTemp;
													while (!data_error && current_offset < info->length)
													{
														size_t data_len = info->data[current_offset];
														if (data_len + 1 > info->length)
														{
															if (ConsoleVerbosity > 0)
																std::cout << "[" << getTimeISO8601(true) << "] EIR data length is longer than EIR packet length. " << data_len << " + 1 > " << info->length << std::endl;
															data_error = true;
														}
														else
														{
															switch (*(info->data + current_offset + 1))
															{
															case 0x01:	// Flags
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Flags) ";
																	//for (uint8_t index = 0x80; index > 0; index >> 1)
																	//	ConsoleOutLine << (index & *(info->data + current_offset + 2));
																	//ConsoleOutLine << ((index & *(info->data + current_offset + 2)) ? "1" : "0");
																	if (ConsoleVerbosity > 3)
																	{
																		if (*(info->data + current_offset + 2) & 0x01)
																			ConsoleOutLine << "[LE Limited Discoverable Mode]";
																		if (*(info->data + current_offset + 2) & 0x02)
																			ConsoleOutLine << "[LE General Discoverable Mode]";
																		if (*(info->data + current_offset + 2) & 0x04)
																			ConsoleOutLine << "[LE General Discoverable Mode]";
																		if (*(info->data + current_offset + 2) & 0x08)
																			ConsoleOutLine << "[Simultaneous LE and BR/EDR (Controller)]";
																		if (*(info->data + current_offset + 2) & 0x10)
																			ConsoleOutLine << "[Simultaneous LE and BR/EDR (Host)]";
																		if (*(info->data + current_offset + 2) & 0x20)
																			ConsoleOutLine << "[??]";
																		if (*(info->data + current_offset + 2) & 0x40)
																			ConsoleOutLine << "[??]";
																		if (*(info->data + current_offset + 2) & 0x80)
																			ConsoleOutLine << "[??]";
																	}
																	else
																		for (auto index = 1; index < *(info->data + current_offset); index++)
																			ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																break;
															case 0x02:	// Incomplete List of 16-bit Service Class UUID
															case 0x03:	// Complete List of 16-bit Service Class UUIDs
																localTemp.SetModel((unsigned short*)(&((info->data + current_offset + 1)[1])));
															case 0x04:	// Incomplete List of 32-bit Service Class UUIDs
															case 0x05:	// Complete List of 32-bit Service Class UUID
															case 0x06:	// Incomplete List of 128-bit Service Class UUIDs
															case 0x07:	// Complete List of 128-bit Service Class UUID
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (UUID) ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																break;
															case 0x08:	// Shortened Local Name
															case 0x09:	// Complete Local Name
																localName.clear();
																for (auto index = 1; index < *(info->data + current_offset); index++)
																	localName.push_back(char((info->data + current_offset + 1)[index]));
																localTemp.SetModel(localName);
																if (localTemp.GetModel() != ThermometerType::Unknown)
																	GoveeThermometers.insert(std::pair<bdaddr_t, ThermometerType>(info->bdaddr, localTemp.GetModel()));
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Name) " << localName;
																}
																break;
															case 0x0A:	// Tx Power Level
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Tx Power) ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																break;
															case 0x16:	// Service Data or Service Data - 16-bit UUID
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Service Data) ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																break;
															case 0x19:	// Appearance
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Appearance) ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																break;
															case 0xFF:	// Manufacturer Specific Data
																if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Manu) ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
																{
																	if (localTemp.ReadMSG((info->data + current_offset)))	// This line decodes temperature from advertisment
																	{
																		TemperatureInAdvertisment = true;
																		if (localTemp.GetModel() == ThermometerType::Unknown)
																		{
																			auto foo = GoveeThermometers.find(info->bdaddr);
																			if (foo != GoveeThermometers.end())
																				localTemp.SetModel(foo->second);
																		}
																		ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature() << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																		if ((localTemp.GetModel() == ThermometerType::H5181) || (localTemp.GetModel() == ThermometerType::H5183))
																			ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature(false, 1) << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																		else if (localTemp.GetModel() == ThermometerType::H5182)
																		{
																			ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature(false, 1) << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																			ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature(false, 2) << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																			ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature(false, 3) << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																		}
																		//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2103";	// https://stackoverflow.com/questions/23777226/how-to-display-degree-celsius-in-a-string-in-c/23777678
																		//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2109";	// http://www.fileformat.info/info/unicode/char/2109/index.htm
																		if (localTemp.GetHumidity() != 0)
																			ConsoleOutLine << " (Humidity) " << localTemp.GetHumidity() << "%";
																		ConsoleOutLine << " (Battery) " << localTemp.GetBattery() << "%";
																		ConsoleOutLine << " " << localTemp.GetModelAsString();
																		std::queue<Govee_Temp> foo;
																		auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
																		ret.first->second.push(localTemp);	// puts the measurement in the queue to be written to the log file
																		AddressInGoveeSet = true;
																		UpdateMRTGData(info->bdaddr, localTemp);	// puts the measurement in the fake MRTG data structure
																		GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, 0));	// Makes sure the Bluetooth Address is in the list to get downloaded historical data
																	}
																	else if (AddressInGoveeSet || (ConsoleVerbosity > 1))
																		ConsoleOutLine << iBeacon(info->data + current_offset);
																}
																break;
															default:
																if ((AddressInGoveeSet && (ConsoleVerbosity > 0)) || (ConsoleVerbosity > 1))
																{
																	ConsoleOutLine << " (Other: " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(*(info->data + current_offset + 1)) << ") ";
																	for (auto index = 1; index < *(info->data + current_offset); index++)
																		ConsoleOutLine << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int((info->data + current_offset + 1)[index]);
																}
															}
															current_offset += data_len + 1;
														}
													}
												}
												if ((AddressInGoveeSet && (ConsoleVerbosity > 0)) || (ConsoleVerbosity > 1))
													std::cout << ConsoleOutLine.str() << std::endl;
												if (TemperatureInAdvertisment && (DaysBetweenDataDownload > 0) && AddressInGoveeSet && !LogDirectory.empty())
												{
													int BatteryToRecord = 0;
													auto RecentTemperature = GoveeTemperatures.find(info->bdaddr);
													if (RecentTemperature != GoveeTemperatures.end())
														BatteryToRecord = RecentTemperature->second.front().GetBattery();
													time_t LastDownloadTime = 0;
													auto RecentDownload = GoveeLastDownload.find(info->bdaddr);
													if (RecentDownload != GoveeLastDownload.end())
														LastDownloadTime = RecentDownload->second;
													time_t TimeNow;
													time(&TimeNow);
													// Don't try to download more often than once a week, because it uses more battery than just the advertisments
													if (difftime(TimeNow, LastDownloadTime) > (60 * 60 * 24 * DaysBetweenDataDownload))
													{
														bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList, HCI_Passive_Scanning);
														time_t DownloadTime = ConnectAndDownload(BlueToothDevice_Handle, info->bdaddr, LastDownloadTime, BatteryToRecord);
														if (DownloadTime > 0)
														{
															if (RecentDownload != GoveeLastDownload.end())
																RecentDownload->second = DownloadTime;
															else
																GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, DownloadTime));
														}
														btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList, HCI_Passive_Scanning);
														if (btRVal < 0)
														{
															bRun = false; // rely on inetd to restart entire process
															ExitValue = EXIT_FAILURE;
														}
													}
												}
											}
											else
											{
												if (ConsoleVerbosity > 2)
												{
													std::cout << "[-------------------]";
													for (auto index = 0; index < bufDataLen; index++)
														std::cout << " " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(buf[index]);
													std::cout << std::endl;
												}
											}
										}
										else if (bRun && (errno == EAGAIN))
										{
											// EAGAIN : Resource temporarily unavailable (may be the same value as EWOULDBLOCK) (POSIX.1-2001).
											std::cerr << "Error: " << strerror(errno) << " (" << errno << ")" << std::endl;
											usleep(100);
										}
										else if (errno == EINTR)
										{
											// EINTR : Interrupted function call (POSIX.1-2001); see signal(7).
											std::cerr << "Error: " << strerror(errno) << " (" << errno << ")" << std::endl;
											bRun = false;
										}
									}
								}
								time_t TimeNow;
								time(&TimeNow);
								if ((!SVGDirectory.empty()) && (difftime(TimeNow, TimeSVG) > DAY_SAMPLE))
								{
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601(true) << "] " << std::dec << DAY_SAMPLE << " seconds or more have passed. Writing SVG Files" << std::endl;
									TimeSVG = (TimeNow / DAY_SAMPLE) * DAY_SAMPLE; // hack to try to line up TimeSVG to be on a five minute period
									WriteAllSVG();
								}
								if (difftime(TimeNow, TimeStart) > LogFileTime)
								{
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601(true) << "] " << std::dec << LogFileTime << " seconds or more have passed. Writing LOG Files" << std::endl;
									TimeStart = TimeNow;
									GenerateLogFile(GoveeTemperatures, GoveeLastDownload);
									GenerateCacheFile(GoveeMRTGLogs); // flush FakeMRTG data to cache files
									if (bMonitorLoggingDirectory)
										MonitorLoggedData();
								}
								const int MaxMinutesBetweenBluetoothAdvertisments(3);
								if (difftime(TimeNow, TimeAdvertisment) > MaxMinutesBetweenBluetoothAdvertisments * 60) // Hack to force scanning restart regularly
								{
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601(true) << "] No recent Bluetooth LE Advertisments! (> " << MaxMinutesBetweenBluetoothAdvertisments << " Minutes)" << std::endl;
									btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList, HCI_Passive_Scanning);
									if (btRVal < 0)
									{
										bRun = false;	// rely on inetd to restart entire process
										ExitValue = EXIT_FAILURE;
									}
								}
							}
							setsockopt(BlueToothDevice_Handle, SOL_HCI, HCI_FILTER, &original_filter, sizeof(original_filter));
						}
					}
					btRVal = bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList, HCI_Passive_Scanning);
				}
			}
			hci_close_dev(BlueToothDevice_Handle);
		}

		GenerateLogFile(GoveeTemperatures, GoveeLastDownload); // flush contents of accumulated map to logfiles
		GenerateCacheFile(GoveeMRTGLogs); // flush FakeMRTG data to cache files

		if (ConsoleVerbosity > 0)
		{
			// dump contents of accumulated map (should now be empty because all the data was flushed to log files)
			for (auto it = GoveeTemperatures.begin(); it != GoveeTemperatures.end(); ++it)
			{
				if (!it->second.empty())
				{
					char addr[19] = { 0 };
					ba2str(&it->first, addr);
					std::cout << "[" << addr << "]" << std::endl;
				}
				while (!it->second.empty())
				{
					std::cout << it->second.front().WriteTXT() << std::endl;
					it->second.pop();
				}
			}
		}
	}
}
