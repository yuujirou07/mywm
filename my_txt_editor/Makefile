CC := gcc
CFLAGS := -Wall -g3 -O2 -fsanitize=address -Wextra -Werror -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600 -I./include -I./include/lsp_src

# build.shの引数outに相当: make OUT=build/main.out
OUT := build/main
PLUGIN_OUT := editor_settings/so_file/start_menu_plug.so
SETTINGS_OUT := editor_settings/so_file/settings.so

MAIN_SRC := src/main.c \
	src/screen_state/txt_editor_state.c \
	src/txt_editor_func.c \
	src/txt_editor_draw.c \
	src/txt_editor_file.c \
	src/txt_editor_syntax.c \
	src/json_read.c \
	src/editor_log.c \
	src/kbd_input_log.c \
	src/lsp_src/language_server_communication.c \
	src/path_util.c \
	src/settings_parse_src/editor_settings_parse.c \
	src/settings_parse_src/settings_items_parse.c \
	src/settings_parse_src/editor_icon_parse.c \
	src/screen_state/txt_editor_edit_screen.c \
	src/screen_state/txt_editor_file_browse_screen.c \
	src/screen_state/txt_editor_line_jump_screen.c \
	src/screen_state/txt_editor_error_screen.c \
	src/screen_state/txt_editor_make_file_screen.c \
	src/screen_state/txt_editor_start_menu_screen.c \
	src/screen_state/txt_editor_settings_screen.c \
	src/screen_state/txt_editor_filetree_screen.c \
	src/plugin_src/edit_input_complete.c \
	src/public_settings_src/c_settings_src.c/c_settings_settings.c \
	src/ftj.c
PLUGIN_SRC := src/plugin_src/start_menu_plug.c \
	src/plugin_src/ascii_art_comb.c \
	src/editor_log.c \
	src/path_util.c
SETTINGS_SRC := editor_settings/src/settings.c \
	editor_settings/settings_libs/src/sh.c

# 本体とプラグインは同じ.cを別フラグ(-fPIC)でコンパイルするので、オブジェクトの置き場を分ける
OBJ_DIR := build/obj
MAIN_OBJ := $(MAIN_SRC:%.c=$(OBJ_DIR)/main/%.o)
PLUGIN_OBJ := $(PLUGIN_SRC:%.c=$(OBJ_DIR)/plugin/%.o)
SETTINGS_OBJ := $(SETTINGS_SRC:%.c=$(OBJ_DIR)/settings/%.o)

# settings_libs/src/sh.cがsettings_helper.hを<名前だけ>でincludeするため
$(SETTINGS_OBJ): CFLAGS += -I./editor_settings/settings_libs/include

.PHONY: all main plugin settings clean

all: main plugin settings

main: $(OUT)
plugin: $(PLUGIN_OUT)
settings: $(SETTINGS_OUT)

$(OUT): $(MAIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -lcjson -lncursesw -ldl -o $@
	@echo "Build OK: $@"

$(PLUGIN_OUT): $(PLUGIN_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -lncursesw -o $@
	@echo "Build OK: $@"

$(SETTINGS_OUT): $(SETTINGS_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -shared $^ -o $@
	@echo "Build OK: $@"

$(OBJ_DIR)/main/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/plugin/%.o $(OBJ_DIR)/settings/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fPIC -MMD -MP -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(OUT) $(PLUGIN_OUT) $(SETTINGS_OUT)

# -MMDが出す.dファイルでヘッダの変更も検知する
-include $(MAIN_OBJ:.o=.d) $(PLUGIN_OBJ:.o=.d) $(SETTINGS_OBJ:.o=.d)
