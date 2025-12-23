NAME := bolt
SOURCE_DIR := src
HEADER_DIR := headers
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj/$(NAME)
BIN := $(BUILD_DIR)/$(NAME)

CC := gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -O3 -flto

HEADERS := $(wildcard $(HEADER_DIR)/*.h)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)
OBJECTS := $(addprefix $(OBJ_DIR)/, $(notdir $(SOURCES:.c=.o)))

$(BIN): $(OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c $(HEADERS)
	@mkdir -p $(OBJ_DIR)
	$(CC) -c $(CFLAGS) -o $@ $<

.PHONY: clean default
clean:
	rm -rf $(BUILD_DIR)

default: $(BIN)
