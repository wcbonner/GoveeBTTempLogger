#pragma once
#ifdef _MSC_VER
#include "resource.h"
#else
#include <cstring>
#include <ctime>
#include <sstream>
#include <string>
#endif // _MSC_VER

std::string timeToISO8601(const time_t& TheTime, const bool LocalTime = false);
#ifdef _MSC_VER
std::string timeToISO8601(const CTimeSpan& TheTimeSpan);
#endif // _MSC_VER
std::string timeToExcelDate(const time_t& TheTime, const bool LocalTime = false);
std::string timeToExcelLocal(const time_t& TheTime);
std::string getTimeISO8601(const bool LocalTime = false);
std::string getTimeRFC1123(void);
time_t ISO8601totime(const std::string& ISOTime);
std::wstring getwTimeISO8601(const bool LocalTime = false);
