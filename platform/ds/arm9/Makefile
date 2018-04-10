#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

TARGET :=	gameyob
#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		:=	build
SOURCES		:=	source ../../common
INCLUDES	:=	include ../include ../../common/include
DATA		:=
GFX			:= gfx/

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	# -mthumb -mthumb-interwork	#This may cause some problem when there are some cross-calls between C&Assembly. It also ends up being slower than non-Thumb code.

DEBUG	:=	-g

CFLAGS	:=	-Wall -Wno-psabi -DNO_VIZ \
 			-march=armv5te -mtune=arm946e-s -fomit-frame-pointer\
			-ffast-math -O3 $(ARCH) $(DEBUG) \
			-DC_IO_FUNCTIONS -DNIFI \
			-include "nds/ndstypes.h"

CFLAGS	+=	$(INCLUDE) -DARM9 -DDS
CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	$(ARCH) -march=armv5te -mtune=arm946e-s

LDFLAGS	=	-specs=ds_arm9.specs $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:=	-lfat -ldsnifi9 -lnds9
 
#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CURDIR) ../ $(LIBNDS)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------
 
export ARM9ELF	:=	$(CURDIR)/$(TARGET).elf
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
					$(foreach dir,$(GFX),$(CURDIR)/$(dir))
 
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
GFX_FILES   :=  $(foreach dir, $(GFX),$(notdir $(wildcard $(dir)/*.png)))
 
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

export OFILES	:=	$(GFX_FILES:.png=.o) \
					$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
 
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)
 
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
 
.PHONY: $(BUILD) clean
 
#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) *.elf *.nds* *.bin 
 

 
#---------------------------------------------------------------------------------
else
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(ARM9ELF)	:	$(OFILES)
	@echo linking $(notdir $@)
	@$(LD)  $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@


# Always rebuild version.cpp
.FORCE:
version.o: .FORCE


#---------------------------------------------------------------------------------
%.s %.h : %.png
	grit $< -ff../gfx/icon.grit -o$*
#---------------------------------------------------------------------------------


#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
