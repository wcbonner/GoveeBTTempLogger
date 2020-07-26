
GoveeBTTempLogger/usr/local/bin/goveebttemplogger: goveebttemplogger.cpp
	mkdir -p GoveeBTTempLogger/usr/local/bin
	g++ -O3 -lbluetooth goveebttemplogger.cpp -o GoveeBTTempLogger/usr/local/bin/goveebttemplogger

deb: GoveeBTTempLogger/usr/local/bin/goveebttemplogger GoveeBTTempLogger/DEBIAN/control GoveeBTTempLogger/etc/systemd/system/goveebttemplogger.service
	mkdir -p GoveeBTTempLogger/var/log/goveebttemplogger
	touch GoveeBTTempLogger/var/log/goveebttemplogger/gvh507x.txt
	chmod a+x GoveeBTTempLogger/DEBIAN/postinst GoveeBTTempLogger/DEBIAN/postrm GoveeBTTempLogger/DEBIAN/prerm
	dpkg-deb --build GoveeBTTempLogger
