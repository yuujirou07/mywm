#ifndef DEFALT_SETTINGS_H
#define DEFALT_SETTINGS_H

#include "editor_icon.h"
#include "input_complete.h"
#include<ncurses.h>
#include<editor_types.h>
#include <stddef.h>

#define MAX_LINES    1000 // 初期設定で扱う最大行数。
#define MAX_LINE_SIZE 1024 // ファイル読込み時の1行バッファ長。
#define LINE_NUMBER_SPACE 4 // 行番号表示へ確保する端末セル数。
#define INDENT_RANGE 8 // Tab入力1回で挿入する空白数。
#define JMP_SET_CUR_POS 10 // 行ジャンプ後のカーソル位置として保持する既定値。
#define DEFAULT_LOAD_LINE_SIZE 9999 // 読み込める論理行数の既定上限。
#define LOAD_BUFFER_LINES 100 // 編集バッファへ最初に確保する行数。
#define SHOW_STATUS_BAR true // ステータスバーを表示する既定値。
#define DEFAULT_DRAW_SPLIT_LINE 1 // 行番号欄と本文の境界線を描く既定値。
#define DEFAULT_STATUS_BAR_SIDE top // ステータスバーを置く既定の画面端。
#define JUMP_LINE_NUM_DIGITS 4 // 行ジャンプ欄へ入力できる最大桁数。
#define DEFAULT_ASK_MAKE_FILE 1 // 未作成ファイルを開く際に作成確認を出す既定値。
#define DEFAULT_PATH_NAME_MAX_SIZE PATH_MAX // ファイルパス配列の要素数。
#define DEFAULT_FILE_SELECT_SCENE_LIGHTING true // ファイル選択行を反転表示する既定値。
#define DEFAULT_SHOW_START_MENU true // 起動時にスタートメニューを表示する既定値。
#define DEFAULT_LSP_PROCESS_LAUNCH_STARTUP_EDITOR true // 起動時にLSPを開始する既定値。
#define DEFAULT_EPOLL_TIME_OUT_MS 16 // LSPイベント待機の既定時間（ミリ秒）。
#define DEFAULT_LSP_USE true // LSP連携を有効にする既定値。
#define DEFAULT_USE_ICON false // ファイル種別アイコンを表示する既定値。
#define DEFAULT_BUILT_IN_SYNTAX true // 組込み構文着色を有効にする既定値。
#define DEFAULT_EDITOR_SCROLL_SENSITIVITY 1 // マウスホイール1回あたりのスクロール行数の既定値。

#define DEFAULT_AUTO_COMPLETE true // 自動補完を有効にする既定値。
#define DEFAULT_AUTO_COMPLETE_WINDOW true // 補完候補ウィンドウを表示する既定値。
#define DEFAULT_AUTO_COMPLETE_WINDOW_WIDTH 12 // 候補ウィンドウの既定幅（端末セル数）。
#define DEFAULT_AUTO_COMPLETE_WINDOW_HEIGHT 7 // 候補ウィンドウの既定高さ（端末行数）。
#define DEFAULT_AUTO_COMPLETE_POSITION_MODE EDIT_COMP_TRACKING // 候補ウィンドウを入力位置に追従させる既定値。

#define DEFAULT_SETTINGS_LANGUAGE 2

#define DEFAULT_USE_KEY_LOG true
#define DEFAULT_KEY_LOG_BUFFER_SIZE 2048

#define DEFAULT_LSP_LANGUAGE "" // 起動対象の言語サーバー名の既定値。空なら未指定。
#define DEFAULT_EDITOR_SETTINGS_PROGRAM_ENABLED true // 設定プログラムを使う既定値。
#define DEFAULT_EDITOR_SETTINGS_PROGRAM_LANGUAGE editor_settings_program_lang_UNKNOWN // 設定プログラムの言語の既定値。



// JSONオブジェクトごとのキー文字列。SETTINGS_JSON_KEY_STR[オブジェクト][キー]で引く。
typedef enum{
    JSON_OBJ_ROOT = 0,
    JSON_OBJ_BUFFER = 1,
    JSON_OBJ_DISPLAY = 2,
    JSON_OBJ_EDITOR = 3,
    JSON_OBJ_AUTO_COMPLETE = 4,
    JSON_OBJ_WINDOW = 5,
    JSON_OBJ_KEY_LOG = 6,
    JSON_OBJ_LSP = 7,
    JSON_OBJ_EDITOR_SETTINGS_PROGRAM = 8,
    JSON_OBJ_NUM = 9,
}settings_json_obj;

typedef enum{
    ROOT_KEY_BUFFER = 0,
    ROOT_KEY_DISPLAY = 1,
    ROOT_KEY_EDITOR = 2,
    ROOT_KEY_AUTO_COMPLETE = 3,
    ROOT_KEY_KEY_LOG = 4,
    ROOT_KEY_SETTINGS_LANGUAGE = 5,
    ROOT_KEY_LSP = 6,
    ROOT_KEY_SETTINGS_PROGRAM = 7,
    ROOT_KEY_EDITOR_SCROLL_SENSITIVITY = 8,
}settings_root_key;

typedef enum{
    BUFFER_KEY_MAX_LINES = 0,
    BUFFER_KEY_MAX_LINE_SIZE = 1,
    BUFFER_KEY_DEFAULT_LOAD_LINE_SIZE = 2,
    BUFFER_KEY_LOAD_BUFFER_LINES = 3,
}settings_buffer_key;

typedef enum{
    DISPLAY_KEY_LINE_NUMBER_SPACE = 0,
    DISPLAY_KEY_SHOW_STATUS_BAR = 1,
    DISPLAY_KEY_STATUS_BAR_SIDE = 2,
    DISPLAY_KEY_DRAW_SPLIT_LINE = 3,
    DISPLAY_KEY_SHOW_START_MENU = 4,
    DISPLAY_KEY_USE_ICON = 5,
}settings_display_key;

typedef enum{
    EDITOR_KEY_INDENT_RANGE = 0,
    EDITOR_KEY_JMP_SET_CUR_POS = 1,
    EDITOR_KEY_BUILT_IN_SYNTAX = 2,
    EDITOR_KEY_AUTO_COMPLETE = 3,
}settings_editor_key;

typedef enum{
    AUTO_COMPLETE_KEY_ENABLED = 0,
    AUTO_COMPLETE_KEY_WINDOW = 1,
}settings_auto_complete_key;

typedef enum{
    WINDOW_KEY_SHOW = 0,
    WINDOW_KEY_WIDTH = 1,
    WINDOW_KEY_HEIGHT = 2,
    WINDOW_KEY_POSITION_MODE = 3,
}settings_window_key;

typedef enum{
    KEY_LOG_KEY_ENABLED = 0,
    KEY_LOG_KEY_BUFFER_SIZE = 1,
}settings_key_log_key;

typedef enum{
    LSP_KEY_LAUNCH_STARTUP_EDITOR = 0,
    LSP_KEY_EPOLL_TIMEOUT_MS = 1,
}settings_lsp_key;

typedef enum{
    EDITOR_SETTINGS_PROGRAM_ENABLED  = 0,
    EDITOR_SETTINGS_PROGRAM_LANGUAGE = 1,
    EDITOR_SETTINGS_PROGRAM_ROOT_SRC_PATH = 2,
}editor_settings_program;

static const char *const SETTINGS_ROOT_KEY_STR[] = {
    [ROOT_KEY_BUFFER] = "buffer",
    [ROOT_KEY_DISPLAY] = "display",
    [ROOT_KEY_EDITOR] = "editor",
    [ROOT_KEY_AUTO_COMPLETE] = "auto_complete",
    [ROOT_KEY_KEY_LOG] = "key_log",
    [ROOT_KEY_SETTINGS_LANGUAGE] = "settings_language",
    [ROOT_KEY_LSP] = "lsp",
    [ROOT_KEY_SETTINGS_PROGRAM] = "editor_settings_program",
    [ROOT_KEY_EDITOR_SCROLL_SENSITIVITY] = "editor_scroll_sensitivity",
};

static const char *const SETTINGS_BUFFER_KEY_STR[] = {
    [BUFFER_KEY_MAX_LINES] = "max_lines",
    [BUFFER_KEY_MAX_LINE_SIZE] = "max_line_size",
    [BUFFER_KEY_DEFAULT_LOAD_LINE_SIZE] = "default_load_line_size",
    [BUFFER_KEY_LOAD_BUFFER_LINES] = "load_buffer_lines",
};

static const char *const SETTINGS_DISPLAY_KEY_STR[] = {
    [DISPLAY_KEY_LINE_NUMBER_SPACE] = "line_number_space",
    [DISPLAY_KEY_SHOW_STATUS_BAR] = "show_status_bar",
    [DISPLAY_KEY_STATUS_BAR_SIDE] = "status_bar_side",
    [DISPLAY_KEY_DRAW_SPLIT_LINE] = "draw_split_line",
    [DISPLAY_KEY_SHOW_START_MENU] = "show_start_menu",
    [DISPLAY_KEY_USE_ICON] = "use_icon",
};

static const char *const SETTINGS_EDITOR_KEY_STR[] = {
    [EDITOR_KEY_INDENT_RANGE] = "indent_range",
    [EDITOR_KEY_JMP_SET_CUR_POS] = "jmp_set_cur_pos",
    [EDITOR_KEY_BUILT_IN_SYNTAX] = "built_in_syntax",
    [EDITOR_KEY_AUTO_COMPLETE] = "auto_complete",
};

static const char *const SETTINGS_AUTO_COMPLETE_KEY_STR[] = {
    [AUTO_COMPLETE_KEY_ENABLED] = "enabled",
    [AUTO_COMPLETE_KEY_WINDOW] = "window",
};

static const char *const SETTINGS_WINDOW_KEY_STR[] = {
    [WINDOW_KEY_SHOW] = "show",
    [WINDOW_KEY_WIDTH] = "width",
    [WINDOW_KEY_HEIGHT] = "height",
    [WINDOW_KEY_POSITION_MODE] = "position_mode",
};

static const char *const SETTINGS_KEY_LOG_KEY_STR[] = {
    [KEY_LOG_KEY_ENABLED] = "enabled",
    [KEY_LOG_KEY_BUFFER_SIZE] = "buffer_size",
};

static const char *const SETTINGS_LSP_KEY_STR[] = {
    [LSP_KEY_LAUNCH_STARTUP_EDITOR] = "launch_startup_editor",
    [LSP_KEY_EPOLL_TIMEOUT_MS] = "epoll_timeout_ms",
};

static const char *const EDITOR_SETTINGS_PROGRAM_KEY_STR[] = {
    [EDITOR_SETTINGS_PROGRAM_ENABLED] = "enabled",
    [EDITOR_SETTINGS_PROGRAM_LANGUAGE] = "language",
    [EDITOR_SETTINGS_PROGRAM_ROOT_SRC_PATH] = "root_src_path",
};


static const char *const *const SETTINGS_JSON_KEY_STR[JSON_OBJ_NUM] = {
    [JSON_OBJ_ROOT] = SETTINGS_ROOT_KEY_STR,
    [JSON_OBJ_BUFFER] = SETTINGS_BUFFER_KEY_STR,
    [JSON_OBJ_DISPLAY] = SETTINGS_DISPLAY_KEY_STR,
    [JSON_OBJ_EDITOR] = SETTINGS_EDITOR_KEY_STR,
    [JSON_OBJ_AUTO_COMPLETE] = SETTINGS_AUTO_COMPLETE_KEY_STR,
    [JSON_OBJ_WINDOW] = SETTINGS_WINDOW_KEY_STR,
    [JSON_OBJ_KEY_LOG] = SETTINGS_KEY_LOG_KEY_STR,
    [JSON_OBJ_LSP] = SETTINGS_LSP_KEY_STR,
    [JSON_OBJ_EDITOR_SETTINGS_PROGRAM] = EDITOR_SETTINGS_PROGRAM_KEY_STR,
};




typedef enum{
    settings_lang_C   = 0,
    settings_lang_LUA = 1,
    settings_lang_UNKNOWN = 2,
    settings_lang_LANG_NUM = 3,
}settings_lang;


typedef enum{
    editor_settings_program_lang_C = 0,
    editor_settings_program_lang_LUA = 1,
    editor_settings_program_lang_UNKNOWN = 2,
    editor_settings_program_lang_LANG_NUM = 3,
}editor_settings_program_lang;


static const char *const SETTINGS_LANGUAGE_JSON_KEY_STR[] =
    {
        "c",
        "lua",
        "unknown",
    };

static const char *const CURSOR_SHAPE[] = 
    {
        "\033[6 q",
        "\033[2 q",
        "\033[0 q",
    };

typedef enum{
    CURSOR_INPUT_MODE   = 0,
    CURSOR_NORMAL_MODE  = 1,
    CURSOR_DEFAULT_MODE = 2,
}CURSOR_MODE;



static const char *const CURSOR_COLOR_ESC[] = 
    {
        "\033]12;#ffffff\a",
        "\033]112\a",
    };


typedef enum{
    CURSOR_COLOR_WHITE   = 0,
    CURSOR_COLOR_DEFAULT = 1,
}CURSOR_COLOR;



settings_lang comb_str_to_lang(char *str);
editor_settings_program_lang comb_str_to_program_lang(char *str);




// LSPの有効状態、起動方法、ポーリング条件を保持する設定。
struct lsp_settings_data{
    bool lsp_use; // LSPとの送受信を行うならtrue。
    bool lsp_launch_startup_editor; // エディタ起動時に言語サーバーを開始するならtrue。
    int lsp_epoll_timeout_ms; // epoll_wait()の待機時間（ミリ秒）。
    char lsp_language[32]; // 起動対象の言語サーバー名を保持するNUL終端文字列。
};

// 自動補完と候補ウィンドウの表示設定。
typedef struct{
    bool auto_complete_enabled; // 自動補完機能を有効にするならtrue。
    bool auto_complete_window_enable; // 候補ウィンドウを表示するならtrue。
    struct pos auto_complete_window_size; // x=幅、y=高さ。どちらも枠を含む端末セル数。
    EDIT_COMPLETE_POSITION_MODE auto_complete_position_mode; // JSONのwindow.position_modeを列挙値で保持する。
}auto_complete_settings_data;


// キー履歴の記録設定。
typedef struct{
    bool use_key_log; // キー履歴を記録するならtrue。
    size_t key_log_buffer_size; // 記録する最大件数。設定JSONでは1〜65535。
}key_log_settings;


typedef struct{
    bool enabled;
    editor_settings_program_lang language;
    char *root_src_path;
}editor_settings_program_data;

// 既定値と設定JSONを統合した、実行中のエディタ設定。
struct editor_settings{
    int max_lines; // 扱う最大行数として保持する設定値。
    int max_line_size; // ファイル読込み時の1行バッファ長。
    int line_number_space; // 行番号欄へ確保する端末セル数。
    int indent_range; // Tab入力1回で挿入する空白数。
    int jmp_set_cur_pos; // 行ジャンプ後のカーソル位置として保持する設定値。
    int default_load_line_size; // 読込みと行管理配列に使う論理行数の上限。
    int load_buffer_lines; // 編集バッファへ最初に確保する行数。
    int bar_side_state; // enum status_bar_sideの値。
    bool show_status_bar; // ステータスバーを表示するならtrue。
    bool draw_split_line; // 行番号欄と本文の境界線を描くならtrue。
    bool ask_make_file; // 未作成ファイルを開く際に作成確認を出すならtrue。
    bool show_start_menu; // 起動時にスタートメニューを表示するならtrue。
    bool file_select_scene_lighting; // ファイルブラウザの選択行を反転表示するならtrue。
    bool use_icon; // ファイル種別アイコンを表示するならtrue。
    bool built_in_syntax; // 組込み構文着色を使うならtrue。
    int editor_scroll_sensitivity; // マウスホイール1回あたりのスクロール行数。
    auto_complete_settings_data auto_complete_settings_data; // 自動補完の設定一式。
    key_log_settings key_log_settings;
    struct lsp_settings_data lsp; // LSP連携の設定一式。
    icon_data icon_data; // 読み込んだ拡張子別アイコン配列を所有する。
    settings_lang settings_lang;
    editor_settings_program_data editor_settings_program;
};






#endif 
