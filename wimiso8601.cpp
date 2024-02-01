#ifdef _MSC_VER
#include "stdafx.h"
#else
#include "wimiso8601.h"
#endif // _MSC_VER
/////////////////////////////////////////////////////////////////////////////
std::string timeToISO8601(const time_t& TheTime, const bool LocalTime)
{
	std::ostringstream ISOTime;
	struct tm UTC;
	struct tm* timecallresult(nullptr);
	if (LocalTime)
#ifdef localtime_r
		timecallresult = localtime_r(&TheTime, &UTC);
#else
		#pragma warning(suppress : 4996)
		timecallresult = localtime(&TheTime);
#endif
	else
#ifdef gmtime_r
		timecallresult = gmtime_r(&TheTime, &UTC);
#else
		#pragma warning(suppress : 4996)
		timecallresult = gmtime(&TheTime);
#endif
	if (nullptr != timecallresult)
	{
#ifndef gmtime_r
		UTC = *timecallresult;
#endif // !gmtime_r

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
#ifdef _MSC_VER
// TODO: Proper ifdef for CTimeSpan based on atltime.h header
std::string timeToISO8601(const CTimeSpan& TheTimeSpan)
{
	std::ostringstream ISOTime;
	ISOTime << "P";
	if (TheTimeSpan.GetDays() > 0)
		ISOTime << TheTimeSpan.GetDays() << "D";
	ISOTime << "T";
	if (TheTimeSpan.GetHours() > 0)
		ISOTime << TheTimeSpan.GetHours() << "H";
	if (TheTimeSpan.GetMinutes() > 0)
		ISOTime << TheTimeSpan.GetMinutes() << "M";
	if (TheTimeSpan.GetSeconds() > 0)
		ISOTime << TheTimeSpan.GetSeconds() << "S";
	return(ISOTime.str());
}
#endif // _MSC_VER
// Microsoft Excel doesn't recognize ISO8601 format dates with the "T" seperating the date and time
// This function puts a space where the T goes for ISO8601. The dates can be decoded with ISO8601totime()
std::string timeToExcelDate(const time_t& TheTime, const bool LocalTime)
{ 
	std::string ExcelDate(timeToISO8601(TheTime, LocalTime)); 
	ExcelDate.replace(10, 1, " "); 
	return(ExcelDate); 
}
std::string timeToExcelLocal(const time_t& TheTime) 
{ 
	return(timeToExcelDate(TheTime, true)); 
}
std::string getTimeISO8601(const bool LocalTime)
{
	time_t timer;
	time(&timer);
	std::string isostring(timeToISO8601(timer, LocalTime));
	std::string rval;
	rval.assign(isostring.begin(), isostring.end());
	return(rval);
}
std::string getTimeRFC1123(void)
{
	//InternetTimeFromSystemTime(&sysTime, INTERNET_RFC1123_FORMAT, tchInternetTime, sizeof(tchInternetTime));
	//HttpResponse << "Date: " << CStringA(CString(tchInternetTime)).GetString() << "\r\n";
	time_t timer;
	#pragma warning(suppress : 4996)
	time(&timer);
	#pragma warning(suppress : 4996)
	std::string RFCTime(asctime(gmtime(&timer)));
	RFCTime.pop_back();	// gets rid of the \n that asctime puts at the end of the line.
	RFCTime.append(" GMT");
	return(RFCTime);
}
time_t ISO8601totime(const std::string& ISOTime)
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
	int hours = 0;
	if (0 != _get_daylight(&hours))
		hours = 0;
	long SecondsFromUTC = 0;
	if (0 != _get_timezone(&SecondsFromUTC))
		SecondsFromUTC = 0;
	long dstbias = 0;
	if (0 != _get_dstbias(&dstbias))
		dstbias = 0;
	UTC.tm_isdst = hours;
#endif
	time_t timer = mktime(&UTC);
#ifdef _MSC_VER
	timer -= SecondsFromUTC;
	timer -= hours * dstbias;
#endif
	return(timer);
}
/////////////////////////////////////////////////////////////////////////////
std::wstring getwTimeISO8601(const bool LocalTime)
{
	std::string isostring(getTimeISO8601(LocalTime));
	std::wstring rval;
	rval.assign(isostring.begin(), isostring.end());

	return(rval);
}
/////////////////////////////////////////////////////////////////////////////
//std::string timeToISO8601(const CTime & TheTime)
//{
//	time_t TheOtherTime;
//	//mktime(
//	//TheTime.
//	return(timeToISO8601(TheOtherTime));
//}
#ifdef OLD_CODE
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
/////////////////////////////////////////////////////////////////////////////
#endif // OLD_CODE
