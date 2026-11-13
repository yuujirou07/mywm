








#include "default_settings.h"
#include"editor.h"
#include "editor_color.h"
#include "editor_types.h"
#include "editor_commands.h"
#include <ncurses.h>
#include <stddef.h>
#include <wchar.h>
#include<stdlib.h>


int init_command_mode(struct editor_state *state){
    state->command_mode_data.command_str_data.command_str[0] = L'\0';
    state->command_mode_data.command_str_data.command_str_len = 0;

    state->command_mode_data.command_list_data.command_allocate_num = 32;
    state->command_mode_data.command_list_data.command_num = 0;

    state->command_mode_data.command_list_data.command_list 
        = malloc(
            sizeof(command_data) * 
                state->command_mode_data.
                    command_list_data.
                        command_allocate_num);
    
    
    return 0;
}

int destory_command_mode(struct editor_state *state){
    if(state == NULL){
        return -1;
    }

    editor_command_mode_data *cmd_data = &state->command_mode_data;
    free(cmd_data->command_list_data.command_list);



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
    
    struct editor_state *state = ctx->state;

    command_str_data *tmp_cmd_str_data =  &state->command_mode_data.command_str_data;
    //iswgraph() 英数字判定
    if(input_result == OK && iswgraph(chr)){
        

        size_t str_arry_len = 
            sizeof(tmp_cmd_str_data->command_str)/sizeof(tmp_cmd_str_data->command_str[0]);
        
        //コマンド文字列上限ガード
        if(str_arry_len <= tmp_cmd_str_data->command_str_len + 1){
            return true;
        }

        tmp_cmd_str_data->command_str[tmp_cmd_str_data->command_str_len] = chr;
        tmp_cmd_str_data->command_str_len++;
        tmp_cmd_str_data->command_str[tmp_cmd_str_data->command_str_len] = L'\0';

        state->render_flags |= RENDER_COMMAND_MODE;
    }
    else if(input_result == KEY_CODE_YES){
        if(chr == KEY_BACKSPACE){
            if(tmp_cmd_str_data->command_str_len < 1){
                return true;
            }
            
            backspace_command_str(ctx);
        }
        else if(chr == KEY_ENTER){
            
        }
    }

  
    return true;
}


int draw_command_str(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    if(state->command_mode_data.command_str_data.command_str_len < 1){
        return -1;
    }
    
    cchar_t tmp_cchar;
    wchar_t tmp_chr[2] = {0};

    tmp_chr[0] = state->command_mode_data.command_str_data
        .command_str[state->command_mode_data.command_str_data.command_str_len - 1];
    tmp_chr[1] = L'\0';
    
    
    short color_id = get_color_id(ctx,COLOR_WHITE,COLOR_BLACK);

    setcchar(&tmp_cchar,tmp_chr,A_NORMAL,color_id,NULL);
    struct pos ms_pos = state->cursor.screen_pos;
    mvadd_wch(ms_pos.y,ms_pos.x,&tmp_cchar);
    //ワイド文字なので文字のセルを取得しその分カーソルを移動する
    state->cursor.screen_pos.x += wcswidth(tmp_chr,1);
    cur_pos_push(state->cursor.screen_pos,state);
    return 0;
}



int backspace_command_str(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;

    command_str_data *command_str_data = 
        &state->command_mode_data.command_str_data;

    //配列外参照ガード
    if(command_str_data->command_str_len < 1){
        return 0;
    }


    struct pos ms_pos = state->cursor.screen_pos;

    size_t chr_len = wcswidth(
        &command_str_data->
            command_str[command_str_data->command_str_len - 1],
        1
        );

    mvhline(ms_pos.y,ms_pos.x - chr_len,L' ',chr_len);
    command_str_data->command_str_len--;
    command_str_data->command_str[command_str_data->command_str_len] = L'\0';
    state->cursor.screen_pos.x -= chr_len;
    cur_pos_push(state->cursor.screen_pos,state);
    return 0;
}

int clear_command_str(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    command_str_data *command_str_data = &state->command_mode_data.command_str_data;

    size_t command_str_len = 
        wcswidth(
            command_str_data->command_str,
            command_str_data->command_str_len
        );

    //カーソル座標を初期座標に戻す
    struct pos ms_pos = state->cursor.screen_pos;
    ms_pos.x -= command_str_len;
    mvhline(ms_pos.y,ms_pos.x,L' ',command_str_len);
    command_str_data->command_str[0] = L'\0';
    command_str_data->command_str_len = 0;
    
    cur_pos_push(ms_pos,state);
    return 0;
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
