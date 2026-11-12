


#ifndef EDITOR_COMMANDS_H
#define EDITOR_COMMANDS_H


#include"editor.h"
#include <wchar.h>




typedef struct{
    wchar_t command_str[256];
    int command_str_len;
}command_str_data;
typedef struct{
    command_str_data command_str_data;
}editor_command_mode_data;

int init_command_mode(struct editor_state *editor_state);

bool handle_command_mode(struct editor_input_context *ctx,wchar_t chr,int input_result);

int cursor_set_command_mode_position(struct editor_state *state);






#endif
