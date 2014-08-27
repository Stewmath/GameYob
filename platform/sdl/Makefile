COMMONSOURCES	:=	$(patsubst %,../common/%,$(notdir $(wildcard ../common/*.cpp)))
SDLSOURCES		:=	$(notdir $(wildcard ./*.cpp))
BLIPOBJS		:= $(patsubst %,Blip_Buffer-0.4.0, Blip_Buffer.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SDLSOURCES) $(COMMONSOURCES))

INCLUDE = ../include Blip_Buffer-0.4.0 Blip_Buffer-0.4.0/sdl_player /usr/include/SDL

CC = g++

DEBUG = -ggdb

CFLAGS = -Wall -include "typedefs.h" `sdl-config --cflags` \
		 -DVERSION_STRING=\"`git describe --always --abbrev=4`\" \
		 -c $(patsubst %,-I%,$(INCLUDE)) $(DEBUG) \

LFLAGS = -Wall `sdl-config --libs` -lGL Blip_Buffer-0.4.0/libBlip.a $(DEBUG)



all: $(OBJS)
	g++ $(OBJS) $(LFLAGS) -o gameyob

clean:
	\rm *.o ../common/*.o

depend: $(SOURCES)
	$(CC) -M $(CFLAGS) $^ > $@

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@


include depend
