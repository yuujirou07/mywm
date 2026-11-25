








#include "editor_color.h"
#include<stdlib.h>
#include "editor_types.h"


// コマンドモードのデータを初期化する。コマンド文字列を空にし、
// コマンドリストを初期容量(32件)で確保する。成功時は0を返す。
int init_command_mode(struct editor_state *state){
    state->command_mode_data.command_str_data.command_str[0] = WNUL;
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

// init_command_mode()で確保したコマンドリストのメモリを解放する。
// stateがNULLの場合は-1、成功時は0を返す。
int destory_command_mode(struct editor_state *state){
    if(state == NULL){
        return -1;
    }

    editor_command_mode_data *cmd_data = &state->command_mode_data;
    free(cmd_data->command_list_data.command_list);



    return 0;
}

// コマンドモード中の1文字入力を処理する。'q'で終了し、それ以外は入力文字列へ反映する。
// 返り値: コマンドモードを続けるならtrue、'q'押下などで終了するならfalse(入力エラー時・ctxがNULLはtrue)。
bool handle_command_mode(struct editor_input_context *ctx,wchar_t chr,int input_result){
    if(ctx == NULL){
        return true;
    }

    struct editor_state *state = ctx->state;
    if(input_result == ERR){
        return true;
    }

    if(chr == L'q'){
        return false;
    }

    command_str_data *tmp_cmd_str_data = &state->command_mode_data.command_str_data;
    if(input_result == OK && iswalnum(chr)){
        size_t command_str_cap =
            sizeof(tmp_cmd_str_data->command_str) / sizeof(tmp_cmd_str_data->command_str[0]);
        if(tmp_cmd_str_data->command_str_len < command_str_cap - 1){
            tmp_cmd_str_data->command_str[tmp_cmd_str_data->command_str_len] = chr;
            tmp_cmd_str_data->command_str_len++;
            tmp_cmd_str_data->command_str[tmp_cmd_str_data->command_str_len] = WNUL;
            state->render_flags |= RENDER_COMMAND_MODE;
        }
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


// 入力中のコマンド文字列の末尾1文字をカーソル位置へ描画する。
// 返り値: 成功時0、入力文字列が空なら-1。
int draw_command_str(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    if(state->command_mode_data.command_str_data.command_str_len < 1){
        return -1;
    }
    
    cchar_t tmp_cchar;
    wchar_t tmp_chr[2] = {0};

    tmp_chr[0] = state->command_mode_data.command_str_data
        .command_str[state->command_mode_data.command_str_data.command_str_len - 1];
    tmp_chr[1] = WNUL;
    
    
    short color_id = PAIR_TEXT;

    setcchar(&tmp_cchar,tmp_chr,A_NORMAL,color_id,NULL);
    struct pos ms_pos = state->cursor.screen_pos;
    mvadd_wch(ms_pos.y,ms_pos.x,&tmp_cchar);
    //ワイド文字なので文字のセルを取得しその分カーソルを移動する
    state->cursor.screen_pos.x += wcswidth(tmp_chr,1);
    cur_pos_push(state->cursor.screen_pos,state);
    return 0;
}



// コマンド文字列の末尾1文字を削除し、画面上の表示とカーソル位置も1文字分戻す。
// 返り値: 常に0。文字列が空なら何もしない。
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
    command_str_data->command_str[command_str_data->command_str_len] = WNUL;
    state->cursor.screen_pos.x -= chr_len;
    cur_pos_push(state->cursor.screen_pos,state);
    return 0;
}

// コマンド文字列を全消去し、画面上の表示を消してカーソルを開始位置へ戻す。
// 返り値: 常に0。
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
    command_str_data->command_str[0] = WNUL;
    command_str_data->command_str_len = 0;
    
    cur_pos_push(ms_pos,state);
    return 0;
}



// カーソルをステータスバー直上のコマンド入力位置へ移動し、入力用カーソルにする。
// 返り値: 成功時0、ステータスバー非表示なら-1。
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
