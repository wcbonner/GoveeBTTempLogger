
GoveeBTTempLogger/usr/local/bin/goveebttemplogger: goveebttemplogger.cpp
	mkdir -p GoveeBTTempLogger/usr/local/bin
	g++ -Wno-psabi -O3 -std=c++11 goveebttemplogger.cpp -o GoveeBTTempLogger/usr/local/bin/goveebttemplogger -lbluetooth

deb: GoveeBTTempLogger/usr/local/bin/goveebttemplogger GoveeBTTempLogger/DEBIAN/control GoveeBTTempLogger/usr/local/lib/systemd/system/goveebttemplogger.service
	chmod a+x GoveeBTTempLogger/DEBIAN/postinst GoveeBTTempLogger/DEBIAN/postrm GoveeBTTempLogger/DEBIAN/prerm
	dpkg-deb --build GoveeBTTempLogger
