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
// Currently the H5075 and 5074 are decoded and logged.
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
#include <iostream>
#include <locale>
#include <queue>
#include <map>
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
#include <sys/ioctl.h>
#include <getopt.h>

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogger Version 1.20200922-1 Built on: " __DATE__ " at " __TIME__);
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
/////////////////////////////////////////////////////////////////////////////
// Class I'm using for storing raw data from the Govee thermometers
class  Govee_Temp {
public:
	time_t Time;
	double Temperature;
	double Humidity;
	int Battery;
	std::string WriteTXT(const char seperator = '\t') const;
	bool ReadMSG(const uint8_t * const data);
	Govee_Temp() : Time(0), Temperature(0), Humidity(0), Battery(0) { };
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
		if ((data[2] == 0x88) && (data[3] == 0xEC))
		{
			if (data_len == 9) // GVH5075_xxxx
			{
				// This data came from https://github.com/Thrilleratplay/GoveeWatcher
				// 88ec00 03519e 6400 Temp: 21.7502°C Temp: 71.1504°F Humidity: 50.2%
				// 1 2 3  4 5 6  7 8
				int iTemp = int(data[5]) << 16 | int(data[6]) << 8 | int(data[7]);
				Temperature = ((float(iTemp) / 10000.0) * 9.0 / 5.0) + 32.0;
				Humidity = float(iTemp % 1000) / 10.0;
				Battery = int(data[8]);
				rval = true;
			}
			else if (data_len == 10) // Govee_H5074_xxxx
			{
				// This data came from https://github.com/neilsheps/GoveeTemperatureAndHumidity
				// 88ec00 dd07 9113 64 02
				// 1 2 3  4 5  6 7  8  9
				int iTemp = int(data[6]) << 8 | int(data[5]);
				int iHumidity = int(data[8]) << 8 | int(data[7]);
				Temperature = ((float(iTemp) / 100.0) * 9.0 / 5.0) + 32.0;
				Humidity = float(iHumidity) / 100.0;
				Battery = int(data[9]);
				rval = true;
			}
			time(&Time);
		}
	}
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
				for (auto index = 6; index < 21; index++)
					ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[index]);
				ssValue << " (Major) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[21]);
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[22]);
				ssValue << " (Minor) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[23]);
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[24]);
				ssValue << " (RSSI) ";
				ssValue << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << int(data[25]);
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
std::map<bdaddr_t, time_t> GoveeLastSeen;
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
std::string LogDirectory("./");
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
				Govee_Temp TheValue;
				TheValue.Time = ISO8601totime(theDate);
				TheValue.Temperature = atof(theTemp.c_str());
				TheValue.Humidity = atof(theHumidity.c_str());
				TheValue.Battery = atol(theBattery.c_str());
				if ((Minutes == 0) && LogValues.empty()) // HACK: Special Case to always accept the last logged value
					LogValues.push(TheValue);
				if ((Minutes * 60.0) < difftime(now, TheValue.Time))	// If this entry is more than Minutes parameter from current time, it's time to stop reading log file.
					break;
				LogValues.push(TheValue);
			}
		} while (TheFile.tellg() > 0);	// If we are at the beginning of the file, there's nothing more to do
		TheFile.close();
		double NumElements = double(LogValues.size());
		if (NumElements > 0)
		{
			while (!LogValues.empty())
			{
				OutValue.Time = OutValue.Time > LogValues.front().Time ? OutValue.Time : LogValues.front().Time;
				OutValue.Temperature += LogValues.front().Temperature;
				OutValue.Humidity += LogValues.front().Humidity;
				OutValue.Battery += LogValues.front().Battery;
				LogValues.pop();
			}
			OutValue.Temperature /= NumElements;
			OutValue.Humidity /= NumElements;
			OutValue.Battery /= int(NumElements);
			rval = true;
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
		std::cout << TheValue.Humidity * 1000.0 << std::endl; // current state of the second variable, normally 'outgoing bytes count'
		std::cout << TheValue.Temperature * 1000.0 << std::endl; // current state of the first variable, normally 'incoming bytes count'
		std::cout << " " << std::endl; // string (in any human readable format), uptime of the target.
		std::cout << TextAddress << std::endl; // string, name of the target.
	}
}
/////////////////////////////////////////////////////////////////////////////
int ConsoleVerbosity = 1;
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
	std::cout << "    -d | --download  periodically attempt to connect and download stored data" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:t:v:m:a:d";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "time",   required_argument, NULL, 't' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "mrtg",   required_argument, NULL, 'm' },
		{ "average",required_argument, NULL, 'a' },
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
			LogDirectory = optarg;
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
								time_t TimeStart;
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
																	ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u00B0" << "F";	// http://www.fileformat.info/info/unicode/char/b0/index.htm
																	//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2103";	// https://stackoverflow.com/questions/23777226/how-to-display-degree-celsius-in-a-string-in-c/23777678
																	//ConsoleOutLine << " (Temp) " << std::dec << localTemp.Temperature << "\u2109";	// http://www.fileformat.info/info/unicode/char/2109/index.htm
																	ConsoleOutLine << " (Humidity) " << localTemp.Humidity << "%";
																	ConsoleOutLine << " (Battery) " << localTemp.Battery << "%";
																	std::queue<Govee_Temp> foo;
																	auto ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
																	ret.first->second.push(localTemp);
																	AddressInGoveeSet = true;
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
											if (DownloadData && AddressInGoveeSet)
											{
												ConsoleOutLine = std::ostringstream();
												time_t TimeNow;
												time(&TimeNow);
												auto foo = GoveeLastSeen.find(info->bdaddr);
												if (foo != GoveeLastSeen.end())
												{
													//if (difftime(foo->second, TimeNow) > (60 * 60 * 24)) // 24 hours
													{
														// Try to connect and download now
														// Bluetooth HCI Command - LE Set Scan Enable (false)
														hci_le_set_scan_enable(device_handle, 0x00, 0x01, 1000); // Disable Scanning on the device
														ConsoleOutLine << "Scanning Stopped" << std::endl;
														// Bluetooth HCI Command - LE Create Connection (BD_ADDR: e3:5e:cc:21:5c:0f (e3:5e:cc:21:5c:0f))
														//struct hci_dev_info devinfo;
														//hci_devinfo(device_id, &devinfo);
														//uint16_t ptype = htobs(devinfo.pkt_type & ACL_PTYPE_MASK);
														uint16_t ptype = HCI_COMMAND_PKT;
														uint16_t handle;
														int timeout = 2000; // 20 seconds
														if (hci_create_connection(device_handle, &(info->bdaddr), ptype, 0, 1, &handle, timeout) != -1)
														{
															ConsoleOutLine << "hci_create_connection" << std::endl;
															// Bluetooth HCI Command - LE Read Remote Features
															uint8_t features[8];
															if (hci_read_remote_features(device_handle, handle, features, timeout) != -1)
															{
																ConsoleOutLine << "Features: " << lmp_featurestostr(features, "\t\t", 50) << std::endl;
															}
															// Bluetooth HCI Command - Read Remote Version Information
															struct hci_version ver;
															if (hci_read_remote_version(device_handle, handle, &ver, timeout) != -1)
															{
																ConsoleOutLine << "Version: " << lmp_vertostr(ver.lmp_ver) << std::endl;
																ConsoleOutLine << "Subversion: " << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << ver.lmp_subver << std::endl;
																ConsoleOutLine << "nManufacture: " << bt_compidtostr(ver.manufacturer) << std::endl;
															}

															hci_disconnect(device_handle, handle, HCI_OE_USER_ENDED_CONNECTION, timeout);
															ConsoleOutLine << "hci_disconnect" << std::endl;
														}
														hci_le_set_scan_enable(device_handle, 0x01, 0x00, 1000); // Enable Scanning on the device
														ConsoleOutLine << "Scanning Started" << std::endl;
													}
														
													foo->second = TimeNow;
												}
												GoveeLastSeen.insert(std::pair<bdaddr_t, time_t>(info->bdaddr, TimeNow));
												std::cout << ConsoleOutLine.str();
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
										usleep(100);
									}
									else if (errno == EINTR)
									{
										std::cerr << "[                   ] Error: " << strerror(errno) << " (" << errno << ")" << std::endl;
										// EINTR : Interrupted function call (POSIX.1-2001); see signal(7).
										bRun = false;
									}
									time_t TimeNow;
									time(&TimeNow);
									if (difftime(TimeNow, TimeStart) > LogFileTime)
									{
										if (ConsoleVerbosity > 0)
											std::cout << "[" << getTimeISO8601() << "] " << LogFileTime << " seconds or more have passed" << std::endl;
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
