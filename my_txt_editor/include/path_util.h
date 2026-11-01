#ifndef PATH_UTIL_H
#define PATH_UTIL_H

#include <stddef.h>


//自分の実行可能データのシンボリックリンク
#define SELF_SIM_LINK "/proc/self/exe"

// my_txt_editor/直下からの相対パス(editor_path_from_exe_dirへ渡す)
#define SO_DIR "editor_settings/so_file/"
#define LOG_DIR "log_data/"
#define START_MENU_SO_PATH SO_DIR "start_menu_plug.so"
#define SETTINGS_SO_PATH SO_DIR "settings.so"
#define EDITOR_SETTINGS_DIR "editor_settings/"
#define EDITOR_SETTINGS_PATH EDITOR_SETTINGS_DIR "my_txt_editor_settings.json"
#define SETTINGS_ITEMS_PATH EDITOR_SETTINGS_DIR "settings_items.json"
#define ICON_SETTINGS_PATH EDITOR_SETTINGS_DIR "editor_icon_settings.json"
#define ASCII_ART_PATH "my_txt_editor_settings_folder/ascii_art_img.txt"
#define KEY_EVENT_LOG_PATH "log_data/key_event.log"
#define EDITOR_LOG_DATA "log_data/editor_log.log"


char *editor_path_from_exe_dir(char *buf, size_t buf_size, const char *relative);

int get_self_path(char *path,size_t s);
char *get_path_dirname(char *path);
#endif
