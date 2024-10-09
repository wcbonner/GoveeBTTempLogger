#pragma once

int hci_le_set_ext_scan_parameters(int dd, uint8_t type, uint16_t interval, uint16_t window, uint8_t own_type, uint8_t filter, int to);
int hci_le_set_ext_scan_enable(int dd, uint8_t enable, uint8_t filter_dup, int to);
int hci_le_set_random_address(int dd, int to);
