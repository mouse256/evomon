
COPTS=-Wall -Werror -g
LIBS=$(shell pkg-config --libs qeo-c)
OBJECTS=build/main.o build/serial.o build/honeywelldecode.o build/evohomeTemperature.o
ifeq ($(TELNET),1)
LIBS+=-lpthread
OBJECTS+=build/telnet.o
endif
DEFINES=

ifeq ($(TESTRUN),1)
  DEFINES+=-DTESTRUN
endif

all: evomonDaemon
clean:
	rm -rf build/*.o
	rm -f evomonDaemon

src/evohomeTemperature.c: src/evohomeTemperature.h
src/serial.c: src/serial.h
src/main.c: src/serial.h src/honeywelldecode.h src/main.h src/evohomeTemperature.h
src/honeywelldecode.c: src/honeywelldecode.h src/main.h src/evohomeTemperature.h

build/.dir_created:
	mkdir -p build
	touch build/.dir_created

build/%.o: src/%.c build/.dir_created
	gcc $(DEFINES) $(COPTS) $(INCLUDES) -o $@ -c $<

evomonDaemon: $(OBJECTS)
	gcc $(COPTS) $(LIBDIRS) -o $@ $^ $(LIBS)

