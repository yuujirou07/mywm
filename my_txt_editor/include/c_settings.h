#ifndef C_SETTINGS_H
#define C_SETTINGS_H

#include "public_data/c_settings/c_settings_setting.h"
#include "editor_types.h"

struct editor_input_context;



void connect_api_mem_data(MY_TXT_EDITOR_API *api, struct editor_input_context *ctx);
void init_settings_src(MY_TXT_EDITOR_API *api, struct editor_input_context *ctx);

void check_key_mapps_entry(struct editor_input_context *ctx,wchar_t chr[2],MY_TXT_EDITOR_API *api);

int add_dl_path(struct editor_input_context *ctx,char *dl_path);
int check_user_settings_event(struct editor_input_context *ctx);
int destroy_dl_path(struct editor_input_context *ctx);


static inline screen_state comb_my_txt_editor_screen_mode_To_screen_state(MY_TXT_EDITOR_SCREEN_MODE screen_mode){
    switch(screen_mode){
        case EDIT_MODE:
            return edit_screen_mode;
        case FILE_SEARCH_MODE:
            return file_browse_screen_mode;
        case FILE_TREE_MODE:
            return filetree_screen_mode;
        case HOME_SCREEN_MODE:
            return start_menu_screen_mode;
        case COMMANDS_MODE:
            return editor_command_mode;
        case UNKNOWN_MODE:
            break;
    }
    return screen_state_log_error_mode;
}

// screen_stateを設定側のMY_TXT_EDITOR_SCREEN_MODEへ変換する(comb_my_txt_editor_screen_mode_To_screen_stateの逆)。
// 返り値: 対応するMODE。設定側に対応するモードが無いscreen_stateはUNKNOWN_MODE。
static inline MY_TXT_EDITOR_SCREEN_MODE comb_screen_state_To_my_txt_editor_screen_mode(screen_state state){
    switch(state){
        case edit_screen_mode:
            return EDIT_MODE;
        case file_browse_screen_mode:
            return FILE_SEARCH_MODE;
        case filetree_screen_mode:
            return FILE_TREE_MODE;
        case start_menu_screen_mode:
            return HOME_SCREEN_MODE;
        case editor_command_mode:
            return COMMANDS_MODE;
        default:
            break;
    }
    return UNKNOWN_MODE;
}


#endif
