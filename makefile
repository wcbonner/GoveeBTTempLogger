
goveebttemplogger: goveebttemplogger.cpp
	mkdir -p GoveeBTTempLogger/usr/local/bin
	g++ -lbluetooth goveebttemplogger.cpp -o GoveeBTTempLogger/usr/local/bin/goveebttemplogger

deb: goveebttemplogger GoveeBTTempLogger/DEBIAN/control GoveeBTTempLogger/etc/systemd/system/goveebttemplogger.service
	mkdir -p GoveeBTTempLogger/var/log/goveebttemplogger
	touch GoveeBTTempLogger/var/log/goveebttemplogger/gvh507x.txt
	dpkg-deb --build GoveeBTTempLogger
