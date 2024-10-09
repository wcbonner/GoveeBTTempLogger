#pragma once

void HCI_BlueZ_MainLoop(std::string& ControllerAddress, std::set<bdaddr_t>& BT_WhiteList, int& ExitValue, bool bMonitorLoggingDirectory);
