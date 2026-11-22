


#ifndef EDITOR_INIT_H
#define EDITOR_INIT_H



#include"editor.h"


int init_curses_color(struct editor_input_context *ctx);
int get_color_id(struct editor_input_context *ctx,int fg,int bg);
short get_color_pair(struct editor_input_context *ctx,int fg,int bg);

int set_cell_bg_color(struct editor_input_context *ctx,short color,int x,int y,int n);

#endif