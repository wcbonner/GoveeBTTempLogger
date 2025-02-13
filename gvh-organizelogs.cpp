/////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 William C Bonner
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

// This is an auxilary program used to occasionally clean up some issues 
// with the GoveeBTTempLogger log files. 
// For best results, run the program, delete the backup files created, run a second time, and delete the new backup files.

#include <algorithm>
#include <arpa/inet.h>
#include <bluetooth/bluetooth.h> // apt install libbluetooth-dev
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <queue>
#include <regex>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>
#include "wimiso8601.h"
#if !defined(__GLIBC__)
    #define stat64 stat
#endif

/////////////////////////////////////////////////////////////////////////////
#if __has_include("goveebttemplogger-version.h")
#include "goveebttemplogger-version.h"
#endif
#ifndef GoveeBTTempLogger_VERSION
#define GoveeBTTempLogger_VERSION "(non-CMake)"
#endif // !GoveeBTTempLogger_VERSION
/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogOrganizer Version " GoveeBTTempLogger_VERSION " Built on: " __DATE__ " at " __TIME__);
std::filesystem::path LogDirectory;
std::filesystem::path BackupDirectory;
/////////////////////////////////////////////////////////////////////////////
bool ValidateDirectory(std::string& DirectoryName)
{
	bool rval = false;
	// I want to make sure the directory name does not end with a "/"
	while ((!DirectoryName.empty()) && (DirectoryName.back() == '/'))
		DirectoryName.pop_back();
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
/////////////////////////////////////////////////////////////////////////////
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
// Create a standardized logfile name for this program based on a Bluetooth address and the global parameter of the log file directory.
std::filesystem::path GenerateLogFileName(const bdaddr_t& a, time_t timer = 0)
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
/////////////////////////////////////////////////////////////////////////////
static void usage(int argc, char** argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -l | --log name      Logging Directory [" << LogDirectory << "]" << std::endl;
	std::cout << "    -f | --file name     Single log file to process [" <<  "]" << std::endl;
	std::cout << "    -b | --backup name   Backup Directory [" << BackupDirectory << "]" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:f:b:";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "file",   required_argument, NULL, 'f' },
		{ "backup", required_argument, NULL, 'b' },
		{ 0, 0, 0, 0 }
};
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char** argv)
{
	///////////////////////////////////////////////////////////////////////////////////////////////

	for (;;)
	{
		std::string TempString;
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
			TempString = std::string(optarg);
			if (ValidateDirectory(TempString))
				LogDirectory = TempString;
			break;
		case 'b':
			TempString = std::string(optarg);
			if (ValidateDirectory(TempString))
				BackupDirectory = TempString;
			break;
		default:
			usage(argc, argv);
			exit(EXIT_FAILURE);
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	std::cout << "[" << getTimeISO8601() << "] " << ProgramVersionString << std::endl;
	///////////////////////////////////////////////////////////////////////////////////////////////
	tzset();
	std::locale mylocale("");   // get global locale
	std::cout.imbue(mylocale);  // imbue global locale
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (LogDirectory.empty() || BackupDirectory.empty())
		usage(argc, argv);
	else
	{
		const std::regex LogFileRegex("(gvh507x_|gvh-)[[:xdigit:]]{12}-[[:digit:]]{4}-[[:digit:]]{2}.txt"); // 2024-10-01 Both old and new format recognized
		std::deque<std::filesystem::path> files;
		for (auto const& dir_entry : std::filesystem::directory_iterator{ LogDirectory })
			if (dir_entry.is_regular_file())
				if (std::regex_match(dir_entry.path().filename().string(), LogFileRegex))
					files.push_back(dir_entry);

		if (!files.empty())
		{
			sort(files.begin(), files.end());
			bdaddr_t LastBlueToothAddress({0});
			while (!files.empty())
			{
				std::filesystem::path FQFileName(*files.begin());
				std::filesystem::path BackupName(BackupDirectory / FQFileName.filename());
				BackupName.replace_extension(FQFileName.extension());
				std::ifstream TheFile(FQFileName);
				if (TheFile.is_open())
				{
					std::deque<std::string> DataLines;
					std::cout << "[" << getTimeISO8601() << "] Reading: " << FQFileName;
					const std::regex ModifiedBluetoothAddressRegex("[[:xdigit:]]{12}");
					std::smatch BluetoothAddressInFilename;
					std::string Stem(FQFileName.stem());
					if (std::regex_search(Stem, BluetoothAddressInFilename, ModifiedBluetoothAddressRegex))
					{
						bdaddr_t TheBlueToothAddress(string2ba(BluetoothAddressInFilename.str()));
						int count(0);
						// Read All Data From Existing Log, removing nulls if possible
						std::string TheLine;
						while (std::getline(TheFile, TheLine))
						{
							// erase any nulls from the data. these are occasionally in the log file when the platform crashed during a write to the logfile.
							for (auto pos = TheLine.find('\000'); pos != std::string::npos; pos = TheLine.find('\000'))
								TheLine.erase(pos);
							DataLines.push_back(TheLine);
							count++;
						}
						std::cout << " (" << count << " lines)";
						TheFile.close();
						// Rename Existing Log to backup.
						bool bBackedUp = false;
						try
						{
							std::filesystem::rename(FQFileName, BackupName);
							std::cout << " Renamed " << FQFileName << " to " << BackupName << std::endl;
							bBackedUp = true;
						}
						catch (const std::filesystem::filesystem_error& ia)
						{
							std::cout << " " << ia.what() << std::endl;
							std::cout << " Unable to Rename " << FQFileName << " to " << BackupName << std::endl;
						}

						if (bBackedUp)
						{
							// Sort all Data
							sort(DataLines.begin(), DataLines.end());
							// Distribute Data to appropriate new Log Files
							std::ofstream LogFile;
							time_t TheTime(0), LastTime(0);
							int LastYear(0), LastMonth(0);
							std::filesystem::path LastFileName;
							std::string LastLine;
							while (!DataLines.empty())
							{
								if (DataLines.begin()->length() > 18)	// line is longer than an ISO8601 String
								{
									if (0 != LastLine.compare(*DataLines.begin()))	// line is unique
									{
										TheTime = ISO8601totime(*DataLines.begin());
										struct tm UTC;
										if (nullptr != gmtime_r(&TheTime, &UTC))
										{
											if ((UTC.tm_year != LastYear) || (UTC.tm_mon != LastMonth))
											{
												LastYear = UTC.tm_year;
												LastMonth = UTC.tm_mon;
												if (LogFile.is_open())
												{
													std::cout << " (" << count << " lines)" << std::endl;
													LogFile.close();
													if (!LastFileName.empty())
													{
														struct utimbuf ut;
														ut.actime = LastTime;
														ut.modtime = LastTime;
														utime(LastFileName.c_str(), &ut);
													}
												}
												LastFileName = GenerateLogFileName(TheBlueToothAddress, TheTime);
												LogFile.open(LastFileName, std::ios_base::out | std::ios_base::app);
												std::cout << "[" << getTimeISO8601() << "] Writing: " << LastFileName;
												count = 0;
											}
											LogFile << *DataLines.begin() << std::endl;
											LastTime = TheTime;
											count++;
										}
										LastLine = *DataLines.begin();
									}
								}
								DataLines.pop_front();
							}
							std::cout << " (" << count << " lines)" << std::endl;
							LogFile.close();
							if (!LastFileName.empty())
							{
								struct utimbuf ut;
								ut.actime = LastTime;
								ut.modtime = LastTime;
								utime(LastFileName.c_str(), &ut);
							}
						}
						files.pop_front();
					}
				}
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	return(EXIT_SUCCESS);
}
