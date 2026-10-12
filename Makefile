TARGET = player

CC = gcc

CFLAGS = -Wall -Wextra -pedantic -std=c11 -Iinclude

PKG_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_mixer SDL2_ttf taglib_c)

PKG_LIBS = $(shell pkg-config --libs sdl2 SDL2_mixer SDL2_ttf taglib_c)

ALL_CFLAGS = $(CFLAGS) $(PKG_CFLAGS)

ALL_LIBS = $(PKG_LIBS)

SRC_DIR = src
BUILD_DIR = build

SRCS = $(wildcard $(SRC_DIR)/*.c)

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(ALL_LIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean