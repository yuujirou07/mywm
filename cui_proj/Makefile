CC ?= gcc
PKG_CONFIG ?= pkg-config

# raylib のビルドフラグは pkg-config から取得する
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
RAYLIB_LIBS   := $(shell $(PKG_CONFIG) --libs raylib)

CFLAGS ?= -Wall -Wextra -g3 -O2
CFLAGS += -D_DEFAULT_SOURCE -I./include $(RAYLIB_CFLAGS)

# openpty(3) は古い glibc では libutil にある
LDLIBS := $(RAYLIB_LIBS) -lm -lutil

OUT := pty_make_v1

SRC := src/pty_make_v1.c \
	src/escape_sequence_parser.c \
	src/mouse_io.c \
	src/pty_drawing.c \
	src/kbd_io.c \
	src/error_log_opt.c

OBJ_DIR := build/obj
OBJ := $(SRC:%.c=$(OBJ_DIR)/%.o)
DEP := $(OBJ:.o=.d)

.PHONY: all run clean rebuild

all: $(OUT)

# run.sh と同じスケール設定で起動する
run: $(OUT)
	WAYLAND_DISPLAY_SCALE=1 GDK_SCALE=1 QT_AUTO_SCREEN_SCALE_FACTOR=0 ./$(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
	@echo "Build OK: $@"

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUT)

rebuild: clean all

# -MMD が出す .d ファイルでヘッダ変更も検知する
-include $(DEP)
