#include <limits.h>
#include <ncurses.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cjson/cJSON.h"
#include "default_settings.h"
#include "editor_log.h"
#include "input_complete.h"
#include "json_read.h"
#include "path_util.h"
#include "txt_editor.h"

// itemがmin以上max以下で、小数部分のないJSON数値ならtrueを返す。
// 返り値: NULL・数値以外・範囲外・小数ならfalse。itemは変更しない。
static bool json_int_in_range(const cJSON *item, int min, int max){
    return cJSON_IsNumber(item) && item->valuedouble >= min &&
        item->valuedouble <= max && item->valuedouble == item->valueint;
}

// settings_dataを設定JSONの有効な値で上書きする。読込元はmy_txt_editor/直下のeditor_settings。
// 返り値: なし。ファイル読込・JSON解析失敗は元の設定を保持し、個別に不正な値は読み飛ばす。
void load_custom_editor_settings(struct editor_settings *settings_data){
    char path[PATH_MAX];
    if(editor_path_from_exe_dir(path, sizeof(path), EDITOR_SETTINGS_PATH) == NULL){
        return;
    }

    char *buf = read_file_all(path);
    if(buf == NULL){
        return;
    }

    cJSON *json_data = cJSON_Parse(buf);
    free(buf);
    if(json_data == NULL){
        return;
    }

    // 旧形式の設定ファイルはルートから読み込む。
    cJSON *buffer = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_BUFFER]);
    if(buffer == NULL)buffer = json_data;
    cJSON *display = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_DISPLAY]);
    if(display == NULL)display = json_data;
    cJSON *editor = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_EDITOR]);
    if(editor == NULL)editor = json_data;

    cJSON *max_lines = cJSON_GetObjectItemCaseSensitive(buffer, SETTINGS_JSON_KEY_STR[JSON_OBJ_BUFFER][BUFFER_KEY_MAX_LINES]);
    if(json_int_in_range(max_lines, 1, INT_MAX)){
        settings_data->max_lines = max_lines->valueint;
    }

    cJSON *max_line_size = cJSON_GetObjectItemCaseSensitive(buffer, SETTINGS_JSON_KEY_STR[JSON_OBJ_BUFFER][BUFFER_KEY_MAX_LINE_SIZE]);
    if(json_int_in_range(max_line_size, 2, EDITOR_LINE_COL_MAX)){
        settings_data->max_line_size = max_line_size->valueint;
    }

    cJSON *line_number_space = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_LINE_NUMBER_SPACE]);
    if(json_int_in_range(line_number_space, 4, INT_MAX - 1)){
        settings_data->line_number_space = line_number_space->valueint;
    }

    cJSON *indent_range = cJSON_GetObjectItemCaseSensitive(editor, SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR][EDITOR_KEY_INDENT_RANGE]);
    if(json_int_in_range(indent_range, 1, INT_MAX)){
        settings_data->indent_range = indent_range->valueint;
    }

    cJSON *jmp_set_cur_pos = cJSON_GetObjectItemCaseSensitive(editor, SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR][EDITOR_KEY_JMP_SET_CUR_POS]);
    if(json_int_in_range(jmp_set_cur_pos, 0, INT_MAX)){
        settings_data->jmp_set_cur_pos = jmp_set_cur_pos->valueint;
    }

    cJSON *default_load_line_size = cJSON_GetObjectItemCaseSensitive(buffer, SETTINGS_JSON_KEY_STR[JSON_OBJ_BUFFER][BUFFER_KEY_DEFAULT_LOAD_LINE_SIZE]);
    if(json_int_in_range(default_load_line_size, 1, INT_MAX)){
        settings_data->default_load_line_size = default_load_line_size->valueint;
    }

    cJSON *load_buffer_lines = cJSON_GetObjectItemCaseSensitive(buffer, SETTINGS_JSON_KEY_STR[JSON_OBJ_BUFFER][BUFFER_KEY_LOAD_BUFFER_LINES]);
    if(json_int_in_range(load_buffer_lines, 1, INT_MAX)){
        settings_data->load_buffer_lines = load_buffer_lines->valueint;
    }

    cJSON *show_status_bar = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_SHOW_STATUS_BAR]);
    if(cJSON_IsBool(show_status_bar)){
        settings_data->show_status_bar = cJSON_IsTrue(show_status_bar);
    }

    cJSON *status_bar_side = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_STATUS_BAR_SIDE]);
    if(cJSON_IsString(status_bar_side) && status_bar_side->valuestring != NULL){
        if(strcmp(status_bar_side->valuestring, "top") == 0){
            settings_data->bar_side_state = top;
        }
        else if(strcmp(status_bar_side->valuestring, "bottom") == 0){
            settings_data->bar_side_state = bottom;
        }
    }

    cJSON *draw_split_line = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_DRAW_SPLIT_LINE]);
    if(cJSON_IsBool(draw_split_line)){
        settings_data->draw_split_line = cJSON_IsTrue(draw_split_line);
    }

    cJSON *show_start_menu = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_SHOW_START_MENU]);
    if(cJSON_IsBool(show_start_menu)){
        settings_data->show_start_menu = cJSON_IsTrue(show_start_menu);
    }

    cJSON *use_icon = cJSON_GetObjectItemCaseSensitive(display, SETTINGS_JSON_KEY_STR[JSON_OBJ_DISPLAY][DISPLAY_KEY_USE_ICON]);
    if(cJSON_IsBool(use_icon)){
        settings_data->use_icon = cJSON_IsTrue(use_icon);
    }

    cJSON *built_in_syntax = cJSON_GetObjectItemCaseSensitive(editor, SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR][EDITOR_KEY_BUILT_IN_SYNTAX]);
    if(cJSON_IsBool(built_in_syntax)){
        settings_data->built_in_syntax = cJSON_IsTrue(built_in_syntax);
    }

    cJSON *auto_complete = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_AUTO_COMPLETE]);
    if(auto_complete == NULL){
        auto_complete = cJSON_GetObjectItemCaseSensitive(editor, SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR][EDITOR_KEY_AUTO_COMPLETE]);
    }
    if(cJSON_IsObject(auto_complete)){
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(auto_complete,SETTINGS_JSON_KEY_STR[JSON_OBJ_AUTO_COMPLETE][AUTO_COMPLETE_KEY_ENABLED]);
        if(cJSON_IsBool(enabled)){
            settings_data->auto_complete_settings_data.auto_complete_enabled = cJSON_IsTrue(enabled);
        }
        cJSON *window = cJSON_GetObjectItemCaseSensitive(auto_complete,SETTINGS_JSON_KEY_STR[JSON_OBJ_AUTO_COMPLETE][AUTO_COMPLETE_KEY_WINDOW]);
        if(cJSON_IsObject(window)){
            cJSON *show = cJSON_GetObjectItemCaseSensitive(window,SETTINGS_JSON_KEY_STR[JSON_OBJ_WINDOW][WINDOW_KEY_SHOW]);
            if(cJSON_IsBool(show)){
                settings_data->auto_complete_settings_data.auto_complete_window_enable = cJSON_IsTrue(show);
            }
            cJSON *width = cJSON_GetObjectItemCaseSensitive(window,SETTINGS_JSON_KEY_STR[JSON_OBJ_WINDOW][WINDOW_KEY_WIDTH]);
            if(json_int_in_range(width, 3, INT_MAX / 2)){
                settings_data->auto_complete_settings_data.auto_complete_window_size.x = width->valueint;
            }
            cJSON *height = cJSON_GetObjectItemCaseSensitive(window,SETTINGS_JSON_KEY_STR[JSON_OBJ_WINDOW][WINDOW_KEY_HEIGHT]);
            if(json_int_in_range(height, 3, INT_MAX / 2)){
                settings_data->auto_complete_settings_data.auto_complete_window_size.y = height->valueint;
            }
            // 不明な文字列や文字列以外の値では既定の配置方式を保持する。
            cJSON *pos_mode = cJSON_GetObjectItemCaseSensitive(window,SETTINGS_JSON_KEY_STR[JSON_OBJ_WINDOW][WINDOW_KEY_POSITION_MODE]);
            if(cJSON_IsString(pos_mode) && pos_mode->valuestring != NULL){
                for(int i = 0; i < get_edit_comp_pos_def_world_num(); i++){
                    if(strcmp(pos_mode->valuestring, get_edit_comp_pos_def_world(i)) == 0){
                        settings_data->auto_complete_settings_data.auto_complete_position_mode = i;
                        break;
                    }
                }
            }
        }
    }
    else if(cJSON_IsBool(auto_complete)){
        settings_data->auto_complete_settings_data.auto_complete_enabled = cJSON_IsTrue(auto_complete);
    }

    cJSON *key_log = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_KEY_LOG]);
    if(cJSON_IsObject(key_log)){
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(key_log, SETTINGS_JSON_KEY_STR[JSON_OBJ_KEY_LOG][KEY_LOG_KEY_ENABLED]);
        if(cJSON_IsBool(enabled)){
            settings_data->key_log_settings.use_key_log = cJSON_IsTrue(enabled);
        }
        cJSON *buffer_size = cJSON_GetObjectItemCaseSensitive(key_log, SETTINGS_JSON_KEY_STR[JSON_OBJ_KEY_LOG][KEY_LOG_KEY_BUFFER_SIZE]);
        if(json_int_in_range(buffer_size, 1, UINT16_MAX)){
            settings_data->key_log_settings.key_log_buffer_size = buffer_size->valueint;
        }
    }

    cJSON *settings_language = cJSON_GetObjectItemCaseSensitive(json_data,SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_SETTINGS_LANGUAGE]);
    if(cJSON_IsString(settings_language)){
        settings_data->settings_lang = comb_str_to_lang(settings_language->valuestring);
    }

    cJSON *lsp = cJSON_GetObjectItemCaseSensitive(json_data, SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_LSP]);
    if(cJSON_IsObject(lsp)){
        cJSON *launch_startup_editor =
            cJSON_GetObjectItemCaseSensitive(lsp, SETTINGS_JSON_KEY_STR[JSON_OBJ_LSP][LSP_KEY_LAUNCH_STARTUP_EDITOR]);
        cJSON *epoll_timeout_ms =
            cJSON_GetObjectItemCaseSensitive(lsp, SETTINGS_JSON_KEY_STR[JSON_OBJ_LSP][LSP_KEY_EPOLL_TIMEOUT_MS]);

        if(cJSON_IsBool(launch_startup_editor)){
            settings_data->lsp.lsp_launch_startup_editor =
                cJSON_IsTrue(launch_startup_editor);
        }
        if(json_int_in_range(epoll_timeout_ms, 0, INT_MAX)){
            settings_data->lsp.lsp_epoll_timeout_ms = epoll_timeout_ms->valueint;
        }
    }

    cJSON *settings_program = cJSON_GetObjectItemCaseSensitive(json_data,SETTINGS_JSON_KEY_STR[JSON_OBJ_ROOT][ROOT_KEY_SETTINGS_PROGRAM]);

    if(cJSON_IsObject(settings_program)){
        cJSON *enabled = cJSON_GetObjectItemCaseSensitive(
            settings_program,
            SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR_SETTINGS_PROGRAM][EDITOR_SETTINGS_PROGRAM_ENABLED]
            );

        if(cJSON_IsBool(enabled)){
            bool tmp_bool = false;
            if(cJSON_IsTrue(enabled)){
                tmp_bool = true;
            }
            settings_data->editor_settings_program.enabled = tmp_bool;
        }

        cJSON *language = cJSON_GetObjectItemCaseSensitive(
            settings_program,
            SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR_SETTINGS_PROGRAM][EDITOR_SETTINGS_PROGRAM_LANGUAGE]
            );

        if(cJSON_IsString(language)){
            settings_data->editor_settings_program.language = comb_str_to_program_lang(language->valuestring);
        }

        cJSON *root_src_path = cJSON_GetObjectItemCaseSensitive(
            settings_program,
            SETTINGS_JSON_KEY_STR[JSON_OBJ_EDITOR_SETTINGS_PROGRAM][EDITOR_SETTINGS_PROGRAM_ROOT_SRC_PATH]
            );
        if(cJSON_IsString(root_src_path)){
            
            settings_data->editor_settings_program.root_src_path = strdup(root_src_path->valuestring);
        }

    }

    cJSON_Delete(json_data);
}

//文字列が規定の言語名と一致しているかstrcmp()で比較し、
// 一致していたらその列挙型の値、すべて一致していなかったらunknownを返す
settings_lang comb_str_to_lang(char *str){
    if(str == NULL){
        error_log("argument error");
        return settings_lang_UNKNOWN;
    }

    for(uint16_t i = 0;i < settings_lang_UNKNOWN;i++){
        if(strcmp(str,SETTINGS_LANGUAGE_JSON_KEY_STR[i]) == 0){
            return i;
        }
    }
    return settings_lang_UNKNOWN;
}

// comb_str_to_lang()のeditor_settings_program用。
editor_settings_program_lang comb_str_to_program_lang(char *str){
    if(str == NULL){
        error_log("argument error");
        return editor_settings_program_lang_UNKNOWN;
    }

    for(uint16_t i = 0;i < editor_settings_program_lang_UNKNOWN;i++){
        if(strcmp(str,SETTINGS_LANGUAGE_JSON_KEY_STR[i]) == 0){
            return i;
        }
    }
    return editor_settings_program_lang_UNKNOWN;
}
