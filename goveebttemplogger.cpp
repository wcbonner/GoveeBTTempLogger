/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 William C Bonner
//
//	MIT License
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files(the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions :
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// GoveeBTTempLogger is designed as a project to run on a Raspberry Pi with
// Bluetooth Low Energy support. It listens for advertisments from Govee
// https://www.govee.com/product/thermometers-hygrometers/indoor-thermometers-hygrometers
// Currently the H5074, GVH5075, and GVH5177 are decoded and logged.
// Each unit has its data logged to it's own file, with a new file created daily.
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
// Links I've found useful in learning about Bluetooth Low Energy (BLE) or Govee
// https://github.com/Thrilleratplay/GoveeWatcher
// https://github.com/neilsheps/GoveeTemperatureAndHumidity
// https://github.com/carsonmcdonald/bluez-experiments/blob/master/experiments/scantest.c
// https://people.csail.mit.edu/albert/bluez-intro/index.html
// https://reelyactive.github.io/ble-identifier-reference.html
// https://ukbaz.github.io/howto/beacon_scan_cmd_line.html
// https://github.com/ukBaz/ukBaz.github.io/blob/master/howto/beacon_scan_cmd_line.html
// http://kktechkaizen.blogspot.com/2014/10/bluetooth-technology-overview.html
// https://github.com/microsoftarchive/msdn-code-gallery-microsoft/tree/master/Official%20Windows%20Platform%20Sample/Bluetooth%20LE%20Explorer%20sample
// https://docs.microsoft.com/en-us/windows/win32/bluetooth/bluetooth-programming-with-windows-sockets
// https://www.reddit.com/r/Govee/comments/f1dfcd/home_assistant_component_for_h5074_and_h5075/fi7hnic/
// https://unix.stackexchange.com/questions/96106/bluetooth-le-scan-as-non-root
// https://docs.microsoft.com/en-us/cpp/linux/configure-a-linux-project?view=vs-2017
// https://reelyactive.github.io/diy/best-practices-ble-identifiers/
// https://github.com/pauloborges/bluez/blob/master/doc/mgmt-api.txt
// https://gist.github.com/mironovdm/cb7f47e8d898e9a3977fc888d990e8a9
// https://www.argenox.com/library/bluetooth-low-energy/using-raspberry-pi-ble/
//

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

/////////////////////////////////////////////////////////////////////////////
#if __has_include("goveebttemplogger-version.h")
#include "goveebttemplogger-version.h"
#endif
#ifndef GoveeBTTempLogger_VERSION
#define GoveeBTTempLogger_VERSION "(non-CMake)"
#endif // !GoveeBTTempLogger_VERSION
/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogger Version " GoveeBTTempLogger_VERSION " Built on: " __DATE__ " at " __TIME__);
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
	for (auto & b : scan_cp.bdaddr.b)
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
int ConsoleVerbosity(1);
bool UseBluetooth(true);
std::filesystem::path LogDirectory;	// If this remains empty, log Files are not created.
std::filesystem::path CacheDirectory;	// If this remains empty, cache Files are not used. Cache Files should greatly speed up startup of the program if logged data runs multiple years over many devices.
std::filesystem::path SVGDirectory;	// If this remains empty, SVG Files are not created. If it's specified, _day, _week, _month, and _year.svg files are created for each bluetooth address seen.
int SVGBattery(0); // 0x01 = Draw Battery line on daily, 0x02 = Draw Battery line on weekly, 0x04 = Draw Battery line on monthly, 0x08 = Draw Battery line on yearly
int SVGMinMax(0); // 0x01 = Draw Temperature and Humiditiy Minimum and Maximum line on daily, 0x02 = on weekly, 0x04 = on monthly, 0x08 = on yearly
bool SVGFahrenheit(true);
std::filesystem::path SVGTitleMapFilename;
std::filesystem::path SVGIndexFilename;
// The following details were taken from https://github.com/oetiker/mrtg
const size_t DAY_COUNT(600);			/* 400 samples is 33.33 hours */
const size_t WEEK_COUNT(600);			/* 400 samples is 8.33 days */
const size_t MONTH_COUNT(600);			/* 400 samples is 33.33 days */
const size_t YEAR_COUNT(2 * 366);		/* 1 sample / day, 366 days, 2 years */
const size_t DAY_SAMPLE(5 * 60);		/* Sample every 5 minutes */
const size_t WEEK_SAMPLE(30 * 60);		/* Sample every 30 minutes */
const size_t MONTH_SAMPLE(2 * 60 * 60);	/* Sample every 2 hours */
const size_t YEAR_SAMPLE(24 * 60 * 60);	/* Sample every 24 hours */
/////////////////////////////////////////////////////////////////////////////
// Class I'm using for storing raw data from the Govee thermometers
enum class ThermometerType
{ 
	Unknown = 0,
	H5072 = 5072,
	H5074 = 5074,
	H5075 = 5075,
	H5100 = 5100,
	H5101 = 5101,
	H5104 = 5104,
	H5105 = 5105,
	H5174 = 5174,
	H5177 = 5177,
	H5179 = 5179,
	H5181 = 5181,
	H5182 = 5182,
	H5183 = 5183,
	H5184 = 5184,
	H5055 = 5055,
};
std::string ThermometerType2String(const ThermometerType GoveeModel)
{
	switch (GoveeModel)
	{
	case ThermometerType::H5072:
		return(std::string("(GVH5072)"));
	case ThermometerType::H5074:
		return(std::string("(GVH5074)"));
	case ThermometerType::H5075:
		return(std::string("(GVH5075)"));
	case ThermometerType::H5100:
		return(std::string("(GVH5100)"));
	case ThermometerType::H5101:
		return(std::string("(GVH5101)"));
	case ThermometerType::H5104:
		return(std::string("(GVH5104)"));
	case ThermometerType::H5105:
		return(std::string("(GVH5105)"));
	case ThermometerType::H5174:
		return(std::string("(GVH5174)"));
	case ThermometerType::H5177:
		return(std::string("(GVH5177)"));
	case ThermometerType::H5179:
		return(std::string("(GVH5179)"));
	case ThermometerType::H5181:
		return(std::string("(GVH5181)"));
	case ThermometerType::H5182:
		return(std::string("(GVH5182)"));
	case ThermometerType::H5183:
		return(std::string("(GVH5183)"));
	case ThermometerType::H5184:
		return(std::string("(GVH5184)"));
	case ThermometerType::H5055:
		return(std::string("(GVH5055)"));
	}
	return(std::string("(ThermometerType::Unknown)"));
}
ThermometerType String2ThermometerType(const std::string Text)
{
	ThermometerType rval = ThermometerType::Unknown;
	// https://regex101.com/ and https://en.cppreference.com/w/cpp/regex/regex_search
	if (std::regex_search(Text, std::regex("GVH5100")))
		rval = ThermometerType::H5100;
	else if (std::regex_search(Text, std::regex("GVH5101")))
		rval = ThermometerType::H5101;
	else if (std::regex_search(Text, std::regex("GVH5104")))
		rval = ThermometerType::H5104;
	else if (std::regex_search(Text, std::regex("GVH5105")))
		rval = ThermometerType::H5105;
	else if (std::regex_search(Text, std::regex("GVH5174")))
		rval = ThermometerType::H5174;
	else if (std::regex_search(Text, std::regex("GVH5177")))
		rval = ThermometerType::H5177;
	else if (std::regex_search(Text, std::regex("GVH5072")))
		rval = ThermometerType::H5072;
	else if (std::regex_search(Text, std::regex("GVH5075")))
		rval = ThermometerType::H5075;
	else if (std::regex_search(Text, std::regex("Govee_H5074|GVH5074")))
		rval = ThermometerType::H5074;
	else if (std::regex_search(Text, std::regex("Govee_H5179|GVH5179")))
		rval = ThermometerType::H5179;
	//The Bluetooth SIG maintains a list of "Assigned Numbers" that includes those UUIDs found in the sample app: https://www.bluetooth.com/specifications/assigned-numbers/
	//Although UUIDs are 128 bits in length, the assigned numbers for Bluetooth LE are listed as 16 bit hex values because the lower 96 bits are consistent across a class of attributes.
	//For example, all BLE characteristic UUIDs are of the form:
	//0000XXXX-0000-1000-8000-00805f9b34fb
	else if (std::regex_search(Text, std::regex("GVH5181|00008151-0000-1000-8000-00805f9b34fb")))
		rval = ThermometerType::H5181;
	else if (std::regex_search(Text, std::regex("GVH5182|00008251-0000-1000-8000-00805f9b34fb")))
		rval = ThermometerType::H5182;
	//[2024-08-15T16:07:11] [C3:31:30:30:13:27] UUIDs: 00008251-0000-1000-8000-00805f9b34fb
	//[2024-08-15T16:07:11] [C3:31:30:30:13:27] ManufacturerData: *** Meat Thermometer ***  1330:2701000101e4018008341cdc8008341cdc
	//[2024-08-15T16:07:11] [C3:31:30:30:13:27] (Temp) 21°C (Alarm) 73.88°C (Temp) 21°C (Alarm) 73.88°C (Humidity) 0% (Battery) 100% (GVH5182)
	else if (std::regex_search(Text, std::regex("GVH5183|00008351-0000-1000-8000-00805f9b34fb")))
		rval = ThermometerType::H5183;
	//[2024-08-15T15:58:15] [A4:C1:38:5D:A1:B4] UUIDs: 00008351-0000-1000-8000-00805f9b34fb
	//[2024-08-15T15:58:15] [A4:C1:38:5D:A1:B4] ManufacturerData: *** Meat Thermometer ***  a15d:b401000101e4008b083426480000 'Apple, Inc.' 004c:0215494e54454c4c495f524f434b535f48575075f2ff0c
	//[2024-08-15T15:58:15] [A4:C1:38:5D:A1:B4] (Temp) 21°C (Alarm) 98°C (Humidity) 0% (Battery) 100% (GVH5183)
	else if (std::regex_search(Text, std::regex("GVH5184|00008451-0000-1000-8000-00805f9b34fb")))
		rval = ThermometerType::H5184;
	else if (std::regex_search(Text, std::regex("GVH5055|00005550-0000-1000-8000-00805f9b34fb")))
		rval = ThermometerType::H5055;
	return(rval);
}
class  Govee_Temp {
public:
	time_t Time;
	std::string WriteTXT(const char seperator = '\t') const;
	std::string WriteCache(void) const;
	std::string WriteConsole(void) const;
	bool ReadCache(const std::string& data);
	bool ReadMSG(const uint8_t * const data);
	bool ReadMSG(const uint16_t Manufacturer, const std::vector<uint8_t>& Data);
	Govee_Temp() : Time(0), Temperature{ 0, 0, 0, 0 }, TemperatureMin{ DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX }, TemperatureMax{ -DBL_MAX, -DBL_MAX, -DBL_MAX, -DBL_MAX }, Humidity(0), HumidityMin(DBL_MAX), HumidityMax(-DBL_MAX), Battery(INT_MAX), Averages(0), Model(ThermometerType::Unknown) { };
	Govee_Temp(const time_t tim, const double tem, const double hum, const int bat)
	{
		Time = tim;
		Temperature[0] = tem;
		TemperatureMin[0] = tem;
		TemperatureMax[0] = tem;
		Humidity = hum;
		HumidityMin = hum;
		HumidityMax = hum;
		Battery = bat;
		Averages = 1;
	};
	Govee_Temp(const std::string & data);
	double GetTemperature(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return((Temperature[index] * 9.0 / 5.0) + 32.0); return(Temperature[index]); };
	double GetTemperatureMin(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return(std::min(((Temperature[index] * 9.0 / 5.0) + 32.0), ((TemperatureMin[index] * 9.0 / 5.0) + 32.0))); return(std::min(Temperature[index], TemperatureMin[index])); };
	double GetTemperatureMax(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return(std::max(((Temperature[index] * 9.0 / 5.0) + 32.0), ((TemperatureMax[index] * 9.0 / 5.0) + 32.0))); return(std::max(Temperature[index], TemperatureMax[index])); };
	void SetMinMax(const Govee_Temp & a);
	double GetHumidity(void) const { return(Humidity); };
	double GetHumidityMin(void) const { return(std::min(Humidity, HumidityMin)); };
	double GetHumidityMax(void) const { return(std::max(Humidity, HumidityMax)); };
	int GetBattery(void) const { return(Battery); };
	ThermometerType GetModel(void) const { return(Model); };
	const std::string GetModelAsString(void) const { return(ThermometerType2String(Model)); };
	ThermometerType SetModel(const std::string& Name);
	ThermometerType SetModel(const unsigned short* UUID);
	ThermometerType SetModel(const ThermometerType newModel) { ThermometerType oldModel = Model; Model = newModel; return(oldModel); };
	enum granularity {day, week, month, year};
	void NormalizeTime(granularity type);
	granularity GetTimeGranularity(void) const;
	bool IsValid(void) const { return((Averages > 0) && (Model != ThermometerType::Unknown)); };
	Govee_Temp& operator +=(const Govee_Temp& b);
protected:
	double Temperature[4];
	double TemperatureMin[4];
	double TemperatureMax[4];
	double Humidity;
	double HumidityMin;
	double HumidityMax;
	int Battery;
	int Averages;
	ThermometerType Model;
};
Govee_Temp::Govee_Temp(const std::string & data)
{
	std::istringstream TheLine(data);
	// erase any nulls from the data. these are occasionally in the log file when the platform crashed during a write to the logfile.
	while (TheLine.peek() == '\000')
		TheLine.get();
	std::string theDay;
	TheLine >> theDay;
	std::string theHour;
	TheLine >> theHour;
	std::string theDate(theDay + " " + theHour);
	Time = ISO8601totime(theDate);
	TheLine >> Temperature[0];
	TemperatureMin[0] = TemperatureMax[0] = Temperature[0];
	TheLine >> Humidity;
	HumidityMin = HumidityMax = Humidity;
	TheLine >> Battery;
	if (!TheLine.eof())
	{
		int theModel(0);
		TheLine >> theModel;
		switch (theModel)
		{
		case 5181:
			Model = ThermometerType::H5181;
			break;
		case 5182:
			Model = ThermometerType::H5182;
			break;
		case 5183:
			Model = ThermometerType::H5183;
			break;
		case 5184:
			Model = ThermometerType::H5184;
			break;
		case 5055:
			Model = ThermometerType::H5055;
			break;
		default:
			Model = ThermometerType::Unknown;
		}
		unsigned long index = 1;
		while ((!TheLine.eof()) && (index < (sizeof(Temperature) / sizeof(Temperature[0]))))
		{
			TheLine >> Temperature[index];
			TemperatureMin[index] = TemperatureMax[index] = Temperature[index];
			index++;
		}
	}
	time_t timeNow(0);
	time(&timeNow);
	if (Time <= timeNow) // Only validate data from the past.
		Averages = 1;
	// h5074, h5075, h5100, h5179 Temperature Range = -20C to 60C
	// h5103 Temperature Range = 0C to 50C
	if (Temperature[0] < -20)
		Averages = 0; // invalidate the data
}
std::string Govee_Temp::WriteTXT(const char seperator) const
{
	std::ostringstream ssValue;
	ssValue << timeToExcelDate(Time);
	ssValue << seperator << Temperature[0];
	ssValue << seperator << Humidity;
	ssValue << seperator << Battery;
	if (Model == ThermometerType::H5181)
	{
		ssValue << seperator << 5181;
		ssValue << seperator << Temperature[1];
	}
	if (Model == ThermometerType::H5182)
	{
		ssValue << seperator << 5182;
		ssValue << seperator << Temperature[1];
		ssValue << seperator << Temperature[2];
		ssValue << seperator << Temperature[3];
	}
	if (Model == ThermometerType::H5183)
	{
		ssValue << seperator << 5183;
		ssValue << seperator << Temperature[1];
	}
	if (Model == ThermometerType::H5184)
	{
		ssValue << seperator << 5184;
		ssValue << seperator << Temperature[1];
		ssValue << seperator << Temperature[2];
		ssValue << seperator << Temperature[3];
	}
	if (Model == ThermometerType::H5055)
	{
		ssValue << seperator << 5055;
		ssValue << seperator << Temperature[1];
		ssValue << seperator << Temperature[2];
		ssValue << seperator << Temperature[3];
	}
	return(ssValue.str());
}
std::string Govee_Temp::WriteCache(void) const
{
	std::ostringstream ssValue;
	ssValue << Time;
	for (auto a : Temperature)
		ssValue << "\t" << a;
	for (auto a : TemperatureMin)
		ssValue << "\t" << a;
	for (auto a : TemperatureMax)
		ssValue << "\t" << a;
	ssValue << "\t" << Humidity;
	ssValue << "\t" << HumidityMin;
	ssValue << "\t" << HumidityMax;
	ssValue << "\t" << Battery;
	ssValue << "\t" << Averages;
	ssValue << "\t" << GetModelAsString();
	return(ssValue.str());
}
std::string Govee_Temp::WriteConsole(void) const
{
	std::ostringstream ssValue;
	ssValue << "(Temp) " << std::setw(4) << std::dec << std::fixed << std::setprecision(1) << GetTemperature() << "\u00B0" << "C";
	if ((Model == ThermometerType::H5182) || (Model == ThermometerType::H5184) || (Model == ThermometerType::H5055))
	{
		ssValue << " (Alarm) " << GetTemperature(false, 1) << "\u00B0" << "C";
		ssValue << " (Temp) " << GetTemperature(false, 2) << "\u00B0" << "C";
		ssValue << " (Alarm) " << GetTemperature(false, 3) << "\u00B0" << "C";
	}
	if (Model == ThermometerType::H5183)
		ssValue << " (Alarm) " << GetTemperature(false, 1) << "\u00B0" << "C";
	if (!((Model == ThermometerType::H5183) || (Model == ThermometerType::H5182) || (Model == ThermometerType::H5184) || (Model == ThermometerType::H5055)))
		ssValue << " (Humidity) " << std::setw(5) << std::right << GetHumidity() << std::left << "%";
	ssValue << " (Battery) " << std::setw(3) << std::right << std::setprecision(0) << GetBattery() << std::left << "%";
	ssValue << " " << GetModelAsString();
	return(ssValue.str());
}
bool Govee_Temp::ReadCache(const std::string& data)
{
	bool rval = false;
	std::istringstream ssValue(data);
	ssValue >> Time;
	for (auto & a : Temperature)
		ssValue >> a;
	for (auto & a : TemperatureMin)
		ssValue >> a;
	for (auto & a : TemperatureMax)
		ssValue >> a;
	ssValue >> Humidity;
	ssValue >> HumidityMin;
	ssValue >> HumidityMax;
	ssValue >> Battery;
	ssValue >> Averages;
	return(rval);
}
ThermometerType Govee_Temp::SetModel(const std::string& Name)
{
	ThermometerType rval = Model;
	Model = String2ThermometerType(Name);
	return(rval);
}
ThermometerType Govee_Temp::SetModel(const unsigned short* UUID)
{
	ThermometerType rval = Model;
	// 88EC could be either GVH5075_ or GVH5174_
	if (0x8151 == *UUID)
		Model = ThermometerType::H5181;
	else if (0x8251 == *UUID)
		Model = ThermometerType::H5182;
	else if (0x8351 == *UUID)
		Model = ThermometerType::H5183;
	else if (0x8451 == *UUID)
		Model = ThermometerType::H5184;
	else if (0x5550 == *UUID)
		Model = ThermometerType::H5055;
	return(rval);
}
bool Govee_Temp::ReadMSG(const uint8_t * const data)
{
	bool rval = false;
	const size_t data_len = data[0];
	if (data[1] == 0xFF) // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/ «Manufacturer Specific Data»
	{
		if ((data_len == 9) && (data[2] == 0x88) && (data[3] == 0xEC)) // GVH5075_xxxx
		{
			if (Model == ThermometerType::Unknown)
				Model = ThermometerType::H5075;
			// This data came from https://github.com/Thrilleratplay/GoveeWatcher
			// 88ec00 03519e 64 00 Temp: 21.7502°C Temp: 71.1504°F Humidity: 50.2%
			// 2 3 4  5 6 7  8
			int iTemp = int(data[5]) << 16 | int(data[6]) << 8 | int(data[7]);
			bool bNegative = iTemp & 0x800000;	// check sign bit
			iTemp = iTemp & 0x7ffff;			// mask off sign bit
			Temperature[0] = float(iTemp / 1000) / 10.0; // issue #49 fix. 
			// After converting the hexadecimal number into decimal the first three digits are the 
			// temperature and the last three digits are the humidity.So "03519e" converts to "217502" 
			// which means 21.7 °C and 50.2 % humidity without any rounding.
			if (bNegative)						// apply sign bit
				Temperature[0] = -1.0 * Temperature[0];
			Humidity = float(iTemp % 1000) / 10.0;
			Battery = int(data[8]);
			Averages = 1;
			time(&Time);
			TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
			rval = true;
		}
		else if ((data_len == 10) && (data[2] == 0x88) && (data[3] == 0xEC))// Govee_H5074_xxxx
		{
			if (Model == ThermometerType::Unknown)
				Model = ThermometerType::H5074;
			// This data came from https://github.com/neilsheps/GoveeTemperatureAndHumidity
			// 88EC00 0902 CD15 64 02 (Temp) 41.378°F (Humidity) 55.81% (Battery) 100%
			// 2 3 4  5 6  7 8  9
			short iTemp = short(data[6]) << 8 | short(data[5]);
			int iHumidity = int(data[8]) << 8 | int(data[7]);
			Temperature[0] = float(iTemp) / 100.0;
			Humidity = float(iHumidity) / 100.0;
			Battery = int(data[9]);
			Averages = 1;
			time(&Time);
			TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
			rval = true;
		}
		else if ((data_len == 12) && (data[2] == 0x01) && (data[3] == 0x88) && (data[4] == 0xEC)) // Govee_H5179
		{
			if (Model == ThermometerType::Unknown)
				Model = ThermometerType::H5179;
			// This is from data provided in https://github.com/wcbonner/GoveeBTTempLogger/issues/36
			// 0188EC00 0101 0A0A B018 64 (Temp) 25.7°C (Humidity) 63.2% (Battery) 100% (GVH5179)
			// 2 3 4 5  6 7  8 9  1011 12
			short iTemp = short(data[9]) << 8 | short(data[8]);
			int iHumidity = int(data[11]) << 8 | int(data[10]);
			Temperature[0] = float(iTemp) / 100.0;
			Humidity = float(iHumidity) / 100.0;
			Battery = int(data[12]);
			Averages = 1;
			time(&Time);
			TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
			rval = true;
		}
		else if ((data_len == 9) && (data[2] == 0x01) && (data[3] == 0x00)) // GVH5177_xxxx or GVH5174_xxxx or GVH5100_xxxx
		{
			// This is a guess based on the H5075 3 byte encoding
			// 01000101 029D1B 64 (Temp) 62.8324°F (Humidity) 29.1% (Battery) 100%
			// 2 3 4 5  6 7 8  9
			// It appears that the H5174 uses the exact same data format as the H5177, with the difference being the broadcase name starting with GVH5174_
			int iTemp = int(data[6]) << 16 | int(data[7]) << 8 | int(data[8]);
			bool bNegative = iTemp & 0x800000;	// check sign bit
			iTemp = iTemp & 0x7ffff;			// mask off sign bit
			Temperature[0] = float(iTemp) / 10000.0;
			Humidity = float(iTemp % 1000) / 10.0;
			if (bNegative)						// apply sign bit
				Temperature[0] = -1.0 * Temperature[0];
			Battery = int(data[9]);
			Averages = 1;
			time(&Time);
			TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
			rval = true;
		}
		else if (data_len == 17 && (data[5] == 0x01) && (data[6] == 0x00) && (data[7] == 0x01) && (data[8] == 0x01)) // GVH5183 (UUID) 5183 B5183011
		{
			if (Model == ThermometerType::Unknown)
				Model = ThermometerType::H5183;
			// Govee Bluetooth Wireless Meat Thermometer, Digital Grill Thermometer with 1 Probe, 230ft Remote Temperature Monitor, Smart Kitchen Cooking Thermometer, Alert Notifications for BBQ, Oven, Smoker, Cakes
			// https://www.amazon.com/gp/product/B092ZTD96V
			// The probe measuring range is 0° to 300°C /32° to 572°F.
			// 5DA1B4 01000101 E4 01 80 0708 13 24 00 00
			// 2 3 4  5 6 7 8  9  0  1  2 3  4  5  6  7
			// (Manu) 5DA1B4 01000101 81 0180 07D0 1324 0000 (Temp) 20°C (Temp) 49°C (Battery) 1% (Other: 00)  (Other: 00)  (Other: 00)  (Other: 00)  (Other: 00)  (Other: BF) 
			// the first three bytes are the last three bytes of the bluetooth address.
			// then next four bytes appear to be a signature for the device type.
			// Model = ThermometerType::H5181;
			// Govee Bluetooth Meat Thermometer, 230ft Range Wireless Grill Thermometer Remote Monitor with Temperature Probe Digital Grilling Thermometer with Smart Alerts for Smoker Cooking BBQ Kitchen Oven
			// https://www.amazon.com/dp/B092ZTJW37/
			short iTemp = short(data[12]) << 8 | short(data[13]);
			Temperature[0] = float(iTemp) / 100.0;
			iTemp = short(data[14]) << 8 | short(data[15]);
			Temperature[1] = float(iTemp) / 100.0; // This appears to be the alarm temperature.
			Humidity = 0;
			Battery = int(data[9] & 0x7F);
			Averages = 1;
			time(&Time);
			for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
				TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
			rval = true;
		}
		else if (data_len == 20 && (data[5] == 0x01) && (data[6] == 0x00) && (data[7] == 0x01) && (data[8] == 0x01)) // GVH5182 (UUID) 5182 (Manu) 30132701000101E4018606A413F78606A41318
		{
			if (Model == ThermometerType::Unknown)
				Model = ThermometerType::H5182;
			// Govee Bluetooth Meat Thermometer, 230ft Range Wireless Grill Thermometer Remote Monitor with Temperature Probe Digital Grilling Thermometer with Smart Alerts for Smoker , Cooking, BBQ, Kitchen, Oven
			// https://www.amazon.com/gp/product/B094N2FX9P
			// 301327 01000101 64 01 80 05DC 1324 86 06A4 FFFF
			// 2 3 4  5 6 7 8  9  0  1  2 3  4 5  6  7 8  9 0
			// (Manu) 301327 01000101 3A 01 86 076C FFFF 86 076C FFFF (Temp) 19°C (Temp) -0.01°C (Temp) 19°C (Temp) -0.01°C (Battery) 58%
			// If the probe is not connected to the device, the temperature data is set to FFFF.
			// If the alarm is not set for the probe, the data is set to FFFF.
			short iTemp = short(data[12]) << 8 | short(data[13]);	// Probe 1 Temperature
			Temperature[0] = float(iTemp) / 100.0;
			iTemp = short(data[14]) << 8 | short(data[15]);			// Probe 1 Alarm Temperature
			Temperature[1] = float(iTemp) / 100.0;
			iTemp = short(data[17]) << 8 | short(data[18]);			// Probe 2 Temperature
			Temperature[2] = float(iTemp) / 100.0;
			iTemp = short(data[19]) << 8 | short(data[20]);			// Probe 2 Alarm Temperature
			Temperature[3] = float(iTemp) / 100.0;
			Humidity = 0;
			Battery = int(data[9] & 0x7F);
			Averages = 1;
			time(&Time);
			for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
				TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
			rval = true;
		}
	}
	return(rval);
}
bool Govee_Temp::ReadMSG(const uint16_t Manufacturer, const std::vector<uint8_t>& Data)
{
	bool rval = false;
	if ((Manufacturer == 0xec88) && (Data.size() == 7))// Govee_H5074_xxxx
	{
		if (Model == ThermometerType::Unknown)
			Model = ThermometerType::H5074;
		//[2024-08-12T22:53:41] [E3:5E:CC:21:5C:0F] Name: Govee_H5074_5C0F
		//[2024-08-12T22:53:41] [E3:5E:CC:21:5C:0F] ManufacturerData:  ec88:00f8099f1c6402
		//[2024-08-12T22:53:41] [E3:5E:CC:21:5C:0F] (Temp) 25.52°C (Humidity) 73.27% (Battery) 100% (GVH5074) 
		short iTemp = short(Data[2]) << 8 | short(Data[1]);
		int iHumidity = int(Data[4]) << 8 | int(Data[3]);
		Temperature[0] = float(iTemp) / 100.0;
		Humidity = float(iHumidity) / 100.0;
		Battery = int(Data[5]);
		Averages = 1;
		time(&Time);
		TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
		rval = true;
	}
	else if ((Manufacturer == 0xec88) && (Data.size() == 6))// GVH5075_xxxx
	{
		if (Model == ThermometerType::Unknown)
			Model = ThermometerType::H5075;
		//[2024-08-12T23:09:07] [A4:C1:38:37:BC:AE] Name: GVH5075_BCAE
		//[2024-08-12T23:09:07] [A4:C1:38:37:BC:AE] ManufacturerData:  ec88:000418876100 004c:0215494e54454c4c495f524f434b535f48575075f2ffc2
		//[2024-08-12T23:09:07] [A4:C1:38:37:BC:AE] (Temp) 26.8°C (Humidity) 42.3% (Battery) 97% (GVH5075)
		int iTemp = int(Data[1]) << 16 | int(Data[2]) << 8 | int(Data[3]);
		bool bNegative = iTemp & 0x800000;	// check sign bit
		iTemp = iTemp & 0x7ffff;			// mask off sign bit
		Temperature[0] = float(iTemp / 1000) / 10.0; // issue #49 fix. 
		// After converting the hexadecimal number into decimal the first three digits are the 
		// temperature and the last three digits are the humidity.So "03519e" converts to "217502" 
		// which means 21.7 °C and 50.2 % humidity without any rounding.
		if (bNegative)						// apply sign bit
			Temperature[0] = -1.0 * Temperature[0];
		Humidity = float(iTemp % 1000) / 10.0;
		Battery = int(Data[4]);
		Averages = 1;
		time(&Time);
		TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
		rval = true;
	}
	else if ((Manufacturer == 0x0001) && (Data.size() == 6))// GVH5177_xxxx or GVH5174_xxxx or GVH5100_xxxx
	{
		// This is a guess based on the H5075 3 byte encoding
		// It appears that the H5174 uses the exact same data format as the H5177, with the difference being the broadcast name starting with GVH5174_
		//[2024-08-13T00:09:15] [A4:C1:38:0D:3B:10] Name: GVH5177_3B10
		//[2024-08-13T00:09:15] [A4:C1:38:0D:3B:10] ManufacturerData: 0001:010104245d54 004c:0215494e54454c4c495f524f434b535f48575177f2ffc2
		//[2024-08-13T00:09:15] [A4:C1:38:0D:3B:10] (Temp) 27.1453°C (Humidity) 45.3% (Battery) 84% (GVH5177)
		int iTemp = int(Data[2]) << 16 | int(Data[3]) << 8 | int(Data[4]);
		bool bNegative = iTemp & 0x800000;	// check sign bit
		iTemp = iTemp & 0x7ffff;			// mask off sign bit
		Temperature[0] = float(iTemp) / 10000.0;
		Humidity = float(iTemp % 1000) / 10.0;
		if (bNegative)						// apply sign bit
			Temperature[0] = -1.0 * Temperature[0];
		Battery = int(Data[5]);
		Averages = 1;
		time(&Time);
		TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
		rval = true;
	}
	else if ((Manufacturer == 0xec88) && (Data.size() == 9)) // Govee_H5179
	{
		if (Model == ThermometerType::Unknown)
			Model = ThermometerType::H5179;
		// This is from data provided in https://github.com/wcbonner/GoveeBTTempLogger/issues/36
		// 0188EC00 0101 0A0A B018 64 (Temp) 25.7°C (Humidity) 63.2% (Battery) 100% (GVH5179)
		// 2 3 4 5  6 7  8 9  1011 12
		short iTemp = short(Data[5]) << 8 | short(Data[4]);
		int iHumidity = int(Data[7]) << 8 | int(Data[6]);
		Temperature[0] = float(iTemp) / 100.0;
		Humidity = float(iHumidity) / 100.0;
		Battery = int(Data[8]);
		Averages = 1;
		time(&Time);
		TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
		rval = true;
	}
	else if (0x004c != Manufacturer) // Ignore 'Apple, Inc.'
	{
		if (Data.size() == 14)	// I'm not checking the Manufacturer data because it appears to be part of the Bluetooth Address on this device
		{
			// Govee Bluetooth Wireless Meat Thermometer, Digital Grill Thermometer with 1 Probe, 230ft Remote Temperature Monitor, Smart Kitchen Cooking Thermometer, Alert Notifications for BBQ, Oven, Smoker, Cakes
			// https://www.amazon.com/gp/product/B092ZTD96V
			// The probe measuring range is 0° to 300°C /32° to 572°F.
			//[2024-08-14T16:43:01] [A4:C1:38:5D:A1:B4] ManufacturerData: a15d:b401000101e4008b09c426480000 004c:0215494e54454c4c495f524f434b535f48575075f2ff0c
			//[2024-08-14T16:43:01] [A4:C1:38:5D:A1:B4] (Temp) 25°C (Alarm) 98°C (Humidity) 0% (Battery) 100% (GVH5183)
			short iTemp = short(Data[8]) << 8 | short(Data[9]);
			Temperature[0] = float(iTemp) / 100.0;
			iTemp = short(Data[10]) << 8 | short(Data[11]);
			Temperature[1] = float(iTemp) / 100.0; // This appears to be the alarm temperature.
			Humidity = 0;
			Battery = int(Data[5] & 0x7F);
			Averages = 1;
			time(&Time);
			for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
				TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
			rval = true;
		}
		else if (Data.size() == 17)	// I'm not checking the Manufacturer data because it appears to be part of the Bluetooth Address on this device
		{
			// Govee Bluetooth Meat Thermometer, 230ft Range Wireless Grill Thermometer Remote Monitor with Temperature Probe Digital Grilling Thermometer with Smart Alerts for Smoker , Cooking, BBQ, Kitchen, Oven
			// https://www.amazon.com/gp/product/B094N2FX9P
			// If the probe is not connected to the device, the temperature data is set to FFFF.
			// If the alarm is not set for the probe, the data is set to FFFF.
			//[2024-08-14T17:47:34] [C3:31:30:30:13:27] ManufacturerData: 1330:2701000101e4018008341cdc8008341cdc 004c:0215494e54454c4c495f524f434b535f48575075f2ff0c
			//[2024-08-14T17:47:34] [C3:31:30:30:13:27] (Temp) 21°C (Alarm) 73.88°C (Temp) 21°C (Alarm) 73.88°C (Humidity) 0% (Battery) 100% (GVH5182)

			// The H5184 seems to use this same data format, and alternates sending probes 1-2 and 3-4. 
			// I've not figured out how to recognize which set of probes are currently being sent.
			// it may be a single bit in byte 12.
			//wim@WimPi4:~ $ ~/GoveeBTTempLogger/build/goveebttemplogger -v 2 | grep CF\:32\:32\:36\:4F\:62
			// Alarms set to 60, 71, 49, and 93                                0  1 2 3 4  5  6 7  8 9  0 1  2  3 4  5 6
			//[2024-08-15T03:08:39] [CF:32:32:36:4F:62] ManufacturerData: 4f36:62 01000101 64 0180 0834 1770 89 0898 1bbc
			//[2024-08-15T03:08:39] [CF:32:32:36:4F:62] (Temp) 21°C (Alarm) 60°C (Temp) 22°C (Alarm) 71°C (Battery) 100% (GVH5182)
			//[2024-08-15T03:08:40] [CF:32:32:36:4F:62] ManufacturerData: 4f36:62 01000101 64 028a 0834 1324 8c 0898 2454
			//[2024-08-15T03:08:40] [CF:32:32:36:4F:62] (Temp) 21°C (Alarm) 49°C (Temp) 22°C (Alarm) 93°C (Battery) 100% (GVH5182)
			//[2024-08-15T03:08:41] [CF:32:32:36:4F:62] ManufacturerData: 4f36:62 01000101 64 0180 0834 1770 89 0898 1bbc 004c:0215494e54454c4c495f524f434b535f48575075f2ff0c
			//[2024-08-15T03:08:41] [CF:32:32:36:4F:62] (Temp) 21°C (Alarm) 60°C (Temp) 22°C (Alarm) 71°C (Battery) 100% (GVH5182)
			//[2024-08-15T03:08:42] [CF:32:32:36:4F:62] ManufacturerData: 4f36:62 01000101 64 028a 0834 1324 8c 0898 2454
			//[2024-08-15T03:08:42] [CF:32:32:36:4F:62] (Temp) 21°C (Alarm) 49°C (Temp) 22°C (Alarm) 93°C (Battery) 100% (GVH5182)

			short iTemp = short(Data[8]) << 8 | short(Data[9]);	// Probe 1 Temperature
			Temperature[0] = float(iTemp) / 100.0;
			iTemp = short(Data[10]) << 8 | short(Data[11]);		// Probe 1 Alarm Temperature
			Temperature[1] = float(iTemp) / 100.0;
			iTemp = short(Data[13]) << 8 | short(Data[14]);		// Probe 2 Temperature
			Temperature[2] = float(iTemp) / 100.0;
			iTemp = short(Data[15]) << 8 | short(Data[16]);		// Probe 2 Alarm Temperature
			Temperature[3] = float(iTemp) / 100.0;
			Humidity = 0;
			Battery = int(Data[5] & 0x7f);
			Averages = 1;
			time(&Time);
			for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
				TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
			rval = true;
		}
		else if (Data.size() == 20)	// I'm not checking the Manufacturer data because it appears to be part of the Bluetooth Address on this device
		{
			// GVH 5055 sample data
			//[                   ] [A4:C1:38:85:8B:A4] UUIDs: 00005550-0000-1000-8000-00805f9b34fb
			//alarms set at 0x31, 0x36, 0x3c, 0x42, 0x4d, 0x5d
			//                                                                  0  1 2  3 4  5 6  7 8  9 0  1  2 3  4 5  6 7  8 9
			// probe 1
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 0100 1a00 ffff 3100 01 ffff ffff 3600 0000
			// probe 2
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 0200 ffff ffff 3100 01 1b00 ffff 3600 0000
			// probe 3
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 4400 1a00 ffff 3c00 00 ffff ffff 4200 0000
			// probe 4
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 4800 ffff ffff 3c00 00 1a00 ffff 4200 0000
			// probe 5
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 9000 1a00 ffff 4d00 0c ffff ffff 5d00 0000
			// probe 6
			// [                   ] [A4:C1:38:85:8B:A4] ManufacturerData: 8b85:a4 0064 a000 ffff ffff 4d00 0c 1900 ffff 5d00 0000
			// It's possible that Byte 11 indicates which probe data is being sent, 0x01:1-2, 0x00:3-4, 0x0c:5-6
			//if (Data[11] == 0x01)
			{
				short iTemp = short(Data[6]) << 8 | short(Data[5]);	// Probe 1 Temperature
				Temperature[0] = float(iTemp);
				iTemp = short(Data[8]) << 8 | short(Data[7]);		// Probe 1 Low Alarm Temperature
				iTemp = short(Data[10]) << 8 | short(Data[9]);		// Probe 1 High Alarm Temperature
				Temperature[1] = float(iTemp);
				iTemp = short(Data[13]) << 8 | short(Data[12]);		// Probe 2 Temperature
				Temperature[2] = float(iTemp);
				iTemp = short(Data[15]) << 8 | short(Data[14]);		// Probe 2 Low Alarm Temperature
				iTemp = short(Data[17]) << 8 | short(Data[16]);		// Probe 2 High Alarm Temperature
				Temperature[3] = float(iTemp);

				Humidity = 0;
				Battery = int(Data[2]);
				Averages = 1;
				time(&Time);
				for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
					TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
				rval = true;
			}
		}
	}
	return(rval);
}
void Govee_Temp::SetMinMax(const Govee_Temp& a)
{
	for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
	{
		TemperatureMin[index] = TemperatureMin[index] < Temperature[index] ? TemperatureMin[index] : Temperature[index];
		TemperatureMax[index] = TemperatureMax[index] > Temperature[index] ? TemperatureMax[index] : Temperature[index];

		TemperatureMin[index] = TemperatureMin[index] < a.TemperatureMin[index] ? TemperatureMin[index] : a.TemperatureMin[index];
		TemperatureMax[index] = TemperatureMax[index] > a.TemperatureMax[index] ? TemperatureMax[index] : a.TemperatureMax[index];
	}
	HumidityMin = HumidityMin < Humidity ? HumidityMin : Humidity;
	HumidityMax = HumidityMax > Humidity ? HumidityMax : Humidity;

	HumidityMin = HumidityMin < a.HumidityMin ? HumidityMin : a.HumidityMin;
	HumidityMax = HumidityMax > a.HumidityMax ? HumidityMax : a.HumidityMax;
}
void Govee_Temp::NormalizeTime(granularity type)
{
	if (type == day)
		Time = (Time / DAY_SAMPLE) * DAY_SAMPLE;
	else if (type == week)
		Time = (Time / WEEK_SAMPLE) * WEEK_SAMPLE;
	else if (type == month)
		Time = (Time / MONTH_SAMPLE) * MONTH_SAMPLE;
	else if (type == year)
	{
		struct tm UTC;
		if (0 != localtime_r(&Time, &UTC))
		{
			UTC.tm_hour = 0;
			UTC.tm_min = 0;
			UTC.tm_sec = 0;
			Time = mktime(&UTC);
		}
	}
}
Govee_Temp::granularity Govee_Temp::GetTimeGranularity(void) const
{
	granularity rval = granularity::day;
	struct tm UTC;
	if (0 != localtime_r(&Time, &UTC))
	{
		//if (((UTC.tm_hour == 0) && (UTC.tm_min == 0)) || ((UTC.tm_hour == 23) && (UTC.tm_min == 0) && (UTC.tm_isdst == 1)))
		if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
				rval = granularity::year;
		else if ((UTC.tm_hour % 2 == 0) && (UTC.tm_min == 0))
			rval = granularity::month;
		else if ((UTC.tm_min == 0) || (UTC.tm_min == 30))
			rval = granularity::week;
	}
	return(rval);
}
Govee_Temp& Govee_Temp::operator +=(const Govee_Temp& b)
{
	if (b.IsValid())
	{
		Time = std::max(Time, b.Time); // Use the maximum time (newest time)
		for (unsigned long index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
		{
			Temperature[index] = ((Temperature[index] * Averages) + (b.Temperature[index] * b.Averages)) / (Averages + b.Averages);
			TemperatureMin[index] = std::min(std::min(Temperature[index], TemperatureMin[index]), b.TemperatureMin[index]);
			TemperatureMax[index] = std::max(std::max(Temperature[index], TemperatureMax[index]), b.TemperatureMax[index]);
		}
		Humidity = ((Humidity * Averages) + (b.Humidity * b.Averages)) / (Averages + b.Averages);
		HumidityMin = std::min(std::min(Humidity, HumidityMin), b.HumidityMin);
		HumidityMax = std::max(std::max(Humidity, HumidityMax), b.HumidityMax);
		Battery = std::min(Battery, b.Battery);
		Averages += b.Averages; // existing average + new average
		Model = b.Model; // This is important in case "a" was initialized but not valid
	}
	return(*this);
}
/////////////////////////////////////////////////////////////////////////////
std::string iBeacon(const uint8_t * const data)
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
// The following operator was required so I could use the std::map<> to use BlueTooth Addresses as the key
bool operator <(const bdaddr_t &a, const bdaddr_t &b)
{
	unsigned long long A = a.b[5];
	A = A << 8 | a.b[4];
	A = A << 8 | a.b[3];
	A = A << 8 | a.b[2];
	A = A << 8 | a.b[1];
	A = A << 8 | a.b[0];
	unsigned long long B = b.b[5];
	B = B << 8 | b.b[4];
	B = B << 8 | b.b[3];
	B = B << 8 | b.b[2];
	B = B << 8 | b.b[1];
	B = B << 8 | b.b[0];
	return(A < B);
}
bool operator ==(const bdaddr_t& a, const bdaddr_t& b)
{
	unsigned long long A = a.b[5];
	A = A << 8 | a.b[4];
	A = A << 8 | a.b[3];
	A = A << 8 | a.b[2];
	A = A << 8 | a.b[1];
	A = A << 8 | a.b[0];
	unsigned long long B = b.b[5];
	B = B << 8 | b.b[4];
	B = B << 8 | b.b[3];
	B = B << 8 | b.b[2];
	B = B << 8 | b.b[1];
	B = B << 8 | b.b[0];
	return(A == B);
}
/////////////////////////////////////////////////////////////////////////////
std::string ba2string(const bdaddr_t& TheBlueToothAddress)
{
	std::ostringstream oss;
	for (auto i = 5; i >= 0; i--)
	{
		oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(TheBlueToothAddress.b[i]);
		if (i > 0)
			oss << ":";
	}
	return (oss.str());
}
const std::regex BluetoothAddressRegex("((([[:xdigit:]]{2}:){5}))[[:xdigit:]]{2}");
bdaddr_t string2ba(const std::string& TheBlueToothAddressString)
{
	bdaddr_t TheBlueToothAddress({ 0 });
	if (TheBlueToothAddressString.length() == 12)
	{
		std::string ssBTAddress(TheBlueToothAddressString);
		for (auto index = ssBTAddress.length() - 2; index > 0; index -= 2)
			ssBTAddress.insert(index, ":");
		TheBlueToothAddress = string2ba(ssBTAddress);	// Recursive call
	}
	else if (std::regex_match(TheBlueToothAddressString, BluetoothAddressRegex))
	{
		std::stringstream ss(TheBlueToothAddressString);
		std::string byteString;
		int index(5);
		// Because I've verified the string format with regex I can safely run this loop knowing it'll get 6 bytes
		while (std::getline(ss, byteString, ':'))
			TheBlueToothAddress.b[index--] = std::stoi(byteString, nullptr, 16);
	}
	return(TheBlueToothAddress);
}
/////////////////////////////////////////////////////////////////////////////
std::map<bdaddr_t, std::queue<Govee_Temp>> GoveeTemperatures;
std::map<bdaddr_t, ThermometerType> GoveeThermometers;
std::map<bdaddr_t, time_t> GoveeLastDownload;
const std::filesystem::path GVHLastDownloadFileName("gvh-lastdownload.txt");
/////////////////////////////////////////////////////////////////////////////
volatile bool bRun = true; // This is declared volatile so that the compiler won't optimized it out of loops later in the code
void SignalHandlerSIGINT(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << std::endl;
}
void SignalHandlerSIGHUP(int signal)
{
	bRun = false;
	std::cerr << "***************** SIGHUP: Caught HangUp, finishing loop and quitting. *****************" << std::endl;
}
void SignalHandlerSIGALRM(int signal)
{
	if (ConsoleVerbosity > 0)
		std::cout << "[" << getTimeISO8601(true) << "] ***************** SIGALRM: Caught Alarm. *****************" << std::endl;
}
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(const std::filesystem::path& DirectoryName)
{
	bool rval = false;
	// https://linux.die.net/man/2/stat
	struct stat64 StatBuffer;
	if (0 == stat64(DirectoryName.c_str(), &StatBuffer))
		if (S_ISDIR(StatBuffer.st_mode))
		{
			// https://linux.die.net/man/2/access
			if (0 == access(DirectoryName.c_str(), R_OK | W_OK))
				rval = true;
			else
			{
				switch (errno)
				{
				case EACCES:
					std::cerr << DirectoryName << " (" << errno << ") The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname." << std::endl;
					break;
				case ELOOP:
					std::cerr << DirectoryName << " (" << errno << ") Too many symbolic links were encountered in resolving pathname." << std::endl;
					break;
				case ENAMETOOLONG:
					std::cerr << DirectoryName << " (" << errno << ") pathname is too long." << std::endl;
					break;
				case ENOENT:
					std::cerr << DirectoryName << " (" << errno << ") A component of pathname does not exist or is a dangling symbolic link." << std::endl;
					break;
				case ENOTDIR:
					std::cerr << DirectoryName << " (" << errno << ") A component used as a directory in pathname is not, in fact, a directory." << std::endl;
					break;
				case EROFS:
					std::cerr << DirectoryName << " (" << errno << ") Write permission was requested for a file on a read-only file system." << std::endl;
					break;
				case EFAULT:
					std::cerr << DirectoryName << " (" << errno << ") pathname points outside your accessible address space." << std::endl;
					break;
				case EINVAL:
					std::cerr << DirectoryName << " (" << errno << ") mode was incorrectly specified." << std::endl;
					break;
				case EIO:
					std::cerr << DirectoryName << " (" << errno << ") An I/O error occurred." << std::endl;
					break;
				case ENOMEM:
					std::cerr << DirectoryName << " (" << errno << ") Insufficient kernel memory was available." << std::endl;
					break;
				case ETXTBSY:
					std::cerr << DirectoryName << " (" << errno << ") Write access was requested to an executable which is being executed." << std::endl;
					break;
				default:
					std::cerr << DirectoryName << " (" << errno << ") An unknown error." << std::endl;
				}
			}
		}
	return(rval);
}
// Create a standardized logfile name for this program based on a Bluetooth address and the global parameter of the log file directory.
std::filesystem::path GenerateLogFileName(const bdaddr_t &a, time_t timer = 0)
{
	std::ostringstream OutputFilename;
	// Original version of filename was formatted gvh507x_XXXX with only last two bytes of bluetooth address
	// Second version of filename was formatted gvh507x_XXXXXXXXXXXX with all six bytes of the bluetooth address
	// Third version of filename is formatted gvh-XXXXXXXXXXXX because I've been tired of the 507x for the past two years (2023-04-03)
	//OutputFilename << "gvh507x_";
	//OutputFilename << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(a.b[1]);
	//OutputFilename << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(a.b[0]);
	// The New Format Log File Name includes the entire Bluetooth Address, making it much easier to recognize and add to MRTG config files.
	OutputFilename << "gvh-";
	std::string btAddress(ba2string(a));
	for (auto pos = btAddress.find(':'); pos != std::string::npos; pos = btAddress.find(':'))
		btAddress.erase(pos, 1);
	OutputFilename << btAddress;
	if (timer == 0)
		time(&timer);
	struct tm UTC;
	if (0 != gmtime_r(&timer, &UTC))
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
			OutputFilename << "-" << std::dec << UTC.tm_year + 1900 << "-" << std::setw(2) << std::setfill('0') << UTC.tm_mon + 1;
	OutputFilename << ".txt";
	std::filesystem::path NewFormatFileName(LogDirectory / OutputFilename.str());
	return(NewFormatFileName);
}
bool GenerateLogFile(std::map<bdaddr_t, std::queue<Govee_Temp>> &AddressTemperatureMap, std::map<bdaddr_t, time_t> &PersistenceData)
{
	bool rval = false;
	if (!LogDirectory.empty())
	{
		if (ConsoleVerbosity > 1)
			std::cout << "[" << getTimeISO8601(true) << "] GenerateLogFile: " << LogDirectory << std::endl;
		for (auto it = AddressTemperatureMap.begin(); it != AddressTemperatureMap.end(); ++it)
		{
			if (!it->second.empty()) // Only open the log file if there are entries to add
			{
				std::filesystem::path filename(GenerateLogFileName(it->first));
				std::ofstream LogFile(filename, std::ios_base::out | std::ios_base::app | std::ios_base::ate);
				if (LogFile.is_open())
				{
					time_t MostRecentData(0);
					while (!it->second.empty())
					{
						LogFile << it->second.front().WriteTXT() << std::endl;
						MostRecentData = std::max(it->second.front().Time, MostRecentData);
						it->second.pop();
					}
					LogFile.close();
					struct utimbuf Log_ut;
					Log_ut.actime = MostRecentData;
					Log_ut.modtime = MostRecentData;
					utime(filename.c_str(), &Log_ut);
					rval = true;
				}
			}
		}
		if (!PersistenceData.empty())
		{
			if (ConsoleVerbosity > 0)
				for (auto & iter : PersistenceData)
					std::cout << "[-------------------] [" << ba2string(iter.first) << "] " << timeToISO8601(iter.second) << std::endl;
			// If PersistenceData has updated information, write new data to file
			std::filesystem::path filename(LogDirectory / GVHLastDownloadFileName);
			time_t MostRecentDownload(0);
			for (auto & it : PersistenceData)
				if (MostRecentDownload < it.second)
					MostRecentDownload = it.second;
			bool NewData(true);
			struct stat64 StatBuffer;
			StatBuffer.st_mtim.tv_sec = 0;
			if (0 == stat64(filename.c_str(), &StatBuffer))
			{
				// compare the date of the file with the most recent data in the structure.
				if (MostRecentDownload <= StatBuffer.st_mtim.tv_sec)
					NewData = false;
			}
			if (NewData)
			{
				std::ofstream PersistenceFile(filename, std::ios_base::out | std::ios_base::trunc);
				if (PersistenceFile.is_open())
				{
					for (auto & it : PersistenceData)
						PersistenceFile << ba2string(it.first) << "\t" << timeToISO8601(it.second) << std::endl;
					PersistenceFile.close();
					struct utimbuf Persistut;
					Persistut.actime = MostRecentDownload;
					Persistut.modtime = MostRecentDownload;
					utime(filename.c_str(), &Persistut);
				}
			}
		}
	}
	else
	{
		// clear the queued data if LogDirectory not specified
		for (auto it = AddressTemperatureMap.begin(); it != AddressTemperatureMap.end(); ++it)
		{
			while (!it->second.empty())
			{
				it->second.pop();
			}
		}
	}
	return(rval);
}
bool GetLogEntry(const bdaddr_t &InAddress, const int Minutes, Govee_Temp & OutValue)
{
	// Returned value is now the average of whatever values were recorded over the previous 5 minutes
	bool rval = false;
	std::ifstream TheFile(GenerateLogFileName(InAddress));
	if (TheFile.is_open())
	{
		time_t now = ISO8601totime(getTimeISO8601());
		std::queue<Govee_Temp> LogValues;

		TheFile.seekg(0, std::ios_base::end);      //Start at end of file
		do
		{
			char ch = ' ';                             //Init ch not equal to '\n'
			while (ch != '\n')
			{
				TheFile.seekg(-2, std::ios_base::cur); //Two steps back, this means we will NOT check the last character
				if ((int)TheFile.tellg() <= 0)         //If passed the start of the file,
				{
					TheFile.seekg(0);                  //this is the start of the line
					break;
				}
				TheFile.get(ch);                       //Check the next character
			}
			auto FileStreamPos = TheFile.tellg(); // Save Current Stream Position
			std::string TheLine;
			std::getline(TheFile, TheLine);
			TheFile.seekg(FileStreamPos);	// Move Stream position to where it was before reading TheLine

			char buffer[256];
			if (TheLine.size() < sizeof(buffer))
			{
				TheLine.copy(buffer, TheLine.size());
				buffer[TheLine.size()] = '\0';
				std::string theDate(strtok(buffer, "\t"));
				std::string theTemp(strtok(NULL, "\t"));
				std::string theHumidity(strtok(NULL, "\t"));
				std::string theBattery(strtok(NULL, "\t"));
				Govee_Temp TheValue(ISO8601totime(theDate), atof(theTemp.c_str()), atof(theHumidity.c_str()), atoi(theBattery.c_str()));
				if ((Minutes == 0) && LogValues.empty()) // HACK: Special Case to always accept the last logged value
					LogValues.push(TheValue);
				if ((Minutes * 60.0) < difftime(now, TheValue.Time))	// If this entry is more than Minutes parameter from current time, it's time to stop reading log file.
					break;
				LogValues.push(TheValue);
			}
		} while (TheFile.tellg() > 0);	// If we are at the beginning of the file, there's nothing more to do
		TheFile.close();
		if (!LogValues.empty())
			OutValue = Govee_Temp();
		while (!LogValues.empty())
		{
			OutValue += LogValues.front();
			LogValues.pop();
			rval = true;	// I'm doing this multiple times, but it was easier than having an extra check
		}
	}
	return(rval);
}
void GetMRTGOutput(const std::string &TextAddress, const int Minutes)
{
	bdaddr_t TheAddress = { 0 };
	str2ba(TextAddress.c_str(), &TheAddress);
	Govee_Temp TheValue;
	if (GetLogEntry(TheAddress, Minutes, TheValue))
	{
		std::cout << std::dec; // make sure I'm putting things in decimal format
		std::cout << TheValue.GetHumidity() * 1000.0 << std::endl; // current state of the second variable, normally 'outgoing bytes count'
		std::cout << ((TheValue.GetTemperature() * 9.0 / 5.0) + 32.0) * 1000.0 << std::endl; // current state of the first variable, normally 'incoming bytes count'
		std::cout << " " << std::endl; // string (in any human readable format), uptime of the target.
		std::cout << TextAddress << std::endl; // string, name of the target.
	}
}
/////////////////////////////////////////////////////////////////////////////
std::map<bdaddr_t, std::vector<Govee_Temp>> GoveeMRTGLogs; // memory map of BT addresses and vector structure similar to MRTG Log Files
std::map<bdaddr_t, std::string> GoveeBluetoothTitles;
/////////////////////////////////////////////////////////////////////////////
std::filesystem::path GenerateCacheFileName(const bdaddr_t& a)
{
	std::string btAddress(ba2string(a));
	for (auto pos = btAddress.find(':'); pos != std::string::npos; pos = btAddress.find(':'))
		btAddress.erase(pos, 1);
	std::ostringstream OutputFilename;
	OutputFilename << "gvh-";
	OutputFilename << btAddress;
	OutputFilename << "-cache.txt";
	std::filesystem::path CacheFileName(CacheDirectory / OutputFilename.str());
	return(CacheFileName);
}
bool GenerateCacheFile(const bdaddr_t& a, const std::vector<Govee_Temp>& GoveeMRTGLog)
{
	bool rval(false);
	if (!GoveeMRTGLog.empty())
	{
		std::filesystem::path MRTGCacheFile(GenerateCacheFileName(a));
		struct stat64 Stat({ 0 });	// Zero the stat64 structure when it's allocated
		stat64(MRTGCacheFile.c_str(), &Stat);	// This shouldn't change Stat if the file doesn't exist.
		if (difftime(GoveeMRTGLog[0].Time, Stat.st_mtim.tv_sec) > 60 * 60) // If Cache File has data older than 60 minutes, write it
		{
			std::ofstream CacheFile(MRTGCacheFile, std::ios_base::out | std::ios_base::trunc);
			if (CacheFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing: " << MRTGCacheFile.string() << std::endl;
				else
					std::cerr << "Writing: " << MRTGCacheFile.string() << std::endl;
				CacheFile << "Cache: " << ba2string(a) << " " << ProgramVersionString << std::endl;
				for (auto i : GoveeMRTGLog)
					CacheFile << i.WriteCache() << std::endl;
				CacheFile.close();
				struct utimbuf ut;
				ut.actime = GoveeMRTGLog[0].Time;
				ut.modtime = GoveeMRTGLog[0].Time;
				utime(MRTGCacheFile.c_str(), &ut);
				rval = true;
			}
		}
	}
	return(rval);
}
void GenerateCacheFile(std::map<bdaddr_t, std::vector<Govee_Temp>> &AddressTemperatureMap)
{
	if (!CacheDirectory.empty())
	{
		if (ConsoleVerbosity > 1)
			std::cout << "[" << getTimeISO8601(true) << "] GenerateCacheFile: " << CacheDirectory << std::endl;
		for (auto it = AddressTemperatureMap.begin(); it != AddressTemperatureMap.end(); ++it)
			GenerateCacheFile(it->first, it->second);
	}
}
void ReadCacheDirectory(void)
{
	const std::regex CacheFileRegex("^gvh-[[:xdigit:]]{12}-cache.txt");
	if (!CacheDirectory.empty())
	{
		if (ConsoleVerbosity > 1)
			std::cout << "[" << getTimeISO8601(true) << "] ReadCacheDirectory: " << CacheDirectory << std::endl;
		std::deque<std::filesystem::path> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ CacheDirectory })
			if (dir_entry.is_regular_file())
				if (std::regex_match(dir_entry.path().filename().string(), CacheFileRegex))
					files.push_back(dir_entry);
		if (!files.empty())
		{
			sort(files.begin(), files.end());
			while (!files.empty())
			{
				std::ifstream TheFile(*files.begin());
				if (TheFile.is_open())
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601(true) << "] Reading: " << files.begin()->string() << std::endl;
					else
						std::cerr << "Reading: " << files.begin()->string() << std::endl;
					std::string TheLine;
					if (std::getline(TheFile, TheLine))
					{
						const std::regex CacheFirstLineRegex("^Cache: ((([[:xdigit:]]{2}:){5}))[[:xdigit:]]{2}.*");
						// every Cache File should have a start line with the name Cache, the Bluetooth Address, and the creator version. 
						// TODO: check to make sure the version is compatible
						if (std::regex_match(TheLine, CacheFirstLineRegex))
						{
							std::smatch BluetoothAddress;
							if (std::regex_search(TheLine, BluetoothAddress, BluetoothAddressRegex))
							{
								bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddress.str()));
								ThermometerType CacheThermometerType = ThermometerType::Unknown;
								auto foo = GoveeThermometers.find(TheBlueToothAddress);
								if (foo != GoveeThermometers.end())
									CacheThermometerType = foo->second;
								std::vector<Govee_Temp> FakeMRTGFile;
								FakeMRTGFile.reserve(2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT); // this might speed things up slightly
								while (std::getline(TheFile, TheLine))
								{
									Govee_Temp TheValue;
									TheValue.ReadCache(TheLine);
									if (TheValue.GetModel() == ThermometerType::Unknown)
										TheValue.SetModel(CacheThermometerType);
									FakeMRTGFile.push_back(TheValue);
								}
								if (FakeMRTGFile.size() == (2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT)) // simple check to see if we are the right size
									GoveeMRTGLogs.insert(std::pair<bdaddr_t, std::vector<Govee_Temp>>(TheBlueToothAddress, FakeMRTGFile));
							}
						}
					}
					TheFile.close();
				}
				files.pop_front();
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
enum class GraphType { daily, weekly, monthly, yearly};
// Returns a curated vector of data points specific to the requested graph type read directly from a real MRTG log file on disk.
void ReadMRTGData(const std::filesystem::path& MRTGLogFileName, std::vector<Govee_Temp>& TheValues, const GraphType graph = GraphType::daily)
{
	std::ifstream TheInFile(MRTGLogFileName);
	if (TheInFile.is_open())
	{
		TheValues.clear();
		std::string Line;
		while (std::getline(TheInFile, Line))
		{
			std::stringstream Source(Line);
			time_t tim;
			double tem;
			double hum;
			Source >> tim;
			Source >> hum;
			Source >> tem;
			hum /= 1000.0;
			tem /= 1000.0;
			Govee_Temp TheValue(tim, tem, hum, 0);
			TheValues.push_back(TheValue);
		}
		TheInFile.close();
		if (graph == GraphType::daily)
		{
			TheValues.resize(603); // get rid of anything beyond the five minute data
			TheValues.erase(TheValues.begin()); // get rid of the first element
		}
		else if (graph == GraphType::weekly)
		{
			TheValues.erase(TheValues.begin()); // get rid of the first element
			std::vector<Govee_Temp> TempValues;
			for (auto iter = TheValues.begin(); iter != TheValues.end(); iter++)
			{
				struct tm UTC;
				if (0 != gmtime_r(&iter->Time, &UTC))
				{
					if ((UTC.tm_min == 0) || (UTC.tm_min == 30))
						TempValues.push_back(*iter);
				}
			}
			TheValues.resize(TempValues.size());
			std::copy(TempValues.begin(), TempValues.end(), TheValues.begin());
		}
		else if (graph == GraphType::monthly)
		{
			TheValues.erase(TheValues.begin()); // get rid of the first element
			std::vector<Govee_Temp> TempValues;
			for (auto iter = TheValues.begin(); iter != TheValues.end(); iter++)
			{
				struct tm UTC;
				if (0 != gmtime_r(&iter->Time, &UTC))
				{
					if ((UTC.tm_hour % 2 == 0) && (UTC.tm_min == 0))
						TempValues.push_back(*iter);
				}
			}
			TheValues.resize(TempValues.size());
			std::copy(TempValues.begin(), TempValues.end(), TheValues.begin());
		}
		else if (graph == GraphType::yearly)
		{
			TheValues.erase(TheValues.begin()); // get rid of the first element
			std::vector<Govee_Temp> TempValues;
			for (auto iter = TheValues.begin(); iter != TheValues.end(); iter++)
			{
				struct tm UTC;
				if (0 != gmtime_r(&iter->Time, &UTC))
				{
					if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
						TempValues.push_back(*iter);
				}
			}
			TheValues.resize(TempValues.size());
			std::copy(TempValues.begin(), TempValues.end(), TheValues.begin());
		}
	}
}
// Returns a curated vector of data points specific to the requested graph type from the internal memory structure map keyed off the Bluetooth address.
void ReadMRTGData(const bdaddr_t& TheAddress, std::vector<Govee_Temp>& TheValues, const GraphType graph = GraphType::daily)
{
	auto it = GoveeMRTGLogs.find(TheAddress);
	if (it != GoveeMRTGLogs.end())
	{
		if (it->second.size() > 0)
		{
			auto DaySampleFirst = it->second.begin() + 2;
			auto DaySampleLast = it->second.begin() + 1 + DAY_COUNT;
			auto WeekSampleFirst = it->second.begin() + 2 + DAY_COUNT;
			auto WeekSampleLast = it->second.begin() + 1 + DAY_COUNT + WEEK_COUNT;
			auto MonthSampleFirst = it->second.begin() + 2 + DAY_COUNT + WEEK_COUNT;
			auto MonthSampleLast = it->second.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
			auto YearSampleFirst = it->second.begin() + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
			auto YearSampleLast = it->second.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT;
			if (graph == GraphType::daily)
			{
				TheValues.resize(DAY_COUNT);
				std::copy(DaySampleFirst, DaySampleLast, TheValues.begin());
				auto iter = TheValues.begin();
				while (iter->IsValid() && (iter != TheValues.end()))
					iter++;
				TheValues.resize(iter - TheValues.begin());
				TheValues.begin()->Time = it->second.begin()->Time; //HACK: include the most recent time sample
			}
			else if (graph == GraphType::weekly)
			{
				TheValues.resize(WEEK_COUNT);
				std::copy(WeekSampleFirst, WeekSampleLast, TheValues.begin());
				auto iter = TheValues.begin();
				while (iter->IsValid() && (iter != TheValues.end()))
					iter++;
				TheValues.resize(iter - TheValues.begin());
			}
			else if (graph == GraphType::monthly)
			{
				TheValues.resize(MONTH_COUNT);
				std::copy(MonthSampleFirst, MonthSampleLast, TheValues.begin());
				auto iter = TheValues.begin();
				while (iter->IsValid() && (iter != TheValues.end()))
					iter++;
				TheValues.resize(iter - TheValues.begin());
			}
			else if (graph == GraphType::yearly)
			{
				TheValues.resize(YEAR_COUNT);
				std::copy(YearSampleFirst, YearSampleLast, TheValues.begin());
				auto iter = TheValues.begin();
				while (iter->IsValid() && (iter != TheValues.end()))
					iter++;
				TheValues.resize(iter - TheValues.begin());
			}
		}
	}
}
// Interesting ideas about SVG and possible tools to look at: https://blog.usejournal.com/of-svg-minification-and-gzip-21cd26a5d007
// Tools Mentioned: svgo gzthermal https://github.com/subzey/svg-gz-supplement/
// Takes a curated vector of data points for a specific graph type and writes a SVG file to disk.
void WriteSVG(std::vector<Govee_Temp>& TheValues, const std::filesystem::path& SVGFileName, const std::string& Title = "", const GraphType graph = GraphType::daily, const bool Fahrenheit = true, const bool DrawBattery = false, const bool MinMax = false)
{
	if (!TheValues.empty())
	{
		// By declaring these items here, I'm then basing all my other dimensions on these
		const int SVGWidth(500);
		const int SVGHeight(135);
		const int FontSize(12);
		const int TickSize(2);
		int GraphWidth = SVGWidth - (FontSize * 5);
		const bool DrawHumidity = TheValues[0].GetHumidity() != 0; // HACK: I should really check the entire data set
		struct stat64 SVGStat({0});	// Zero the stat64 structure on allocation
		if (-1 == stat64(SVGFileName.c_str(), &SVGStat))
			if (ConsoleVerbosity > 3)
				std::cout << "[" << getTimeISO8601(true) << "] " << std::strerror(errno) << ": " << SVGFileName << std::endl;
		if (TheValues.begin()->Time > SVGStat.st_mtim.tv_sec)	// only write the file if we have new data
		{
			std::ofstream SVGFile(SVGFileName);
			if (SVGFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Temperature (" << std::fixed << std::setprecision(1) << TheValues[0].GetTemperature(Fahrenheit) << "\u00B0" << (Fahrenheit ? "F)" : "C)");
				std::string YLegendTemperature(tempOString.str());
				tempOString = std::ostringstream();
				tempOString << "Humidity (" << std::fixed << std::setprecision(1) << TheValues[0].GetHumidity() << "%)";
				std::string YLegendHumidity(tempOString.str());
				tempOString = std::ostringstream();
				tempOString << "Battery (" << TheValues[0].GetBattery() << "%)";
				std::string YLegendBattery(tempOString.str());
				int GraphTop = FontSize + TickSize;
				int GraphBottom = SVGHeight - GraphTop;
				int GraphRight = SVGWidth - GraphTop;
				if (DrawHumidity)
				{
					GraphWidth -= FontSize * 2;
					GraphRight -= FontSize + TickSize * 2;
				}
				if (DrawBattery)
					GraphWidth -= FontSize;
				int GraphLeft = GraphRight - GraphWidth;
				int GraphVerticalDivision = (GraphBottom - GraphTop) / 4;
				double TempMin = DBL_MAX;
				double TempMax = -DBL_MAX;
				double HumiMin = DBL_MAX;
				double HumiMax = -DBL_MAX;
				if (MinMax)
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetTemperatureMin(Fahrenheit));
						TempMax = std::max(TempMax, TheValues[index].GetTemperatureMax(Fahrenheit));
						HumiMin = std::min(HumiMin, TheValues[index].GetHumidityMin());
						HumiMax = std::max(HumiMax, TheValues[index].GetHumidityMax());
					}
				else
					for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					{
						TempMin = std::min(TempMin, TheValues[index].GetTemperature(Fahrenheit));
						TempMax = std::max(TempMax, TheValues[index].GetTemperature(Fahrenheit));
						HumiMin = std::min(HumiMin, TheValues[index].GetHumidity());
						HumiMax = std::max(HumiMax, TheValues[index].GetHumidity());
					}

				double TempVerticalDivision = (TempMax - TempMin) / 4;
				double TempVerticalFactor = (GraphBottom - GraphTop) / (TempMax - TempMin);
				double HumiVerticalDivision = (HumiMax - HumiMin) / 4;
				double HumiVerticalFactor = (GraphBottom - GraphTop) / (HumiMax - HumiMin);
				int FreezingLine = 0; // outside the range of the graph
				if (Fahrenheit)
				{
					if ((TempMin < 32) && (32 < TempMax))
						FreezingLine = ((TempMax - 32.0) * TempVerticalFactor) + GraphTop;
				}
				else
				{
					if ((TempMin < 0) && (0 < TempMax))
						FreezingLine = (TempMax * TempVerticalFactor) + GraphTop;
				}

				SVGFile << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>" << std::endl;
				SVGFile << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\">" << std::endl;
				SVGFile << "\t<!-- Created by: " << ProgramVersionString << " -->" << std::endl;
				SVGFile << "\t<clipPath id=\"GraphRegion\"><polygon points=\"" << GraphLeft << "," << GraphTop << " " << GraphRight << "," << GraphTop << " " << GraphRight << "," << GraphBottom << " " << GraphLeft << "," << GraphBottom << "\" /></clipPath>" << std::endl;
				SVGFile << "\t<style>" << std::endl;
				SVGFile << "\t\ttext { font-family: sans-serif; font-size: " << FontSize << "px; fill: dimgrey; }" << std::endl;
				SVGFile << "\t\tline { stroke: dimgrey; }" << std::endl;
				SVGFile << "\t\tpolygon { fill-opacity: 0.5; }" << std::endl;
				SVGFile << "\t</style>" << std::endl;
#ifdef DEBUG
				SVGFile << "<!-- HumiMax: " << HumiMax << " -->" << std::endl;
				SVGFile << "<!-- HumiMin: " << HumiMin << " -->" << std::endl;
				SVGFile << "<!-- HumiVerticalFactor: " << HumiVerticalFactor << " -->" << std::endl;
#endif // DEBUG
				SVGFile << "\t<rect style=\"fill-opacity:0;stroke:grey;stroke-width:2\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\" />" << std::endl;

				// Legend Text
				int LegendIndex = 1;
				SVGFile << "\t<text x=\"" << GraphLeft << "\" y=\"" << GraphTop - 2 << "\">" << Title << "</text>" << std::endl;
				SVGFile << "\t<text style=\"text-anchor:end\" x=\"" << GraphRight << "\" y=\"" << GraphTop - 2 << "\">" << timeToExcelLocal(TheValues[0].Time) << "</text>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"50%\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendTemperature << "</text>" << std::endl;
				if (DrawHumidity)
				{
					LegendIndex++;
					SVGFile << "\t<text style=\"fill:green;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"50%\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendHumidity << "</text>" << std::endl;
				}
				if (DrawBattery)
				{
					LegendIndex++;
					SVGFile << "\t<text style=\"fill:OrangeRed\" text-anchor=\"middle\" x=\"" << FontSize * LegendIndex << "\" y=\"50%\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendBattery << "</text>" << std::endl;
				}
				if (DrawHumidity)
				{
					if (MinMax)
					{
						SVGFile << "\t<!-- Humidity Max -->" << std::endl;
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green;clip-path:url(#GraphRegion)\" points=\"";
						SVGFile << GraphLeft + 1 << "," << GraphBottom - 1 << " ";
						for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
							SVGFile << index + GraphLeft << "," << int(((HumiMax - TheValues[index].GetHumidityMax()) * HumiVerticalFactor) + GraphTop) << " ";
						if (GraphWidth < TheValues.size())
							SVGFile << GraphRight - 1 << "," << GraphBottom - 1;
						else
							SVGFile << GraphRight - (GraphWidth - TheValues.size()) << "," << GraphBottom - 1;
						SVGFile << "\" />" << std::endl;
						SVGFile << "\t<!-- Humidity Min -->" << std::endl;
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green;clip-path:url(#GraphRegion)\" points=\"";
						SVGFile << GraphLeft + 1 << "," << GraphBottom - 1 << " ";
						for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
							SVGFile << index + GraphLeft << "," << int(((HumiMax - TheValues[index].GetHumidityMin()) * HumiVerticalFactor) + GraphTop) << " ";
						if (GraphWidth < TheValues.size())
							SVGFile << GraphRight - 1 << "," << GraphBottom - 1;
						else
							SVGFile << GraphRight - (GraphWidth - TheValues.size()) << "," << GraphBottom - 1;
						SVGFile << "\" />" << std::endl;
					}
					else
					{
						// Humidity Graphic as a Filled polygon
						SVGFile << "\t<!-- Humidity -->" << std::endl;
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green;clip-path:url(#GraphRegion)\" points=\"";
						SVGFile << GraphLeft + 1 << "," << GraphBottom - 1 << " ";
						for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
							SVGFile << index + GraphLeft << "," << int(((HumiMax - TheValues[index].GetHumidity()) * HumiVerticalFactor) + GraphTop) << " ";
						if (GraphWidth < TheValues.size())
							SVGFile << GraphRight - 1 << "," << GraphBottom - 1;
						else
							SVGFile << GraphRight - (GraphWidth - TheValues.size()) << "," << GraphBottom - 1;
						SVGFile << "\" />" << std::endl;
					}
				}

				// Top Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end;dominant-baseline:middle\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop << "\">" << std::fixed << std::setprecision(1) << TempMax << "</text>" << std::endl;
				if (DrawHumidity)
					SVGFile << "\t<text style=\"fill:green;dominant-baseline:middle\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop << "\">" << std::fixed << std::setprecision(1) << HumiMax << "</text>" << std::endl;

				// Bottom Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphBottom << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end;dominant-baseline:middle\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphBottom << "\">" << std::fixed << std::setprecision(1) << TempMin << "</text>" << std::endl;
				if (DrawHumidity)
					SVGFile << "\t<text style=\"fill:green;dominant-baseline:middle\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphBottom << "\">" << std::fixed << std::setprecision(1) << HumiMin << "</text>" << std::endl;

				// Left Line
				SVGFile << "\t<line x1=\"" << GraphLeft << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Right Line
				SVGFile << "\t<line x1=\"" << GraphRight << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Vertical Division Dashed Lines
				for (auto index = 1; index < 4; index++)
				{
					SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop + (GraphVerticalDivision * index) << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop + (GraphVerticalDivision * index) << "\" />" << std::endl;
					SVGFile << "\t<text style=\"fill:blue;text-anchor:end;dominant-baseline:middle\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << TempMax - (TempVerticalDivision * index) << "</text>" << std::endl;
					if (DrawHumidity)
						SVGFile << "\t<text style=\"fill:green;dominant-baseline:middle\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << HumiMax - (HumiVerticalDivision * index) << "</text>" << std::endl;
				}

				// Horizontal Line drawn at the freezing point
				if ((GraphTop < FreezingLine) && (FreezingLine < GraphBottom))
				{
					SVGFile << "\t<!-- FreezingLine = " << FreezingLine << " -->" << std::endl;
					SVGFile << "\t<line style=\"fill:red;stroke:red;stroke-dasharray:1\" x1=\"" << GraphLeft - TickSize << "\" y1=\"" << FreezingLine << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << FreezingLine << "\" />" << std::endl;
				}

				// Horizontal Division Dashed Lines
				for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
				{
					struct tm UTC;
					if (0 != localtime_r(&TheValues[index].Time, &UTC))
					{
						if (graph == GraphType::daily)
						{
							if (UTC.tm_min == 0)
							{
								if (UTC.tm_hour == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								if (UTC.tm_hour % 2 == 0)
									SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << UTC.tm_hour << "</text>" << std::endl;
							}
						}
						else if (graph == GraphType::weekly)
						{
							const std::string Weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
							if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
							{
								if (UTC.tm_wday == 0)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							}
							else if ((UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Weekday[UTC.tm_wday] << "</text>" << std::endl;
						}
						else if (graph == GraphType::monthly)
						{
							if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							if ((UTC.tm_wday == 0) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_wday == 3) && (UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">Week " << UTC.tm_yday / 7 + 1 << "</text>" << std::endl;
						}
						else if (graph == GraphType::yearly)
						{
							const std::string Month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
							if ((UTC.tm_yday == 0) && (UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 15) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<text style=\"text-anchor:middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Month[UTC.tm_mon] << "</text>" << std::endl;
						}
					}
				}

				// Directional Arrow
				SVGFile << "\t<polygon style=\"fill:red;stroke:red;fill-opacity:1;\" points=\"" << GraphLeft - 3 << "," << GraphBottom << " " << GraphLeft + 3 << "," << GraphBottom - 3 << " " << GraphLeft + 3 << "," << GraphBottom + 3 << "\" />" << std::endl;

				if (MinMax)
				{
					// Temperature Values as a filled polygon showing the minimum and maximum
					SVGFile << "\t<!-- Temperature MinMax -->" << std::endl;
					SVGFile << "\t<polygon style=\"fill:blue;stroke:blue;clip-path:url(#GraphRegion)\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetTemperatureMax(Fahrenheit)) * TempVerticalFactor) + GraphTop) << " ";
					for (auto index = (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()) - 1; index > 0; index--)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetTemperatureMin(Fahrenheit)) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}
				else
				{
					// Temperature Values as a continuous line
					SVGFile << "\t<!-- Temperature -->" << std::endl;
					SVGFile << "\t<polyline style=\"fill:none;stroke:blue;clip-path:url(#GraphRegion)\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetTemperature(Fahrenheit)) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}

				// Battery Values as a continuous line
				if (DrawBattery)
				{
					SVGFile << "\t<!-- Battery -->" << std::endl;
					double BatteryVerticalFactor = (GraphBottom - GraphTop) / 100.0;
					SVGFile << "\t<polyline style=\"fill:none;stroke:OrangeRed;clip-path:url(#GraphRegion)\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((100 - TheValues[index].GetBattery()) * BatteryVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}

				SVGFile << "</svg>" << std::endl;
				SVGFile.close();
				struct utimbuf SVGut;
				SVGut.actime = TheValues.begin()->Time;
				SVGut.modtime = TheValues.begin()->Time;
				utime(SVGFileName.c_str(), &SVGut);
			}
		}
	}
}
// Takes a Bluetooth address and current datapoint and updates the mapped structure in memory simulating the contents of a MRTG log file.
void UpdateMRTGData(const bdaddr_t& TheAddress, const Govee_Temp& TheValue)
{
	if (TheValue.IsValid())	// Sanity Check
	{
		std::vector<Govee_Temp> foo;
		auto ret = GoveeMRTGLogs.insert(std::pair<bdaddr_t, std::vector<Govee_Temp>>(TheAddress, foo));
		std::vector<Govee_Temp>& FakeMRTGFile = ret.first->second;
		if (FakeMRTGFile.empty())
		{
			FakeMRTGFile.resize(2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT);
			FakeMRTGFile[0] = TheValue;	// current value
			FakeMRTGFile[1] = TheValue;
			for (auto index = 0; index < DAY_COUNT; index++)
				FakeMRTGFile[index + 2].Time = FakeMRTGFile[index + 1].Time - DAY_SAMPLE;
			for (auto index = 0; index < WEEK_COUNT; index++)
				FakeMRTGFile[index + 2 + DAY_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT].Time - WEEK_SAMPLE;
			for (auto index = 0; index < MONTH_COUNT; index++)
				FakeMRTGFile[index + 2 + DAY_COUNT + WEEK_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT + WEEK_COUNT].Time - MONTH_SAMPLE;
			for (auto index = 0; index < YEAR_COUNT; index++)
				FakeMRTGFile[index + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time = FakeMRTGFile[index + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time - YEAR_SAMPLE;
		}
		else
		{
			if (TheValue.Time > FakeMRTGFile[0].Time)
			{
				FakeMRTGFile[0] = TheValue;	// current value
				FakeMRTGFile[1] += TheValue; // averaged value up to DAY_SAMPLE size
			}
		}
		bool ZeroAccumulator = false;
		auto DaySampleFirst = FakeMRTGFile.begin() + 2;
		auto DaySampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT;
		auto WeekSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT;
		auto WeekSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT;
		auto MonthSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT;
		auto MonthSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
		auto YearSampleFirst = FakeMRTGFile.begin() + 2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
		auto YearSampleLast = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT;
		// For every time difference between FakeMRTGFile[1] and FakeMRTGFile[2] that's greater than DAY_SAMPLE we shift that data towards the back.
		while (difftime(FakeMRTGFile[1].Time, DaySampleFirst->Time) > DAY_SAMPLE)
		{
			ZeroAccumulator = true;
			// shuffle all the day samples toward the end
			std::copy_backward(DaySampleFirst, DaySampleLast - 1, DaySampleLast);
			*DaySampleFirst = FakeMRTGFile[1];
			DaySampleFirst->NormalizeTime(Govee_Temp::granularity::day);
			if (difftime(DaySampleFirst->Time, (DaySampleFirst + 1)->Time) > DAY_SAMPLE)
				DaySampleFirst->Time = (DaySampleFirst + 1)->Time + DAY_SAMPLE;
			if (DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::year)
			{
				if (ConsoleVerbosity > 2)
					std::cout << "[" << getTimeISO8601(true) << "] shuffling year " << timeToExcelLocal(DaySampleFirst->Time) << " > " << timeToExcelLocal(YearSampleFirst->Time) << std::endl;
				// shuffle all the year samples toward the end
				std::copy_backward(YearSampleFirst, YearSampleLast - 1, YearSampleLast);
				*YearSampleFirst = Govee_Temp();
				for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 24))); iter++) // One Day of day samples
					*YearSampleFirst += *iter;
			}
			if ((DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::year) ||
				(DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::month))
			{
				if (ConsoleVerbosity > 2)
					std::cout << "[" << getTimeISO8601(true) << "] shuffling month " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
				// shuffle all the month samples toward the end
				std::copy_backward(MonthSampleFirst, MonthSampleLast - 1, MonthSampleLast);
				*MonthSampleFirst = Govee_Temp();
				for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 2))); iter++) // two hours of day samples
					*MonthSampleFirst += *iter;
			}
			if ((DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::year) ||
				(DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::month) ||
				(DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::week))
			{
				if (ConsoleVerbosity > 2)
					std::cout << "[" << getTimeISO8601(true) << "] shuffling week " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
				// shuffle all the month samples toward the end
				std::copy_backward(WeekSampleFirst, WeekSampleLast - 1, WeekSampleLast);
				*WeekSampleFirst = Govee_Temp();
				for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < 6)); iter++) // Half an hour of day samples
					*WeekSampleFirst += *iter;
			}
		}
		if (ZeroAccumulator)
		{
			FakeMRTGFile[1] = Govee_Temp();
		}
	}
}
void ReadLoggedData(const std::filesystem::path& filename)
{
	const std::regex ModifiedBluetoothAddressRegex("[[:xdigit:]]{12}");
	std::smatch BluetoothAddressInFilename;
	std::string Stem(filename.stem().string());
	if (std::regex_search(Stem, BluetoothAddressInFilename, ModifiedBluetoothAddressRegex))
	{
		bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddressInFilename.str()));

		// Only read the file if it's newer than what we may have cached
		bool bReadFile = true;
		struct stat64 FileStat;
		FileStat.st_mtim.tv_sec = 0;
		if (0 == stat64(filename.c_str(), &FileStat))	// returns 0 if the file-status information is obtained
		{
			auto it = GoveeMRTGLogs.find(TheBlueToothAddress);
			if (it != GoveeMRTGLogs.end())
				if (!it->second.empty())
					if (FileStat.st_mtim.tv_sec < (it->second.begin()->Time))	// only read the file if it more recent than existing data
						bReadFile = false;
		}

		if (bReadFile)
		{
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] Reading: " << filename.string() << std::endl;
			else
				std::cerr << "Reading: " << filename.string() << std::endl;
			std::ifstream TheFile(filename);
			if (TheFile.is_open())
			{
				ThermometerType CacheThermometerType = ThermometerType::Unknown;
				auto foo = GoveeThermometers.find(TheBlueToothAddress);
				if (foo != GoveeThermometers.end())
					CacheThermometerType = foo->second;

				std::vector<std::string> SortableFile;
				std::string RawLine;
				while (std::getline(TheFile, RawLine))
					SortableFile.push_back(RawLine);
				TheFile.close();
				sort(SortableFile.begin(), SortableFile.end());
				for (auto SortedLine = SortableFile.begin(); SortedLine != SortableFile.end(); SortedLine++)
				{
					Govee_Temp TheValue(*SortedLine);
					if (TheValue.GetModel() == ThermometerType::Unknown)
						TheValue.SetModel(CacheThermometerType);
					if (TheValue.IsValid())
						UpdateMRTGData(TheBlueToothAddress, TheValue);
				}
			}
		}
	}
}
// Finds log files specific to this program then reads the contents into the memory mapped structure simulating MRTG log files.
void ReadLoggedData(void)
{
	const std::regex LogFileRegex("gvh-[[:xdigit:]]{12}-[[:digit:]]{4}-[[:digit:]]{2}.txt");
	if (!LogDirectory.empty())
	{
		if (ConsoleVerbosity > 1)
			std::cout << "[" << getTimeISO8601(true) << "] ReadLoggedData: " << LogDirectory << std::endl;
		std::deque<std::filesystem::path> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ LogDirectory })
			if (dir_entry.is_regular_file())
				if (std::regex_match(dir_entry.path().filename().string(), LogFileRegex))
					files.push_back(dir_entry);
		if (!files.empty())
		{
			sort(files.begin(), files.end());
			while (!files.empty())
			{
				ReadLoggedData(*files.begin());
				files.pop_front();
			}
		}
	}
}
void MonitorLoggedData(const int SecondsRecent = 35*60)
{
	if (!LogDirectory.empty())
	{
		for (auto it = GoveeMRTGLogs.begin(); it != GoveeMRTGLogs.end(); it++)
		{
			std::filesystem::path filename(GenerateLogFileName(it->first));
			struct stat64 FileStat;
			FileStat.st_mtim.tv_sec = 0;
			if (0 == stat64(filename.c_str(), &FileStat))	// returns 0 if the file-status information is obtained
				if (!it->second.empty())
					if (FileStat.st_mtim.tv_sec > (it->second.begin()->Time + (SecondsRecent)))	// only read the file if it's at least thirty five minutes more recent than existing data
						ReadLoggedData(filename);
		}
	}
}
bool ReadTitleMap(const std::filesystem::path& TitleMapFilename)
{
	bool rval = false;
	static time_t LastModified = 0;
	struct stat64 TitleMapFileStat;
	TitleMapFileStat.st_mtim.tv_sec = 0;
	if (0 == stat64(TitleMapFilename.c_str(), &TitleMapFileStat))
	{
		rval = true;
		if (TitleMapFileStat.st_mtim.tv_sec > LastModified)	// only read the file if it's modified
		{
			std::ifstream TheFile(TitleMapFilename);
			if (TheFile.is_open())
			{
				LastModified = TitleMapFileStat.st_mtim.tv_sec;	// only update our time if the file is actually read
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Reading: " << TitleMapFilename.string() << std::endl;
				else
					std::cerr << "Reading: " << TitleMapFilename.string() << std::endl;
				std::string TheLine;

				while (std::getline(TheFile, TheLine))
				{
					std::smatch BluetoothAddress;
					if (std::regex_search(TheLine, BluetoothAddress, BluetoothAddressRegex))
					{
						bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddress.str()));
						const std::string delimiters(" \t");
						auto i = TheLine.find_first_of(delimiters);		// Find first delimiter
						i = TheLine.find_first_not_of(delimiters, i);	// Move past consecutive delimiters
						std::string theTitle = (i == std::string::npos) ? "" : TheLine.substr(i);
						GoveeBluetoothTitles.insert(std::make_pair(TheBlueToothAddress, theTitle));
					}
				}
				TheFile.close();
			}
		}
	}
	return(rval);
}
void WriteAllSVG()
{
	ReadTitleMap(SVGTitleMapFilename);
	for (auto it = GoveeMRTGLogs.begin(); it != GoveeMRTGLogs.end(); it++)
	{
		const bdaddr_t TheAddress = it->first;
		std::string btAddress(ba2string(TheAddress));
		for (auto pos = btAddress.find(':'); pos != std::string::npos; pos = btAddress.find(':'))
			btAddress.erase(pos, 1);
		ThermometerType CacheThermometerType = ThermometerType::Unknown;
		auto foo = GoveeThermometers.find(TheAddress);
		if (foo != GoveeThermometers.end())
			CacheThermometerType = foo->second;
		std::string ssTitle(btAddress + " " + ThermometerType2String(CacheThermometerType)); // default title
		if (GoveeBluetoothTitles.find(TheAddress) != GoveeBluetoothTitles.end())
			ssTitle = GoveeBluetoothTitles.find(TheAddress)->second;
		std::filesystem::path OutputPath;
		std::ostringstream OutputFilename;
		OutputFilename.str("");
		OutputFilename << "gvh-";
		OutputFilename << btAddress;
		OutputFilename << "-day.svg";
		OutputPath = SVGDirectory / OutputFilename.str();
		std::vector<Govee_Temp> TheValues;
		ReadMRTGData(TheAddress, TheValues, GraphType::daily);
		WriteSVG(TheValues, OutputPath, ssTitle, GraphType::daily, SVGFahrenheit, SVGBattery & 0x01, SVGMinMax & 0x01);
		OutputFilename.str("");
		OutputFilename << "gvh-";
		OutputFilename << btAddress;
		OutputFilename << "-week.svg";
		OutputPath = SVGDirectory / OutputFilename.str();
		ReadMRTGData(TheAddress, TheValues, GraphType::weekly);
		WriteSVG(TheValues, OutputPath, ssTitle, GraphType::weekly, SVGFahrenheit, SVGBattery & 0x02, SVGMinMax & 0x02);
		OutputFilename.str("");
		OutputFilename << "gvh-";
		OutputFilename << btAddress;
		OutputFilename << "-month.svg";
		OutputPath = SVGDirectory / OutputFilename.str();
		ReadMRTGData(TheAddress, TheValues, GraphType::monthly);
		WriteSVG(TheValues, OutputPath, ssTitle, GraphType::monthly, SVGFahrenheit, SVGBattery & 0x04, SVGMinMax & 0x04);
		OutputFilename.str("");
		OutputFilename << "gvh-";
		OutputFilename << btAddress;
		OutputFilename << "-year.svg";
		OutputPath = SVGDirectory / OutputFilename.str();
		ReadMRTGData(TheAddress, TheValues, GraphType::yearly);
		WriteSVG(TheValues, OutputPath, ssTitle, GraphType::yearly, SVGFahrenheit, SVGBattery & 0x08, SVGMinMax & 0x08);
	}
}
void WriteSVGIndex(const std::filesystem::path LogDirectory, const std::filesystem::path SVGIndexFilename)
{
	const std::regex LogFileRegex("gvh-[[:xdigit:]]{12}-[[:digit:]]{4}-[[:digit:]]{2}.txt");
	if (!LogDirectory.empty())
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] Reading: " << LogDirectory << std::endl;
		std::set<std::string> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ LogDirectory })
			if (dir_entry.is_regular_file())
				if (std::regex_match(dir_entry.path().filename().string(), LogFileRegex))
					{
						const std::regex ModifiedBluetoothAddressRegex("[[:xdigit:]]{12}");
						std::smatch BluetoothAddressInFilename;
						std::string Stem(dir_entry.path().stem().string());
						if (std::regex_search(Stem, BluetoothAddressInFilename, ModifiedBluetoothAddressRegex))
							files.insert(BluetoothAddressInFilename.str());
					}
		if (!files.empty())
		{
			std::ofstream SVGIndexFile(SVGIndexFilename);
			if (SVGIndexFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing: " << SVGIndexFilename << std::endl;
				SVGIndexFile << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">" << std::endl;
				SVGIndexFile << "<html>" << std::endl;
				SVGIndexFile << "<head>" << std::endl;
				SVGIndexFile << "\t<title>" << ProgramVersionString << "</title>" << std::endl;
				SVGIndexFile << "\t<meta http-equiv=\"content-type\" content=\"text/html; charset=iso-8859-15\">" << std::endl;
				SVGIndexFile << "\t<meta http-equiv=\"Refresh\" CONTENT=\"300\">" << std::endl;
				SVGIndexFile << "\t<meta http-equiv=\"Cache-Control\" content=\"no-cache\">" << std::endl;
				SVGIndexFile << "\t<meta http-equiv=\"Pragma\" CONTENT=\"no-cache\">" << std::endl;
				SVGIndexFile << "\t<style type=\"text/css\">" << std::endl;
				SVGIndexFile << "\t\tbody { color: black; }" << std::endl;
				SVGIndexFile << "\t\t.image { float: left; position: relative; zoom: 85%; }" << std::endl;
				SVGIndexFile << "\t\t@media only screen and (max-width: 980px) {" << std::endl;
				SVGIndexFile << "\t\t\t.image { float: left; position: relative; zoom: 190%; }" << std::endl;
				SVGIndexFile << "\t\t}" << std::endl;
				SVGIndexFile << "\t</style>" << std::endl;
				SVGIndexFile << "</head>" << std::endl;
				SVGIndexFile << "<body>" << std::endl;
				SVGIndexFile << "\t<div>" << std::endl;
				SVGIndexFile << std::endl;

				SVGIndexFile << "\t<div>" << std::endl;
				for (auto & ssBTAddress : files)
					SVGIndexFile << "\t<a href=\"#" << ssBTAddress << "\">" << ssBTAddress << "</a>" << std::endl;
				SVGIndexFile << "\t</div>" << std::endl;
				SVGIndexFile << std::endl;

				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Writing:";
				for (auto & ssBTAddress : files)
				{
					SVGIndexFile << "\t<div id=\"" << ssBTAddress << "\">" << std::endl; 
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"" << ssBTAddress << "\" src=\"gvh-" << ssBTAddress << "-day.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"" << ssBTAddress << "\" src=\"gvh-" << ssBTAddress << "-week.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"" << ssBTAddress << "\" src=\"gvh-" << ssBTAddress << "-month.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"" << ssBTAddress << "\" src=\"gvh-" << ssBTAddress << "-year.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t</div>" << std::endl;
					SVGIndexFile << std::endl;
					if (ConsoleVerbosity > 0)
						std::cout << " " << ssBTAddress;
				}
				if (ConsoleVerbosity > 0)
					std::cout << std::endl;
				SVGIndexFile << "\t</div>" << std::endl;
				SVGIndexFile << "</body>" << std::endl;
				SVGIndexFile << "</html>" << std::endl;
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Done" << std::endl;
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
// 2022-12-26 I finally found an example of using BlueTooth Low Energy (BTLE) GATT via the older sockets interface https://github.com/dlenski/ttblue
// I'll be heavily borrowing code from ttblue to see if I can't download historical data from the Govee Thermometers
// https://software-dl.ti.com/lprf/simplelink_cc2640r2_latest/docs/blestack/ble_user_guide/html/ble-stack-3.x/hci.html
// http://gbrault.github.io/gattclient/hci_8c.html#a8310688dd135cec47d8144c5b90da4fb
// https://novelbits.io/bluetooth-le-att-gatt-explained-connection-oriented-communication/
// https://epxx.co/artigos/bluetooth_gatt.html
// https://novelbits.io/design-bluetooth-low-energy-gatt-server-database/
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
class BlueToothServiceCharacteristic {public: uint16_t starting_handle; uint8_t properties; uint16_t ending_handle; bt_uuid_t theUUID; };
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
uint8_t bt_ScanType(0x01);		// Scan Type: Active (0x01)
// In passive scanning, the BLE module just listens to other node advertisements.
// in active scanning the module will request more information once an advertisement is received, and the advertiser will answer with information like friendly name and supported profiles.
int bt_LEScan(int BlueToothDevice_Handle, const bool enable, const std::set<bdaddr_t>& BT_WhiteList)
{
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
		bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList); // call this routine recursively to disable any existing scanning
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
				for (auto & iter : GoveeMRTGLogs)
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
				for (auto & iter : BT_WhiteList)
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
												struct __attribute__((__packed__)) bt_error { uint8_t opcode; uint8_t req_opcode; uint16_t handle; uint8_t errcode; } * result = (bt_error*) & (buf[0]);
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
						for (auto index = 0; index < sizeof(MyRequest.buf) / sizeof(MyRequest.buf[0])-1; index++)
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
												Govee_Temp localTemp(TimeDownloadStart-(60*offset--), Temperature, Humidity, BatteryToRecord);
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
time_t ConnectAndDownload(DBusConnection* dbusConnection, const bdaddr_t GoveeBTAddress, const time_t GoveeLastReadTime = 0, const int BatteryToRecord = 0)
{
	time_t TimeDownloadStart(0);
	return(TimeDownloadStart);
}
/////////////////////////////////////////////////////////////////////////////
const char * dbus_message_iter_type_to_string(const int type)
{
	switch (type)
	{
	case DBUS_TYPE_INVALID:
		return "Invalid";
	case DBUS_TYPE_VARIANT:
		return "Variant";
	case DBUS_TYPE_ARRAY:
		return "Array";
	case DBUS_TYPE_BYTE:
		return "Byte";
	case DBUS_TYPE_BOOLEAN:
		return "Boolean";
	case DBUS_TYPE_INT16:
		return "Int16";
	case DBUS_TYPE_UINT16:
		return "UInt16";
	case DBUS_TYPE_INT32:
		return "Int32";
	case DBUS_TYPE_UINT32:
		return "UInt32";
	case DBUS_TYPE_INT64:
		return "Int64";
	case DBUS_TYPE_UINT64:
		return "UInt64";
	case DBUS_TYPE_DOUBLE:
		return "Double";
	case DBUS_TYPE_STRING:
		return "String";
	case DBUS_TYPE_OBJECT_PATH:
		return "ObjectPath";
	case DBUS_TYPE_SIGNATURE:
		return "Signature";
	case DBUS_TYPE_STRUCT:
		return "Struct";
	case DBUS_TYPE_DICT_ENTRY:
		return "DictEntry";
	default:
		return "Unknown Type";
	}
}
bool bluez_find_adapters(DBusConnection* dbus_conn, std::map<bdaddr_t, std::string>& AdapterMap)
{
	std::ostringstream ssOutput;
	// Initialize D-Bus error
	DBusError dbus_error;
	dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe
	DBusMessage* dbus_msg = dbus_message_new_method_call("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (!dbus_msg)
	{
		if (ConsoleVerbosity > 0)
			ssOutput << "[                   ] ";
		ssOutput << "Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	}
	else
	{
		dbus_error_init(&dbus_error);
		DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &dbus_error);
		dbus_message_unref(dbus_msg);
		if (!dbus_reply)
		{
			if (ConsoleVerbosity > 0)
				ssOutput << "[                   ] ";
			ssOutput << "Can't get bluez managed objects" << std::endl;
			if (dbus_error_is_set(&dbus_error))
			{
				if (ConsoleVerbosity > 0)
					ssOutput << "[                   ] ";
				ssOutput << dbus_error.message << std::endl;
				dbus_error_free(&dbus_error);
			}
		}
		else
		{
			if (dbus_message_get_type(dbus_reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
			{
				const std::string dbus_reply_Signature(dbus_message_get_signature(dbus_reply));
				int indent(16);
				if (ConsoleVerbosity > 1)
				{
					ssOutput << "[                   ] " << std::right << std::setw(indent) << "Message Type: " << std::string(dbus_message_type_to_string(dbus_message_get_type(dbus_reply))) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaed63e4c2baaa50d782e8ebb7643def19
					ssOutput << "[                   ] " << std::right << std::setw(indent) << "Signature: " << dbus_reply_Signature << std::endl;
					ssOutput << "[                   ] " << std::right << std::setw(indent) << "Destination: " << std::string(dbus_message_get_destination(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaed63e4c2baaa50d782e8ebb7643def19
					ssOutput << "[                   ] " << std::right << std::setw(indent) << "Sender: " << std::string(dbus_message_get_sender(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaed63e4c2baaa50d782e8ebb7643def19
					//if (NULL != dbus_message_get_path(dbus_reply)) std::cout << std::right << std::setw(indent) << "Path: " << std::string(dbus_message_get_path(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga18adf731bb42d324fe2624407319e4af
					//if (NULL != dbus_message_get_interface(dbus_reply)) std::cout << std::right << std::setw(indent) << "Interface: " << std::string(dbus_message_get_interface(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga1ad192bd4538cae556121a71b4e09d42
					//if (NULL != dbus_message_get_member(dbus_reply)) std::cout << std::right << std::setw(indent) << "Member: " << std::string(dbus_message_get_member(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaf5c6b705c53db07a5ae2c6b76f230cf9
					//if (NULL != dbus_message_get_container_instance(dbus_reply)) std::cout << std::right << std::setw(indent) << "Container Instance: " << std::string(dbus_message_get_container_instance(dbus_reply)) << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaed63e4c2baaa50d782e8ebb7643def19
				}
				if (!dbus_reply_Signature.compare("a{oa{sa{sv}}}"))
				{
					DBusMessageIter root_iter;
					dbus_message_iter_init(dbus_reply, &root_iter);
					do {
						DBusMessageIter array1_iter;
						dbus_message_iter_recurse(&root_iter, &array1_iter);
						do {
							indent += 4;
							DBusMessageIter dict1_iter;
							dbus_message_iter_recurse(&array1_iter, &dict1_iter);
							DBusBasicValue value;
							dbus_message_iter_get_basic(&dict1_iter, &value);
							std::string dict1_object_path(value.str);
							if (ConsoleVerbosity > 1)
								ssOutput << "[                   ] " << std::right << std::setw(indent) << "Object Path: " << dict1_object_path << std::endl;
							dbus_message_iter_next(&dict1_iter);
							DBusMessageIter array2_iter;
							dbus_message_iter_recurse(&dict1_iter, &array2_iter);
							do
							{
								DBusMessageIter dict2_iter;
								dbus_message_iter_recurse(&array2_iter, &dict2_iter);
								dbus_message_iter_get_basic(&dict2_iter, &value);
								std::string dict2_string(value.str);
								if (ConsoleVerbosity > 1)
									ssOutput << "[                   ] " << std::right << std::setw(indent) << "String: " << dict2_string << std::endl;
								if (!dict2_string.compare("org.bluez.Adapter1"))
								{
									indent += 4;
									dbus_message_iter_next(&dict2_iter);
									DBusMessageIter array3_iter;
									dbus_message_iter_recurse(&dict2_iter, &array3_iter);
									do {
										DBusMessageIter dict3_iter;
										dbus_message_iter_recurse(&array3_iter, &dict3_iter);
										dbus_message_iter_get_basic(&dict3_iter, &value);
										std::string dict3_string(value.str);
										if (!dict3_string.compare("Address"))
										{
											dbus_message_iter_next(&dict3_iter);
											if (DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&dict3_iter))
											{
												// recurse into variant to get string
												DBusMessageIter variant_iter;
												dbus_message_iter_recurse(&dict3_iter, &variant_iter);
												if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&variant_iter))
												{
													dbus_message_iter_get_basic(&variant_iter, &value);
													std::string Address(value.str);
													if (ConsoleVerbosity > 1)
														ssOutput << "[                   ] " << std::right << std::setw(indent) << "Address: " << Address << std::endl;
													bdaddr_t localBTAddress({ 0 });
													str2ba(Address.c_str(), &localBTAddress);
													AdapterMap.insert(std::pair<bdaddr_t, std::string>(localBTAddress, dict1_object_path));
												}
											}
										}
									} while (dbus_message_iter_next(&array3_iter));
									indent -= 4;
								}
							} while (dbus_message_iter_next(&array2_iter));
							indent -= 4;
						} while (dbus_message_iter_next(&array1_iter));
					} while (dbus_message_iter_next(&root_iter));
				}
			}
			dbus_message_unref(dbus_reply);
		}
	}
	for (const auto& [key, value] : AdapterMap)
	{
		if (ConsoleVerbosity > 0)
			ssOutput << "[                   ] ";
		ssOutput << "Host Controller Address: " << ba2string(key) << " BlueZ Adapter Path: " << value << std::endl;
	}
	if (ConsoleVerbosity > 0)
		std::cout << ssOutput.str();
	else
		std::cerr << ssOutput.str();
	return(!AdapterMap.empty());
}
void bluez_power_on(DBusConnection* dbus_conn, const char* adapter_path, const bool PowerOn = true)
{

	// This was hacked from looking at https://git.kernel.org/pub/scm/network/connman/connman.git/tree/gdbus/client.c#n667
	// https://www.mankier.com/5/org.bluez.Adapter#Interface-boolean_Powered_%5Breadwrite%5D
	DBusMessage* dbus_msg = dbus_message_new_method_call("org.bluez", adapter_path, "org.freedesktop.DBus.Properties", "Set"); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga98ddc82450d818138ef326a284201ee0
	if (!dbus_msg)
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[                   ] Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
		else
			std::cerr << "Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	}
	else
	{
		DBusMessageIter iterParameter;
		dbus_message_iter_init_append(dbus_msg, &iterParameter); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaf733047c467ce21f4a53b65a388f1e9d
		const char* adapter = "org.bluez.Adapter1";
		dbus_message_iter_append_basic(&iterParameter, DBUS_TYPE_STRING, &adapter); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga17491f3b75b3203f6fc47dcc2e3b221b
		const char* powered = "Powered";
		dbus_message_iter_append_basic(&iterParameter, DBUS_TYPE_STRING, &powered);
		DBusMessageIter variant;
		dbus_message_iter_open_container(&iterParameter, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &variant); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga943150f4e87fd8507da224d22c266100
		dbus_bool_t cpTrue = PowerOn ? TRUE : FALSE;
		dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &cpTrue);
		dbus_message_iter_close_container(&iterParameter, &variant); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaf00482f63d4af88b7851621d1f24087a
		dbus_connection_send(dbus_conn, dbus_msg, NULL); // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#gae1cb64f4cf550949b23fd3a756b2f7d0
		if (ConsoleVerbosity > 0)
			std::cout << "[                   ] " << dbus_message_get_path(dbus_msg) << ": " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg) << powered << ": " << std::boolalpha << PowerOn << std::endl;
		else
			std::cerr << dbus_message_get_path(dbus_msg) << ": " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg) << powered << ": " << std::boolalpha << PowerOn << std::endl;
		dbus_message_unref(dbus_msg); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gab69441efe683918f6a82469c8763f464
	}
}
void bluez_filter_le(DBusConnection* dbus_conn, const char* adapter_path, const bool DuplicateData = true, const bool bFilter = true)
{
	std::ostringstream ssOutput;
	// https://www.mankier.com/5/org.bluez.Adapter#Interface-void_SetDiscoveryFilter(dict_filter)
	DBusMessage* dbus_msg = dbus_message_new_method_call("org.bluez", adapter_path, "org.bluez.Adapter1", "SetDiscoveryFilter");
	if (!dbus_msg)
	{
		if (ConsoleVerbosity > 0)
			ssOutput << "[                   ] ";
		ssOutput << "Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	}
	else
	{
		if (bFilter)
		{
			DBusMessageIter iterParameter;
			dbus_message_iter_init_append(dbus_msg, &iterParameter);
			DBusMessageIter iterArray;
			dbus_message_iter_open_container(&iterParameter, DBUS_TYPE_ARRAY, "{sv}", &iterArray);
			DBusMessageIter iterDict;
			dbus_message_iter_open_container(&iterArray, DBUS_TYPE_DICT_ENTRY, NULL, &iterDict);
			const char* cpTransport = "Transport";
			dbus_message_iter_append_basic(&iterDict, DBUS_TYPE_STRING, &cpTransport);
			DBusMessageIter iterVariant;
			dbus_message_iter_open_container(&iterDict, DBUS_TYPE_VARIANT, DBUS_TYPE_STRING_AS_STRING, &iterVariant);
			const char* cpBTLE = "le";
			dbus_message_iter_append_basic(&iterVariant, DBUS_TYPE_STRING, &cpBTLE);
			dbus_message_iter_close_container(&iterDict, &iterVariant);
			dbus_message_iter_close_container(&iterArray, &iterDict);
			dbus_message_iter_open_container(&iterArray, DBUS_TYPE_DICT_ENTRY, NULL, &iterDict);
			const char* cpDuplicateData = "DuplicateData";
			dbus_message_iter_append_basic(&iterDict, DBUS_TYPE_STRING, &cpDuplicateData);
			dbus_message_iter_open_container(&iterDict, DBUS_TYPE_VARIANT, DBUS_TYPE_BOOLEAN_AS_STRING, &iterVariant);
			dbus_bool_t cpTrue = DuplicateData ? TRUE : FALSE;
			dbus_message_iter_append_basic(&iterVariant, DBUS_TYPE_BOOLEAN, &cpTrue);
			dbus_message_iter_close_container(&iterDict, &iterVariant);
			dbus_message_iter_close_container(&iterArray, &iterDict);
			dbus_message_iter_close_container(&iterParameter, &iterArray);
		}
		else
		{
			DBusMessageIter iterParameter;
			dbus_message_iter_init_append(dbus_msg, &iterParameter);
			DBusMessageIter iterArray;
			dbus_message_iter_open_container(&iterParameter, DBUS_TYPE_ARRAY, "{}", &iterArray);
			DBusMessageIter iterDict;
			dbus_message_iter_open_container(&iterArray, DBUS_TYPE_DICT_ENTRY, NULL, &iterDict);
			dbus_message_iter_close_container(&iterArray, &iterDict);
			dbus_message_iter_close_container(&iterParameter, &iterArray);
		}
		// Initialize D-Bus error
		DBusError dbus_error;
		dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe
		DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_INFINITE, &dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga8d6431f17a9e53c9446d87c2ba8409f0
		if (ConsoleVerbosity > 0)
			ssOutput << "[                   ] ";
		ssOutput << dbus_message_get_path(dbus_msg) << ": " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg) << std::endl;
		if (!dbus_reply)
		{
			if (ConsoleVerbosity > 0)
				ssOutput << "[                   ] ";
			ssOutput << "Error: " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg);
			if (dbus_error_is_set(&dbus_error))
			{
				ssOutput << ": " << dbus_error.message;
				dbus_error_free(&dbus_error);
			}
			ssOutput << " " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
		}
		else
			dbus_message_unref(dbus_reply);
		dbus_message_unref(dbus_msg);
	}
	if (ConsoleVerbosity > 0)
		std::cout << ssOutput.str();
	else
		std::cerr << ssOutput.str();
}
bool bluez_discovery(DBusConnection* dbus_conn, const char* adapter_path, const bool bStartDiscovery = true)
{
	bool bStarted = false;
	// https://git.kernel.org/pub/scm/bluetooth/bluez.git/tree/doc/adapter-api.txt
	DBusMessage* dbus_msg = dbus_message_new_method_call("org.bluez", adapter_path, "org.bluez.Adapter1", bStartDiscovery ? "StartDiscovery" : "StopDiscovery");
	if (!dbus_msg)
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[                   ] Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
		else
			std::cerr << "Can't allocate dbus_message_new_method_call: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	}
	else
	{
		DBusError dbus_error;
		dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe
		DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_INFINITE, &dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga8d6431f17a9e53c9446d87c2ba8409f0
		if (ConsoleVerbosity > 0)
			std::cout << "[                   ] " << dbus_message_get_path(dbus_msg) << ": " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg) << std::endl;
		else
			std::cerr << dbus_message_get_path(dbus_msg) << ": " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg) << std::endl;
		if (!dbus_reply)
		{
			std::cout << __FILE__ << "(" << __LINE__ << "): Error: " << dbus_message_get_interface(dbus_msg) << ": " << dbus_message_get_member(dbus_msg);
			if (dbus_error_is_set(&dbus_error))
			{
				std::cout << ": " << dbus_error.message;
				dbus_error_free(&dbus_error);
			}
			std::cout << std::endl;
		}
		else
		{
			bStarted = true;
			dbus_message_unref(dbus_reply);
		}
		dbus_message_unref(dbus_msg);
	}
	return(bStarted);
}
/////////////////////////////////////////////////////////////////////////////
std::string bluez_dbus_msg_iter(DBusMessageIter& array_iter, const bdaddr_t& dbusBTAddress, Govee_Temp& dbusTemp)
{
	std::ostringstream ssOutput;
	do
	{
		DBusMessageIter dict2_iter;
		dbus_message_iter_recurse(&array_iter, &dict2_iter);
		DBusBasicValue value;
		dbus_message_iter_get_basic(&dict2_iter, &value);
		std::string Key(value.str);
		dbus_message_iter_next(&dict2_iter);
		DBusMessageIter variant_iter;
		dbus_message_iter_recurse(&dict2_iter, &variant_iter);
		do
		{
			auto dbus_message_Type = dbus_message_iter_get_arg_type(&variant_iter);
			if (!Key.compare("Name"))
			{
				if ((DBUS_TYPE_STRING == dbus_message_Type) || (DBUS_TYPE_OBJECT_PATH == dbus_message_Type))
				{
					dbus_message_iter_get_basic(&variant_iter, &value);
					ssOutput << "[" << getTimeISO8601(true) << "] [" << ba2string(dbusBTAddress) << "] " << Key << ": " << value.str << std::endl;
					dbusTemp.SetModel(std::string(value.str));
					if (dbusTemp.GetModel() != ThermometerType::Unknown)
						GoveeThermometers.insert(std::pair<bdaddr_t, ThermometerType>(dbusBTAddress, dbusTemp.GetModel()));
				}
			}
			else if (!Key.compare("UUIDs"))
			{
				DBusMessageIter array3_iter;
				dbus_message_iter_recurse(&variant_iter, &array3_iter);
				do
				{
					if (DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&array3_iter))
					{
						dbus_message_iter_get_basic(&array3_iter, &value);
						ssOutput << "[                   ] [" << ba2string(dbusBTAddress) << "] " << Key << ": " << value.str << std::endl;
						dbusTemp.SetModel(std::string(value.str));
						if (dbusTemp.GetModel() != ThermometerType::Unknown)
							GoveeThermometers.insert(std::pair<bdaddr_t, ThermometerType>(dbusBTAddress, dbusTemp.GetModel()));
					}
				} while (dbus_message_iter_next(&array3_iter));
			}
			else if (!Key.compare("ManufacturerData"))
			{
				if (DBUS_TYPE_ARRAY == dbus_message_Type)
				{
					DBusMessageIter array3_iter;
					dbus_message_iter_recurse(&variant_iter, &array3_iter);
					do
					{
						if (DBUS_TYPE_DICT_ENTRY == dbus_message_iter_get_arg_type(&array3_iter))
						{
							DBusMessageIter dict1_iter;
							dbus_message_iter_recurse(&array3_iter, &dict1_iter);
							if (DBUS_TYPE_UINT16 == dbus_message_iter_get_arg_type(&dict1_iter))
							{
								DBusBasicValue value;
								dbus_message_iter_get_basic(&dict1_iter, &value);
								uint16_t ManufacturerID(value.u16);
								if (ConsoleVerbosity > 5)
								{
									// Total Hack 
									uint16_t BTManufacturer(uint16_t(dbusBTAddress.b[1]) << 8 | uint16_t(dbusBTAddress.b[2]));
									if (BTManufacturer == ManufacturerID)
										ssOutput << "[                   ] [" << ba2string(dbusBTAddress) << "] *** Meat Thermometer ***" << std::endl;
								}
								dbus_message_iter_next(&dict1_iter);
								if (DBUS_TYPE_VARIANT == dbus_message_iter_get_arg_type(&dict1_iter))
								{
									DBusMessageIter variant2_iter;
									dbus_message_iter_recurse(&dict1_iter, &variant2_iter);
									if (DBUS_TYPE_ARRAY == dbus_message_iter_get_arg_type(&variant2_iter))
									{
										std::vector<uint8_t> ManufacturerData;
										DBusMessageIter array4_iter;
										dbus_message_iter_recurse(&variant2_iter, &array4_iter);
										do
										{
											if (DBUS_TYPE_BYTE == dbus_message_iter_get_arg_type(&array4_iter))
											{
												dbus_message_iter_get_basic(&array4_iter, &value);
												ManufacturerData.push_back(value.byt);
											}
										} while (dbus_message_iter_next(&array4_iter));
										ssOutput << "[                   ] [" << ba2string(dbusBTAddress) << "] " << Key << ": " << std::setfill('0') << std::hex << std::setw(4) << ManufacturerID << ":";
										for (auto& Data : ManufacturerData)
											ssOutput << std::setw(2) << int(Data);
										if (ConsoleVerbosity > 4)
										{
											// https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/company_identifiers/company_identifiers.yaml
											ssOutput << " ";
											if (0x0001 == ManufacturerID)
												ssOutput << "'Nokia Mobile Phones'";
											if (0x0006 == ManufacturerID)
												ssOutput << "'Microsoft'";
											if (0x004c == ManufacturerID)
												ssOutput << "'Apple, Inc.'";
											if (0x058e == ManufacturerID)
												ssOutput << "'Meta Platforms Technologies, LLC'";
											if (0x02E1 == ManufacturerID)
												ssOutput << "'Victron Energy BV'";
										}
										ssOutput << std::endl;
										if (dbusTemp.GetModel() == ThermometerType::Unknown)
										{
											auto foo = GoveeThermometers.find(dbusBTAddress);
											if (foo != GoveeThermometers.end())
												dbusTemp.SetModel(foo->second);
										}
										else
											GoveeThermometers.insert(std::pair<bdaddr_t, ThermometerType>(dbusBTAddress, dbusTemp.GetModel()));
										dbusTemp.ReadMSG(ManufacturerID, ManufacturerData);
									}
								}
							}
						}
					} while (dbus_message_iter_next(&array3_iter));
				}
			}
			else if (!Key.compare("RSSI"))
			{
				if (DBUS_TYPE_INT16 == dbus_message_Type)
				{
					dbus_message_iter_get_basic(&variant_iter, &value);
					ssOutput << "[                   ] [" << ba2string(dbusBTAddress) << "] " << Key << ": " << value.i16 << std::endl;
				}
			}
			else
				ssOutput << "[                   ] [" << ba2string(dbusBTAddress) << "] " << Key << std::endl;
		} while (dbus_message_iter_next(&variant_iter));
	} while (dbus_message_iter_next(&array_iter));
	return(ssOutput.str());
}
void bluez_dbus_FindExistingDevices(DBusConnection* dbus_conn, const std::set<bdaddr_t>& BT_WhiteList)
{
	// This function is mainly useful after a rapid restart of the program. BlueZ keeps around information on devices for three minutes after scanning has been stopped.
	std::ostringstream ssOutput;
	DBusMessage* dbus_msg = dbus_message_new_method_call("org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
	if (dbus_msg)
	{
		// Initialize D-Bus error
		DBusError dbus_error;
		dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe
		DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(dbus_conn, dbus_msg, DBUS_TIMEOUT_USE_DEFAULT, &dbus_error);
		dbus_message_unref(dbus_msg);
		if (dbus_reply)
		{
			if (dbus_message_get_type(dbus_reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN)
			{
				const std::string dbus_reply_Signature(dbus_message_get_signature(dbus_reply));
				int indent(16);
				if (!dbus_reply_Signature.compare("a{oa{sa{sv}}}"))
				{
					DBusMessageIter root_iter;
					dbus_message_iter_init(dbus_reply, &root_iter);
					do {
						DBusMessageIter array1_iter;
						dbus_message_iter_recurse(&root_iter, &array1_iter);
						do {
							indent += 4;
							DBusMessageIter dict1_iter;
							dbus_message_iter_recurse(&array1_iter, &dict1_iter);
							DBusBasicValue value;
							dbus_message_iter_get_basic(&dict1_iter, &value);
							std::string dict1_object_path(value.str);
							dbus_message_iter_next(&dict1_iter);
							DBusMessageIter array2_iter;
							dbus_message_iter_recurse(&dict1_iter, &array2_iter);
							do
							{
								DBusMessageIter dict2_iter;
								dbus_message_iter_recurse(&array2_iter, &dict2_iter);
								dbus_message_iter_get_basic(&dict2_iter, &value);
								std::string dict2_string(value.str);
								if (!dict2_string.compare("org.bluez.Device1"))
								{
									if (ConsoleVerbosity > 1)
										ssOutput << "[" << getTimeISO8601(true) << "] " << std::right << std::setw(indent) << "Object Path: " << dict1_object_path << std::endl;
									dbus_message_iter_next(&dict2_iter);
									DBusMessageIter array3_iter;
									dbus_message_iter_recurse(&dict2_iter, &array3_iter);
									bdaddr_t localBTAddress({ 0 });
									const std::regex ModifiedBluetoothAddressRegex("((([[:xdigit:]]{2}_){5}))[[:xdigit:]]{2}");
									std::smatch AddressMatch;
									if (std::regex_search(dict1_object_path, AddressMatch, ModifiedBluetoothAddressRegex))
									{
										std::string BluetoothAddress(AddressMatch.str());
										std::replace(BluetoothAddress.begin(), BluetoothAddress.end(), '_', ':');
										localBTAddress = string2ba(BluetoothAddress);
									}
									auto BT_AddressInWhitelist = BT_WhiteList.find(localBTAddress);
									if (BT_WhiteList.empty() || (BT_AddressInWhitelist != BT_WhiteList.end()))
									{
										Govee_Temp localTemp;
										ssOutput << bluez_dbus_msg_iter(array3_iter, localBTAddress, localTemp);
										if (localTemp.IsValid())
										{
											std::queue<Govee_Temp> foo;
											auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(localBTAddress, foo));
											ret.first->second.push(localTemp);	// puts the measurement in the queue to be written to the log file
											UpdateMRTGData(localBTAddress, localTemp);	// puts the measurement in the fake MRTG data structure
											GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(localBTAddress, 0));	// Makes sure the Bluetooth Address is in the list to get downloaded historical data
											if (ConsoleVerbosity > 0)
												ssOutput << "[" << getTimeISO8601(true) << "] [" << ba2string(localBTAddress) << "]" << " " << localTemp.WriteConsole() << std::endl;
										}
									}
								}
							} while (dbus_message_iter_next(&array2_iter));
							indent -= 4;
						} while (dbus_message_iter_next(&array1_iter));
					} while (dbus_message_iter_next(&root_iter));
				}
			}
			dbus_message_unref(dbus_reply);
		}
	}
	if (ConsoleVerbosity > 0)
		std::cout << ssOutput.str();
}
void bluez_dbus_msg_InterfacesAdded(DBusMessage* dbus_msg, bdaddr_t & dbusBTAddress, Govee_Temp & dbusTemp, const std::set<bdaddr_t>& BT_WhiteList)
{
	std::ostringstream ssOutput;
	if (std::string(dbus_message_get_signature(dbus_msg)).compare("oa{sa{sv}}"))
		ssOutput << "Invalid Signature: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	else
	{
		DBusMessageIter root_iter;
		dbus_message_iter_init(dbus_msg, &root_iter);
		DBusBasicValue value;
		dbus_message_iter_get_basic(&root_iter, &value);
		std::string root_object_path(value.str);

		std::string BluetoothAddress;
		const std::regex ModifiedBluetoothAddressRegex("((([[:xdigit:]]{2}_){5}))[[:xdigit:]]{2}");
		std::smatch AddressMatch;
		if (std::regex_search(root_object_path, AddressMatch, ModifiedBluetoothAddressRegex))
		{
			BluetoothAddress = AddressMatch.str();
			std::replace(BluetoothAddress.begin(), BluetoothAddress.end(), '_', ':');
			dbusBTAddress = string2ba(BluetoothAddress);
		}
		auto BT_AddressInWhitelist = BT_WhiteList.find(dbusBTAddress);
		if (BT_WhiteList.empty() || (BT_AddressInWhitelist != BT_WhiteList.end()))
		{
			dbus_message_iter_next(&root_iter);
			DBusMessageIter array1_iter;
			dbus_message_iter_recurse(&root_iter, &array1_iter);
			do
			{
				DBusMessageIter dict1_iter;
				dbus_message_iter_recurse(&array1_iter, &dict1_iter);
				DBusBasicValue value;
				dbus_message_iter_get_basic(&dict1_iter, &value);
				std::string val(value.str);
				if (!val.compare("org.bluez.Device1"))
				{
					dbus_message_iter_next(&dict1_iter);
					DBusMessageIter array2_iter;
					dbus_message_iter_recurse(&dict1_iter, &array2_iter);
					ssOutput << bluez_dbus_msg_iter(array2_iter, dbusBTAddress, dbusTemp);
				}
			} while (dbus_message_iter_next(&array1_iter));
		}
	}
	if (ConsoleVerbosity > 1)
		std::cout << ssOutput.str();
}
void bluez_dbus_msg_PropertiesChanged(DBusMessage* dbus_msg, bdaddr_t& dbusBTAddress, Govee_Temp& dbusTemp, const std::set<bdaddr_t> & BT_WhiteList)
{
	std::ostringstream ssOutput;
	if (std::string(dbus_message_get_signature(dbus_msg)).compare("sa{sv}as"))
		ssOutput << "Invalid Signature: " << __FILE__ << "(" << __LINE__ << ")" << std::endl;
	else
	{
		// TODO: convert dbus_msg_Path to dbusBTAddress using regex
		const std::string dbus_msg_Path(dbus_message_get_path(dbus_msg)); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#ga18adf731bb42d324fe2624407319e4af
		std::string BluetoothAddress;
		const std::regex ModifiedBluetoothAddressRegex("((([[:xdigit:]]{2}_){5}))[[:xdigit:]]{2}");
		std::smatch AddressMatch;
		if (std::regex_search(dbus_msg_Path, AddressMatch, ModifiedBluetoothAddressRegex))
		{
			BluetoothAddress = AddressMatch.str();
			std::replace(BluetoothAddress.begin(), BluetoothAddress.end(), '_', ':');
			dbusBTAddress = string2ba(BluetoothAddress);
		}
		auto BT_AddressInWhitelist = BT_WhiteList.find(dbusBTAddress);
		if (BT_WhiteList.empty() || (BT_AddressInWhitelist != BT_WhiteList.end()))
		{
			DBusMessageIter root_iter;
			std::string root_object_path;
			dbus_message_iter_init(dbus_msg, &root_iter);
			DBusBasicValue value;
			dbus_message_iter_get_basic(&root_iter, &value);
			root_object_path = std::string(value.str);
			dbus_message_iter_next(&root_iter);
			DBusMessageIter array_iter;
			dbus_message_iter_recurse(&root_iter, &array_iter);
			ssOutput << bluez_dbus_msg_iter(array_iter, dbusBTAddress, dbusTemp);
		}
	}
	if (ConsoleVerbosity > 1)
		std::cout << ssOutput.str();
}
/////////////////////////////////////////////////////////////////////////////
int LogFileTime(60);
int MinutesAverage(5);
int DaysBetweenDataDownload(0);
const int MaxMinutesBetweenBluetoothAdvertisments(3);
static void usage(int argc, char **argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -l | --log name      Logging Directory [" << LogDirectory << "]" << std::endl;
	std::cout << "    -t | --time seconds  Time between log file writes [" << LogFileTime << "]" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -m | --mrtg XX:XX:XX:XX:XX:XX Get last value for this address" << std::endl;
	std::cout << "    -o | --only XX:XX:XX:XX:XX:XX only report this address" << std::endl;
	std::cout << "    -C | --controller XX:XX:XX:XX:XX:XX use the controller with this address" << std::endl;
	std::cout << "    -a | --average minutes [" << MinutesAverage << "]" << std::endl;
	std::cout << "    -f | --cache name    cache file directory [" << CacheDirectory << "]" << std::endl;
	std::cout << "    -s | --svg name      SVG output directory [" << SVGDirectory << "]" << std::endl;
	std::cout << "    -i | --index name    HTML index file for SVG files" << std::endl;
	std::cout << "    -T | --titlemap name SVG title fully qualified filename [" << SVGTitleMapFilename << "]" << std::endl;
	std::cout << "    -c | --celsius       SVG output using degrees C [" << std::boolalpha << !SVGFahrenheit << "]" << std::endl;
	std::cout << "    -b | --battery graph Draw the battery status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly" << std::endl;
	std::cout << "    -x | --minmax graph  Draw the minimum and maximum temperature and humidity status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly" << std::endl;
	std::cout << "    -d | --download      Periodically attempt to connect and download stored data" << std::endl;
	std::cout << "    -p | --passive       Bluetooth LE Passive Scanning" << std::endl;
	std::cout << "    -n | --no-bluetooth  Monitor Logging Directory and process logs without Bluetooth Scanning" << std::endl;
	std::cout << "    -H | --HCI           Prefer deprecated BlueZ HCI interface instead of DBus" << std::endl;
	std::cout << "    -M | --monitor       Monitor Logging Directory" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:t:v:m:o:C:a:f:s:i:T:cb:x:dpnHM";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "time",   required_argument, NULL, 't' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "mrtg",	required_argument, NULL, 'm' },
		{ "only",	required_argument, NULL, 'o' },
		{ "controller", required_argument, NULL, 'C' },
		{ "average",required_argument, NULL, 'a' },
		{ "cache",	required_argument, NULL, 'f' },
		{ "svg",	required_argument, NULL, 's' },
		{ "index",	required_argument, NULL, 'i' },
		{ "titlemap",required_argument,NULL, 'T' },
		{ "celsius",no_argument,       NULL, 'c' },
		{ "battery",required_argument, NULL, 'b' },
		{ "minmax",	required_argument, NULL, 'x' },
		{ "download",no_argument,      NULL, 'd' },
		{ "passive",no_argument,       NULL, 'p' },
		{ "no-bluetooth",no_argument,  NULL, 'n' },
		{ "HCI",	no_argument,       NULL, 'H' },
		{ "monitor",no_argument,       NULL, 'M' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////
	bool bUse_HCI_Interface(false);
	bool bMonitorLoggingDirectory(false);
	std::string ControllerAddress;
	std::string MRTGAddress;
	std::set<bdaddr_t> BT_WhiteList;
	bdaddr_t OnlyFilterAddress = { 0 };

	for (;;)
	{
		std::string TempString;
		std::filesystem::path TempPath;
		int idx;
		int c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c)
		{
		case 0: /* getopt_long() flag */
			break;
		case '?':
		case 'h':	// --help
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'l':	// --log
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				LogDirectory = TempPath;
			break;
		case 't':	// --time
			try { LogFileTime = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'v':	// --verbose
			try { ConsoleVerbosity = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'm':	// --mrtg
			MRTGAddress = std::string(optarg);
			break;
		case 'o':	// --only
			if (0 == str2ba(optarg, &OnlyFilterAddress))
				BT_WhiteList.insert(OnlyFilterAddress);
			break;
		case 'C':	// --controller
			ControllerAddress = std::string(optarg);
			break;
		case 'a':	// --average
			try { MinutesAverage = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'f':	// --cache
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				CacheDirectory = TempPath;
			break;
		case 'd':	// --download
			DaysBetweenDataDownload = 14;
			break;
		case 'n':	// --no-bluetooth
			UseBluetooth = false;
			break;
		case 'p':	// --passive
			bt_ScanType = 0;
			break;
		case 's':	// --svg
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				SVGDirectory = TempPath;
			break;
		case 'i':	// --index
			TempPath = std::string(optarg);
			SVGIndexFilename = TempPath;
			break;
		case 'T':	// --titlemap
			TempPath = std::string(optarg);
			if (ReadTitleMap(TempPath))
				SVGTitleMapFilename = TempPath;
			break;
		case 'c':	// --celsius
			SVGFahrenheit = false;
			break;
		case 'b':	// --battery
			try { SVGBattery = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'x':	// --minmax
			try { SVGMinMax = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'H':
			bUse_HCI_Interface = true;
			break;
		case 'M':
			bMonitorLoggingDirectory = true;
			break;
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (!MRTGAddress.empty())
	{
		GetMRTGOutput(MRTGAddress, MinutesAverage);
		exit(EXIT_SUCCESS);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (!SVGIndexFilename.empty())
	{
		WriteSVGIndex(LogDirectory, SVGIndexFilename);
		exit(EXIT_SUCCESS);
	}
	int ExitValue = EXIT_SUCCESS;
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeISO8601(true) << "] " << ProgramVersionString << std::endl;
		if (ConsoleVerbosity > 1)
		{
			std::cout << "[                   ]      log: " << LogDirectory << std::endl;
			std::cout << "[                   ]    cache: " << CacheDirectory << std::endl;
			std::cout << "[                   ]      svg: " << SVGDirectory << std::endl;
			std::cout << "[                   ]  battery: " << SVGBattery << std::endl;
			std::cout << "[                   ]   minmax: " << SVGMinMax << std::endl;
			std::cout << "[                   ]  celsius: " << std::boolalpha << !SVGFahrenheit << std::endl;
			std::cout << "[                   ] titlemap: " << SVGTitleMapFilename << std::endl;
			std::cout << "[                   ]     time: " << LogFileTime << std::endl;
			std::cout << "[                   ]  average: " << MinutesAverage << std::endl;
			std::cout << "[                   ] download: " << DaysBetweenDataDownload << " (days betwen data download)" << std::endl;
			std::cout << "[                   ]  passive: " << std::boolalpha << (bt_ScanType == 0) << std::endl;
			std::cout << "[                   ] no-bluetooth: " << std::boolalpha << !UseBluetooth << std::endl;
			std::cout << "[                   ]      HCI: " << std::boolalpha << bUse_HCI_Interface << std::endl;
		}
		if (!BT_WhiteList.empty())
		{
			std::cout << "[                   ] only listening to:";
			for (auto& address : BT_WhiteList)
				std::cout << " [" << ba2string(address) << "]";
			std::cout << std::endl;
		}
	}
	else
		std::cerr << ProgramVersionString << " (starting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (UseBluetooth)
	{
		if (!SVGDirectory.empty())
		{
			if (SVGTitleMapFilename.empty()) // If this wasn't set as a parameter, look in the SVG Directory for a default titlemap
				SVGTitleMapFilename = std::filesystem::path(SVGDirectory / "gvh-titlemap.txt");
			ReadTitleMap(SVGTitleMapFilename);
			if (!CacheDirectory.empty()) // 2024-09-25 This location was a bad choice and has been deprecated to the logfile location
			{
				std::filesystem::path CacheTypesFileName(CacheDirectory / "gvh-types-cache.txt");
				std::ifstream TheFile(CacheTypesFileName);
				if (TheFile.is_open())
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601(true) << "] Reading: " << CacheTypesFileName.string() << std::endl;
					else
						std::cerr << "Reading: " << CacheTypesFileName.string() << std::endl;
					std::string TheLine;
					while (std::getline(TheFile, TheLine))
					{
						std::smatch BluetoothAddress;
						if (std::regex_search(TheLine, BluetoothAddress, BluetoothAddressRegex))
						{
							bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddress.str()));
							const std::string delimiters(" \t");
							auto i = TheLine.find_first_of(delimiters);		// Find first delimiter
							i = TheLine.find_first_not_of(delimiters, i);	// Move past consecutive delimiters
							std::string theType = (i == std::string::npos) ? "" : TheLine.substr(i);
							GoveeThermometers.insert(std::make_pair(TheBlueToothAddress, String2ThermometerType(theType)));
						}
					}
					TheFile.close();
				}
			}
			if (!LogDirectory.empty())
			{
				std::filesystem::path CacheTypesFileName(LogDirectory / "gvh-thermometer-types.txt");
				std::ifstream TheFile(CacheTypesFileName);
				if (TheFile.is_open())
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601(true) << "] Reading: " << CacheTypesFileName.string() << std::endl;
					else
						std::cerr << "Reading: " << CacheTypesFileName.string() << std::endl;
					std::string TheLine;
					while (std::getline(TheFile, TheLine))
					{
						std::smatch BluetoothAddress;
						if (std::regex_search(TheLine, BluetoothAddress, BluetoothAddressRegex))
						{
							bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddress.str()));
							const std::string delimiters(" \t");
							auto i = TheLine.find_first_of(delimiters);		// Find first delimiter
							i = TheLine.find_first_not_of(delimiters, i);	// Move past consecutive delimiters
							std::string theType = (i == std::string::npos) ? "" : TheLine.substr(i);
							GoveeThermometers.insert(std::make_pair(TheBlueToothAddress, String2ThermometerType(theType)));
						}
					}
					TheFile.close();
				}
			}
			ReadCacheDirectory(); // if cache directory is configured, read it before reading all the normal logs
			ReadLoggedData(); // only read the logged data if creating SVG files
			GenerateCacheFile(GoveeMRTGLogs); // update cache files if any new data was in logs
			WriteAllSVG();
		}
		///////////////////////////////////////////////////////////////////////////////////////////////
		// Read Persistence Data about when the last connection and download of data was done as opposed to listening for advertisments
		// We don't want to connect too often because it uses more battery on the device, but it's nice to have a more consistent 
		// timeline of data occasionally.
		if (!LogDirectory.empty())
		{
			std::filesystem::path filename(LogDirectory / GVHLastDownloadFileName);
			std::ifstream TheFile(filename);
			if (TheFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Reading LastDownload: " << filename.string() << std::endl;
				else
					std::cerr << "Reading LastDownload: " << filename.string() << std::endl;
				std::string TheLine;
				while (std::getline(TheFile, TheLine))
				{
					// rudimentary line checking. It has a BT Address and has a Tab character
					std::smatch BluetoothAddress;
					if (std::regex_search(TheLine, BluetoothAddress, BluetoothAddressRegex))
					{
						bdaddr_t TheBlueToothAddress({ 0 });
						str2ba(BluetoothAddress.str().c_str(), &TheBlueToothAddress);
						const std::string delimiters(" \t");
						auto i = TheLine.find_first_of(delimiters);		// Find first delimiter
						i = TheLine.find_first_not_of(delimiters, i);	// Move past consecutive delimiters
						if (i != std::string::npos)
							GoveeLastDownload.insert(std::make_pair(TheBlueToothAddress, ISO8601totime(TheLine.substr(i))));
					}
				}
				TheFile.close();
			}
		}
		///////////////////////////////////////////////////////////////////////////////////////////////
		if (!bUse_HCI_Interface)	// BlueZ over DBus is the recommended method of Bluetooth
		{
			DBusError dbus_error;
			dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe

			// Connect to the system bus
			DBusConnection* dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusBus.html#ga77ba5250adb84620f16007e1b023cf26
			if (dbus_error_is_set(&dbus_error)) // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#gab0ed62e9fc2685897eb2d41467c89405
			{
				std::cout << "[" << getTimeISO8601(true) << "] Error connecting to the D-Bus system bus: " << dbus_error.message << std::endl;
				dbus_error_free(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#gaac6c14ead14829ee4e090f39de6a7568
			}
			else
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Connected to D-Bus as \"" << dbus_bus_get_unique_name(dbus_conn) << "\"" << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusBus.html#ga8c10339a1e62f6a2e5752d9c2270d37b
				else
					std::cerr << "Connected to D-Bus as \"" << dbus_bus_get_unique_name(dbus_conn) << "\"" << std::endl; // https://dbus.freedesktop.org/doc/api/html/group__DBusBus.html#ga8c10339a1e62f6a2e5752d9c2270d37b
				std::map<bdaddr_t, std::string> BlueZAdapterMap;
				bUse_HCI_Interface = !bluez_find_adapters(dbus_conn, BlueZAdapterMap);
				if (bUse_HCI_Interface && BlueZAdapterMap.empty())
				{
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601(true) << "] Could not get list of adapters from BlueZ over DBus. Reverting to HCI interface." << std::endl;
					else
						std::cerr << "Could not get list of adapters from BlueZ over DBus. Reverting to HCI interface." << std::endl;
				}
				if (!BlueZAdapterMap.empty())
				{
					std::string BlueZAdapter(BlueZAdapterMap.cbegin()->second);
					if (!ControllerAddress.empty())
						if (auto search = BlueZAdapterMap.find(string2ba(ControllerAddress)); search != BlueZAdapterMap.end())
							BlueZAdapter = search->second;

					bluez_power_on(dbus_conn, BlueZAdapter.c_str());
					bluez_filter_le(dbus_conn, BlueZAdapter.c_str());
					bluez_dbus_FindExistingDevices(dbus_conn, BT_WhiteList); // This pulls data from BlueZ on devices that BlueZ is already keeping track of
					if (bluez_discovery(dbus_conn, BlueZAdapter.c_str(), true))
					{
						dbus_connection_flush(dbus_conn); // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga10e68d9d2f41d655a4151ddeb807ff54
						std::vector<std::string> MatchRules;
						MatchRules.push_back("type='signal',sender='org.bluez',member='InterfacesAdded'");
						MatchRules.push_back("type='signal',sender='org.bluez',member='PropertiesChanged'");
						for (auto& MatchRule : MatchRules)
						{
							dbus_error_init(&dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusErrors.html#ga8937f0b7cdf8554fa6305158ce453fbe
							dbus_bus_add_match(dbus_conn, MatchRule.c_str(), &dbus_error); // https://dbus.freedesktop.org/doc/api/html/group__DBusBus.html#ga4eb6401ba014da3dbe3dc4e2a8e5b3ef
							if (dbus_error_is_set(&dbus_error))
							{
								std::cout << "Error adding a match rule on the D-Bus system bus: " << dbus_error.message << std::endl;
								dbus_error_free(&dbus_error);
							}
						}
						// Set up CTR-C signal handler
						typedef void(*SignalHandlerPointer)(int);
						SignalHandlerPointer previousHandlerSIGINT = std::signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
						SignalHandlerPointer previousHandlerSIGHUP = std::signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler

						// Main loop
						bRun = true;
						time_t TimeStart(0), TimeSVG(0), TimeAdvertisment(0);
						time(&TimeStart);
						while (bRun)
						{
							// Wait for access to the D-Bus
							if (!dbus_connection_read_write(dbus_conn, 1000)) // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga371163b4955a6e0bf0f1f70f38390c14
							{
								if (ConsoleVerbosity > 0)
									std::cout << "[" << getTimeISO8601(true) << "] D-Bus connection was closed" << std::endl;
								else
									std::cerr << "D-Bus connection was closed" << std::endl;
								bRun = false;
							}
							else
							{
								// Pop first message on D-Bus connection
								DBusMessage* dbus_msg = dbus_connection_pop_message(dbus_conn); // https://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga1e40d994ea162ce767e78de1c4988566

								// If there is nothing to receive we get a NULL
								if (dbus_msg != nullptr)
								{
									if (DBUS_MESSAGE_TYPE_SIGNAL == dbus_message_get_type(dbus_msg))
									{
										const std::string dbus_msg_Member(dbus_message_get_member(dbus_msg)); // https://dbus.freedesktop.org/doc/api/html/group__DBusMessage.html#gaf5c6b705c53db07a5ae2c6b76f230cf9
										bdaddr_t localBTAddress({ 0 });
										Govee_Temp localTemp;
										if (!dbus_msg_Member.compare("InterfacesAdded"))
											bluez_dbus_msg_InterfacesAdded(dbus_msg, localBTAddress, localTemp, BT_WhiteList);
										else if (!dbus_msg_Member.compare("PropertiesChanged"))
											bluez_dbus_msg_PropertiesChanged(dbus_msg, localBTAddress, localTemp, BT_WhiteList);
										if (localTemp.IsValid())
										{
											std::queue<Govee_Temp> foo;
											auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(localBTAddress, foo));
											ret.first->second.push(localTemp);	// puts the measurement in the queue to be written to the log file
											UpdateMRTGData(localBTAddress, localTemp);	// puts the measurement in the fake MRTG data structure
											GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(localBTAddress, 0));	// Makes sure the Bluetooth Address is in the list to get downloaded historical data
											if (ConsoleVerbosity > 0)
												std::cout << "[" << getTimeISO8601(true) << "] [" << ba2string(localBTAddress) << "]" << " " << localTemp.WriteConsole() << std::endl;
										}
									}
									dbus_message_unref(dbus_msg); // Free the message
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
							if ((DaysBetweenDataDownload > 0) && !LogDirectory.empty())
							{
								for (auto it = GoveeTemperatures.begin(); it != GoveeTemperatures.end(); ++it)
								{
									if (!it->second.empty())
									{
										int BatteryToRecord = it->second.front().GetBattery();
										time_t LastDownloadTime = 0;
										auto RecentDownload = GoveeLastDownload.find(it->first);
										if (RecentDownload != GoveeLastDownload.end())
											LastDownloadTime = RecentDownload->second;
										// Don't try to download more often than once a week, because it uses more battery than just the advertisments
										if (difftime(TimeNow, LastDownloadTime) > (60 * 60 * 24 * DaysBetweenDataDownload))
										{
											time_t DownloadTime = ConnectAndDownload(dbus_conn, it->first, LastDownloadTime, BatteryToRecord);
											if (DownloadTime > 0)
											{
												if (RecentDownload != GoveeLastDownload.end())
													RecentDownload->second = DownloadTime;
												else
													GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(it->first, DownloadTime));
											}
										}
									}
								}
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
							if (difftime(TimeNow, TimeStart) > 60*30) // Issue StartDiscovery command every 30 minutes to make sure it's not been turned off by another bluetooth process
								bRun = bluez_discovery(dbus_conn, BlueZAdapter.c_str(), true);
						}
						bluez_discovery(dbus_conn, BlueZAdapter.c_str(), false);

						std::signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
						std::signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler

						GenerateLogFile(GoveeTemperatures, GoveeLastDownload); // flush contents of accumulated map to logfiles
						GenerateCacheFile(GoveeMRTGLogs); // flush FakeMRTG data to cache files
						if (!LogDirectory.empty())
						{
							std::filesystem::path CacheTypesFileName(LogDirectory / "gvh-thermometer-types.txt");
							std::ofstream CacheFile(CacheTypesFileName, std::ios_base::out | std::ios_base::trunc);
							if (CacheFile.is_open())
							{
								if (ConsoleVerbosity > 0)
									std::cout << "[" << getTimeISO8601(true) << "] Writing: " << CacheTypesFileName.string() << std::endl;
								else
									std::cerr << "Writing: " << CacheTypesFileName.string() << std::endl;
								for (auto i : GoveeThermometers)
									CacheFile << ba2string(i.first) << "\t" << ThermometerType2String(i.second) << std::endl;
								CacheFile.close();
							}
						}
					}
					bluez_filter_le(dbus_conn, BlueZAdapter.c_str(), false, false); // remove discovery filter
				}
				// Close the connection. When using the System Bus, unreference the connection instead of closing it
				dbus_connection_unref(dbus_conn);
			}
		}
		///////////////////////////////////////////////////////////////////////////////////////////////
		if (bUse_HCI_Interface)	// The HCI interface for bluetooth is deprecated, with BlueZ over DBus being preferred
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
				// Set up CTR-C signal handler
				typedef void(*SignalHandlerPointer)(int);
				SignalHandlerPointer previousHandlerSIGINT = std::signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
				SignalHandlerPointer previousHandlerSIGHUP = std::signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler

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

						auto btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
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
																bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList);
																time_t DownloadTime = ConnectAndDownload(BlueToothDevice_Handle, info->bdaddr, LastDownloadTime, BatteryToRecord);
																if (DownloadTime > 0)
																{
																	if (RecentDownload != GoveeLastDownload.end())
																		RecentDownload->second = DownloadTime;
																	else
																		GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, DownloadTime));
																}
																btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
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
										if (difftime(TimeNow, TimeAdvertisment) > MaxMinutesBetweenBluetoothAdvertisments * 60) // Hack to force scanning restart regularly
										{
											if (ConsoleVerbosity > 0)
												std::cout << "[" << getTimeISO8601(true) << "] No recent Bluetooth LE Advertisments! (> " << MaxMinutesBetweenBluetoothAdvertisments << " Minutes)" << std::endl;
											btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
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
							btRVal = bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList);
						}
					}
					hci_close_dev(BlueToothDevice_Handle);
				}
				std::signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
				std::signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler

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
	}
	else if ((!UseBluetooth) && (!LogDirectory.empty()) && (!SVGDirectory.empty()))
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601(true) << "] " << ProgramVersionString << " Running in --no-bluetooth mode" << std::endl;
		else
			std::cerr << ProgramVersionString << " Running in --no-bluetooth mode" << std::endl;

		if (SVGTitleMapFilename.empty()) // If this wasn't set as a parameter, look in the SVG Directory for a default titlemap
		{
			std::ostringstream TitleMapFilename;
			TitleMapFilename << SVGDirectory;
			TitleMapFilename << "/gvh-titlemap.txt";
			SVGTitleMapFilename = TitleMapFilename.str();
		}
		ReadTitleMap(SVGTitleMapFilename);
		ReadCacheDirectory(); // if cache directory is configured, read it before reading all the normal logs
		ReadLoggedData();

		auto previousHandlerSIGINT = std::signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
		auto previousHandlerSIGHUP = std::signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler
		auto previousAlarmHandler = std::signal(SIGALRM, SignalHandlerSIGALRM);	// Install Alarm signal handler
		bRun = true;
		while (bRun)
		{
			sigset_t set;
			sigemptyset(&set);
			sigaddset(&set, SIGALRM);
			sigaddset(&set, SIGINT);
			sigaddset(&set, SIGHUP);
			alarm(5 * 60);
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601(true) << "] Alarm Set" << std::endl;
			int sig = 0;
			int s = sigwait(&set, &sig);
			if (sig == SIGALRM)
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601(true) << "] Alarm Recieved" << std::endl;
				MonitorLoggedData(LogFileTime * 2);
				WriteAllSVG();
			}
		}
		std::signal(SIGALRM, previousAlarmHandler);	// Restore original Alarm signal handler
		std::signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
		std::signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	return(ExitValue);
}
