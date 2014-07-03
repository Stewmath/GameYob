COMMONSOURCES	:=	$(patsubst %,../common/%,$(notdir $(wildcard ../common/*.cpp)))
SDLSOURCES		:=	$(notdir $(wildcard ./*.cpp))
BLIPOBJS		:= $(patsubst %,Blip_Buffer-0.4.0, Blip_Buffer.cpp)
OBJS = $(patsubst %.cpp,%.o,$(SDLSOURCES) $(COMMONSOURCES))

INCLUDE = ../include ../Blip_Buffer-0.4.0 ../Blip_Buffer-0.4.0/sdl_player /usr/include/SDL

CC = g++
DEBUG = -ggdb
CFLAGS = -Wall -O2 `sdl-config --cflags` `sdl-config --libs`  -c $(patsubst %,-I%,$(INCLUDE)) $(DEBUG)
LFLAGS = -Wall $(DEBUG)

all: $(OBJS)
	g++ $(OBJS) -o gameyob

clean:
	\rm *.o ../sdl.a

depend: $(SOURCES)
	$(CC) -M $(CFLAGS) $^ > $@

.cpp.o:
	$(CC) $(CFLAGS) -c $< -o $@


include depend
