/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 William C Bonner
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
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <queue>
#include <set>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>
#include <vector>
#include "att-types.h"
#include "uuid.h"

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogger Version 2.20230801-1 Built on: " __DATE__ " at " __TIME__);
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t & TheTime, const bool LocalTime = false)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	struct tm* timecallresult(nullptr);
	if (LocalTime)
		timecallresult = localtime_r(&TheTime, &UTC);
	else
		timecallresult = gmtime_r(&TheTime, &UTC);
	if (nullptr != timecallresult)
	{
		ISOTime.fill('0');
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
		{
			ISOTime << UTC.tm_year + 1900 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mon + 1 << "-";
			ISOTime.width(2);
			ISOTime << UTC.tm_mday << "T";
		}
		ISOTime.width(2);
		ISOTime << UTC.tm_hour << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_min << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_sec;
	}
	return(ISOTime.str());
}
std::string getTimeISO8601(const bool LocalTime = false)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToISO8601(timer, LocalTime));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());
	return(rval);
}
time_t ISO8601totime(const std::string & ISOTime)
{
	time_t timer(0);
	if (ISOTime.length() >= 19)
	{
		struct tm UTC;
		UTC.tm_year = stoi(ISOTime.substr(0, 4)) - 1900;
		UTC.tm_mon = stoi(ISOTime.substr(5, 2)) - 1;
		UTC.tm_mday = stoi(ISOTime.substr(8, 2));
		UTC.tm_hour = stoi(ISOTime.substr(11, 2));
		UTC.tm_min = stoi(ISOTime.substr(14, 2));
		UTC.tm_sec = stoi(ISOTime.substr(17, 2));
		UTC.tm_gmtoff = 0;
		UTC.tm_isdst = -1;
		UTC.tm_zone = 0;
		#ifdef _MSC_VER
		_tzset();
		_get_daylight(&(UTC.tm_isdst));
		#endif
		#ifdef __USE_MISC
		timer = timegm(&UTC);
		if (timer == -1)
			return(0);	// if timegm() returned an error value, leave time set at epoch
		#else
		timer = mktime(&UTC);
		if (timer == -1)
			return(0);	// if mktime() returned an error value, leave time set at epoch
		timer -= timezone; // HACK: Works in my initial testing on the raspberry pi, but it's currently not DST
		#endif
		#ifdef _MSC_VER
		long Timezone_seconds = 0;
		_get_timezone(&Timezone_seconds);
		timer -= Timezone_seconds;
		int DST_hours = 0;
		_get_daylight(&DST_hours);
		long DST_seconds = 0;
		_get_dstbias(&DST_seconds);
		timer += DST_hours * DST_seconds;
		#endif
	}
	return(timer);
}
// Microsoft Excel doesn't recognize ISO8601 format dates with the "T" seperating the date and time
// This function puts a space where the T goes for ISO8601. The dates can be decoded with ISO8601totime()
std::string timeToExcelDate(const time_t & TheTime, const bool LocalTime = false) { std::string ExcelDate(timeToISO8601(TheTime, LocalTime)); ExcelDate.replace(10, 1, " "); return(ExcelDate); }
std::string timeToExcelLocal(const time_t& TheTime) { return(timeToExcelDate(TheTime, true)); }
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
/////////////////////////////////////////////////////////////////////////////
int ConsoleVerbosity(1);
std::filesystem::path LogDirectory;	// If this remains empty, log Files are not created.
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
	H5074 = 5074, 
	H5075 = 5075, 
	H5174 = 5174,
	H5177 = 5177,
	H5179 = 5179,
	H5183 = 5183, 
	H5182 = 5182,
	H5181 = 5181
};
class  Govee_Temp {
public:
	time_t Time;
	std::string WriteTXT(const char seperator = '\t') const;
	bool ReadMSG(const uint8_t * const data);
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
	ThermometerType SetModel(const std::string& Name);
	ThermometerType SetModel(const unsigned short* UUID);
	enum granularity {day, week, month, year};
	void NormalizeTime(granularity type);
	granularity GetTimeGranularity(void) const;
	bool IsValid(void) const { return(Averages > 0); };
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
	std::string TheLine(data);
	// erase any nulls from the data. these are occasionally in the log file when the platform crashed during a write to the logfile.
	for (auto pos = TheLine.find('\000'); pos != std::string::npos; pos = TheLine.find('\000'))
		TheLine.erase(pos);
	char buffer[256];
	if (!TheLine.empty() && (TheLine.size() < sizeof(buffer)))
	{
		// minor garbage check looking for corrupt data with no tab characters
		if (TheLine.find('\t') != std::string::npos)
		{
			TheLine.copy(buffer, TheLine.size());
			buffer[TheLine.size()] = '\0';
			std::string theDate(strtok(buffer, "\t"));
			Time = ISO8601totime(theDate);
			std::string theTemp(strtok(NULL, "\t"));
			TemperatureMax[0] = TemperatureMin[0] = Temperature[0] = std::atof(theTemp.c_str());
			std::string theHumidity(strtok(NULL, "\t"));
			Humidity = HumidityMin = HumidityMax = std::atof(theHumidity.c_str());
			std::string theBattery(strtok(NULL, "\t"));
			Battery = std::atoi(theBattery.c_str());
			char* theModel = strtok(NULL, "\t");
			if (theModel != NULL)
			{
				switch (std::atoi(theModel))
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
				default:
					Model = ThermometerType::Unknown;
				}
				auto index = 1;
				char* nextTemp = strtok(NULL, "\t");
				while ((nextTemp != NULL) && (index < (sizeof(Temperature) / sizeof(Temperature[0]))))
				{
					TemperatureMax[index] = TemperatureMin[index] = Temperature[index] = std::atof(nextTemp);
					nextTemp = strtok(NULL, "\t");
					index++;
				}
			}
			time_t timeNow(0);
			time(&timeNow);
			if (Time <= timeNow) // Only validate data from the past.
				Averages = 1;
		}
	}
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
	return(ssValue.str());
}
ThermometerType Govee_Temp::SetModel(const std::string& Name)
{
	ThermometerType rval = Model;
	if (0 == Name.substr(0, 8).compare("GVH5177_"))
		Model = ThermometerType::H5177;
	else if (0 == Name.substr(0, 8).compare("GVH5174_"))
		Model = ThermometerType::H5174;
	else if (0 == Name.substr(0, 12).compare("Govee_H5179_"))
		Model = ThermometerType::H5179;
	else if (0 == Name.substr(0, 8).compare("GVH5075_"))
		Model = ThermometerType::H5075;
	else if (0 == Name.substr(0, 12).compare("Govee_H5074_"))
		Model = ThermometerType::H5074;
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
			// 0188EC00 0101 2008 121B 64
			// 2 3 4 5  6 7  8 9
			short iTemp = short(data[9]) << 8 | short(data[8]);
			int iHumidity = int(data[11]) << 8 | int(data[12]);
			Temperature[0] = float(iTemp) / 100.0;
			Humidity = float(iHumidity) / 100.0;
			Battery = int(data[12]);
			Averages = 1;
			time(&Time);
			TemperatureMin[0] = TemperatureMax[0] = Temperature[0];	//HACK: make sure that these values are set
			rval = true;
		}
		else if ((data_len == 9) && (data[2] == 0x01) && (data[3] == 0x00)) // GVH5177_xxxx or GVH5174_xxxx
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
			for (auto index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
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
			Battery = int(data[9]);
			Averages = 1;
			time(&Time);
			for (auto index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
				TemperatureMin[index] = TemperatureMax[index] = Temperature[index];	//HACK: make sure that these values are set
			rval = true;
		}
	}
	return(rval);
}
void Govee_Temp::SetMinMax(const Govee_Temp& a)
{
	for (auto index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
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
		for (auto index = 0; index < (sizeof(Temperature) / sizeof(Temperature[0])); index++)
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
std::string ba2string(const bdaddr_t& a) { char addr_str[18]; ba2str(&a, addr_str); std::string rVal(addr_str); return(rVal); }
bdaddr_t string2ba(const std::string& a) { std::string ssBTAddress(a); if (ssBTAddress.length() == 12)for (auto index = ssBTAddress.length() - 2; index > 0; index -= 2)ssBTAddress.insert(index, ":"); bdaddr_t TheBlueToothAddress({ 0 }); if (ssBTAddress.length() == 17)str2ba(ssBTAddress.c_str(), &TheBlueToothAddress); return(TheBlueToothAddress); }
/////////////////////////////////////////////////////////////////////////////
std::map<bdaddr_t, std::queue<Govee_Temp>> GoveeTemperatures;
std::map<bdaddr_t, time_t> GoveeLastDownload;
const std::string GVHLastDownloadFileName("gvh-lastdownload.txt");
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
	OutputFilename << "gvh507x_";
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
	std::filesystem::path OldFormatFileName(LogDirectory / OutputFilename.str());

	// The New Format Log File Name includes the entire Bluetooth Address, making it much easier to recognize and add to MRTG config files.
	OutputFilename.str("");
	OutputFilename << "gvh-";
	OutputFilename << btAddress;
	if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
		OutputFilename << "-" << std::dec << UTC.tm_year + 1900 << "-" << std::setw(2) << std::setfill('0') << UTC.tm_mon + 1;
	OutputFilename << ".txt";
	std::filesystem::path NewFormatFileName(LogDirectory / OutputFilename.str());

	// This is a temporary hack to transparently change log file name formats
	std::ifstream OldFile(OldFormatFileName);
	if (OldFile.is_open())
	{
		OldFile.close();
		try 
		{ 
			std::filesystem::rename(OldFormatFileName, NewFormatFileName);
			std::cerr << "[                   ] Renamed " << OldFormatFileName << " to " << NewFormatFileName << std::endl;
		}
		catch (const std::filesystem::filesystem_error& ia)
		{ 
			std::cerr << "[                   ] " << ia.what() << std::endl;
			std::cerr << "[                   ] Unable to Rename " << OldFormatFileName << " to " << NewFormatFileName << std::endl;
		}
	}

	return(NewFormatFileName);
}
bool GenerateLogFile(std::map<bdaddr_t, std::queue<Govee_Temp>> &AddressTemperatureMap, std::map<bdaddr_t, time_t> &PersistenceData)
{
	bool rval = false;
	if (!LogDirectory.empty())
	{
		for (auto it = AddressTemperatureMap.begin(); it != AddressTemperatureMap.end(); ++it)
		{
			if (!it->second.empty()) // Only open the log file if there are entries to add
			{
				std::ofstream LogFile(GenerateLogFileName(it->first), std::ios_base::out | std::ios_base::app | std::ios_base::ate);
				if (LogFile.is_open())
				{
					while (!it->second.empty())
					{
						LogFile << it->second.front().WriteTXT() << std::endl;
						it->second.pop();
					}
					LogFile.close();
					rval = true;
				}
			}
		}
		if (!PersistenceData.empty())
		{
			if (ConsoleVerbosity > 0)
				for (auto iter = PersistenceData.begin(); iter != PersistenceData.end(); iter++)
					std::cout << "[-------------------] [" << ba2string(iter->first) << "] " << timeToISO8601(iter->second) << std::endl;
			// If PersistenceData has updated information, write new data to file
			std::filesystem::path filename(LogDirectory / GVHLastDownloadFileName);
			time_t MostRecentDownload(0);
			for (auto it = PersistenceData.begin(); it != PersistenceData.end(); ++it)
				if (MostRecentDownload < it->second)
					MostRecentDownload = it->second;
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
					for (auto it = PersistenceData.begin(); it != PersistenceData.end(); ++it)
						PersistenceFile << ba2string(it->first) << "\t" << timeToISO8601(it->second) << std::endl;
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
	// By declaring these items here, I'm then basing all my other dimensions on these
	const int SVGWidth(500);
	const int SVGHeight(135);
	const int FontSize(12);
	const int TickSize(2);
	int GraphWidth = SVGWidth - (FontSize * 5);
	const bool DrawHumidity = TheValues[0].GetHumidity() != 0; // HACK: I should really check the entire data set
	if (!TheValues.empty())
	{
		struct stat64 SVGStat;
		SVGStat.st_mtim.tv_sec = 0;
		if (-1 == stat64(SVGFileName.c_str(), &SVGStat))
			if (ConsoleVerbosity > 0)
				perror(SVGFileName.c_str());
				//std::cout << "[" << getTimeISO8601() << "] stat returned error on : " << SVGFileName << std::endl;
		if (TheValues.begin()->Time > SVGStat.st_mtim.tv_sec)	// only write the file if we have new data
		{
			std::ofstream SVGFile(SVGFileName);
			if (SVGFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName.string() << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Temperature (" << std::fixed << std::setprecision(1) << TheValues[0].GetTemperature(Fahrenheit) << (Fahrenheit ? "°F)" : "°C)");
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
				SVGFile << "\t<style>" << std::endl;
				SVGFile << "\t\ttext { font-family: sans-serif; font-size: " << FontSize << "px; fill: black; }" << std::endl;
				SVGFile << "\t\tline { stroke: black; }" << std::endl;
				SVGFile << "\t\tpolygon { fill-opacity: 0.5; }" << std::endl;
#ifdef _DARK_STYLE_
				SVGFile << "\t@media only screen and (prefers-color-scheme: dark) {" << std::endl;
				SVGFile << "\t\ttext { fill: grey; }" << std::endl;
				SVGFile << "\t\tline { stroke: grey; }" << std::endl;
				SVGFile << "\t}" << std::endl;
#endif // _DARK_STYLE_
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
				SVGFile << "\t<text style=\"fill:blue;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendTemperature << "</text>" << std::endl;
				if (DrawHumidity)
				{
					LegendIndex++;
					SVGFile << "\t<text style=\"fill:green;text-anchor:middle\" x=\"" << FontSize * LegendIndex << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendHumidity << "</text>" << std::endl;
				}
				if (DrawBattery)
				{
					LegendIndex++;
					SVGFile << "\t<text style=\"fill:OrangeRed\" text-anchor=\"middle\" x=\"" << FontSize * LegendIndex << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * LegendIndex << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendBattery << "</text>" << std::endl;
				}
				if (DrawHumidity)
				{
					if (MinMax)
					{
						SVGFile << "\t<!-- Humidity Max -->" << std::endl;
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green\" points=\"";
						SVGFile << GraphLeft + 1 << "," << GraphBottom - 1 << " ";
						for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
							SVGFile << index + GraphLeft << "," << int(((HumiMax - TheValues[index].GetHumidityMax()) * HumiVerticalFactor) + GraphTop) << " ";
						if (GraphWidth < TheValues.size())
							SVGFile << GraphRight - 1 << "," << GraphBottom - 1;
						else
							SVGFile << GraphRight - (GraphWidth - TheValues.size()) << "," << GraphBottom - 1;
						SVGFile << "\" />" << std::endl;
						SVGFile << "\t<!-- Humidity Min -->" << std::endl;
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green\" points=\"";
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
						SVGFile << "\t<polygon style=\"fill:lime;stroke:green\" points=\"";
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
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 5 << "\">" << std::fixed << std::setprecision(1) << TempMax << "</text>" << std::endl;
				if (DrawHumidity)
					SVGFile << "\t<text style=\"fill:green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop + 4 << "\">" << std::fixed << std::setprecision(1) << HumiMax << "</text>" << std::endl;

				// Bottom Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphBottom << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;
				SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphBottom + 5 << "\">" << std::fixed << std::setprecision(1) << TempMin << "</text>" << std::endl;
				if (DrawHumidity)
					SVGFile << "\t<text style=\"fill:green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphBottom + 4 << "\">" << std::fixed << std::setprecision(1) << HumiMin << "</text>" << std::endl;

				// Left Line
				SVGFile << "\t<line x1=\"" << GraphLeft << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Right Line
				SVGFile << "\t<line x1=\"" << GraphRight << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Vertical Division Dashed Lines
				for (auto index = 1; index < 4; index++)
				{
					SVGFile << "\t<line style=\"stroke-dasharray:1\" x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop + (GraphVerticalDivision * index) << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop + (GraphVerticalDivision * index) << "\" />" << std::endl;
					SVGFile << "\t<text style=\"fill:blue;text-anchor:end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << TempMax - (TempVerticalDivision * index) << "</text>" << std::endl;
					if (DrawHumidity)
						SVGFile << "\t<text style=\"fill:green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << HumiMax - (HumiVerticalDivision * index) << "</text>" << std::endl;
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
					SVGFile << "\t<polygon style=\"fill:blue;stroke:blue\" points=\"";
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
					SVGFile << "\t<polyline style=\"fill:none;stroke:blue\" points=\"";
					for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
						SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetTemperature(Fahrenheit)) * TempVerticalFactor) + GraphTop) << " ";
					SVGFile << "\" />" << std::endl;
				}

				// Battery Values as a continuous line
				if (DrawBattery)
				{
					SVGFile << "\t<!-- Battery -->" << std::endl;
					double BatteryVerticalFactor = (GraphBottom - GraphTop) / 100.0;
					SVGFile << "\t<polyline style=\"fill:none;stroke:OrangeRed\" points=\"";
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
void UpdateMRTGData(const bdaddr_t& TheAddress, Govee_Temp& TheValue)
{
	std::vector<Govee_Temp> foo;
	auto ret = GoveeMRTGLogs.insert(std::pair<bdaddr_t, std::vector<Govee_Temp>>(TheAddress, foo));
	std::vector<Govee_Temp> &FakeMRTGFile = ret.first->second;
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
		if (difftime(DaySampleFirst->Time, (DaySampleFirst+1)->Time) > DAY_SAMPLE)
			DaySampleFirst->Time = (DaySampleFirst + 1)->Time + DAY_SAMPLE;
		if (DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::year)
		{
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling year " << timeToExcelLocal(DaySampleFirst->Time) << " > " << timeToExcelLocal(YearSampleFirst->Time) << std::endl;
			// shuffle all the year samples toward the end
			std::copy_backward(YearSampleFirst, YearSampleLast - 1, YearSampleLast);
			*YearSampleFirst = Govee_Temp();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < (12 * 24))); iter++) // One Day of day samples
				*YearSampleFirst += *iter;
		}
		if ((DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::year) ||
			(DaySampleFirst->GetTimeGranularity() == Govee_Temp::granularity::month))
		{
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling month " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
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
			if (ConsoleVerbosity > 1)
				std::cout << "[" << getTimeISO8601() << "] shuffling week " << timeToExcelLocal(DaySampleFirst->Time) << std::endl;
			// shuffle all the month samples toward the end
			std::copy_backward(WeekSampleFirst, WeekSampleLast - 1, WeekSampleLast);
			*WeekSampleFirst = Govee_Temp();
			for (auto iter = DaySampleFirst; (iter->IsValid() && ((iter - DaySampleFirst) < 6)); iter++) // Half an hour of day samples
				*WeekSampleFirst += *iter;
		}
	}
	if (ZeroAccumulator)
		FakeMRTGFile[1] = Govee_Temp();
}
void ReadLoggedData(const std::filesystem::path& filename)
{
	if (ConsoleVerbosity > 0)
		std::cout << "[" << getTimeISO8601() << "] Reading: " << filename.string() << std::endl;
	else
		std::cerr << "Reading: " << filename.string() << std::endl;
	std::string ssBTAddress;
	// TODO: make sure the filename looks like my standard filename gvh507x_A4C13813AE36-2020-09.txt
	auto pos = filename.stem().string().find("gvh-");
	if (pos != std::string::npos)
		ssBTAddress = filename.stem().string().substr(4, 12);	// new filename format (2023-04-03)
	else
		ssBTAddress = filename.stem().string().substr(9, 12);	// old filname format
	for (auto index = ssBTAddress.length() - 2; index > 0; index -= 2)
		ssBTAddress.insert(index, ":");
	bdaddr_t TheBlueToothAddress;
	str2ba(ssBTAddress.c_str(), &TheBlueToothAddress);
	std::ifstream TheFile(filename);
	if (TheFile.is_open())
	{
		std::vector<std::string> SortableFile;
		std::string TheLine;
		while (std::getline(TheFile, TheLine))
			SortableFile.push_back(TheLine);
		TheFile.close();
		sort(SortableFile.begin(), SortableFile.end());
		for (auto iter = SortableFile.begin(); iter != SortableFile.end(); iter++)
		{
			Govee_Temp TheValue(*iter);
			if (TheValue.IsValid())
				UpdateMRTGData(TheBlueToothAddress, TheValue);
		}
	}
}
// Finds log files specific to this program then reads the contents into the memory mapped structure simulating MRTG log files.
void ReadLoggedData(void)
{
	if (!LogDirectory.empty())
	{
		std::deque<std::filesystem::path> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ LogDirectory })
			if (dir_entry.is_regular_file())
				if (dir_entry.path().filename() != GVHLastDownloadFileName)
					if (dir_entry.path() != SVGTitleMapFilename)
						if (dir_entry.path().extension() == ".txt")
							if (dir_entry.path().stem().string().substr(0, 3) == "gvh")
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
void MonitorLoggedData(void)
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
					if (FileStat.st_mtim.tv_sec > (it->second.begin()->Time + (35 * 60)))	// only read the file if it's at least thirty five minutes more recent than existing data
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
					std::cout << "[" << getTimeISO8601() << "] Reading: " << TitleMapFilename.string() << std::endl;
				else
					std::cerr << "Reading: " << TitleMapFilename.string() << std::endl;
				std::string TheLine;

				static const std::string addressFormat("01:02:03:04:05:06");
				while (std::getline(TheFile, TheLine))
				{
					const std::string delimiters(" \t");
					auto i = TheLine.find_first_of(delimiters);
					if (i == std::string::npos || i != addressFormat.size())
						// No delimited mapping or not the correct length for a Bluetooth address.
						continue;

					std::string theAddress = TheLine.substr(0, i);
					i = TheLine.find_first_not_of(delimiters, i);
					std::string theTitle = (i == std::string::npos) ? "" : TheLine.substr(i);

					bdaddr_t TheBlueToothAddress;
					str2ba(theAddress.c_str(), &TheBlueToothAddress);
					GoveeBluetoothTitles.insert(std::make_pair(TheBlueToothAddress, theTitle));
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
		std::string ssTitle(btAddress);
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
	if (!LogDirectory.empty())
	{
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601() << "] Reading: " << LogDirectory << std::endl;
		std::set<std::string> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ LogDirectory })
			if (dir_entry.is_regular_file())
				if (dir_entry.path() != GVHLastDownloadFileName)
					if (dir_entry.path() != SVGTitleMapFilename)
						if (dir_entry.path().extension() == ".txt")
							if (dir_entry.path().string().substr(0, 3) == "gvh")
							{
								std::string ssBTAddress(dir_entry.path().stem().string().substr(4, 12));
								files.insert(ssBTAddress);
							}
		if (!files.empty())
		{
			std::ofstream SVGIndexFile(SVGIndexFilename);
			if (SVGIndexFile.is_open())
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Writing: " << SVGIndexFilename << std::endl;
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
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Writing:";
				for (auto ssBTAddress = files.begin(); ssBTAddress != files.end(); ssBTAddress++)
				{
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"Graph\" src=\"gvh-" << *ssBTAddress << "-day.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"Graph\" src=\"gvh-" << *ssBTAddress << "-week.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"Graph\" src=\"gvh-" << *ssBTAddress << "-month.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << "\t<div class=\"image\"><img alt=\"Graph\" src=\"gvh-" << *ssBTAddress << "-year.svg\" width=\"500\" height=\"135\"></div>" << std::endl;
					SVGIndexFile << std::endl;
					if (ConsoleVerbosity > 0)
						std::cout << " " << *ssBTAddress;
				}
				if (ConsoleVerbosity > 0)
					std::cout << std::endl;
				SVGIndexFile << "\t</div>" << std::endl;
				SVGIndexFile << "</body>" << std::endl;
				SVGIndexFile << "</html>" << std::endl;
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Done" << std::endl;
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
#ifdef FOO
/* Error codes for Error response PDU */
#define ATT_ECODE_INVALID_HANDLE	0x01
#define ATT_ECODE_READ_NOT_PERM		0x02
#define ATT_ECODE_WRITE_NOT_PERM	0x03
#define ATT_ECODE_INVALID_PDU		0x04
#define ATT_ECODE_AUTHENTICATION	0x05
#define ATT_ECODE_REQ_NOT_SUPP		0x06
#define ATT_ECODE_INVALID_OFFSET	0x07
#define ATT_ECODE_AUTHORIZATION		0x08
#define ATT_ECODE_PREP_QUEUE_FULL	0x09
#define ATT_ECODE_ATTR_NOT_FOUND	0x0A
#define ATT_ECODE_ATTR_NOT_LONG		0x0B
#define ATT_ECODE_INSUFF_ENCR_KEY_SIZE	0x0C
#define ATT_ECODE_INVAL_ATTR_VALUE_LEN	0x0D
#define ATT_ECODE_UNLIKELY			0x0E
#define ATT_ECODE_INSUFF_ENC		0x0F
#define ATT_ECODE_UNSUPP_GRP_TYPE	0x10
#define ATT_ECODE_INSUFF_RESOURCES	0x11
/* Application error */
#define ATT_ECODE_IO				0x80
#define ATT_ECODE_TIMEOUT			0x81
#define ATT_ECODE_ABORTED			0x82
//struct bt_att_pdu_error_rsp {
//	uint8_t opcode;
//	uint16_t handle;
//	uint8_t ecode;
//} __packed;
const char* att_ecode2str(uint8_t status)
{
	switch (status) {
	case ATT_ECODE_INVALID_HANDLE:
		return "Invalid handle";
	case ATT_ECODE_READ_NOT_PERM:
		return "Attribute can't be read";
	case ATT_ECODE_WRITE_NOT_PERM:
		return "Attribute can't be written";
	case ATT_ECODE_INVALID_PDU:
		return "Attribute PDU was invalid";
	case ATT_ECODE_AUTHENTICATION:
		return "Attribute requires authentication before read/write";
	case ATT_ECODE_REQ_NOT_SUPP:
		return "Server doesn't support the request received";
	case ATT_ECODE_INVALID_OFFSET:
		return "Offset past the end of the attribute";
	case ATT_ECODE_AUTHORIZATION:
		return "Attribute requires authorization before read/write";
	case ATT_ECODE_PREP_QUEUE_FULL:
		return "Too many prepare writes have been queued";
	case ATT_ECODE_ATTR_NOT_FOUND:
		return "No attribute found within the given range";
	case ATT_ECODE_ATTR_NOT_LONG:
		return "Attribute can't be read/written using Read Blob Req";
	case ATT_ECODE_INSUFF_ENCR_KEY_SIZE:
		return "Encryption Key Size is insufficient";
	case ATT_ECODE_INVAL_ATTR_VALUE_LEN:
		return "Attribute value length is invalid";
	case ATT_ECODE_UNLIKELY:
		return "Request attribute has encountered an unlikely error";
	case ATT_ECODE_INSUFF_ENC:
		return "Encryption required before read/write";
	case ATT_ECODE_UNSUPP_GRP_TYPE:
		return "Attribute type is not a supported grouping attribute";
	case ATT_ECODE_INSUFF_RESOURCES:
		return "Insufficient Resources to complete the request";
	case ATT_ECODE_IO:
		return "Internal application error: I/O";
	case ATT_ECODE_TIMEOUT:
		return "A timeout occured";
	case ATT_ECODE_ABORTED:
		return "The operation was aborted";
	default:
		return "Unexpected error code";
	}
}
int att_read(int fd, const uint16_t handle, void* buf)
{
	struct 
	{ 
		uint8_t opcode; 
		uint16_t handle; 
	} __attribute__((packed)) pkt = { BT_ATT_OP_READ_REQ, htobs(handle) };
	int result = send(fd, &pkt, sizeof(pkt), 0);
	if (result < 0)
		return result;

	struct 
	{ 
		uint8_t opcode; 
		uint8_t buf[BT_ATT_DEFAULT_LE_MTU]; 
	} __attribute__((packed)) rpkt = { 0 };
	result = recv(fd, &rpkt, sizeof(rpkt), 0);
	if (result < 0)
		return result;
	else if (rpkt.opcode == BT_ATT_OP_ERROR_RSP && result == 1 + sizeof(struct bt_att_pdu_error_rsp)) 
	{
		struct bt_att_pdu_error_rsp* err = (struct bt_att_pdu_error_rsp*)rpkt.buf;
		if (ConsoleVerbosity > 0)
			std::cerr << "ATT error for opcode " << std::hex << std::showbase << err->opcode << ", handle " << btohs(err->handle) << ": " << att_ecode2str(err->ecode) << std::endl;
		return -2;
	}
	else if (rpkt.opcode != BT_ATT_OP_READ_RSP) 
	{
		if (ConsoleVerbosity > 0)
			std::cerr << "Expect ATT READ response opcode (" << std::hex << std::showbase << BT_ATT_OP_READ_RSP << ") but received " << rpkt.opcode << std::endl;
		return -2;
	}
	else 
	{
		int length = result - 1;
		memcpy(buf, rpkt.buf, length);
		return length;
	}
}
int att_write(int fd, const uint16_t handle, const void* buf, const int length)
{
	struct write_packet { uint8_t opcode; uint16_t handle; uint8_t buf[0]; } __attribute__((packed));// pkt;
	if ((sizeof(write_packet) + length) > BT_ATT_DEFAULT_LE_MTU)
		return(-1);
	write_packet* pkt = (write_packet*) new uint8_t[sizeof(write_packet) + length];
	pkt->opcode = BT_ATT_OP_WRITE_CMD;
	pkt->handle = htobs(handle);
	memcpy(pkt->buf, buf, length);
	int result = send(fd, pkt, (sizeof(write_packet) + length), 0);
	delete[] pkt;
	if (result < 0)
		return(result);
	return(length);
}
#endif // FOO
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
	const uint16_t bt_ScanInterval(64);	// Scan Interval: 64 (40 msec) (how long to wait between scans).
	const uint16_t bt_ScanWindow(48);	// Scan Window: 48 (30 msec) (how long to scan)
	const uint8_t bt_ScanFilterDuplicates(0x00);	// Set this once, to make sure I'm consistent through the file.
	// https://development.libelium.com/ble-networking-guide/scanning-ble-devices
	// https://electronics.stackexchange.com/questions/82098/ble-scan-interval-and-window
	// https://par.nsf.gov/servlets/purl/10275622
	// https://e2e.ti.com/support/wireless-connectivity/bluetooth-group/bluetooth/f/bluetooth-forum/616269/cc2640r2f-q1-how-scan-interval-window-work-during-scanning-duration
	// https://www.scirp.org/journal/paperinformation.aspx?paperid=106311
	int btRVal = 0;
	uint8_t bt_ScanFilterPolicy = 0x00; // Scan Filter Policy: Accept all advertisements, except directed advertisements not addressed to this device (0x00)
	if (enable)
	{
		bt_LEScan(BlueToothDevice_Handle, false, BT_WhiteList);
		if (!BT_WhiteList.empty())
		{
			const bdaddr_t TestAddress = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
			if (TestAddress == *BT_WhiteList.begin()) // if first element in whitelist is FFFFFFFFFF
			{
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] BlueTooth Address Filter:";
				else
					std::cerr << "BlueTooth Address Filter:";
				for (auto it = GoveeMRTGLogs.begin(); it != GoveeMRTGLogs.end(); it++)
				{
					const bdaddr_t TheAddress = it->first;
					hci_le_add_white_list(BlueToothDevice_Handle, &TheAddress, LE_PUBLIC_ADDRESS, bt_TimeOut);
					if (ConsoleVerbosity > 0)
						std::cout << " [" << ba2string(TheAddress) << "]";
					else
						std::cerr << " [" << ba2string(TheAddress) << "]";
				}
				if (ConsoleVerbosity > 0)
					std::cout << std::endl;
				else
					std::cerr << std::endl;
			}
			else
			{
				for (auto iter = BT_WhiteList.begin(); iter != BT_WhiteList.end(); iter++)
				{
					bdaddr_t FilterAddress = { 0 };
					FilterAddress = *iter;
					hci_le_add_white_list(BlueToothDevice_Handle, &FilterAddress, LE_PUBLIC_ADDRESS, bt_TimeOut);
				}
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
					std::cout << "[" << getTimeISO8601() << "] Scanning Started. ScanInterval(" << double(bt_ScanInterval)*0.625 << " msec) ScanWindow(" << double(bt_ScanWindow)*0.625 << " msec) ScanType(" << uint(bt_ScanType) << ")" << std::endl;
				else
					std::cerr << ProgramVersionString << " (listening for Bluetooth Low Energy Advertisements) ScanInterval(" << double(bt_ScanInterval) * 0.625 << " msec) ScanWindow(" << double(bt_ScanWindow) * 0.625 << " msec) ScanType(" << uint(bt_ScanType) << ")" << std::endl;
			}
		}
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
			std::cout << "[" << getTimeISO8601() << "] Scanning Stopped." << std::endl;
	}
	return(btRVal);
}
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
		int iRet = hci_le_create_conn(
			BlueToothDevice_Handle,
			96, // interval, Scan Interval: 96 (60 msec)
			48, // window, Scan Window: 48 (30 msec)
			0x00, // initiator_filter, Initiator Filter Policy: Use Peer Address (0x00)
			0x00, // peer_bdaddr_type, Peer Address Type: Public Device Address (0x00)
			GoveeBTAddress, // BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)
			0x01, // own_bdaddr_type, Own Address Type: Random Device Address (0x01)
			24, // min_interval, Connection Interval Min: 24 (30 msec)
			40, // max_interval, Connection Interval Max: 40 (50 msec)
			0, // latency, Connection Latency: 0 (number events)
			2000, // supervision_timeout, Supervision Timeout: 2000 (20 sec)
			0, // min_ce_length, Min CE Length: 0 (0 msec)
			0, // max_ce_length, Max CE Length: 0 (0 msec)
			&handle,
			15000);	// A 15 second timeout gives me a better chance of success
		if (ConsoleVerbosity > 0)
			std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] hci_le_create_conn Return(" << std::dec << iRet << ") handle (" << std::hex << std::setw(4) << std::setfill('0') << handle << ")" << std::endl;
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
					std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "]     Features: " << cp << std::endl;
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
				std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "]      Version: " << lmp_vertostr(ver.lmp_ver) << std::endl;
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
					std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Failed to create L2CAP socket: " << strerror(errno) << " (" << errno << ")" << std::endl;
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
						std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Failed to bind L2CAP socket: " << strerror(errno) << " (" << errno << ")" << std::endl;
					close(l2cap_socket);
				}
				else
				{
					/* Set up destination address */
					struct sockaddr_l2 dstaddr;
					memset(&dstaddr, 0, sizeof(dstaddr));
					dstaddr.l2_family = AF_BLUETOOTH;
					dstaddr.l2_cid = htobs(ATT_CID);
					dstaddr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
					bacpy(&dstaddr.l2_bdaddr, &GoveeBTAddress);
					if (connect(l2cap_socket, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0)
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Failed to connect: " << strerror(errno) << " (" << errno << ")" << std::endl;
						close(l2cap_socket);
					}
					else
					{
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Connected L2CAP LE connection on ATT channel: " << ATT_CID << std::endl;

						unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
						std::vector<BlueToothService> BTServices;
						// First we query the device to get the list of SERVICES.
						// What I end up with is a starting handle, ending handle, and either 16 bit or 128 bit UUID for the service.
						GATT_DeclarationPacket primary_service_declaration = { BT_ATT_OP_READ_BY_GRP_TYPE_REQ, 0x0001, 0xffff, GATT_PRIM_SVC_UUID };
						do {
							if (ConsoleVerbosity > 1)
								std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Group Type Request, GATT Primary Service Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << primary_service_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << primary_service_declaration.ending_handle << std::endl;
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
													std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->starting_handle << "..0x" << std::setw(4) << std::setfill('0') << attribute_data->ending_handle << " UUID: " << bt_UUID_2_String(&theUUID) << std::endl;
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
													std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->starting_handle << "..0x" << std::setw(4) << std::setfill('0') << attribute_data->ending_handle << " UUID: " << bt_UUID_2_String(&theUUID) << std::endl;
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
							std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_GRP_TYPE_REQ GATT_PRIM_SVC_UUID BT_ATT_OP_ERROR_RSP" << std::endl;

						// Next I go through my stored set of SERVICES requesting CHARACTERISTICS based on the combination of starting handle and ending handle
						for (auto bts = BTServices.begin(); bts != BTServices.end(); bts++)
						{
							GATT_DeclarationPacket gatt_include_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, bts->starting_handle, bts->ending_handle, GATT_INCLUDE_UUID };
							do {
								if (ConsoleVerbosity > 1)
									std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Type Request, GATT Include Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_include_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_include_declaration.ending_handle << std::endl;
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
								std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_INCLUDE_UUID BT_ATT_OP_ERROR_RSP" << std::endl;

							GATT_DeclarationPacket gatt_characteristic_declaration = { BT_ATT_OP_READ_BY_TYPE_REQ, bts->starting_handle, bts->ending_handle, GATT_CHARAC_UUID };
							do {
								if (ConsoleVerbosity > 1)
									std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> Read By Type Request, GATT Characteristic Declaration, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_characteristic_declaration.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_characteristic_declaration.ending_handle << std::endl;
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
													std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << Characteristic.starting_handle;
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
								std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_READ_BY_TYPE_REQ GATT_CHARAC_UUID BT_ATT_OP_ERROR_RSP" << std::endl;
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
										std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> Find Information Request, Handles: 0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_information.starting_handle << "..0x" << std::hex << std::setw(4) << std::setfill('0') << gatt_information.ending_handle << std::endl;
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
														std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== Handle: 0x" << std::hex << std::setw(4) << std::setfill('0') << attribute_data->handle;
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
										std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> BT_ATT_OP_WRITE_REQ Handle: ";
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
													std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_WRITE_RSP" << std::endl;
											}
											else if (buf[0] == BT_ATT_OP_ERROR_RSP)
											{
												struct __attribute__((__packed__)) bt_error { uint8_t opcode; uint8_t req_opcode; uint16_t handle; uint8_t errcode; } * result = (bt_error*) & (buf[0]);
												if (ConsoleVerbosity > 1)
												{
													std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_ERROR_RSP";
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
									std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] ==> BT_ATT_OP_WRITE_REQ Handle: ";
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
											std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_WRITE_RSP" << std::endl;
									}
									else if (buf[0] == BT_ATT_OP_HANDLE_VAL_NOT)
									{
										struct __attribute__((__packed__)) bt_handle_value { uint8_t opcode;  uint16_t handle; uint8_t value[20]; } *data = (bt_handle_value*)&(buf[0]);
										if (ConsoleVerbosity > 1)
										{
											std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] <== BT_ATT_OP_HANDLE_VAL_NOT";
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
										std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Reading from device. RetryCount = " << std::dec << RetryCount << std::endl;
									usleep(100000); // 1,000,000 = 1 second.
									if (--RetryCount < 0)
										bDownloadInProgress = false;
								}
							}
						}
						if (ConsoleVerbosity > 0)
							std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Closing l2cap_socket" << std::endl;
						close(l2cap_socket);
					}
				}
			}
		}
		if (handle != 0)
		{
			hci_disconnect(BlueToothDevice_Handle, handle, HCI_OE_USER_ENDED_CONNECTION, 2000);
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] hci_disconnect" << std::endl;
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
			std::cout << "[" << getTimeISO8601() << "] [" << ba2string(GoveeBTAddress) << "] Download from device. " << timeToExcelLocal(TimeStart) << " " << timeToExcelLocal(TimeStop) << " (" << std::dec << DataPointsRecieved << ")" << std::endl;
		else
			std::cerr << "Download from device: [" << ba2string(GoveeBTAddress) << "] " << timeToExcelLocal(TimeStart) << " " << timeToExcelLocal(TimeStop) << " (" << std::dec << DataPointsRecieved << ")" << std::endl;
		TimeDownloadStart -= static_cast<long>(60) * offset;
	}
	return(TimeDownloadStart);
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
	std::cout << "    -s | --svg name      SVG output directory [" << SVGDirectory << "]" << std::endl;
	std::cout << "    -i | --index name    HTML index file for SVG files" << std::endl;
	std::cout << "    -T | --titlemap name SVG title fully qualified filename [" << SVGTitleMapFilename << "]" << std::endl;
	std::cout << "    -c | --celsius       SVG output using degrees C [" << std::boolalpha << !SVGFahrenheit << "]" << std::endl;
	std::cout << "    -b | --battery graph Draw the battery status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly" << std::endl;
	std::cout << "    -x | --minmax graph  Draw the minimum and maximum temperature and humidity status on SVG graphs. 1:daily, 2:weekly, 4:monthly, 8:yearly" << std::endl;
	std::cout << "    -d | --download      Periodically attempt to connect and download stored data" << std::endl;
	std::cout << "    -p | --passive       Bluetooth LE Passive Scanning" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:t:v:m:o:C:a:s:i:T:cb:x:dp";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "time",   required_argument, NULL, 't' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "mrtg",	required_argument, NULL, 'm' },
		{ "only",	required_argument, NULL, 'o' },
		{ "controller", required_argument, NULL, 'C' },
		{ "average",required_argument, NULL, 'a' },
		{ "svg",	required_argument, NULL, 's' },
		{ "index",	required_argument, NULL, 'i' },
		{ "titlemap",required_argument,NULL, 'T' },
		{ "celsius",no_argument,       NULL, 'c' },
		{ "battery",required_argument, NULL, 'b' },
		{ "minmax",	required_argument, NULL, 'x' },
		{ "download",no_argument,      NULL, 'd' },
		{ "passive",no_argument,       NULL, 'p' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////
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
		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'l':
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				LogDirectory = TempPath;
			break;
		case 't':
			try { LogFileTime = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'v':
			try { ConsoleVerbosity = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'm':
			MRTGAddress = std::string(optarg);
			break;
		case 'o':
			if (0 == str2ba(optarg, &OnlyFilterAddress))
				BT_WhiteList.insert(OnlyFilterAddress);
			break;
		case 'C':
			ControllerAddress = std::string(optarg);
			break;
		case 'a':
			try { MinutesAverage = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'd':
			DaysBetweenDataDownload = 14;
			break;
		case 'p':
			bt_ScanType = 0;
			break;
		case 's':
			TempPath = std::string(optarg);
			while (TempPath.filename().empty() && (TempPath != TempPath.root_directory())) // This gets rid of the "/" on the end of the path
				TempPath = TempPath.parent_path();
			if (ValidateDirectory(TempPath))
				SVGDirectory = TempPath;
			break;
		case 'i':
			TempPath = std::string(optarg);
			SVGIndexFilename = TempPath;
			break;
		case 'T':
			TempPath = std::string(optarg);
			if (ReadTitleMap(TempPath))
				SVGTitleMapFilename = TempPath;
			break;
		case 'c':
			SVGFahrenheit = false;
			break;
		case 'b':
			try { SVGBattery = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'x':
			try { SVGMinMax = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
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
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeISO8601() << "] " << ProgramVersionString << std::endl;
		if (ConsoleVerbosity > 2)
		{
			std::cout << "[                   ]      log: " << LogDirectory << std::endl;
			std::cout << "[                   ]      svg: " << SVGDirectory << std::endl;
			std::cout << "[                   ]  battery: " << SVGBattery << std::endl;
			std::cout << "[                   ]   minmax: " << SVGMinMax << std::endl;
			std::cout << "[                   ]  celsius: " << std::boolalpha << !SVGFahrenheit << std::endl;
			std::cout << "[                   ] titlemap: " << SVGTitleMapFilename << std::endl;
			std::cout << "[                   ]     time: " << LogFileTime << std::endl;
			std::cout << "[                   ]  average: " << MinutesAverage << std::endl;
			std::cout << "[                   ] download: " << DaysBetweenDataDownload << std::endl;
		}
	}
	else
		std::cerr << ProgramVersionString << " (starting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (!SVGDirectory.empty())
	{
		if (SVGTitleMapFilename.empty()) // If this wasn't set as a parameter, look in the SVG Directory for a default titlemap
		{
			std::ostringstream TitleMapFilename;
			TitleMapFilename << SVGDirectory;
			TitleMapFilename << "/gvh-titlemap.txt";
			SVGTitleMapFilename = TitleMapFilename.str();
		}
		ReadTitleMap(SVGTitleMapFilename);
		ReadLoggedData();
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
				std::cout << "[" << getTimeISO8601() << "] Reading: " << filename.string() << std::endl;
			else
				std::cerr << "Reading: " << filename.string() << std::endl;
			std::string TheLine;
			while (std::getline(TheFile, TheLine))
			{
				// rudimentary line checking. It's at least as long as the BT Address and has a Tab character
				if ((TheLine.size() > 18) &&
					(TheLine.find("\t") != std::string::npos))
				{
					char buffer[256];
					if (TheLine.size() < sizeof(buffer))
					{
						TheLine.copy(buffer, TheLine.size());
						buffer[TheLine.size()] = '\0';
						std::string theAddress(strtok(buffer, "\t"));
						std::string theDate(strtok(NULL, "\t"));
						bdaddr_t TheBlueToothAddress({0});
						str2ba(theAddress.c_str(), &TheBlueToothAddress);
						GoveeLastDownload.insert(std::make_pair(TheBlueToothAddress, ISO8601totime(theDate)));
					}
				}
			}
			TheFile.close();
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	int BlueToothDevice_ID;
	if (ControllerAddress.empty())
		BlueToothDevice_ID = hci_get_route(NULL);
	else
		BlueToothDevice_ID = hci_devid(ControllerAddress.c_str());
	if (BlueToothDevice_ID < 0)
		std::cerr << "[                   ] Error: Bluetooth device not found" << std::endl;
	else
	{
		// Set up CTR-C signal handler
		typedef void(*SignalHandlerPointer)(int);
		SignalHandlerPointer previousHandlerSIGINT = signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
		SignalHandlerPointer previousHandlerSIGHUP = signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler

		// 2022-12-26: I came across information tha signal() is bad and I shoudl be using sigaction() instead
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
			int on = 1; // Nonblocking on = 1, off = 0;
			if (ioctl(BlueToothDevice_Handle, FIONBIO, (char*)&on) < 0)
				std::cerr << "[                   ] Error: Could set device to non-blocking: " << strerror(errno) << std::endl;
			else
			{
				char LocalName[HCI_MAX_NAME_LENGTH] = { 0 };
				hci_read_local_name(BlueToothDevice_Handle, sizeof(LocalName), LocalName, bt_TimeOut);

				if (ConsoleVerbosity > 0)
				{
					if (!ControllerAddress.empty())
						std::cout << "[" << getTimeISO8601() << "] Controller Address: " << ControllerAddress << std::endl;
					std::cout << "[" << getTimeISO8601() << "] LocalName: " << LocalName << std::endl;
					if (BT_WhiteList.empty())
						std::cout << "[" << getTimeISO8601() << "] No BlueTooth Address Filter" << std::endl;
					else
					{
						std::cout << "[" << getTimeISO8601() << "] BlueTooth Address Filter:";
						for (auto iter = BT_WhiteList.begin(); iter != BT_WhiteList.end(); iter++)
							std::cout << " [" << ba2string(*iter) << "]";
						std::cout << std::endl;
					}
				}
				auto btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
				if (btRVal >= 0)
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
												std::cout << "[" << getTimeISO8601() << "] Read: " << std::dec << bufDataLen << " Bytes" << std::endl;
											std::ostringstream ConsoleOutLine;
											ConsoleOutLine << "[" << getTimeISO8601() << "]" << std::setw(3) << bufDataLen;

											// At this point I should have an HCI Event in buf (hci_event_hdr)
											evt_le_meta_event* meta = (evt_le_meta_event*)(buf + (HCI_EVENT_HDR_SIZE + 1));
											if (meta->subevent == EVT_LE_ADVERTISING_REPORT)
											{
												time(&TimeAdvertisment);
												const le_advertising_info* const info = (le_advertising_info*)(meta->data + 1);
												bool AddressInGoveeSet = (GoveeTemperatures.end() != GoveeTemperatures.find(info->bdaddr));
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
																std::cout << "[" << getTimeISO8601() << "] EIR data length is longer than EIR packet length. " << data_len << " + 1 > " << info->length << std::endl;
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
																	if (localTemp.ReadMSG((info->data + current_offset)))
																	{
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
																		switch (localTemp.GetModel())
																		{
																		case ThermometerType::H5074:
																			ConsoleOutLine << " (GVH5074)";
																			break;
																		case ThermometerType::H5075:
																			ConsoleOutLine << " (GVH5075)";
																			break;
																		case ThermometerType::H5174:
																			ConsoleOutLine << " (GVH5174)";
																			break;
																		case ThermometerType::H5177:
																			ConsoleOutLine << " (GVH5177)";
																			break;
																		case ThermometerType::H5179:
																			ConsoleOutLine << " (GVH5179)";
																			break;
																		case ThermometerType::H5183:
																			ConsoleOutLine << " (GVH5183)";
																			break;
																		case ThermometerType::H5182:
																			ConsoleOutLine << " (GVH5182)";
																			break;
																		case ThermometerType::H5181:
																			ConsoleOutLine << " (GVH5181)";
																			break;
																		default:
																			ConsoleOutLine << " (ThermometerType::Unknown)";
																		}
																		std::queue<Govee_Temp> foo;
																		auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
																		ret.first->second.push(localTemp);
																		AddressInGoveeSet = true;
																		UpdateMRTGData(info->bdaddr, localTemp);
																		GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, 0));
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
												if ((DaysBetweenDataDownload > 0) && AddressInGoveeSet && !LogDirectory.empty())
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
														bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
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
										std::cout << "[" << getTimeISO8601() << "] " << std::dec << DAY_SAMPLE << " seconds or more have passed. Writing SVG Files" << std::endl;
									TimeSVG = (TimeNow / DAY_SAMPLE) * DAY_SAMPLE; // hack to try to line up TimeSVG to be on a five minute period
									WriteAllSVG();
								}
								if (difftime(TimeNow, TimeStart) > LogFileTime)
								{
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601() << "] " << std::dec << LogFileTime << " seconds or more have passed. Writing LOG Files" << std::endl;
									TimeStart = TimeNow;
									GenerateLogFile(GoveeTemperatures, GoveeLastDownload);
									MonitorLoggedData();
								}
								if (difftime(TimeNow, TimeAdvertisment) > MaxMinutesBetweenBluetoothAdvertisments * 60) // Hack to force scanning restart regularly
								{
									if (ConsoleVerbosity > 0)
										std::cout << "[" << getTimeISO8601() << "] No recent Bluetooth LE Advertisments! (> " << MaxMinutesBetweenBluetoothAdvertisments << " Minutes)" << std::endl;
									btRVal = bt_LEScan(BlueToothDevice_Handle, true, BT_WhiteList);
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
		signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
		signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler

		GenerateLogFile(GoveeTemperatures, GoveeLastDownload); // flush contents of accumulated map to logfiles

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
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cerr << ProgramVersionString << " (exiting)" << std::endl;
	return(EXIT_SUCCESS);
}
