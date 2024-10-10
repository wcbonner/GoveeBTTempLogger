#pragma once

int hci_le_set_ext_scan_parameters(int dd, uint8_t type, uint16_t interval, uint16_t window, uint8_t own_type, uint8_t filter, int to);
int hci_le_set_ext_scan_enable(int dd, uint8_t enable, uint8_t filter_dup, int to);
int hci_le_set_random_address(int dd, int to);
std::string iBeacon(const uint8_t* const data);
int bt_LEScan(int BlueToothDevice_Handle, const bool enable, const std::set<bdaddr_t>& BT_WhiteList, const bool HCI_Passive_Scanning);
void bt_ListDevices(void);
time_t ConnectAndDownload(int BlueToothDevice_Handle, const bdaddr_t GoveeBTAddress, const time_t GoveeLastReadTime, const int BatteryToRecord);
void BlueZ_HCI_MainLoop(std::string& ControllerAddress, std::set<bdaddr_t>& BT_WhiteList, int& ExitValue, const bool bMonitorLoggingDirectory, const bool HCI_Passive_Scanning);
