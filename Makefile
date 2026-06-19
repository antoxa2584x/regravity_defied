# Compiler and tools
CROSS = arm-none-eabi-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
PYTHON ?= python3

# Target name
TARGET = ReGravity_Defied

# Game version, baked in via -DGAME_VERSION and shown on the About screen.
VERSION = 0.9

# Build directories
SRC_DIR = src
BUILD_DIR = build
LEVELS_DIR = levels

# Each levels/*.mrg produces its own ROM: $(TARGET)_<name>_v<version>.gba
LEVEL_FILES = $(wildcard $(LEVELS_DIR)/*.mrg)
MODS = $(patsubst $(LEVELS_DIR)/%.mrg,%,$(LEVEL_FILES))
ROMS = $(foreach m,$(MODS),$(TARGET)_$(m)_v$(VERSION).gba)

# Compiler flags
CFLAGS = -mthumb -mthumb-interwork -mlittle-endian -mcpu=arm7tdmi \
         -mtune=arm7tdmi -fno-strict-aliasing -fno-exceptions \
         -O3 -Wall -DGAME_VERSION='"$(VERSION)"'

# Linker flags
LDFLAGS = -mthumb -mthumb-interwork -nostartfiles -T gba.ld

# Source files. Portable game code + the *_gba.c hardware backends; the *_nds.c
# backends are for the native DS build only (see Makefile.nds).
SRCS_C = $(filter-out %_nds.c,$(wildcard $(SRC_DIR)/*.c))
SRCS_S = $(wildcard $(SRC_DIR)/*.s)

# Default target: build one ROM per levels/*.mrg
all: $(ROMS)

# Regenerate the committed asset headers from assets/ (needs Pillow + miniaudio).
.PHONY: assets
assets:
	$(PYTHON) tools/convert_assets.py
	$(PYTHON) tools/convert_sound.py

# Per-mod build rules. For each levels/<mod>.mrg we compile the sources into a
# dedicated build/<mod>/ directory (so MOD_NAME differs per ROM), embed that
# mod's level data, and link a separate $(TARGET)_<mod>_v<version>.gba ROM.
define MOD_template
$(1)_OBJS = $$(patsubst $$(SRC_DIR)/%.c, $$(BUILD_DIR)/$(1)/%.o, $$(SRCS_C)) \
            $$(patsubst $$(SRC_DIR)/%.s, $$(BUILD_DIR)/$(1)/%.o, $$(SRCS_S)) \
            $$(BUILD_DIR)/$(1)/levels.o

# Compile C sources with this mod's name baked in for the on-screen label.
$$(BUILD_DIR)/$(1)/%.o: $$(SRC_DIR)/%.c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -DMOD_NAME='"$(1)"' -c $$< -o $$@

# Assemble assembly sources.
$$(BUILD_DIR)/$(1)/%.o: $$(SRC_DIR)/%.s
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -c $$< -o $$@

# Embed this mod's level data. Copy to a fixed "levels.mrg" name and run objcopy
# from that directory so the generated symbol is always _binary_levels_mrg_start
# regardless of the source mod name.
$$(BUILD_DIR)/$(1)/levels.o: $$(LEVELS_DIR)/$(1).mrg
	@mkdir -p $$(dir $$@)
	cp $$< $$(BUILD_DIR)/$(1)/levels.mrg
	cd $$(BUILD_DIR)/$(1) && $$(OBJCOPY) -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata,alloc,load,readonly,data,contents levels.mrg levels.o

$(TARGET)_$(1)_v$(VERSION).elf: $$($(1)_OBJS)
	$$(CC) $$($(1)_OBJS) $$(LDFLAGS) -o $$@

$(TARGET)_$(1)_v$(VERSION).gba: $(TARGET)_$(1)_v$(VERSION).elf
	$$(OBJCOPY) -v -O binary $$< $$@
	$$(PYTHON) gbafix.py $$@ -t "REGRAVITY" -c "RGDE" -m "00" -v $(VERSION)
endef

$(foreach m,$(MODS),$(eval $(call MOD_template,$(m))))

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(foreach m,$(MODS),$(TARGET)_$(m)_v$(VERSION).elf $(TARGET)_$(m)_v$(VERSION).gba)

debug: CFLAGS += -DDEBUG
debug: all

.PHONY: all clean debug
