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
#include <dirent.h>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <queue>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>

/////////////////////////////////////////////////////////////////////////////
static const std::string ProgramVersionString("GoveeBTTempLogOrganizer Version 1.20230404-1 Built on: " __DATE__ " at " __TIME__);
std::string LogDirectory;
std::string BackupDirectory;
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t& TheTime, const bool LocalTime = false)
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
time_t ISO8601totime(const std::string& ISOTime)
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
std::string timeToExcelDate(const time_t& TheTime, const bool LocalTime = false) { std::string ExcelDate(timeToISO8601(TheTime, LocalTime)); ExcelDate.replace(10, 1, " "); return(ExcelDate); }
std::string timeToExcelLocal(const time_t& TheTime) { return(timeToExcelDate(TheTime, true)); }/////////////////////////////////////////////////////////////////////////////
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
std::string ba2string(const bdaddr_t& a) { char addr_str[18]; ba2str(&a, addr_str); std::string rVal(addr_str); return(rVal); }
bdaddr_t string2ba(const std::string& a) { std::string ssBTAddress(a);if (ssBTAddress.length() == 12)for (auto index = ssBTAddress.length() - 2; index > 0; index -= 2)ssBTAddress.insert(index, ":");bdaddr_t TheBlueToothAddress({ 0 });if (ssBTAddress.length() == 17)str2ba(ssBTAddress.c_str(), &TheBlueToothAddress);return(TheBlueToothAddress); }
/////////////////////////////////////////////////////////////////////////////
// Create a standardized logfile name for this program based on a Bluetooth address and the global parameter of the log file directory.
std::string GenerateLogFileName(const bdaddr_t& a, time_t timer = 0)
{
	std::ostringstream OutputFilename;
	OutputFilename << LogDirectory;
	if (LogDirectory.back() != '/')
		OutputFilename << "/";
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
	std::string OldFormatFileName(OutputFilename.str());

	// The New Format Log File Name includes the entire Bluetooth Address, making it much easier to recognize and add to MRTG config files.
	OutputFilename.str("");
	OutputFilename << LogDirectory;
	if (LogDirectory.back() != '/')
		OutputFilename << "/";
	OutputFilename << "gvh-";
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
/////////////////////////////////////////////////////////////////////////////
static void usage(int argc, char** argv)
{
	std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
	std::cout << "  " << ProgramVersionString << std::endl;
	std::cout << "  Options:" << std::endl;
	std::cout << "    -h | --help          Print this message" << std::endl;
	std::cout << "    -l | --log name      Logging Directory [" << LogDirectory << "]" << std::endl;
	std::cout << "    -b | --backup name   Backup Directory [" << BackupDirectory << "]" << std::endl;
	std::cout << std::endl;
}
static const char short_options[] = "hl:b:";
static const struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "log",    required_argument, NULL, 'l' },
		{ "backup",    required_argument, NULL, 'b' },
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
	///////////////////////////////////////////////////////////////////////////////////////////////
	if (LogDirectory.empty() || BackupDirectory.empty())
		usage(argc, argv);
	else
	{
		DIR* dp;
		if ((dp = opendir(LogDirectory.c_str())) != NULL)
		{
			std::deque<std::string> files;
			struct dirent* dirp;
			while ((dirp = readdir(dp)) != NULL)
				if (DT_REG == dirp->d_type)
				{
					std::string filename(dirp->d_name); // gvh-E38EC8C1989A-2023-04.txt
					// std::cout <<  filename << " length: " << filename.length() << std::endl;
					if (filename.length() == 28)
						if ((filename.substr(0, 4) == "gvh-") && (filename.substr(filename.size() - 4, 4) == ".txt"))
							files.push_back(filename);
				}
			closedir(dp);
			if (!files.empty())
			{
				sort(files.begin(), files.end());
				bdaddr_t LastBlueToothAddress({0});
				while (!files.empty())
				{
					std::string FQFileName(LogDirectory + "/" + *files.begin());
					std::string BackupName(BackupDirectory + "/" + *files.begin());
					std::ifstream TheFile(FQFileName);
					if (TheFile.is_open())
					{
						std::deque<std::string> DataLines;
						std::cout << "[" << getTimeISO8601() << "] Reading: " << FQFileName;
						int count(0);
						std::string ssBTAddress;
						auto pos = FQFileName.rfind("gvh-");
						if (pos != std::string::npos)
							ssBTAddress = FQFileName.substr(pos + 4, 12);	// new filename format (2023-04-03)
						bdaddr_t TheBlueToothAddress(string2ba(ssBTAddress));
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
						if (rename(FQFileName.c_str(), BackupName.c_str()) == 0)
						{
							std::cout << " Renamed " << FQFileName << " to " << BackupName << std::endl;
							// Sort all Data
							sort(DataLines.begin(), DataLines.end());
							// Distribute Data to appropriate new Log Files
							std::ofstream LogFile;
							time_t TheTime(0), LastTime(0);
							int LastYear(0), LastMonth(0);
							std::string LastFileName;
							while (!DataLines.empty())
							{
								auto TheLine(DataLines.begin());
								TheTime = ISO8601totime(*TheLine);
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
									LogFile << *TheLine << std::endl;
									LastTime = TheTime;
									count++;
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
						else
							std::cout << " Unable to Rename " << FQFileName << " to " << BackupName << std::endl;
						files.pop_front();
					}
				}
			}
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////
	return(EXIT_SUCCESS);
}
