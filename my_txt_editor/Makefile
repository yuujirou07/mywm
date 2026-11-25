CC ?= gcc

CFLAGS ?= -Wall -Wextra -g3 -O0
# gdb用: 環境変数CFLAGSで上書きされても必ず付くよう += で足す
CFLAGS += -ggdb3 -O0 -fno-inline -fno-omit-frame-pointer
CFLAGS += -fsanitize=address
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
	src/screen_state/file_tree_screen/filetree_data.c \
	src/screen_state/file_tree_screen/filetree_screen.c \
	src/screen_state/file_tree_screen/filetree_draw.c \
	src/screen_state/file_tree_screen/filetree_mouse.c \
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
	src/lsp_src/lsp_communication.c \
	src/lsp_src/lsp_settings.c \
	src/settings_parse_src/editor_settings_parse.c \
	src/settings_parse_src/settings_items_parse.c \
	src/settings_parse_src/editor_icon_parse.c \
	src/plugin_src/edit_input_complete.c \
	src/public_settings_src/c_settings_src.c/c_settings_settings.c

PLUGIN_OUT := editor_settings/so_file/start_menu_plug.so
PLUGIN_SRC := src/plugin_src/start_menu_plug.c \
	src/plugin_src/ascii_art_comb.c \
	src/editor_log.c \
	src/path_util.c

SETTINGS_OUT := editor_settings/so_file/settings.so
SETTINGS_SRC := editor_settings/src/settings.c \
	editor_settings/settings_libs/src/sh.c

OBJ_DIR := build/obj
OBJ := $(SRC:%.c=$(OBJ_DIR)/%.o)
DEP := $(OBJ:.o=.d)

.PHONY: all run clean rebuild

all: $(OUT) $(PLUGIN_OUT) $(SETTINGS_OUT)

$(PLUGIN_OUT): $(PLUGIN_SRC)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared -fPIC $^ -lncursesw -o $@
	@echo "Build OK: $@"

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
	rm -rf $(OBJ_DIR) $(OUT) $(PLUGIN_OUT) $(SETTINGS_OUT)

rebuild: clean all

# -MMDが出す.dファイルでヘッダ変更も検知する
-include $(DEP)
