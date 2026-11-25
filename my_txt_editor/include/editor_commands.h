


#ifndef EDITOR_COMMANDS_H
#define EDITOR_COMMANDS_H


#include <stdbool.h>
#include <stddef.h>
#include <wchar.h>

/* editor.hはこのヘッダを参照しているため、ここでincludeすると循環参照になり
   プリコンパイル済みヘッダの構築に失敗する。必要な前方宣言のみ置く。 */
struct editor_state;
struct editor_input_context;




// コマンドモードで入力中の文字列と、その文字数。
// command_strは256要素の固定長バッファ。
typedef struct{
    wchar_t command_str[256];
    size_t command_str_len;
}command_str_data;

// 登録されている1つのコマンド名(ワイド文字列)。
// 256要素の固定長バッファで保持する。
typedef struct{
    wchar_t command[256];
}command_data;

// command_dataの可変長配列。command_numが使用数、command_allocate_numが確保数。
// コマンド候補の一覧として使う。
typedef struct{
    command_data *command_list;
    size_t command_num;
    size_t command_allocate_num;
}command_list_data;


// コマンドモード全体の状態。
// 入力中の文字列と、登録済みコマンド一覧をまとめて持つ。
typedef struct{
    command_str_data command_str_data;
    command_list_data command_list_data;
}editor_command_mode_data;

int init_command_mode(struct editor_state *editor_state);

bool handle_command_mode(struct editor_input_context *ctx,wchar_t chr,int input_result);

int cursor_set_command_mode_position(struct editor_state *state);


int draw_command_str(struct editor_input_context *ctx);

int clear_command_str(struct editor_input_context *ctx);


int backspace_command_str(struct editor_input_context *ctx);

int destory_command_mode(struct editor_state *state);

#endif
