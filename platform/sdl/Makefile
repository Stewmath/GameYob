# GameYob SDL Makefile

CC = gcc
CXX = g++

TARGET :=	gameyob
#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# LIBDIRS is a list of directories containing libraries
# DATA is a list of directories containing binary files
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		:=	build
SOURCES		:= . ../common Blip_Buffer-0.4.0 Blip_Buffer-0.4.0/sdl_player
INCLUDES	:= ../common/include Blip_Buffer-0.4.0 Blip_Buffer-0.4.0/sdl_player /usr/include/SDL
LIBDIRS		:= 
DATA		:=

DEBUG = -ggdb

CXXFLAGS =	-O2 -Wall `sdl-config --cflags` $(DEBUG) \
			-include "typedefs.h" \
			-DVERSION_STRING=\"`git describe --always --abbrev=4`\" \
			-DSDL -DC_IO_FUNCTIONS \
			$(INCLUDE)

LDFLAGS = -Wall `sdl-config --libs` -lGL $(DEBUG)



ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
 
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
 
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
 
#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export MAKEDIR	:= $(CURDIR)
 
.PHONY: $(BUILD) clean
 
#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET)
 

 
#---------------------------------------------------------------------------------
else
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(MAKEDIR)/$(TARGET):	$(OFILES)
	@echo linking $(notdir $@)
	@$(LD)  $(LDFLAGS) $(LIBPATHS) $(OFILES) -o $@


%.o: %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $*.d $(CFLAGS) -c $< -o $@

%.o: %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $*.d $(CXXFLAGS) -c $< -o $@

# Always rebuild version.cpp
.FORCE:
version.o: .FORCE


-include $(DEPSDIR)/*.d
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
