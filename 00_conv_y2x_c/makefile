# ============================
#  OS judge
# ============================

TARGET   ?= regmap

SCHEMA         := ../10_schema/schema_$(TARGET).yaml
GENERATED_PATH := user_tools/tool_$(TARGET)
GENERATED_HDR  := user_tools/tool_$(TARGET)/generated_typical.h
GENERATED_SRC  := user_tools/tool_$(TARGET)/generated_typical.c

GENERATED_OBJ  := obj/tool_$(TARGET)/generated_typical.o

LIBYAML_DIR := 3rd_party/libyaml
OBJ_DIR := obj

UNAME_S := $(shell uname -s 2>/dev/null || echo Windows_CMD)
ifneq ($(findstring CYGWIN,$(UNAME_S)),)
	IS_POSIX = 1
else ifneq ($(findstring MINGW,$(UNAME_S)),)
	IS_POSIX = 1
else ifeq ($(UNAME_S),Linux)
	IS_POSIX = 1
else
	IS_POSIX = 0
endif

YAML_DEFINES =	-DYAML_DECLARE_STATIC \
				-DYAML_VERSION_MAJOR=0 \
				-DYAML_VERSION_MINOR=2 \
				-DYAML_VERSION_PATCH=5 \
				-DYAML_VERSION_STRING=\"0.2.5\"



ifeq ($(IS_POSIX),1)
	# --- Linux / Cygwin / MSYS2 / Git Bash ---
	CC = gcc
	GEN_EXE = bin/conv_y2x_gen_typical.elf
	APP_EXE = bin/conv_y2x_tool_$(TARGET).elf
	RM = rm -f -r
	MKDIR = mkdir -p
	CFLAGS = -I$(LIBYAML_DIR)/include -I$(LIBYAML_DIR) $(YAML_DEFINES) -g -Wall
	LDFLAGS = 
else
	# --- pure Windows cmd.exe ---
	CC = gcc
	GEN_EXE = bin/conv_y2x_gen_typical.exe
	APP_EXE = bin/conv_y2x_tool_$(TARGET).exe
	RM = del /Q 2>NUL || true
	MKDIR = if not exist bin mkdir
	CFLAGS = -I"mingw/include" -I$(LIBYAML_DIR)/include -I$(LIBYAML_DIR) $(YAML_DEFINES) -g -Wall
	LDFLAGS = -L"mingw/lib"
endif

# ============================
#  src
# ============================
YAML_SRC = $(wildcard $(LIBYAML_DIR)/src/*.c)
YAML_OBJ = $(YAML_SRC:$(LIBYAML_DIR)/src/%.c=obj/libyaml_%.o)

GEN_SRCS := pre_gen/main_gen_typical.c
GEN_OBJS := $(GEN_SRCS:%.c=obj/%.o) $(YAML_OBJ)

APP_SRCS := $(wildcard user_tools/*.c user_tools/tool_$(TARGET)/*.c)
APP_OBJS := $(patsubst %.c, $(OBJ_DIR)/%.o, $(APP_SRCS)) $(GENERATED_OBJ) $(YAML_OBJ)

# ============================
#  buld rule
# ============================
.PHONY: all clean run

all: $(APP_EXE)

# make tool.exe -------------
$(APP_EXE): $(APP_OBJS)
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)


# related APP_EXE and GEN_EXE ------------
$(GENERATED_OBJ): $(GENERATED_SRC) $(GENERATED_HDR)
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# run gen_typical.exe -------------
$(GENERATED_HDR) $(GENERATED_SRC): $(GEN_EXE) 
	./$(GEN_EXE) $(SCHEMA) $(GENERATED_PATH) 

# make gen_typical.exe ------------
$(GEN_EXE): $(GEN_OBJS)
	$(MKDIR) bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

obj/libyaml_%.o: $(LIBYAML_DIR)/src/%.c
	$(MKDIR) obj
	$(CC) $(CFLAGS) -c $< -o $@

# --------------------------
$(OBJ_DIR)/%.o: %.c
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(APP_EXE)
	./$(APP_EXE) user_config.yaml

clean:
	$(RM) obj/*
	$(RM) bin/*
