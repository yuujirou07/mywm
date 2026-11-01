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
            return edit_screen;
        case FILE_SEARCH_MODE:
            return file_browse_screen;
        case FILE_TREE_MODE:
            return filetree_screen;
        case HOME_SCREEN_MODE:
            return start_menu_screen;
    }
    return screen_state_log_error;
}


#endif
