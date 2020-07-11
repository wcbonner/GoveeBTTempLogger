/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 William C Bonner
// This code borrows from plenty of other sources I've found.
// I try to leave credits in comments scattered through the code itself and
// would appreciate similar credit if you use portions of my code.
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
//

#include <cstdio>
#include <ctime>
#include <csignal>
#include <iostream>
#include <locale>
#include <queue>
#include <map>
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
	UTC.tm_year = atol(ISOTime.substr(0, 4).c_str()) - 1900;
	UTC.tm_mon = atol(ISOTime.substr(5, 2).c_str()) - 1;
	UTC.tm_mday = atol(ISOTime.substr(8, 2).c_str());
	UTC.tm_hour = atol(ISOTime.substr(11, 2).c_str());
	UTC.tm_min = atol(ISOTime.substr(14, 2).c_str());
	UTC.tm_sec = atol(ISOTime.substr(17, 2).c_str());
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
std::string timeToExcelDate(const time_t & TheTime)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	if (0 != gmtime_r(&TheTime, &UTC))
	{
		ISOTime.fill('0');
		ISOTime << UTC.tm_year + 1900 << "-";
		ISOTime.width(2);
		ISOTime << UTC.tm_mon + 1 << "-";
		ISOTime.width(2);
		ISOTime << UTC.tm_mday << " ";
		ISOTime.width(2);
		ISOTime << UTC.tm_hour << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_min << ":";
		ISOTime.width(2);
		ISOTime << UTC.tm_sec;
	}
	return(ISOTime.str());
}
/////////////////////////////////////////////////////////////////////////////
#define EIR_NAME_SHORT              0x08
#define EIR_NAME_COMPLETE           0x09
#define EIR_MANUFACTURE_SPECIFIC    0xFF
/////////////////////////////////////////////////////////////////////////////
// Class I'm using for storing raw data from the Govee thermometers
class  Govee_Temp {
public:
	time_t Time;
	double Temperature;
	double Humidity;
	int Battery;
	std::string WriteTXT(const char seperator = '\t') const;
	bool ReadMSG(const uint8_t *data, const size_t data_len, const le_advertising_info *info);
};
std::string Govee_Temp::WriteTXT(const char seperator) const
{
	std::stringstream ssValue;
	ssValue << timeToExcelDate(Time);
	ssValue << seperator << Temperature;
	ssValue << seperator << Humidity;
	ssValue << seperator << Battery;
	return(ssValue.str());
}
bool Govee_Temp::ReadMSG(const uint8_t *data, const size_t data_len, const le_advertising_info *info)
{
	bool rval = false;
	if ((data[1] == 0x88) && (data[2] == 0xEC))
	{
		if (data_len == 9)
		{
			// This data came from https://github.com/Thrilleratplay/GoveeWatcher
			// 88ec00 03519e 6400 Temp: 21.7502°C Temp: 71.1504°F Humidity: 50.2%
			// 1 2 3  4 5 6  7 8
			int iTemp = int(data[4]) << 16 | int(data[5]) << 8 | int(data[6]);
			Temperature = ((float(iTemp) / 10000.0) * 9.0 / 5.0) + 32.0;
			Humidity = float(iTemp % 1000) / 10.0;
			Battery = int(data[7]);
			rval = true;
		}
		else if (data_len == 10)
		{
			// This data came from https://github.com/neilsheps/GoveeTemperatureAndHumidity
			// 88ec00 dd07 9113 64 02
			// 1 2 3  4 5  6 7  8  9
			int iTemp = int(data[5]) << 8 | int(data[4]);
			int iHumidity = int(data[7]) << 8 | int(data[6]);
			Temperature = ((float(iTemp) / 100.0) * 9.0 / 5.0) + 32.0;
			Humidity = float(iHumidity) / 100.0;
			Battery = int(data[8]);
			rval = true;
		}
		time(&Time);
	}
	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
// The following operators were required so I could use the std::map<> to use BlueTooth Addresses as the key
bool operator ==(const bdaddr_t &a, const bdaddr_t &b)
{
	return(
		(a.b[0] == b.b[0]) &&
		(a.b[1] == b.b[1]) &&
		(a.b[2] == b.b[2]) &&
		(a.b[3] == b.b[3]) &&
		(a.b[4] == b.b[4]) &&
		(a.b[5] == b.b[5]));
};
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
/////////////////////////////////////////////////////////////////////////////
volatile bool bRun = true; // This is declared volatile so that the compiler won't optimized it out of loops later in the code
void SignalHandlerSIGINT(int signal)
{
	bRun = false;
	//cerr << "[" << getTimeISO8601() << "] SIGINT: Caught Ctrl-C, finishing loop and quitting. *****************" << endl;
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
	OutputFilename << LogDirectory;
	OutputFilename << "gvh507x_";
	OutputFilename << std::hex << std::setw(2) << std::setfill('0') << std::uppercase << int(a.b[1]) << int(a.b[0]);
	time_t timer;
	time(&timer);
	struct tm UTC;
	if (0 != gmtime_r(&timer, &UTC))
		if (!((UTC.tm_year == 70) && (UTC.tm_mon == 0) && (UTC.tm_mday == 1)))
			OutputFilename << "-" << std::dec << UTC.tm_year + 1900 << "-" << std::setw(2) << std::setfill('0') << UTC.tm_mon + 1;
	OutputFilename << ".txt";
	return(OutputFilename.str());
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
bool GetLastLogEntry(const bdaddr_t &TheAddress, Govee_Temp & TheValue)
{
	bool rval = false;
	std::ifstream TheFile(GenerateLogFileName(TheAddress));
	if (TheFile.is_open())
	{
		TheFile.seekg(0, std::ios_base::end);      //Start at end of file
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
		std::string TheLastLine;
		std::getline(TheFile, TheLastLine);
		TheFile.close();

		char buffer[256];
		if (TheLastLine.size() < sizeof(buffer))
		{
			TheLastLine.copy(buffer, TheLastLine.size() + 1);
			buffer[TheLastLine.size()] = '\0';
			std::string theDate(strtok(buffer, "\t"));
			std::string theTemp(strtok(NULL, "\t"));
			std::string theHumidity(strtok(NULL, "\t"));
			TheValue.Temperature = atof(theTemp.c_str());
			TheValue.Humidity = atof(theHumidity.c_str());
			rval = true;
		}
	}
	return(rval);
}
void GetMRTGOutput(const std::string &TextAddress)
{
	bdaddr_t TheAddress = { 0 };
	str2ba(TextAddress.c_str(), &TheAddress);
	Govee_Temp TheValue;
	if (GetLastLogEntry(TheAddress, TheValue))
	{
		std::cout << std::dec; // make sure I'm putting things in decimal format
		std::cout << TheValue.Humidity << std::endl; // current state of the second variable, normally 'outgoing bytes count'
		std::cout << TheValue.Temperature << std::endl; // current state of the first variable, normally 'incoming bytes count'
		std::cout << " " << std::endl; // string (in any human readable format), telling the uptime of the target.
		std::cout << TextAddress << std::endl; // string, telling the name of the target.
	}
}
/* Example MRTG Config File Entry
######################################################################
Target[GVH5075_A26A]: `/home/wim/projects/GoveeBTTempLogger/bin/ARM/Debug/GoveeBTTempLogger.out -l /media/acid/web/www.WimsWorld.com/mrtg/govee/ -m A4:C1:38:65:A2:6A`
MaxBytes[GVH5075_A26A]: 120
Options[GVH5075_A26A]: gauge, nopercent, unknaszero, transparent
PNGTitle[GVH5075_A26A]: Temperature and Humidity on A4:C1:38:65:A2:6A
YLegend[GVH5075_A26A]: Temperature (F)
ShortLegend[GVH5075_A26A]: (F)
Title[GVH5075_A26A]: Temperature and Humidity on A4:C1:38:65:A2:6A
WithPeak[GVH5075_A26A]: ymw
LegendO[GVH5075_A26A]: Temperature
LegendI[GVH5075_A26A]: Humidity
PageTop[GVH5075_A26A]: <H1>Temperature and Humidity on Govee A4:C1:38:65:A2:6A</H1>
*/
/////////////////////////////////////////////////////////////////////////////
int ConsoleVerbosity = 1;
static void usage(int argc, char **argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  Version 1.0 Built on: " __DATE__ " at " __TIME__ << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -l | --log name      Logging Directory [" << LogDirectory << "]" << std::endl;
	std::cout << "    -v | --verbose level stdout verbosity level [" << ConsoleVerbosity << "]" << std::endl;
	std::cout << "    -m | --mrtg XX:XX:XX:XX:XX:XX Get last value for this address" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "l:v:m:h";
static const struct option long_options[] = {
		{ "log",    required_argument, NULL, 'l' },
		{ "verbose",required_argument, NULL, 'v' },
		{ "mrtg",   required_argument, NULL, 'm' },
		{ "help",   no_argument,       NULL, 'h' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////
	// Parse Options
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
		case 'l':
			LogDirectory = optarg;
			break;
		case 'v':
			errno = 0;
			ConsoleVerbosity = strtol(optarg, NULL, 0);
			if (errno)
			{
				std::cerr << optarg << " error " << errno << ", " << strerror(errno) << std::endl;
				exit(EXIT_FAILURE);
			}
			break;
		case 'm':
			GetMRTGOutput(std::string(optarg));
			exit(EXIT_SUCCESS);
		case 'h':
			usage(argc, argv);
			exit(EXIT_SUCCESS);
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (ConsoleVerbosity >= 1)
	{
		std::cout << "[" << getTimeISO8601() << "] " << "hello from GoveeBTTempLogger!" << std::endl;
		std::cout << "[                   ] Built on: " __DATE__ " at " __TIME__ << std::endl;
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
			// Set fd non-blocking
			int on = 1;
			if (ioctl(device_handle, FIONBIO, (char *)&on) < 0)
				std::cerr << "[                   ] Error: Could set device to non-blocking: " << strerror(errno) << std::endl;
			else
			{
				if (hci_le_set_scan_parameters(device_handle, 0x01, htobs(0x0010), htobs(0x0010), 0x00, 0x00, 1000) < 0)
					std::cerr << "[                   ] Error: Failed to set scan parameters: " << strerror(errno) << std::endl;
				else
				{
					if (hci_le_set_scan_enable(device_handle, 0x01, 1, 1000) < 0)
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
								if (ConsoleVerbosity >= 1)
									std::cout << "[" << getTimeISO8601() << "] Scanning..." << std::endl;

								bRun = true;
								bool error = false;
								time_t TimeStart;
								time(&TimeStart);
								while (bRun && !error)
								{
									int len = 0;
									unsigned char buf[HCI_MAX_EVENT_SIZE];
									// The following while loop attempts to read from the non-blocking socket. 
									// As long as the read call simply times out, we sleep for 100 microseconds and try again.
									while ((len = read(device_handle, buf, sizeof(buf))) < 0)
									{
										if (bRun == false)
											break;
										if (errno == EINTR)
										{
											// EINTR : Interrupted function call (POSIX.1-2001); see signal(7).
											bRun = false;
											break;
										}
										if (errno == EAGAIN || errno == EINTR)
										{
											// EAGAIN : Resource temporarily unavailable (may be the same value as EWOULDBLOCK) (POSIX.1-2001).
											usleep(100);
											continue;
										}
										error = true;
									}
									if (bRun && !error)
									{
										evt_le_meta_event *meta = (evt_le_meta_event *)(buf + (1 + HCI_EVENT_HDR_SIZE));
										len -= (1 + HCI_EVENT_HDR_SIZE);
										if (meta->subevent != EVT_LE_ADVERTISING_REPORT)
											continue;
										le_advertising_info *info = (le_advertising_info *)(meta->data + 1);
										if (info->length == 0)
											continue;
										int current_offset = 0;
										bool data_error = false;
										while (!data_error && current_offset < info->length)
										{
											size_t data_len = info->data[current_offset];
											if (data_len + 1 > info->length)
											{
												if (ConsoleVerbosity >= 1)
													std::cout << "[" << getTimeISO8601() << "] EIR data length is longer than EIR packet length. " << data_len << " + 1 > " << info->length << std::endl;
												data_error = true;
											}
											else
											{
												// Bluetooth Extended Inquiry Response
												// I'm paying attention to only three types of EIR, Short Name, Complete Name, and Manufacturer Specific Data
												// The names are how I learn which Bluetooth Addresses I'm going to listen to
												bool AddressInGoveeSet = (GoveeTemperatures.end() != GoveeTemperatures.find(info->bdaddr));
												char addr[19] = { 0 };
												ba2str(&info->bdaddr, addr);
												if ((info->data + current_offset + 1)[0] == EIR_NAME_SHORT || 
													(info->data + current_offset + 1)[0] == EIR_NAME_COMPLETE)
												{
													std::string name((char *)&((info->data + current_offset + 1)[1]), data_len - 1);
													if ((name.compare(0, 7, "GVH5075") == 0) || 
														(name.compare(0, 11, "Govee_H5074") == 0))
													{
														std::queue<Govee_Temp> foo;
														GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
													}
													if (ConsoleVerbosity >= 1)
														std::cout << "[" << getTimeISO8601() << "] [" << addr << "] Name: " << name << std::endl;
												}
												else if (AddressInGoveeSet)
												{
													if ((info->data + current_offset + 1)[0] == EIR_MANUFACTURE_SPECIFIC)
													{
														//std::cout << "[" << getTimeISO8601() << "] [" << addr << "]";
														//std::cout << " Manufacturer: length = " << std::dec << std::setw(2) << std::setfill(' ') << data_len - 1;
														//std::cout << " Data: ";
														//for (size_t i = 1; i < data_len; i++)
														//	std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)(info->data + current_offset + 1)[i];
														//std::cout << std::endl;
														Govee_Temp localTemp;
														if (localTemp.ReadMSG((info->data + current_offset + 1), data_len, info))
														{
															if (ConsoleVerbosity >= 1)
															{
																std::cout << "[" << getTimeISO8601() << "] [" << addr << "]";
																std::cout << " Temp: " << std::dec << localTemp.Temperature << "°F";
																std::cout << " Humidity: " << localTemp.Humidity << "%";
																std::cout << " Battery: " << localTemp.Battery << "%";
																std::cout << std::endl;
															}
															std::queue<Govee_Temp> foo;
															std::pair<std::map<bdaddr_t, std::queue<Govee_Temp>>::iterator, bool> ret = GoveeTemperatures.insert(std::pair<bdaddr_t, std::queue<Govee_Temp>>(info->bdaddr, foo));
															ret.first->second.push(localTemp);
														}
													}
												}
												current_offset += data_len + 1;
											}
										}
										time_t TimeNow;
										time(&TimeNow);
										if (difftime(TimeNow, TimeStart) > 60)
										{
											if (ConsoleVerbosity >= 1)
												std::cout << "[" << getTimeISO8601() << "] A minute or more has passed" << std::endl;
											TimeStart = TimeNow;
											GenerateLogFile(GoveeTemperatures);
										}
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

		if (ConsoleVerbosity >= 1)
		{
			// dump contents of accumulated map (should now be empty because all the data was flushed to log files)
			for (auto it = GoveeTemperatures.begin(); it != GoveeTemperatures.end(); ++it)
			{
				char addr[19] = { 0 };
				ba2str(&it->first, addr);
				std::cout << "[" << addr << "]" << std::endl;
				while (!it->second.empty())
				{
					std::cout << it->second.front().WriteTXT() << std::endl;
					it->second.pop();
				}
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	return(EXIT_SUCCESS);
}