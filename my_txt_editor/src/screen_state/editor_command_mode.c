








#include "default_settings.h"
#include"editor.h"
#include "editor_types.h"
#include "editor_commands.h"
#include <ncurses.h>
#include <wchar.h>


int init_command_mode(struct editor_state *state){
    state->command_mode_data.command_str_data.command_str[0] = L'\0';
    state->command_mode_data.command_str_data.command_str_len = 0;
    return 0;
}




bool handle_command_mode(struct editor_input_context *ctx,wchar_t chr,int input_result){
    if(ctx == NULL){
        return true;
    }

    if(input_result == ERR){
        return true;
    }

    if(chr == L'q'){
        return false;
    }
    

    if(input_result == OK && iswalnum(chr)){
        struct *command_str_data tmp_cmd_str_data = 


    }

  
    return true;
}

int cursor_set_command_mode_position(struct editor_state *state){
    if(state->settings_data->show_status_bar == false){
        return -1;
    }
    struct pos status_bar_pos = state->status_bar->pos;
    struct pos cmd_mode_cur_pos = {status_bar_pos.x+1,status_bar_pos.y-1};

    SetCursorMode(state,CURSOR_INPUT_MODE);
    cur_pos_push(cmd_mode_cur_pos,state);

    return 0;
}
