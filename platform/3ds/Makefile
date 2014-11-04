#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

ifeq ($(strip $(CTRULIB)),)
# THIS IS TEMPORARY - in the future it should be at $(DEVKITPRO)/libctru
$(error "Please set CTRULIB in your environment. export CTRULIB=<path to>libctru")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules


#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# SPECS is the directory containing the important build and link files
#---------------------------------------------------------------------------------
export TARGET		:=	gameyob
BUILD		:=	build
SOURCES		:=	. ../common
DATA		:=	data
INCLUDES	:=	include ../common/include


EMBEDDED_ROM := false

export	OUTPUT_FORMAT ?= 3dsx

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-marm

CFLAGS	:=	-Wall -O3 -mthumb-interwork -save-temps \
			-mcpu=mpcore -mtune=mpcore -fomit-frame-pointer \
			-mfpu=vfp -ffast-math -mword-relocations \
			$(ARCH)

CFLAGS	+=	-DVERSION_STRING=\"`git describe --always --abbrev=4`\"\
			-include "3ds/types.h"

CFLAGS	+=	$(INCLUDE) -DARM11 -D_3DS

ifeq ($(EMBEDDED_ROM), true)
# Embedded rom from file "data/rom.gb"
CFLAGS	+=	-DEMBEDDED_ROM
endif


CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) \
			-Wl,-d,-q,--use-blx,-Map,$(TARGET).map

LIBS	:= -lctru

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB)
 
  
#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR))) # make not invoked from "build" directory
#---------------------------------------------------------------------------------
 
export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

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

ifeq ($(EMBEDDED_ROM), true)

ifeq ($(filter rom.gb.o,$(OFILES)),)
$(error $(CURDIR)/data/rom.gb not found)
endif

endif

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all
 
#---------------------------------------------------------------------------------
all: $(BUILD)

# invoke make from the "build" directory to run the 2nd part of this makefile
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dhb $(TARGET).elf $(TARGET).3ds $(TARGET).3dsx
 

cia:
	@make $(MAKEFLAGS) OUTPUT_FORMAT=cia

3ds:
	@make $(MAKEFLAGS) OUTPUT_FORMAT=3ds

3dsx:
	@make $(MAKEFLAGS) OUTPUT_FORMAT=3dsx
 
#---------------------------------------------------------------------------------
else # make invoked from "build" directory
 
DEPENDS	:=	$(OFILES:.o=.d)
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------

.PHONY: all

all: $(OUTPUT).$(OUTPUT_FORMAT)

$(OUTPUT).3dsx	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OUTPUT).cia: $(OUTPUT).elf
	@cp $(OUTPUT).elf $(TARGET)_stripped.elf
	@$(PREFIX)strip $(TARGET)_stripped.elf
	makerom -f cia -o $(OUTPUT).cia -rsf $(TOPDIR)/resources/build_cia.rsf -target t -exefslogo -elf $(TARGET)_stripped.elf -icon $(TOPDIR)/resources/icon.icn -banner $(TOPDIR)/resources/banner.bnr
	@echo "built ... $(notdir $@)"

$(OUTPUT).3ds: $(OUTPUT).elf $(TOPDIR)/resources/gw_workaround.rsf $(TOPDIR)/resources/banner.bnr $(TOPDIR)/resources/icon.icn
	@cp $(OUTPUT).elf $(TARGET)_stripped.elf
	@$(PREFIX)strip $(TARGET)_stripped.elf
	makerom -f cci -o $(OUTPUT).3ds -rsf $(TOPDIR)/resources/gw_workaround.rsf -target d -exefslogo -elf $(TARGET)_stripped.elf -icon $(TOPDIR)/resources/icon.icn -banner $(TOPDIR)/resources/banner.bnr
	@echo "built ... $(notdir $@)"

# Always rebuild version.cpp
.FORCE:
version.o: .FORCE

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

%.gb.o	:	%.gb
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------

