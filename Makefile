# Sharp68000Box360 v5.2b — libXenon cloud build
.SUFFIXES:

ifeq ($(strip $(DEVKITXENON)),)
$(error "DEVKITXENON must point to the libXenon devkit directory")
endif

include $(DEVKITXENON)/rules

TARGET   := xenon
BUILD    := build-xenon
SOURCES  := source
DATA     :=
INCLUDES :=

CFLAGS   := -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
CXXFLAGS := $(CFLAGS)
LDFLAGS  := $(MACHDEP) -Wl,--gc-sections -Wl,-Map,$(TARGET).map
LIBS     := -lxenon -lm
LIBDIRS  :=

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT  := $(CURDIR)/$(TARGET)
export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) -I$(CURDIR)/$(BUILD) -I$(LIBXENON_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBXENON_LIB)

.PHONY: all clean
all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).elf32 $(OUTPUT).map

else
DEPENDS := $(OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).elf32

$(OUTPUT).elf32: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)
endif
