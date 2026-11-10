#include "kbd_input_log.h"
#include <limits.h>
#include "path_util.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>
#include "editor_types.h"
#include "editor_log.h"
#include "editor.h"

FILE *key_log_file = NULL;


// stateのキー履歴へ文字ch・get_wch結果result・入力時のscreen_stateを追加する。
// 履歴は設定上限までで打ち止めにし、長時間使ってもメモリが増え続けないようにしている。
// 返り値: 追加できたら0。記録数が設定上限(key_log_buffer_size)に達している場合と、確保・再確保に失敗した場合は-1。
int KL_add(struct editor_state *state,wchar_t ch,int result,screen_state screen_state){
    if(state->key_bord_data.Key_log_data.key_count >= 
            state->settings_data->key_log_settings.key_log_buffer_size){
        return -1; 
    }

    if(state->key_bord_data.Key_log_data.key_log == NULL){
        state->key_bord_data.Key_log_data.key_allocate_num = KEY_LOG_ALLOCATE_NUM;
        state->key_bord_data.Key_log_data.key_count = 0;
        state->key_bord_data.Key_log_data.key_log = 
            malloc(sizeof(key_data) * 
                state->key_bord_data.Key_log_data.key_allocate_num);

        if(state->key_bord_data.Key_log_data.key_log == NULL){
            error_log("malloc");
            return -1;
        }
    }
    else if(state->key_bord_data.Key_log_data.key_allocate_num <= 
        state->key_bord_data.Key_log_data.key_count){
        size_t tmp_realloc_num = 
            (state->key_bord_data.Key_log_data.key_count * 2 >= 
                state->settings_data->key_log_settings.key_log_buffer_size)?
                state->settings_data->key_log_settings.key_log_buffer_size:
                state->key_bord_data.Key_log_data.key_count * 2;
        

        key_data *tmp_key_data = 
            realloc(
                state->key_bord_data.Key_log_data.key_log,
                sizeof(key_data) * tmp_realloc_num
            );
        if(tmp_key_data == NULL){
            error_log("malloc");
            return -1;
        }
        state->key_bord_data.Key_log_data.key_log = tmp_key_data;
        state->key_bord_data.Key_log_data.key_allocate_num = tmp_realloc_num;
    }

    key_data *tmp_key_log = 
        &state->key_bord_data.Key_log_data.key_log[state->key_bord_data.Key_log_data.key_count];
    tmp_key_log->screen_state = screen_state;
    tmp_key_log->input_result = result;
    tmp_key_log->key = ch;
    state->key_bord_data.Key_log_data.key_count++;
    return 0;
}


// 記録済みのキー履歴を、全画面分まとめて返す。
// 引数: state=キー履歴を持つエディタ状態、n=件数の出力先。 返り値: 履歴配列の先頭(読み取り専用、件数は*n)。履歴が空ならNULLで*nは0。
// 所有権: 配列はstateが所有する。KL_add()で再確保されると無効になるので、取得後すぐに使う。
const key_data *KL_GetLog(struct editor_state *state,size_t *n){
    *n = state->key_bord_data.Key_log_data.key_count;
    if(*n == 0){
        return NULL;
    }
    return state->key_bord_data.Key_log_data.key_log;
}



// キー履歴の出力先ファイルを追記モードで開く。relative_pathはmy_txt_editor/直下からの相対パス。既に開いていれば何もしない。
// 返り値: 成功0、開けなかった場合-1(error_logへ記録する)。
int KL_open_key_log_file(const char *relative_path){
    if(key_log_file == NULL){
        char path[PATH_MAX];
        if(editor_path_from_exe_dir(path,sizeof(path),relative_path) == NULL){
            error_log("can not resolve key log file path");
            return -1;
        }
        key_log_file = fopen(path,"a+");
        if(key_log_file == NULL){
            error_log("can not open key log file");
            return -1;
        }
    }
    return 0;
}

int KL_get_key_data_log(struct editor_state *state,key_data **key,size_t *n){
    if(state == NULL || key == NULL || n == NULL){
        return -1;
    }

    *key = state->key_bord_data.Key_log_data.key_log;
    *n = state->key_bord_data.Key_log_data.key_count;
    return 0;
}
