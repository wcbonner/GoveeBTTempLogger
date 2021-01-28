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

#include <cstdio>
#include <ctime>
#include <csignal>
#include <cmath>
#include <climits>
#include <iostream>
#include <locale>
#include <queue>
#include <map>
#include <vector>
#include <algorithm>
#include <locale>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <netdb.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>


/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogger Version 2.20210127-1 Built on: " __DATE__ " at " __TIME__);
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t & TheTime)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
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
std::string getTimeISO8601(void)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToISO8601(timer));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());

	return(rval);
}
time_t ISO8601totime(const std::string & ISOTime)
{
	struct tm UTC;
	UTC.tm_year = stol(ISOTime.substr(0, 4)) - 1900;
	UTC.tm_mon = stol(ISOTime.substr(5, 2)) - 1;
	UTC.tm_mday = stol(ISOTime.substr(8, 2));
	UTC.tm_hour = stol(ISOTime.substr(11, 2));
	UTC.tm_min = stol(ISOTime.substr(14, 2));
	UTC.tm_sec = stol(ISOTime.substr(17, 2));
#ifdef _MSC_VER
	_tzset();
	_get_daylight(&(UTC.tm_isdst));
#endif
	time_t timer = mktime(&UTC);
#ifdef _MSC_VER
	long Timezone_seconds = 0;
	_get_timezone(&Timezone_seconds);
	timer -= Timezone_seconds;
	int DST_hours = 0;
	_get_daylight(&DST_hours);
	long DST_seconds = 0;
	_get_dstbias(&DST_seconds);
	timer += DST_hours * DST_seconds;
#else
	timer -= timezone; // HACK: Works in my initial testing on the raspberry pi, but it's currently not DST
#endif
	return(timer);
}
// Microsoft Excel doesn't recognize ISO8601 format dates with the "T" seperating the date and time
// This function puts a space where the T goes for ISO8601. The dates can be decoded with ISO8601totime()
std::string timeToExcelDate(const time_t & TheTime) 
{
	std::ostringstream ExcelDate;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
	{
		ExcelDate.fill('0');
		ExcelDate << UTC.tm_year + 1900 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mon + 1 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mday << " ";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_hour << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_min << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_sec;
	}
	return(ExcelDate.str());
}
std::string timeToExcelLocal(const time_t& TheTime)
{
	std::ostringstream ExcelDate;
	struct tm UTC;
	if (0 != localtime_r(&TheTime, &UTC))
	{
		ExcelDate.fill('0');
		ExcelDate << UTC.tm_year + 1900 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mon + 1 << "-";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_mday << " ";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_hour << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_min << ":";
		ExcelDate.width(2);
		ExcelDate << UTC.tm_sec;
	}
	return(ExcelDate.str());
}
/////////////////////////////////////////////////////////////////////////////
// Class I'm using for storing raw data from the Govee thermometers
class  Govee_Temp {
public:
	time_t Time;
	std::string WriteTXT(const char seperator = '\t') const;
	bool ReadMSG(const uint8_t * const data);
	Govee_Temp() : Time(0), Temperature(0), Humidity(0), Battery(INT_MAX), Averages(0) { };
	Govee_Temp(const time_t tim, const double tem, const double hum, const int bat)
	{
		Time = tim;
		Temperature = tem;
		Humidity = hum;
		Battery = bat;
		Averages = 1;
	};
	double GetTemperature(const bool Fahrenheit = false) const { if (Fahrenheit) return((Temperature * 9.0 / 5.0) + 32.0); return(Temperature); };
	double GetHumidity(void) const { return(Humidity); };
	double GetBattery(void) const { return(Battery); };
	friend Govee_Temp Average(const Govee_Temp &a, const Govee_Temp &b);
	friend Govee_Temp Average(std::vector<Govee_Temp>::iterator first, std::vector<Govee_Temp>::iterator last);
protected:
	double Temperature;
	double Humidity;
	int Battery;
	int Averages;
};
std::string Govee_Temp::WriteTXT(const char seperator) const
{
	std::ostringstream ssValue;
	ssValue << timeToExcelDate(Time);
	ssValue << seperator << Temperature;
	ssValue << seperator << Humidity;
	ssValue << seperator << Battery;
	return(ssValue.str());
}
bool Govee_Temp::ReadMSG(const uint8_t * const data)
{
	bool rval = false;
	const size_t data_len = data[0];
	if (data[1] == 0xFF) // https://www.bluetooth.com/specifications/assigned-numbers/generic-access-profile/ «Manufacturer Specific Data»
	{
		if ((data_len == 9) && (data[2] == 0x88) && (data[3] == 0xEC)) // GVH5075_xxxx
		{
			// This data came from https://github.com/Thrilleratplay/GoveeWatcher
			// 88ec00 03519e 64 00 Temp: 21.7502°C Temp: 71.1504°F Humidity: 50.2%
			// 2 3 4  5 6 7  8
			int iTemp = int(data[5]) << 16 | int(data[6]) << 8 | int(data[7]);
			bool bNegative = iTemp & 0x800000;	// check sign bit
			iTemp = iTemp & 0x7ffff;			// mask off sign bit
			Temperature = float(iTemp) / 10000.0;
			if (bNegative)						// apply sign bit
				Temperature = -1.0 * Temperature;
			Humidity = float(iTemp % 1000) / 10.0;
			Battery = int(data[8]);
			Averages = 1;
			time(&Time);
			rval = true;
		}
		else if ((data_len == 10) && (data[2] == 0x88) && (data[3] == 0xEC))// Govee_H5074_xxxx
		{
			// This data came from https://github.com/neilsheps/GoveeTemperatureAndHumidity
			// 88EC00 0902 CD15 64 02 (Temp) 41.378°F (Humidity) 55.81% (Battery) 100%
			// 2 3 4  5 6  7 8  9
			short iTemp = short(data[6]) << 8 | short(data[5]);
			int iHumidity = int(data[8]) << 8 | int(data[7]);
			Temperature = float(iTemp) / 100.0;
			Humidity = float(iHumidity) / 100.0;
			Battery = int(data[9]);
			Averages = 1;
			time(&Time);
			rval = true;
		}
		else if ((data_len == 9) && (data[2] == 0x01) && (data[3] == 0x00)) // GVH5177_xxxx
		{
			// This is a guess based on the H5075 3 byte encoding
			// 01000101 029D1B 64 (Temp) 62.8324°F (Humidity) 29.1% (Battery) 100%
			// 2 3 4 5  6 7 8  9
			int iTemp = int(data[6]) << 16 | int(data[7]) << 8 | int(data[8]);
			bool bNegative = iTemp & 0x800000;	// check sign bit
			iTemp = iTemp & 0x7ffff;			// mask off sign bit
			Temperature = float(iTemp) / 10000.0;
			Humidity = float(iTemp % 1000) / 10.0;
			if (bNegative)						// apply sign bit
				Temperature = -1.0 * Temperature;
			Battery = int(data[9]);
			Averages = 1;
			time(&Time);
			rval = true;
		}
	}
	return(rval);
}
Govee_Temp Average(const Govee_Temp &a, const Govee_Temp &b)
{
	Govee_Temp rval;
	rval.Time = a.Time < b.Time ? a.Time : b.Time; // Use the minimum time (oldest time)
	rval.Battery = a.Battery < b.Battery ? a.Battery : b.Battery; // use the minimum battery
	rval.Averages = a.Averages + b.Averages; // existing average + new average
	rval.Temperature = ((a.Temperature * a.Averages) + (b.Temperature * b.Averages)) / rval.Averages;
	rval.Humidity = ((a.Humidity * a.Averages) + (b.Humidity * b.Averages)) / rval.Averages;
	rval = b; // HACK: Averaging still needs work, just use last value passed
	return(rval);
}
Govee_Temp Average(std::vector<Govee_Temp>::iterator first, std::vector<Govee_Temp>::iterator last)
{
	Govee_Temp rval(*last);
	while (first < last)
	{
		last--;
		rval.Time = rval.Time > last->Time ? rval.Time : last->Time;
		rval.Temperature += last->Temperature;
		rval.Humidity += last->Humidity;
		rval.Battery += last->Battery;
		rval.Averages++;
	}
	double NumElements = double(rval.Averages);
	rval.Temperature /= NumElements;
	rval.Humidity /= NumElements;
	rval.Battery /= int(NumElements);
	return(rval);
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
/////////////////////////////////////////////////////////////////////////////
std::map<bdaddr_t, std::queue<Govee_Temp>> GoveeTemperatures;
std::map<bdaddr_t, time_t> GoveeLastDownload;
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
int ConsoleVerbosity = 1;
std::string LogDirectory("./");
std::string SVGDirectory;	// If this remains empty, SVG Files are not created. If it's specified, _day, _week, _month, and _year.svg files are created for each bluetooth address seen.
// The following details were taken from https://github.com/oetiker/mrtg
const size_t DAY_COUNT = 600;			/* 400 samples is 33.33 hours */
const size_t WEEK_COUNT = 600;			/* 400 samples is 8.33 days */
const size_t MONTH_COUNT = 600;			/* 400 samples is 33.33 days */
const size_t YEAR_COUNT = 2 * 366;		/* 1 sample / day, 366 days, 2 years */
const size_t DAY_SAMPLE = 5 * 60;		/* Sample every 5 minutes */
const size_t WEEK_SAMPLE = 30 * 60;		/* Sample every 30 minutes */
const size_t MONTH_SAMPLE = 2 * 60 * 60;/* Sample every 2 hours */
const size_t YEAR_SAMPLE = 24 * 60 * 60;/* Sample every 24 hours */
// One 'rounding error' per sample period, so add 4 to total and for good mesure we take 10 :-)
const size_t MAX_HISTORY = DAY_COUNT + WEEK_COUNT + MONTH_COUNT + YEAR_COUNT + 10;
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(std::string& DirectoryName)
{
	//TODO: I want to make sure the dorectory name ends with a "/"
	if (DirectoryName.back() != '/')
		DirectoryName += '/';
	//TODO: I want to make sure the dorectory exists
	//TODO: I want to make sure the dorectory is writable by the current user
	return(true);
}
std::string GenerateLogFileName(const bdaddr_t &a)
{
	std::ostringstream OutputFilename;
	if (LogDirectory.back() != '/')
		LogDirectory.push_back('/');
	OutputFilename << LogDirectory;
	OutputFilename << "gvh507x_";
	OutputFilename << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(a.b[1]);
	OutputFilename << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(a.b[0]);
	time_t timer;
	time(&timer);
	struct tm UTC;
	if (0 != gmtime_r(&timer, &UTC))
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
			OutputFilename << "-" << std::dec << UTC.tm_year + 1900 << "-" << std::setw(2) << std::setfill('0') << UTC.tm_mon + 1;
	OutputFilename << ".txt";
	std::string OldFormatFileName(OutputFilename.str());

	// The New Format Log File Name includes the entire Bluetooth Address, making it much easier to recognize and add to MRTG config files.
	OutputFilename.str("");
	OutputFilename << LogDirectory;
	OutputFilename << "gvh507x_";
	char addr[19] = { 0 };
	ba2str(&a, addr);
	std::string btAddress(addr);
	for (auto pos = btAddress.find(':'); pos != std::string::npos; pos = btAddress.find(':'))
		btAddress.erase(pos, 1);
	OutputFilename << btAddress;
	if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
		OutputFilename << "-" << std::dec << UTC.tm_year + 1900 << "-" << std::setw(2) << std::setfill('0') << UTC.tm_mon + 1;
	OutputFilename << ".txt";
	std::string NewFormatFileName(OutputFilename.str());

	// This is a temporary hack to transparently change log file name formats
	std::ifstream OldFile(OldFormatFileName);
	if (OldFile.is_open())
	{
		OldFile.close();
		if (rename(OldFormatFileName.c_str(), NewFormatFileName.c_str()) == 0)
			std::cerr << "[                   ] Renamed " << OldFormatFileName << " to " << NewFormatFileName << std::endl;
		else 
			std::cerr << "[                   ] Unable to Rename " << OldFormatFileName << " to " << NewFormatFileName << std::endl;
	}

	return(NewFormatFileName);
}
bool GenerateLogFile(std::map<bdaddr_t, std::queue<Govee_Temp>> &AddressTemperatureMap)
{
	bool rval = false;
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
				Govee_Temp TheValue(ISO8601totime(theDate), atof(theTemp.c_str()), atof(theHumidity.c_str()), atol(theBattery.c_str()));
				if ((Minutes == 0) && LogValues.empty()) // HACK: Special Case to always accept the last logged value
					LogValues.push(TheValue);
				if ((Minutes * 60.0) < difftime(now, TheValue.Time))	// If this entry is more than Minutes parameter from current time, it's time to stop reading log file.
					break;
				LogValues.push(TheValue);
			}
		} while (TheFile.tellg() > 0);	// If we are at the beginning of the file, there's nothing more to do
		TheFile.close();
		while (!LogValues.empty())
		{
			OutValue.Time = OutValue.Time > LogValues.front().Time ? OutValue.Time : LogValues.front().Time;
			OutValue = Average(LogValues.front(), OutValue);
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
void ReadMRTGData(const std::string& MRTGLogFileName, std::vector<Govee_Temp>& TheValues, const GraphType graph = GraphType::daily)
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
void ReadMRTGData(const bdaddr_t& TheAddress, std::vector<Govee_Temp>& TheValues, const GraphType graph = GraphType::daily)
{
	auto it = GoveeMRTGLogs.find(TheAddress);
	if (it != GoveeMRTGLogs.end())
	{
		if (it->second.size() > 0)
		{
			TheValues.resize(it->second.size());
			std::copy(it->second.begin(), it->second.end(), TheValues.begin());
			if (graph == GraphType::daily)
			{
				TheValues.erase(TheValues.begin()); // get rid of the first element
				TheValues.resize(DAY_COUNT); // get rid of anything beyond the five minute data
			}
			else if (graph == GraphType::weekly)
			{
				TheValues.erase(TheValues.begin()); // get rid of the first element
				std::vector<Govee_Temp> TempValues;
				for (auto iter = TheValues.begin(); iter != TheValues.end(); iter++)
				{
					struct tm UTC;
					if (0 != localtime_r(&iter->Time, &UTC))
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
					if (0 != localtime_r(&iter->Time, &UTC))
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
					if (0 != localtime_r(&iter->Time, &UTC))
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
}
void WriteMRTGSVG(std::vector<Govee_Temp>& TheValues, const std::string& SVGFileName, const std::string& Title = "", const GraphType graph = GraphType::daily, const bool Fahrenheit = true)
{
	// By declaring these items here, I'm then basing all my other dimensions on these
	const int SVGWidth = 500;
	const int SVGHeight = 135;
	const int FontSize = 12;
	const int TickSize = 2;
	const int GraphWidth = SVGWidth - (FontSize * 7);
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
					std::cout << "[" << getTimeISO8601() << "] Writing: " << SVGFileName << " With Title: " << Title << std::endl;
				else
					std::cerr << "Writing: " << SVGFileName << " With Title: " << Title << std::endl;
				std::ostringstream tempOString;
				tempOString << "Temperature (" << std::fixed << std::setprecision(1) << TheValues[0].GetTemperature(Fahrenheit) << (Fahrenheit ? "°F)" : "°C)");
				std::string YLegendLeft(tempOString.str());
				tempOString = std::ostringstream();
				tempOString << "Humidity (" << std::fixed << std::setprecision(1) << TheValues[0].GetHumidity() << "%)";
				std::string YLegendRight(tempOString.str());
				int GraphTop = FontSize + TickSize;
				int GraphBottom = SVGHeight - GraphTop;
				int GraphRight = SVGWidth - (GraphTop * 2) - 2;
				int GraphLeft = GraphRight - GraphWidth;
				int GraphVerticalDivision = (GraphBottom - GraphTop) / 4;
				double TempMin = 100;
				double TempMax = -100;
				double HumiMin = 100;
				double HumiMax = -100;
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

				// MRTG Log File has 602 rows of five minute intervals, 602 rows 30 minute intervals, 602 rows of two hour intervals

				SVGFile << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>" << std::endl;
				SVGFile << "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\">" << std::endl;
				SVGFile << "\t<!-- Created by: " << ProgramVersionString << " -->" << std::endl;
				SVGFile << "\t<style>" << std::endl;
				SVGFile << "\t\ttext {" << std::endl;
				SVGFile << "\t\t\tfont-family: Consolas;" << std::endl;
				SVGFile << "\t\t\tfont-size: " << FontSize << "px;" << std::endl;
				SVGFile << "\t\t}" << std::endl;
				SVGFile << "\t\tline {" << std::endl;
				SVGFile << "\t\t\tstroke: black;" << std::endl;
				SVGFile << "\t\t}" << std::endl;
				SVGFile << "\t</style>" << std::endl;
				SVGFile << "\t<rect width=\"" << SVGWidth << "\" height=\"" << SVGHeight << "\" stroke=\"grey\" stroke-width=\"2\" fill-opacity=\"0\" />" << std::endl;

				// Legend Text
				SVGFile << "\t<text x=\"" << GraphLeft << "\" y=\"" << GraphTop - 2 << "\">" << Title << "</text>" << std::endl;
				SVGFile << "\t<text fill=\"blue\" text-anchor=\"middle\" x=\"" << FontSize << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendLeft << "</text>" << std::endl;
				SVGFile << "\t<text fill=\"green\" text-anchor=\"middle\" x=\"" << FontSize * 2 << "\" y=\"" << (GraphTop + GraphBottom) / 2 << "\" transform=\"rotate(270 " << FontSize * 2 << "," << (GraphTop + GraphBottom) / 2 << ")\">" << YLegendRight << "</text>" << std::endl;
				SVGFile << "\t<text text-anchor=\"end\" x=\"" << GraphRight << "\" y=\"" << GraphTop - 2 << "\">" << timeToExcelLocal(TheValues[0].Time) << "</text>" << std::endl;

				// Humidity Graphic as a Filled polygon
				SVGFile << "\t<polygon style=\"fill:lime;stroke:green\" points=\"";
				SVGFile << GraphLeft + 1 << "," << GraphBottom - 1 << " ";
				for (auto index = 0; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					SVGFile << index + GraphLeft << "," << int(((HumiMax - TheValues[index].GetHumidity()) * HumiVerticalFactor) + GraphTop) << " ";
				if (GraphWidth < TheValues.size())
					SVGFile << GraphRight - 1 << "," << GraphBottom - 1;
				else
					SVGFile << GraphRight - (GraphWidth - TheValues.size()) << "," << GraphBottom - 1;
				SVGFile << "\" />" << std::endl;

				// Top Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop << "\"/>" << std::endl;
				SVGFile << "\t<text fill=\"blue\" text-anchor=\"end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 5 << "\">" << std::fixed << std::setprecision(1) << TempMax << "</text>" << std::endl;
				SVGFile << "\t<text fill=\"green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop + 4 << "\">" << std::fixed << std::setprecision(1) << HumiMax << "</text>" << std::endl;

				// Bottom Line
				SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphBottom << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;
				SVGFile << "\t<text fill=\"blue\" text-anchor=\"end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphBottom + 5 << "\">" << std::fixed << std::setprecision(1) << TempMin << "</text>" << std::endl;
				SVGFile << "\t<text fill=\"green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphBottom + 4 << "\">" << std::fixed << std::setprecision(1) << HumiMin << "</text>" << std::endl;

				// Left Line
				SVGFile << "\t<line x1=\"" << GraphLeft << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Right Line
				SVGFile << "\t<line x1=\"" << GraphRight << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphRight << "\" y2=\"" << GraphBottom << "\"/>" << std::endl;

				// Vertical Division Dashed Lines
				for (auto index = 1; index < 4; index++)
				{
					SVGFile << "\t<line x1=\"" << GraphLeft - TickSize << "\" y1=\"" << GraphTop + (GraphVerticalDivision * index) << "\" x2=\"" << GraphRight + TickSize << "\" y2=\"" << GraphTop + (GraphVerticalDivision * index) << "\" stroke-dasharray=\"1,1\" />" << std::endl;
					SVGFile << "\t<text fill=\"blue\" text-anchor=\"end\" x=\"" << GraphLeft - TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << TempMax - (TempVerticalDivision * index) << "</text>" << std::endl;
					SVGFile << "\t<text fill=\"green\" x=\"" << GraphRight + TickSize << "\" y=\"" << GraphTop + 4 + (GraphVerticalDivision * index) << "\">" << std::fixed << std::setprecision(1) << HumiMax - (HumiVerticalDivision * index) << "</text>" << std::endl;
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
									SVGFile << "\t<line stroke-dasharray=\"1,1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								if (UTC.tm_hour % 2 == 0)
									SVGFile << "\t<text text-anchor=\"middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << UTC.tm_hour << "</text>" << std::endl;
							}
						}
						else if (graph == GraphType::weekly)
						{
							const std::string Weekday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
							if ((UTC.tm_hour == 0) && (UTC.tm_min == 0))
							{
								if (UTC.tm_wday == 1)
									SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
								else
									SVGFile << "\t<line stroke-dasharray=\"1,1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							}
							else if ((UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text text-anchor=\"middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Weekday[UTC.tm_wday] << "</text>" << std::endl;
						}
						else if (graph == GraphType::monthly)
						{
							if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							if ((UTC.tm_wday == 0) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line stroke-dasharray=\"1,1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_wday == 3) && (UTC.tm_hour == 12) && (UTC.tm_min == 0))
								SVGFile << "\t<text text-anchor=\"middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">Week " << UTC.tm_yday / 7 + 1 << "</text>" << std::endl;
						}
						else if (graph == GraphType::yearly)
						{
							const std::string Month[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
							if ((UTC.tm_yday == 0) && (UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line style=\"stroke:red\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 1) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<line stroke-dasharray=\"1,1\" x1=\"" << GraphLeft + index << "\" y1=\"" << GraphTop << "\" x2=\"" << GraphLeft + index << "\" y2=\"" << GraphBottom + TickSize << "\" />" << std::endl;
							else if ((UTC.tm_mday == 15) && (UTC.tm_hour == 0) && (UTC.tm_min == 0))
								SVGFile << "\t<text text-anchor=\"middle\" x=\"" << GraphLeft + index << "\" y=\"" << SVGHeight - 2 << "\">" << Month[UTC.tm_mon] << "</text>" << std::endl;
						}
					}
				}

				// Directional Arrow
				SVGFile << "\t<polygon stroke=\"red\" fill=\"red\" points=\"" << GraphLeft - 3 << "," << GraphBottom << " " << GraphLeft + 3 << "," << GraphBottom - 3 << " " << GraphLeft + 3 << "," << GraphBottom + 3 << "\" />" << std::endl;

				SVGFile << "\t<polyline style=\"fill:none;stroke:blue\" points=\"";
				for (auto index = 1; index < (GraphWidth < TheValues.size() ? GraphWidth : TheValues.size()); index++)
					SVGFile << index + GraphLeft << "," << int(((TempMax - TheValues[index].GetTemperature(Fahrenheit)) * TempVerticalFactor) + GraphTop) << " ";
				SVGFile << "\" />" << std::endl;
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
void UpdateMRTGData(const bdaddr_t& TheAddress, Govee_Temp& TheValue)
{
	std::vector<Govee_Temp> foo;
	auto ret = GoveeMRTGLogs.insert(std::pair<bdaddr_t, std::vector<Govee_Temp>>(TheAddress, foo));
	std::vector<Govee_Temp> &FakeMRTGFile = ret.first->second;
	if (FakeMRTGFile.empty())
		FakeMRTGFile.resize(MAX_HISTORY);
	FakeMRTGFile[0] = TheValue;	// current value
	FakeMRTGFile[1] = Average(FakeMRTGFile[1], FakeMRTGFile[0]); // averaged value up to DAY_SAMPLE size
	//FakeMRTGFile[1] = Average(FakeMRTGFile.begin(), FakeMRTGFile.begin() + 1);
	// For every time difference between FakeMRTGFile[1] and FakeMRTGFile[2] that's greater than DAY_SAMPLE we shift that data towards the back.
	while (difftime(FakeMRTGFile[1].Time, FakeMRTGFile[2].Time) > DAY_SAMPLE)
	{
		// the next line is a hack to make the time line up. It's taking advantage of truncation in integer arithmatic.
		FakeMRTGFile[2].Time = (FakeMRTGFile[2].Time / DAY_SAMPLE) * DAY_SAMPLE;
		// For every time difference between FakeMRTGFile[1 + DAY_COUNT] and FakeMRTGFile[2 + DAY_COUNT] that's greater than WEEK_SAMPLE we shift that data towards the back.
		while (difftime(FakeMRTGFile[1 + DAY_COUNT].Time, FakeMRTGFile[2 + DAY_COUNT].Time) > WEEK_SAMPLE)
		{
			// Average Last two hours worth of values into the last value, since that is the value that will be moved into the first month value.
			auto Last = FakeMRTGFile.begin() + 1 + DAY_COUNT;
			auto First = Last - 1;
			while (difftime((First->Time / WEEK_SAMPLE) * WEEK_SAMPLE, Last->Time) < WEEK_SAMPLE)
				First--;
			First++;
			*Last = Average(First, Last);
			// Average routine ends here
			// the next line is a hack to make the time line up. It's taking advantage of truncation in integer arithmatic.
			FakeMRTGFile[2 + DAY_COUNT].Time = (FakeMRTGFile[2 + DAY_COUNT].Time / WEEK_SAMPLE) * WEEK_SAMPLE;
			// For every time difference between FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT] and FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT] that's greater than MONTH_SAMPLE we shift that data towards the back.
			while (difftime(FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT].Time, FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT].Time) > MONTH_SAMPLE)
			{
				// Average Last two hours worth of values into the last value, since that is the value that will be moved into the first month value.
				auto Last = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT;
				auto First = Last - 1;
				while (difftime((First->Time / MONTH_SAMPLE) * MONTH_SAMPLE, Last->Time) < MONTH_SAMPLE)
					First--;
				First++;
				*Last = Average(First, Last);
				// Average routine ends here
				if (ConsoleVerbosity > 1)
					std::cout << "[" << getTimeISO8601() << "] shuffling month " << timeToExcelLocal(FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT].Time) << " > " << timeToExcelLocal(FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT].Time) << std::endl;
				// the next line is a hack to make the time line up. It's taking advantage of truncation in integer arithmatic.
				FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT].Time = (FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT].Time / MONTH_SAMPLE) * MONTH_SAMPLE;
				// For every time difference between FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT] and FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT] that's greater than YEAR_SAMPLE we shift that data towards the back.
				while (difftime(FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time, FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time) > YEAR_SAMPLE)
				{
					// Average Last Days worth of values into the last value, since that is the value that will be moved into the first year value.
					auto Last = FakeMRTGFile.begin() + 1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT;
					struct tm LastDate;
					localtime_r(&(Last->Time), &LastDate);
					auto First = Last - 1;
					struct tm FirstDate;
					localtime_r(&(First->Time), &FirstDate);
					while (FirstDate.tm_mday == LastDate.tm_mday)
					{
						First--;
						localtime_r(&(First->Time), &FirstDate);
					}
					First++;
					*Last = Average(First, Last);
					// Average routine ends here
					if (ConsoleVerbosity > 0)
						std::cout << "[" << getTimeISO8601() << "] shuffling year " << timeToExcelLocal(FakeMRTGFile[1 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time) << " > " << timeToExcelLocal(FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time) << std::endl;
					if (ConsoleVerbosity > 1)
						std::cout << "[" << getTimeISO8601() << "] Timestamp normalized from " << timeToExcelLocal(FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time) << " to ";
					struct tm UTC;
					if (0 != localtime_r(&(FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time), &UTC))
						{
						UTC.tm_hour = 0;
						UTC.tm_min = 0;
						UTC.tm_sec = 0;
						FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time = mktime(&UTC);
					}
					if (ConsoleVerbosity > 1)
						std::cout << timeToExcelLocal(FakeMRTGFile[2 + DAY_COUNT + WEEK_COUNT + MONTH_COUNT].Time) << std::endl;
					// shuffle all the year samples toward the end
					std::copy_backward(
						FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + 1,
						FakeMRTGFile.end() - 1,
						FakeMRTGFile.end());
				}
				// shuffle all the month samples toward the end
				std::copy_backward(
					FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + 1,
					FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + 1,
					FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + MONTH_COUNT + 2);
			}
			// shuffle all the week samples toward the end
			std::copy_backward(
				FakeMRTGFile.begin() + DAY_COUNT + 1,
				FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + 1,
				FakeMRTGFile.begin() + DAY_COUNT + WEEK_COUNT + 2);
		}
		// shuffle all the day samples toward the end
		std::copy_backward(
			FakeMRTGFile.begin() + 1,
			FakeMRTGFile.begin() + DAY_COUNT + 1,
			FakeMRTGFile.begin() + DAY_COUNT + 2);
	}
}
void ReadLoggedData(void)
{
	DIR* dp;
	if ((dp = opendir(LogDirectory.c_str())) != NULL)
	{
		std::deque<std::string> files;
		struct dirent* dirp;
		while ((dirp = readdir(dp)) != NULL)
			if (DT_REG == dirp->d_type)
			{
				std::string filename = LogDirectory + std::string(dirp->d_name);
				if ((filename.substr(LogDirectory.size(), 3) == "gvh") && (filename.substr(filename.size()-4, 4) == ".txt"))
					files.push_back(filename);
			}
		closedir(dp);
		if (!files.empty())
		{
			sort(files.begin(), files.end());
			while (!files.empty())
			{
				std::string filename(files.begin()->c_str());
				files.pop_front();
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] Reading: " << filename << std::endl;
				else
					std::cerr << "Reading: " << filename << std::endl;
				// TODO: make sure the filename looks like my standard filename gvh507x_A4C13813AE36-2020-09.txt
				std::string ssBTAddress(filename.substr(LogDirectory.length() + 8, 12));
				for (auto index = ssBTAddress.length() - 2; index > 0;index -= 2)
					ssBTAddress.insert(index, ":");
				bdaddr_t TheBlueToothAddress;
				str2ba(ssBTAddress.c_str(), &TheBlueToothAddress);
				std::ifstream TheFile(filename);
				if (TheFile.is_open())
				{
					std::string TheLine;
					while (std::getline(TheFile, TheLine))
					{
						char buffer[256];
						if (TheLine.size() < sizeof(buffer))
						{
							TheLine.copy(buffer, TheLine.size());
							buffer[TheLine.size()] = '\0';
							std::string theDate(strtok(buffer, "\t"));
							std::string theTemp(strtok(NULL, "\t"));
							std::string theHumidity(strtok(NULL, "\t"));
							std::string theBattery(strtok(NULL, "\t"));
							Govee_Temp TheValue(ISO8601totime(theDate), atof(theTemp.c_str()), atof(theHumidity.c_str()), atol(theBattery.c_str()));
							UpdateMRTGData(TheBlueToothAddress, TheValue);
						}
					}
					TheFile.close();
				}
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
void ConnectAndDownload(int device_handle)
{
	time_t TimeNow;
	time(&TimeNow);
	for (auto iter = GoveeLastDownload.begin(); iter != GoveeLastDownload.end(); iter++)
	{
		char addr[19] = { 0 };
		ba2str(&iter->first, addr);
#ifdef DEBUG
		if (difftime(TimeNow, iter->second) > (60 * 10)) // 10 minutes
#else
		if (difftime(TimeNow, iter->second) > (60 * 60 * 24)) // 24 hours
#endif // DEBUG
		{
			std::cout << "[-------------------] [" << addr << "] " << timeToISO8601(iter->second) << std::endl;
			bool bDownloadInProgress = true;

			// Bluetooth HCI Command - LE Set Scan Enable (false)
			hci_le_set_scan_enable(device_handle, 0x00, 0x01, 1000); // Disable Scanning on the device
			std::cout << "[" << getTimeISO8601() << "] Scanning Stopped" << std::endl;

			// Bluetooth HCI Command - LE Create Connection (BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f))
			uint16_t handle = 0;
			int iRet = hci_le_create_conn(
				device_handle,
				96, // interval, Scan Interval: 96 (60 msec)
				48, // window, Scan Window: 48 (30 msec)
				0x00, // initiator_filter, Initiator Filter Policy: Use Peer Address (0x00)
				0x00, // peer_bdaddr_type, Peer Address Type: Public Device Address (0x00)
				iter->first, // BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f)
				0x01, // own_bdaddr_type, Own Address Type: Random Device Address (0x01)
				24, // min_interval, Connection Interval Min: 24 (30 msec)
				40, // max_interval, Connection Interval Max: 40 (50 msec)
				0, // latency, Connection Latency: 0 (number events)
				2000, // supervision_timeout, Supervision Timeout: 2000 (20 sec)
				0, // min_ce_length, Min CE Length: 0 (0 msec)
				0, // max_ce_length, Max CE Length: 0 (0 msec)
				&handle,
				15000);	// A 15 second timeout gives me a better chance of success
			std::cout << "[" << getTimeISO8601() << "] hci_le_create_conn [" << addr << "] Return(" << std::dec << iRet << ") handle (" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << handle << ")" << std::endl;

			if ((iRet == 0) && (handle != 0))
			{
				// Bluetooth HCI Command - LE Read Remote Features
				uint8_t features[8];
				//if (hci_read_remote_features(device_handle, handle, features, timeout) != -1)
				if (hci_le_read_remote_features(device_handle, handle, features, 15000) != -1)
				{
					// TODO: I think the lmp fumction below may leak memory with a malloc
					// Commented out till I figure this out.
					//std::string ssFeatures(lmp_featurestostr(features, "", 50));
					//std::cout << "[" << getTimeISO8601() << "] Features: " << ssFeatures << std::endl;
					std::cout << "[" << getTimeISO8601() << "] Features: TODO: Fix this so it works!" << std::endl;
				}
			}

			if ((iRet == 0) && (handle != 0))
			{
				// Bluetooth HCI Command - Read Remote Version Information
				struct hci_version ver;
				if (hci_read_remote_version(device_handle, handle, &ver, 15000) != -1)
				{
					std::cout << "[" << getTimeISO8601() << "] Version: " << lmp_vertostr(ver.lmp_ver) << std::endl;
					std::cout << "[-------------------] Subversion: " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << ver.lmp_subver << std::endl;
					std::cout << "[-------------------] Manufacture: " << bt_compidtostr(ver.manufacturer) << std::endl;
				}
			}

			// allocate a socket
			int l2cap_socket = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
			if (l2cap_socket > 0)
			{
				// set the connection parameters (who to connect to)
				struct sockaddr_l2 l2cap_address = { 0 };
				l2cap_address.l2_family = AF_BLUETOOTH;
				l2cap_address.l2_psm = htobs(0x1001);
				l2cap_address.l2_bdaddr = iter->first;
				//str2ba(dest, &l2cap_address.l2_bdaddr);

				// connect to server
				int status = connect(l2cap_socket, (struct sockaddr*)&l2cap_address, sizeof(l2cap_address));

				// send a message
				if (status == 0) {
					status = write(l2cap_socket, "hello!", 6);
				}
				close(l2cap_socket);
			}

			unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
			// The following while loop attempts to read from the non-blocking socket. 
			// As long as the read call simply times out, we sleep for 100 microseconds and try again.
			int RetryCount = 50;
			while (bDownloadInProgress)
			{
				ssize_t bufDataLen = read(device_handle, buf, sizeof(buf));
				if (bufDataLen > 1)
				{
					RetryCount = 50; 
					if (buf[0] == HCI_EVENT_PKT)
					{
						// At this point I should have an HCI Event in buf (hci_event_hdr)
						hci_event_hdr* header = (hci_event_hdr*)(buf + 1);
						if (header->evt == EVT_LE_META_EVENT)
						{
							evt_le_meta_event* meta = (evt_le_meta_event*)(header + HCI_EVENT_HDR_SIZE);
							if (meta->subevent == EVT_LE_CONN_COMPLETE)
							{
								evt_le_connection_complete* concomp = (evt_le_connection_complete*)(meta->data);
								char metaaddr[19] = { 0 };
								ba2str(&(concomp->peer_bdaddr), metaaddr);
								std::cout << "[-------------------] EVT_LE_CONN_COMPLETE [" << metaaddr << "] Status(" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(concomp->status) << ") Handle(" << int(concomp->handle) << ")" << std::endl;
								if (concomp->status == 0x00)
								{
									handle = concomp->handle;
									if (handle != 0)
									{
										// Bluetooth HCI Command - LE Read Remote Features
										uint8_t features[8];
										//if (hci_read_remote_features(device_handle, handle, features, timeout) != -1)
										if (hci_le_read_remote_features(device_handle, handle, features, 2000) != -1)
										{
											// TODO: I think the lmp fumction below may leak memory with a malloc
											// Commented out till I figure this out.
											//std::string ssFeatures(lmp_featurestostr(features, "", 50));
											//std::cout << "[" << getTimeISO8601() << "] Features: " << ssFeatures << std::endl;
											std::cout << "[-------------------] Features: TODO: Fix this so it works!" << std::endl;
										}
										// Bluetooth HCI Command - Read Remote Version Information
										struct hci_version ver;
										if (hci_read_remote_version(device_handle, handle, &ver, 2000) != -1)
										{
											std::cout << "[-------------------] Version: " << lmp_vertostr(ver.lmp_ver) << std::endl;
											std::cout << "[-------------------] Subversion: " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << ver.lmp_subver << std::endl;
											std::cout << "[-------------------] Manufacture: " << bt_compidtostr(ver.manufacturer) << std::endl;
										}
									}

								}
							}
							else if (meta->subevent == EVT_LE_ADVERTISING_REPORT)
							{
								le_advertising_info* advrpt = (le_advertising_info*)(meta->data);
								char metaaddr[19] = { 0 };
								ba2str(&(advrpt->bdaddr), metaaddr);
								std::cout << "[-------------------] EVT_LE_ADVERTISING_REPORT [" << metaaddr << "] evt_type(" << advrpt->evt_type << ")" << std::endl;
							}
							else if (meta->subevent == EVT_PHYSICAL_LINK_COMPLETE)
							{
								evt_physical_link_complete * ptr = (evt_physical_link_complete*)(meta->data);
								std::cout << "[-------------------] EVT_PHYSICAL_LINK_COMPLETE Status(" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(ptr->status) << ") Handle(" << int(ptr->handle) << ")" << std::endl;
								if (ptr->status == 0)
									handle = ptr->handle;
							}
							else
								std::cout << "[-------------------] EVT_LE_META_EVENT subevent(" << std::hex << int(meta->subevent) << ")" << std::endl;
						}
						else if (header->evt == EVT_CMD_COMPLETE)
						{
							evt_cmd_complete* ptr = (evt_cmd_complete*)(buf + (HCI_EVENT_HDR_SIZE + HCI_TYPE_LEN));
							std::cout << "[-------------------] EVT_CMD_COMPLETE" << std::endl;

						}
						else if (header->evt == EVT_CMD_STATUS)
						{
							evt_cmd_status* ptr = (evt_cmd_status*)(buf + (HCI_EVENT_HDR_SIZE + HCI_TYPE_LEN));
							std::cout << "[-------------------] EVT_CMD_STATUS" << std::endl;
						}
						else if (header->evt == EVT_DISCONN_COMPLETE)
						{
							evt_disconn_complete* ptr = (evt_disconn_complete*)(buf + (HCI_EVENT_HDR_SIZE + HCI_TYPE_LEN));
							std::cout << "[-------------------] EVT_DISCONN_COMPLETE" << std::endl;
							bDownloadInProgress = false;
						}
						else if (header->evt == EVT_READ_REMOTE_VERSION_COMPLETE)
						{
							std::cout << "[-------------------] EVT_READ_REMOTE_VERSION_COMPLETE" << std::endl;
						}
						else if (header->evt == EVT_NUM_COMP_PKTS)
						{
							std::cout << "[-------------------] EVT_NUM_COMP_PKTS" << std::endl;
						}
					}
					else if (buf[0] == HCI_ACLDATA_PKT)
					{
						std::cout << "[-------------------] HCI_ACLDATA_PKT" << std::endl;
					}
				}
				else
				{
					usleep(100000); // 1,000,000 = 1 second.
					if (--RetryCount < 0)
						bDownloadInProgress = false;
				}

			}
			if (handle != 0)
			{
				hci_disconnect(device_handle, handle, HCI_OE_USER_ENDED_CONNECTION, 2000);
				std::cout << "[-------------------] hci_disconnect" << std::endl;
			}
			time(&TimeNow);
			iter->second = TimeNow;
			hci_le_set_scan_parameters(device_handle, 0x01, htobs(0x1f40), htobs(0x1f40), 0x01, 0x00, 1000);
			hci_le_set_scan_enable(device_handle, 0x01, 0x00, 1000); // Enable Scanning on the device
			std::cout << "[" << getTimeISO8601() << "] Scanning Started" << std::endl;
		}
	}
}
/////////////////////////////////////////////////////////////////////////////
int LogFileTime = 60;
int MinutesAverage = 5;
bool DownloadData = false;
static void usage(int argc, char **argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -l | --log name      Logging Directory [" << LogDirectory << "]" << std::endl;
	std::cout << "    -t | --time seconds  time between log file writes [" << LogFileTime << "]" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -m | --mrtg XX:XX:XX:XX:XX:XX Get last value for this address" << std::endl;
	std::cout << "    -a | --average minutes [" << MinutesAverage << "]" << std::endl;
	std::cout << "    -s | --svg name      SVG output directory" << std::endl;
	std::cout << "    -d | --download  periodically attempt to connect and download stored data" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:t:v:m:a:s:d";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "time",   required_argument, NULL, 't' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "mrtg",   required_argument, NULL, 'm' },
		{ "average",required_argument, NULL, 'a' },
		{ "svg",	required_argument, NULL, 's' },
		{ "download",no_argument,NULL, 'd' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::string MRTGAddress;
	for (;;)
	{
		int idx;
		int c = getopt_long(argc, argv, short_options, long_options, &idx);
		if (-1 == c)
			break;
		switch (c)
		{
		case 0: /* getopt_long() flag */
			break;
		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'l':
			LogDirectory = std::string(optarg);
			if (!ValidateDirectory(LogDirectory))
				LogDirectory = "./";
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
		case 'a':
			try { MinutesAverage = std::stoi(optarg); }
			catch (const std::invalid_argument& ia) { std::cerr << "Invalid argument: " << ia.what() << std::endl; exit(EXIT_FAILURE); }
			catch (const std::out_of_range& oor) { std::cerr << "Out of Range error: " << oor.what() << std::endl; exit(EXIT_FAILURE); }
			break;
		case 'd':
			DownloadData = true;
			break;
		case 's':
			SVGDirectory = std::string(optarg);
			if (!ValidateDirectory(SVGDirectory))
				SVGDirectory.clear();
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
	if (ConsoleVerbosity > 0)
	{
		std::cout << "[" << getTimeISO8601() << "] " << ProgramVersionString << std::endl;
	}
	else
		std::cerr << ProgramVersionString << " (starting)" << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (!SVGDirectory.empty())
	{
		std::ostringstream TitleMapFilename;
		TitleMapFilename << SVGDirectory;
		TitleMapFilename << "gvh-titlemap.txt";
		std::ifstream TheFile(TitleMapFilename.str());
		if (TheFile.is_open())
		{
			if (ConsoleVerbosity > 0)
				std::cout << "[" << getTimeISO8601() << "] Reading: " << TitleMapFilename.str() << std::endl;
			std::string TheLine;
			while (std::getline(TheFile, TheLine))
			{
				char buffer[256];
				if (TheLine.size() < sizeof(buffer))
				{
					TheLine.copy(buffer, TheLine.size());
					buffer[TheLine.size()] = '\0';
					std::string theAddress(strtok(buffer, "\t"));
					std::string theTitle(strtok(NULL, "\t"));
					bdaddr_t TheBlueToothAddress;
					str2ba(theAddress.c_str(), &TheBlueToothAddress);
					GoveeBluetoothTitles.insert(std::pair<bdaddr_t, std::string>(TheBlueToothAddress, theTitle));
				}
			}
			TheFile.close();
		}
		ReadLoggedData();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	int device_id = hci_get_route(NULL);
	if (device_id < 0)
		std::cerr << "[                   ] Error: Bluetooth device not found" << std::endl;
	else
	{
		// Set up CTR-C signal handler
		typedef void(*SignalHandlerPointer)(int);
		SignalHandlerPointer previousHandlerSIGINT = signal(SIGINT, SignalHandlerSIGINT);	// Install CTR-C signal handler
		SignalHandlerPointer previousHandlerSIGHUP = signal(SIGHUP, SignalHandlerSIGHUP);	// Install Hangup signal handler

		int device_handle = hci_open_dev(device_id);
		if (device_handle < 0)
			std::cerr << "[                   ] Error: Cannot open device: " << strerror(errno) << std::endl;
		else
		{
			int on = 1;
			if (ioctl(device_handle, FIONBIO, (char *)&on) < 0)
				std::cerr << "[                   ] Error: Could set device to non-blocking: " << strerror(errno) << std::endl;
			else
			{
				// I came across the note: The Host shall not issue this command when scanning is enabled in the Controller; if it is the Command Disallowed error code shall be used. http://pureswift.github.io/Bluetooth/docs/Structs/HCILESetScanParameters.html
				hci_le_set_scan_enable(device_handle, 0x00, 0x01, 1000); // Disable Scanning on the device before setting scan parameters!
				char LocalName[0xff] = { 0 };
				hci_read_local_name(device_handle, sizeof(LocalName), LocalName, 1000);
				if (ConsoleVerbosity > 0)
					std::cout << "[" << getTimeISO8601() << "] LocalName: " << LocalName << std::endl;
				// Scan Type: Active (0x01)
				// Scan Interval: 18 (11.25 msec)
				// Scan Window: 18 (11.25 msec)
				// Own Address Type: Random Device Address (0x01)
				// Scan Filter Policy: Accept all advertisements, except directed advertisements not addressed to this device (0x00)
				if (hci_le_set_scan_parameters(device_handle, 0x01, htobs(0x0012), htobs(0x0012), 0x01, 0x00, 1000) < 0)
					std::cerr << "[                   ] Error: Failed to set scan parameters: " << strerror(errno) << std::endl;
				else
				{
					// Scan Interval : 8000 (5000 msec)
					// Scan Window: 8000 (5000 msec)
					if (hci_le_set_scan_parameters(device_handle, 0x01, htobs(0x1f40), htobs(0x1f40), 0x01, 0x00, 1000) < 0)
						std::cerr << "[                   ] Error: Failed to set scan parameters(Scan Interval : 8000 (5000 msec)): " << strerror(errno) << std::endl;
					// Scan Enable: true (0x01)
					// Filter Duplicates: false (0x00)
					if (hci_le_set_scan_enable(device_handle, 0x01, 0x00, 1000) < 0)
						std::cerr << "[                   ] Error: Failed to enable scan: " << strerror(errno) << std::endl;
					else
					{
						// Save the current HCI filter (Host Controller Interface)
						struct hci_filter original_filter;
						socklen_t olen = sizeof(original_filter);
						if (0 == getsockopt(device_handle, SOL_HCI, HCI_FILTER, &original_filter, &olen))
						{
							// Create and set the new filter
							struct hci_filter new_filter;
							hci_filter_clear(&new_filter);
							hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
							hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
							if (setsockopt(device_handle, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0)
								std::cerr << "[                   ] Error: Could not set socket options: " << strerror(errno) << std::endl;
							else
							{
								if (ConsoleVerbosity > 0)
									std::cout << "[" << getTimeISO8601() << "] Scanning..." << std::endl;

								bRun = true;
								time_t TimeStart, TimeSVG = 0;
								time(&TimeStart);
								while (bRun)
								{
									unsigned char buf[HCI_MAX_EVENT_SIZE];
									// The following while loop attempts to read from the non-blocking socket. 
									// As long as the read call simply times out, we sleep for 100 microseconds and try again.
									ssize_t bufDataLen = read(device_handle, buf, sizeof(buf));
									if (bufDataLen > HCI_MAX_EVENT_SIZE)
										std::cerr << "[                   ] Error: bufDataLen (" << bufDataLen << ") > HCI_MAX_EVENT_SIZE (" << HCI_MAX_EVENT_SIZE << ")" << std::endl;
									if (bufDataLen > (HCI_EVENT_HDR_SIZE + 1 + LE_ADVERTISING_INFO_SIZE))
									{
										if (ConsoleVerbosity > 3)
											std::cout << "[" << getTimeISO8601() << "] Read: " << std::dec << bufDataLen << " Bytes" << std::endl;
										std::ostringstream ConsoleOutLine;
										ConsoleOutLine << "[" << getTimeISO8601() << "]" << std::setw(3) << bufDataLen;

										// At this point I should have an HCI Event in buf (hci_event_hdr)
										evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (HCI_EVENT_HDR_SIZE + 1));
										if (meta->subevent == EVT_LE_ADVERTISING_REPORT)
										{
											const le_advertising_info * const info = (le_advertising_info *)(meta->data + 1);
											bool AddressInGoveeSet = (GoveeTemperatures.end() != GoveeTemperatures.find(info->bdaddr));
											char addr[19] = { 0 };
											ba2str(&info->bdaddr, addr);
											ConsoleOutLine << " [" << addr << "]";
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
															if (AddressInGoveeSet || (ConsoleVerbosity > 1))
															{
																ConsoleOutLine << " (Name) ";
																for (auto index = 1; index < *(info->data + current_offset); index++)
																	ConsoleOutLine << char((info->data + current_offset + 1)[index]);
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
																Govee_Temp localTemp;
																if (localTemp.ReadMSG((info->data + current_offset)))
																{
																	//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "°F";
																	ConsoleOutLine << " (Temp) " << std::dec << localTemp.GetTemperature() << "\u00B0" << "C";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																	//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2103";	// https://stackoverflow.com/questions/23777226/how-to-display-degree-celsius-in-a-string-in-c/23777678
																	//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2109";	// http://www.fileformat.info/info/unicode/char/2109/index.htm
																	ConsoleOutLine << " (Humidity) " << localTemp.GetHumidity() << "%";
																	ConsoleOutLine << " (Battery) " << localTemp.GetBattery() << "%";
																	std::queue<Govee_Temp> foo;
																	auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
																	ret.first->second.push(localTemp);
																	AddressInGoveeSet = true;
																	GoveeLastDownload.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, 0));
																	UpdateMRTGData(info->bdaddr, localTemp);
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
										usleep(100);
									}
									else if (errno == EINTR)
									{
										std::cerr << "[                   ] Error: " << strerror(errno) << " (" << errno << ")" << std::endl;
										// EINTR : Interrupted function call (POSIX.1-2001); see signal(7).
										bRun = false;
									}
									if (DownloadData)
										ConnectAndDownload(device_handle);
									time_t TimeNow;
									time(&TimeNow);
									if ((!SVGDirectory.empty()) && (difftime(TimeNow, TimeSVG) > DAY_SAMPLE))
									{
										if (ConsoleVerbosity > 0)
											std::cout << "[" << getTimeISO8601() << "] " << std::dec << DAY_SAMPLE << " seconds or more have passed. Writing SVG Files" << std::endl;
										TimeSVG = (TimeNow / DAY_SAMPLE) * DAY_SAMPLE; // hack to try to line up TimeSVG to be on a five minute period
										for (auto it = GoveeMRTGLogs.begin(); it != GoveeMRTGLogs.end(); it++)
										{
											const bdaddr_t TheAddress = it->first;
											char addr[19] = { 0 };
											ba2str(&TheAddress, addr);
											std::string btAddress(addr);
											for (auto pos = btAddress.find(':'); pos != std::string::npos; pos = btAddress.find(':'))
												btAddress.erase(pos, 1);
											std::string ssTitle(btAddress);
											if (GoveeBluetoothTitles.find(TheAddress) != GoveeBluetoothTitles.end())
												ssTitle = GoveeBluetoothTitles.find(TheAddress)->second;
											std::ostringstream OutputFilename;
											OutputFilename.str("");
											OutputFilename << SVGDirectory;
											OutputFilename << "gvh-";
											OutputFilename << btAddress;
											OutputFilename << "-day.svg";
											std::vector<Govee_Temp> TheValues;
											ReadMRTGData(TheAddress, TheValues, GraphType::daily);
											WriteMRTGSVG(TheValues, OutputFilename.str(), ssTitle, GraphType::daily);
											OutputFilename.str("");
											OutputFilename << SVGDirectory;
											OutputFilename << "gvh-";
											OutputFilename << btAddress;
											OutputFilename << "-week.svg";
											ReadMRTGData(TheAddress, TheValues, GraphType::weekly);
											WriteMRTGSVG(TheValues, OutputFilename.str(), ssTitle, GraphType::weekly);
											OutputFilename.str("");
											OutputFilename << SVGDirectory;
											OutputFilename << "gvh-";
											OutputFilename << btAddress;
											OutputFilename << "-month.svg";
											ReadMRTGData(TheAddress, TheValues, GraphType::monthly);
											WriteMRTGSVG(TheValues, OutputFilename.str(), ssTitle, GraphType::monthly);
											OutputFilename.str("");
											OutputFilename << SVGDirectory;
											OutputFilename << "gvh-";
											OutputFilename << btAddress;
											OutputFilename << "-year.svg";
											ReadMRTGData(TheAddress, TheValues, GraphType::yearly);
											WriteMRTGSVG(TheValues, OutputFilename.str(), ssTitle, GraphType::yearly);
										}
									}
									if (difftime(TimeNow, TimeStart) > LogFileTime)
									{
										if (ConsoleVerbosity > 0)
										{
											std::cout << "[" << getTimeISO8601() << "] " << std::dec << LogFileTime << " seconds or more have passed. Writing LOG Files" << std::endl;
											for (auto iter = GoveeLastDownload.begin(); iter != GoveeLastDownload.end(); iter++)
											{
												char addr[19] = { 0 };
												ba2str(&iter->first, addr);
												std::cout << "[-------------------] [" << addr << "] " << timeToISO8601(iter->second) << std::endl;
											}
										}
										TimeStart = TimeNow;
										GenerateLogFile(GoveeTemperatures);
									}
								}
								setsockopt(device_handle, SOL_HCI, HCI_FILTER, &original_filter, sizeof(original_filter));
							}
						}
						hci_le_set_scan_enable(device_handle, 0x00, 1, 1000);
					}
				}
			}
			hci_close_dev(device_handle);
		}
		signal(SIGHUP, previousHandlerSIGHUP);	// Restore original Hangup signal handler
		signal(SIGINT, previousHandlerSIGINT);	// Restore original Ctrl-C signal handler

		GenerateLogFile(GoveeTemperatures); // flush contents of accumulated map to logfiles

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
