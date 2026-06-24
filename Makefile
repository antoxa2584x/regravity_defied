# Bare-metal Game Boy Advance build (arm-none-eabi, no SDK).
#
# Compiles the portable game core (src/core) + generated assets (src/generated)
# with the GBA hardware backend (src/platform/gba) into one ROM per levels/*.mrg,
# emitted to release/gba/. Object files live under build/gba/<mod>/.
#
#   make            # one ROM per levels/*.mrg  ->  release/gba/ReGravity_Defied_<mod>_v<ver>.gba
#   make assets     # regenerate src/generated/gd_assets.h + gd_sound.h from assets/
#   make debug      # build with mGBA debug logging (-DDEBUG)
#   make clean      # remove build/gba and release/gba

CROSS   = arm-none-eabi-
CC      = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
PYTHON ?= python3

TARGET  = ReGravity_Defied
VERSION = 1.0

# Source layout: portable core + generated assets + the GBA platform backend.
CORE_DIR    = src/core
GEN_DIR     = src/generated
PLAT_DIR    = src/platform/gba
BUILD_DIR   = build/gba
RELEASE_DIR = release/gba
LEVELS_DIR  = levels

INCLUDE = -I$(CORE_DIR) -I$(GEN_DIR) -I$(PLAT_DIR)

# Each levels/*.mrg produces its own ROM: release/gba/$(TARGET)_<name>_v<ver>.gba
LEVEL_FILES = $(wildcard $(LEVELS_DIR)/*.mrg)
MODS = $(patsubst $(LEVELS_DIR)/%.mrg,%,$(LEVEL_FILES))
ROMS = $(foreach m,$(MODS),$(RELEASE_DIR)/$(TARGET)_$(m)_v$(VERSION).gba)

CFLAGS = -mthumb -mthumb-interwork -mlittle-endian -mcpu=arm7tdmi \
         -mtune=arm7tdmi -fno-strict-aliasing -fno-exceptions \
         -O3 -Wall -DGAME_VERSION='"$(VERSION)"' $(INCLUDE)
LDFLAGS = -mthumb -mthumb-interwork -nostartfiles -T $(PLAT_DIR)/gba.ld

# Portable core + GBA backend C sources, plus the GBA startup assembly.
SRCS_C = $(wildcard $(CORE_DIR)/*.c) $(wildcard $(PLAT_DIR)/*.c)
SRCS_S = $(wildcard $(PLAT_DIR)/*.s)

all: $(ROMS)

# Regenerate the committed asset headers from assets/ (needs Pillow + miniaudio).
.PHONY: assets
assets:
	$(PYTHON) tools/convert_assets.py
	$(PYTHON) tools/convert_sound.py

# Per-mod build rules. Each levels/<mod>.mrg compiles into build/gba/<mod>/ (so
# MOD_NAME differs per ROM), embeds that mod's level data, links a .elf, and
# objcopies + fixes up a release/gba/$(TARGET)_<mod>_v<ver>.gba ROM. Object base
# names are unique across core/ and platform/gba/, so they flatten into one dir.
define MOD_template
$(1)_OBJS = $$(addprefix $$(BUILD_DIR)/$(1)/, $$(notdir $$(SRCS_C:.c=.o)) $$(notdir $$(SRCS_S:.s=.o))) \
            $$(BUILD_DIR)/$(1)/levels.o

$$(BUILD_DIR)/$(1)/%.o: $$(CORE_DIR)/%.c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -DMOD_NAME='"$(1)"' -c $$< -o $$@

$$(BUILD_DIR)/$(1)/%.o: $$(PLAT_DIR)/%.c
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -DMOD_NAME='"$(1)"' -c $$< -o $$@

$$(BUILD_DIR)/$(1)/%.o: $$(PLAT_DIR)/%.s
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -c $$< -o $$@

# Embed this mod's level data. Copy to a fixed "levels.mrg" name and run objcopy
# from that directory so the generated symbol is always _binary_levels_mrg_start
# regardless of the source mod name.
$$(BUILD_DIR)/$(1)/levels.o: $$(LEVELS_DIR)/$(1).mrg
	@mkdir -p $$(dir $$@)
	cp $$< $$(BUILD_DIR)/$(1)/levels.mrg
	cd $$(BUILD_DIR)/$(1) && $$(OBJCOPY) -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata,alloc,load,readonly,data,contents levels.mrg levels.o

$$(BUILD_DIR)/$(1)/$(TARGET).elf: $$($(1)_OBJS)
	$$(CC) $$($(1)_OBJS) $$(LDFLAGS) -o $$@

$$(RELEASE_DIR)/$(TARGET)_$(1)_v$(VERSION).gba: $$(BUILD_DIR)/$(1)/$(TARGET).elf
	@mkdir -p $$(RELEASE_DIR)
	$$(OBJCOPY) -O binary $$< $$@
	$$(PYTHON) gbafix.py $$@ -t "REGRAVITY" -c "RGDE" -m "00" -v $(VERSION)
endef

$(foreach m,$(MODS),$(eval $(call MOD_template,$(m))))

clean:
	rm -rf $(BUILD_DIR) $(RELEASE_DIR)

debug: CFLAGS += -DDEBUG
debug: all

.PHONY: all clean debug
