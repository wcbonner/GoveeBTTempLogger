
goveebttemplogger: goveebttemplogger.cpp
	g++ -lbluetooth goveebttemplogger.cpp -o GoveeBTTempLogger/usr/local/bin/goveebttemplogger

deb: goveebttemplogger GoveeBTTempLogger/DEBIAN/control
	dpkg-deb --build GoveeBTTempLogger
