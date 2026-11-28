#ifndef PATH_UTIL_H_
#define PATH_UTIL_H_

#include <stddef.h>
#include "editor_types.h"


//自分の実行可能データのシンボリックリンク
#define SELF_SIM_LINK_ "/proc/self/exe"

// 設定ディレクトリ(editor_config_path()の基準)からの相対パス。.soは設定ディレクトリの下に置く。
#define SO_DIR_ "so_file/"
#define START_MENU_SO_PATH_ SO_DIR_ "start_menu_plug.so"
#define SETTINGS_SO_PATH_ SO_DIR_ "settings.so"

#ifdef EDITOR_DEV_MODE
    // my_txt_editor/直下からの相対パス(editor_path_from_exe_dirへ渡す)
    #define LOG_DIR_ "log_data/"
    #define EDITOR_SETTINGS_DIR_ "editor_settings/"
    #define EDITOR_SETTINGS_PATH_ EDITOR_SETTINGS_DIR_ "my_txt_editor_settings.json"
    #define SETTINGS_ITEMS_PATH_ EDITOR_SETTINGS_DIR_ "settings_items.json"
    #define ICON_SETTINGS_PATH_ EDITOR_SETTINGS_DIR_ "editor_icon_settings.json"
    #define ASCII_ART_PATH_ "my_txt_editor_settings_folder/ascii_art_img.txt"
    #define KEY_EVENT_LOG_PATH_ "log_data/key_event.log"
    #define EDITOR_LOG_DATA_ "log_data/editor_log.log"
    
#endif

char *editor_path_from_exe_dir(char *buf, size_t buf_size, const char *relative);

// 設定ディレクトリを解決してrelativeを連結し、buf_sizeバイトのbufへ書く。
// 解決順: EDITOR_DEV_MODE=プロジェクト直下のeditor_settings/ / 通常=$MY_TXT_EDITOR_CONFIG,
// $XDG_CONFIG_HOME/my_txt_editor, $HOME/.config/my_txt_editor のうち最初に存在するディレクトリ。
// どれも無ければ実行ファイル基準のeditor_settings/へ戻す。返り値: 成功時buf、失敗はNULL。
char *editor_config_path(char *buf, size_t buf_size, const char *relative);

int get_self_path(char *path,size_t s);
char *get_path_dirname(char *path);
enum path_type get_path_type(const char *path);

enum path_type get_path_type(const char *path);


#endif
