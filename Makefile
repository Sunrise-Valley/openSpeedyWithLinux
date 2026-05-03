CC = gcc
CFLAGS = -Wall -Wextra -O2 -fPIC -std=c11
LDFLAGS = -lrt -ldl -lpthread -lm

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TEST_DIR = tests

.PHONY: all clean test install release

all: $(BUILD_DIR)/libopenspeedy.so $(BUILD_DIR)/speedctl \
     $(BUILD_DIR)/test_sleep $(BUILD_DIR)/test_time

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# libopenspeedy.so — LD_PRELOAD library
$(BUILD_DIR)/libopenspeedy.so: $(SRC_DIR)/libopenspeedy.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< -I$(INC_DIR) $(LDFLAGS)

# speedctl — CLI controller
$(BUILD_DIR)/speedctl: $(SRC_DIR)/speedctl.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR) -lrt

# Test programs
$(BUILD_DIR)/test_sleep: $(TEST_DIR)/test_sleep.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR)

$(BUILD_DIR)/test_time: $(TEST_DIR)/test_time.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -I$(INC_DIR)

test: all
	@echo "=== Test without speed modifier ==="
	./$(BUILD_DIR)/test_sleep
	@echo ""
	@echo "=== Test with 2x speed ==="
	./$(BUILD_DIR)/speedctl set 2.0
	LD_PRELOAD=./$(BUILD_DIR)/libopenspeedy.so ./$(BUILD_DIR)/test_sleep
	./$(BUILD_DIR)/speedctl reset
	@echo ""
	@echo "=== Test CLOCK_MONOTONIC scaling ==="
	./$(BUILD_DIR)/speedctl set 2.0
	LD_PRELOAD=./$(BUILD_DIR)/libopenspeedy.so ./$(BUILD_DIR)/test_time
	./$(BUILD_DIR)/speedctl reset

clean:
	rm -rf $(BUILD_DIR)

# Install to system (needs sudo or INSTALL_DIR override)
install: all
	INSTALL_DIR="$${INSTALL_DIR:-/usr/local}" bash gui/install.sh

# Build release tarball
release: all
	@rm -rf /tmp/openspeedy-release
	@mkdir -p /tmp/openspeedy-release/openSpeedy-linux-v0.2.0
	@cp $(BUILD_DIR)/libopenspeedy.so $(BUILD_DIR)/speedctl \
	    gui/openspeedy-gui gui/openspeedy.desktop \
	    README.md /tmp/openspeedy-release/openSpeedy-linux-v0.2.0/
	@chmod +x /tmp/openspeedy-release/openSpeedy-linux-v0.2.0/openspeedy-gui
	@cd /tmp/openspeedy-release && tar czf openSpeedy-linux-v0.2.0.tar.gz openSpeedy-linux-v0.2.0/
	@echo "Release: /tmp/openspeedy-release/openSpeedy-linux-v0.2.0.tar.gz"
	@ls -lh /tmp/openspeedy-release/openSpeedy-linux-v0.2.0.tar.gz
