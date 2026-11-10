CC ?= gcc

CFLAGS ?= -Wall -Wextra -g3 -O2
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
CFLAGS += -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -I./include -I./include/lsp_src

LDLIBS := -lcjson -lncursesw -ldl -lm

OUT := build/main

SRC := src/main.c \
	src/screen_state/editor_state.c \
	src/screen_state/editor_edit_screen.c \
	src/screen_state/editor_file_browse_screen.c \
	src/screen_state/editor_line_jump_screen.c \
	src/screen_state/editor_error_screen.c \
	src/screen_state/editor_make_file_screen.c \
	src/screen_state/editor_start_menu_screen.c \
	src/screen_state/editor_settings_screen.c \
	src/screen_state/editor_filetree_screen.c \
	src/screen_state/editor_command_mode.c \
	src/editor_func.c \
	src/editor_draw.c \
	src/editor_file.c \
	src/editor_syntax.c \
	src/editor_color.c \
	src/editor_mouse.c \
	src/json_read.c \
	src/editor_log.c \
	src/kbd_input_log.c \
	src/path_util.c \
	src/ftj.c \
	src/lsp_src/language_server_communication.c \
	src/settings_parse_src/editor_settings_parse.c \
	src/settings_parse_src/settings_items_parse.c \
	src/settings_parse_src/editor_icon_parse.c \
	src/plugin_src/edit_input_complete.c \
	src/public_settings_src/c_settings_src.c/c_settings_settings.c

SETTINGS_OUT := editor_settings/so_file/settings.so
SETTINGS_SRC := editor_settings/src/settings.c \
	editor_settings/settings_libs/src/sh.c

OBJ_DIR := build/obj
OBJ := $(SRC:%.c=$(OBJ_DIR)/%.o)
DEP := $(OBJ:.o=.d)

.PHONY: all run clean rebuild

all: $(OUT) $(SETTINGS_OUT)

$(SETTINGS_OUT): $(SETTINGS_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I./editor_settings/settings_libs/include -shared -fPIC $^ -o $@
	@echo "Build OK: $@"

$(OUT): $(OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@
	@echo "Build OK: $@"

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(OUT)
	./$(OUT)

clean:
	rm -rf $(OBJ_DIR) $(OUT) $(SETTINGS_OUT)

rebuild: clean all

# -MMDが出す.dファイルでヘッダ変更も検知する
-include $(DEP)
