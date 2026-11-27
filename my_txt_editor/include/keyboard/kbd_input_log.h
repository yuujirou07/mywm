#ifndef KBD_INPUT_LOG_H_
#define KBD_INPUT_LOG_H_

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#include "editor_types.h"


#define KEY_LOG_ALLOCATE_NUM_ 32

struct editor_state;

// キー入力ログ1件分。入力文字、getch系の結果、入力時の画面状態を記録する。
typedef struct{
    wchar_t key;
    int input_result;
    screen_state screen_state;
}key_data;

// key_dataの可変長配列。key_countが記録数、key_allocate_numが確保数。
// キー入力ログの保持領域として使う。
typedef struct{
    key_data *key_log;
    size_t key_count;
    size_t key_allocate_num;
}key_log_data;


// キーボード入力に関する状態。現在はキー入力ログのみを持つ。
typedef struct{
    key_log_data key_log_data;
}key_board_data;

int KL_add(struct editor_state *state,wchar_t ch,int result,screen_state screen_state);
int KL_write_file();
const key_data *KL_GetLog(struct editor_state *state,size_t *n);

int KL_open_key_log_file(const char *relative_path);

int KL_get_key_data_log(struct editor_state *state,key_data **key,size_t *n);

#endif
