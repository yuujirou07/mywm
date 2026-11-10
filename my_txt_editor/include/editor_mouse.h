#ifndef TXT_EDITOR_MOUSE_H
#define TXT_EDITOR_MOUSE_H

#include <ncurses.h>
#include <stdbool.h>
#include "editor_types.h"


#define MOUSE_DOUBLE_CLICK_INTERVAL 290.0



struct editor_input_context;
struct editor_state;

void handle_mouse(struct editor_input_context *ctx,int dir_num);
void editor_screen_mouse_event(struct editor_input_context *ctx);
void file_browse_screen_mouse_event(WINDOW *win, MEVENT *event, struct editor_state *state,int dir_num);
int filetree_mouse_event(struct editor_input_context *ctx);
bool box_contains_point(struct box b,struct pos p);
struct pos editor_mouse_to_buffer_pos(struct editor_state *state,
                        struct pos mouse_pos);
struct pos editor_pos_to_buffer_pos(
                struct editor_state *state,
                struct pos editor_pos);
bool screen_pos_to_box_pos(struct box b1,struct pos p1,struct pos *rt1);

#endif
