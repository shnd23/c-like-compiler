CC ?= cc
BUILD ?= build
STD := -std=c11
WARN := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Wformat=2
CFLAGS ?= -O2
ALLFLAGS := $(STD) $(WARN) $(CFLAGS) -Iinclude

LIB_SRC := $(filter-out src/main.c,$(wildcard src/*.c))
LIB_OBJ := $(patsubst src/%.c,$(BUILD)/%.o,$(LIB_SRC))
MAIN_OBJ := $(BUILD)/main.o

.PHONY: all test golden unit asan clean

all: $(BUILD)/minic

$(BUILD)/minic: $(LIB_OBJ) $(MAIN_OBJ)
	$(CC) $(ALLFLAGS) -o $@ $^ -lm

$(BUILD)/unit_tests: $(LIB_OBJ) tests/unit.c
	$(CC) $(ALLFLAGS) -D_POSIX_C_SOURCE=200809L -o $@ $^ -lm

$(BUILD)/%.o: src/%.c $(wildcard include/*.h) | $(BUILD)
	$(CC) $(ALLFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

unit: $(BUILD)/unit_tests
	$(BUILD)/unit_tests

golden: $(BUILD)/minic
	sh ../tests/golden.sh check $(CURDIR)/$(BUILD)/minic

test: unit golden

asan:
	$(MAKE) BUILD=build-asan CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test

clean:
	rm -rf build build-asan
