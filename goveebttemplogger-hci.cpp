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

