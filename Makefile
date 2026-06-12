# Compiler and tools
CROSS = arm-none-eabi-
CC = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
PYTHON ?= python3

# Target name
TARGET = regravity_defied

# Build directories
SRC_DIR = src
BUILD_DIR = build

# Compiler flags
CFLAGS = -mthumb -mthumb-interwork -mlittle-endian -mcpu=arm7tdmi \
         -mtune=arm7tdmi -fno-strict-aliasing -fno-exceptions \
         -O3 -Wall

# Linker flags
LDFLAGS = -mthumb -mthumb-interwork -nostartfiles -T gba.ld

# Source files
SRCS_C = $(wildcard $(SRC_DIR)/*.c)
SRCS_S = $(wildcard $(SRC_DIR)/*.s)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS_C)) \
       $(patsubst $(SRC_DIR)/%.s, $(BUILD_DIR)/%.o, $(SRCS_S)) \
       $(BUILD_DIR)/levels.o

# Default target
all: $(TARGET).gba

# Regenerate the committed asset headers from assets/ (needs Pillow + miniaudio).
.PHONY: assets
assets:
	$(PYTHON) tools/convert_assets.py
	$(PYTHON) tools/convert_sound.py

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile C source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Embed levels.mrg
$(BUILD_DIR)/levels.o: levels.mrg | $(BUILD_DIR)
	$(OBJCOPY) -I binary -O elf32-littlearm -B arm --rename-section .data=.rodata,alloc,load,readonly,data,contents $< $@

# Link object files to ELF file
$(TARGET).elf: $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

# Convert ELF file to GBA ROM
$(TARGET).gba: $(TARGET).elf
	$(OBJCOPY) -v -O binary $< $@
	$(PYTHON) gbafix.py $@

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(TARGET).elf $(TARGET).gba

debug: CFLAGS += -DDEBUG
debug: all

.PHONY: all clean debug
