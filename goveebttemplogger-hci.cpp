#include <algorithm>
#include <arpa/inet.h>
#include <bluetooth/bluetooth.h> // apt install libbluetooth-dev
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <cfloat>
#include <climits>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dbus/dbus.h> //  sudo apt install libdbus-1-dev
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <queue>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // For close()
#include <utime.h>
#include <vector>
#include "wimiso8601.h"
#include "att-types.h"
#include "uuid.h"
#include "goveebttemplogger.h"
#include "goveebttemplogger-hci.h"

/////////////////////////////////////////////////////////////////////////////
#ifndef BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS
#define BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS		0x2041
int hci_le_set_ext_scan_parameters(int dd, uint8_t type, uint16_t interval, uint16_t window, uint8_t own_type, uint8_t filter, int to)
{
	struct bt_hci_cmd_le_set_ext_scan_params {
		uint8_t  own_addr_type;
		uint8_t  filter_policy;
		uint8_t  num_phys;
		uint8_t  type;
		uint16_t interval;
		uint16_t window;
	} __attribute__((packed)) param_cp;
	memset(&param_cp, 0, sizeof(param_cp));
	param_cp.type = type;
	param_cp.interval = interval;
	param_cp.window = window;
	param_cp.own_addr_type = own_type;
	param_cp.filter_policy = filter;
	param_cp.num_phys = 1;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS;
	rq.cparam = &param_cp;
	rq.clen = sizeof(bt_hci_cmd_le_set_ext_scan_params);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // !BT_HCI_CMD_LE_SET_EXT_SCAN_PARAMS
#ifndef BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE
#define BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE		0x2042
int hci_le_set_ext_scan_enable(int dd, uint8_t enable, uint8_t filter_dup, int to)
{
	struct bt_hci_cmd_le_set_ext_scan_enable {
		uint8_t  enable;
		uint8_t  filter_dup;
		uint16_t duration;
		uint16_t period;
	} __attribute__((packed)) scan_cp;
	memset(&scan_cp, 0, sizeof(scan_cp));
	scan_cp.enable = enable;
	scan_cp.filter_dup = filter_dup;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE;
	rq.cparam = &scan_cp;
	rq.clen = sizeof(scan_cp);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // !BT_HCI_CMD_LE_SET_EXT_SCAN_ENABLE
#ifndef BT_HCI_CMD_LE_SET_RANDOM_ADDRESS
// 2023-11-29 Added this function to fix problem with Raspberry Pi Zero 2 W Issue https://github.com/wcbonner/GoveeBTTempLogger/issues/50
int hci_le_set_random_address(int dd, int to)
{
	le_set_random_address_cp scan_cp{ 0 };
	std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count());	// 2023-12-01 switch to c++ std library <random>
	for (auto& b : scan_cp.bdaddr.b)
		b = generator() % 256;
	uint8_t status;
	struct hci_request rq;
	memset(&rq, 0, sizeof(rq));
	rq.ogf = OGF_LE_CTL;
	rq.ocf = OCF_LE_SET_RANDOM_ADDRESS;
	rq.cparam = &scan_cp;
	rq.clen = sizeof(scan_cp);
	rq.rparam = &status;
	rq.rlen = 1;
	if (hci_send_req(dd, &rq, to) < 0)
		return -1;
	if (status) {
		errno = EIO;
		return -1;
	}
	return 0;
}
#endif // BT_HCI_CMD_LE_SET_RANDOM_ADDRESS
/////////////////////////////////////////////////////////////////////////////
