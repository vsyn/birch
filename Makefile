# Copyright 2021 Julian Ingram
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

DEFINES :=

CC := clang
CFLAGS += -g3 -O0 -Werror -Wall -Wextra $(DEFINES:%=-D%)
# expanded below
DEPFLAGS = -MMD -MP -MF $(@:$(BUILD_DIR)/%.o=$(DEP_DIR)/%.d)
LDFLAGS := -g3 -O0
SRCS := bit_arr.c dir_tree.c birch.c birch_main.c
TARGET ?= birch
RM := rm -rf
MKDIR := mkdir -p
CP := cp -r
# BUILD_DIR and DEP_DIR should both have non-empty values
BUILD_DIR ?= build
DEP_DIR ?= $(BUILD_DIR)/deps
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)
DEPS := $(ALL_SRCS:%.c=$(DEP_DIR)/%.d)

.PHONY: all
all: $(TARGET)

# link
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# compile and/or generate dep files
$(BUILD_DIR)/%.o: %.c
	$(MKDIR) $(BUILD_DIR)/$(dir $<)
	$(MKDIR) $(DEP_DIR)/$(dir $<)
	$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(TARGET) $(DEP_DIR) $(BUILD_DIR)

-include $(DEPS)
