CXX ?= g++
CXXFLAGS += -std=c++17

uuid.o: uuid.c uuid.h

GoveeBTTempLogger/usr/local/bin/goveebttemplogger: goveebttemplogger.cpp uuid.o
	mkdir -p $(shell dirname $@)
	$(CXX) -Wno-psabi -O3 $(CXXFLAGS) $? -o$@ -lbluetooth
	sudo setcap 'cap_net_raw,cap_net_admin+eip' $@

GoveeBTTempLogger/usr/local/bin/gvh-organizelogs: gvh-organizelogs.cpp
	$(CXX) -Wno-psabi -O3 $(CXXFLAGS) $? -o$@ -lbluetooth

deb: GoveeBTTempLogger/usr/local/bin/goveebttemplogger GoveeBTTempLogger/usr/local/bin/gvh-organizelogs GoveeBTTempLogger/DEBIAN/control GoveeBTTempLogger/usr/local/lib/systemd/system/goveebttemplogger.service
	# Set architecture for the resulting .deb to the actually built architecture
	sed -i "s/Architecture: .*/Architecture: $(shell dpkg --print-architecture)/" GoveeBTTempLogger/DEBIAN/control
	chmod a+x GoveeBTTempLogger/DEBIAN/postinst GoveeBTTempLogger/DEBIAN/postrm GoveeBTTempLogger/DEBIAN/prerm
	dpkg-deb --build GoveeBTTempLogger
	dpkg-name --overwrite GoveeBTTempLogger.deb
	dpkg-deb --build GoveeBTTempLogger

install-deb: deb
	apt install ./GoveeBTTempLogger_`grep Version: GoveeBTTempLogger/DEBIAN/control | awk '{print $$2}'`_`dpkg --print-architecture`.deb

clean:
	-rm -f uuid.o
	-rm -rf GoveeBTTempLogger/usr/local/bin
	git restore GoveeBTTempLogger/DEBIAN/control

.PHONY: clean deb install-deb
