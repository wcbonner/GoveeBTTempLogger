#pragma once

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
std::string ThermometerType2String(const ThermometerType GoveeModel);
ThermometerType String2ThermometerType(const std::string Text);

class  Govee_Temp {
public:
	time_t Time;
	std::string WriteTXT(const char seperator = '\t') const;
	std::string WriteCache(void) const;
	std::string WriteConsole(void) const;
	bool ReadCache(const std::string& data);
	bool ReadMSG(const uint8_t* const data);
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
	Govee_Temp(const std::string& data);
	double GetTemperature(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return((Temperature[index] * 9.0 / 5.0) + 32.0); return(Temperature[index]); };
	double GetTemperatureMin(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return(std::min(((Temperature[index] * 9.0 / 5.0) + 32.0), ((TemperatureMin[index] * 9.0 / 5.0) + 32.0))); return(std::min(Temperature[index], TemperatureMin[index])); };
	double GetTemperatureMax(const bool Fahrenheit = false, const int index = 0) const { if (Fahrenheit) return(std::max(((Temperature[index] * 9.0 / 5.0) + 32.0), ((TemperatureMax[index] * 9.0 / 5.0) + 32.0))); return(std::max(Temperature[index], TemperatureMax[index])); };
	void SetMinMax(const Govee_Temp& a);
	double GetHumidity(void) const { return(Humidity); };
	double GetHumidityMin(void) const { return(std::min(Humidity, HumidityMin)); };
	double GetHumidityMax(void) const { return(std::max(Humidity, HumidityMax)); };
	int GetBattery(void) const { return(Battery); };
	ThermometerType GetModel(void) const { return(Model); };
	const std::string GetModelAsString(void) const { return(ThermometerType2String(Model)); };
	ThermometerType SetModel(const std::string& Name);
	ThermometerType SetModel(const unsigned short* UUID);
	ThermometerType SetModel(const ThermometerType newModel) { ThermometerType oldModel = Model; Model = newModel; return(oldModel); };
	enum granularity { day, week, month, year };
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

std::string ba2string(const bdaddr_t& TheBlueToothAddress);
bdaddr_t string2ba(const std::string& TheBlueToothAddressString);
bool operator <(const bdaddr_t& a, const bdaddr_t& b);
bool operator ==(const bdaddr_t& a, const bdaddr_t& b);
bool GenerateLogFile(std::map<bdaddr_t, std::queue<Govee_Temp>>& AddressTemperatureMap, std::map<bdaddr_t, time_t>& PersistenceData);
void GenerateCacheFile(std::map<bdaddr_t, std::vector<Govee_Temp>>& AddressTemperatureMap);
void UpdateMRTGData(const bdaddr_t& TheAddress, const Govee_Temp& TheValue);
void MonitorLoggedData(const int SecondsRecent = 35 * 60);
void WriteAllSVG();
