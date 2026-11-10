


#ifndef EDITOR_COMMANDS_H
#define EDITOR_COMMANDS_H


#include"editor.h"


bool handle_command_mode(struct editor_input_context *ctx,wchar_t chr,int input_result);

int cursor_set_command_mode_position(struct editor_state *state);






#endif