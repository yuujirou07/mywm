#ifndef TXT_EDITOR_DRAW_H
#define TXT_EDITOR_DRAW_H

#include <ncurses.h>
#include <stdint.h>
#include "editor_types.h"

struct editor_state;
struct editor_input_context;
struct dir_entry;
struct clear_box_data;

// 罫線を全体描画するか、スクロールで欠けた部分だけ補修するかを指定する。
enum line_mode {
    all_draw_mode, // 始点から終点まで罫線を描き直す。
    fix_scr_line_damage, // スクロールで破損した罫線だけを補修する。
};

// editor_draw.c
void draw_editor_buffer_line(struct editor_state *state, int line, int write_y);
void draw_line_numbers(struct editor_state *state);
void draw_line(struct pos start_pos,struct pos end_pos,enum line_mode mode);
void draw_box(struct box box);
void request_draw_box(struct editor_state *state,struct box box);
void draw_now_path_name(struct box file_browse_box,char *path_name);
void draw_edit_screen_base(struct editor_state *state,WINDOW *win,struct pos start_pos,struct pos end_pos);
void draw_box_inside_dir(struct editor_state *state,struct dir_entry *table);
void draw_select_dir_scene_color(struct editor_state *state,int dir_num,int num);
void show_file_browse(struct editor_state *state);
void set_file_select_line(struct editor_state *state,int dir_num,int line);
void editor_screen_move_line(struct editor_input_context *ctx,int num);
void editor_error_screen(struct editor_state *state,char *error_comment);
void draw_file_data(struct editor_state *state);
void draw_status_bar_line(struct editor_state *state,struct box status_bar);
void draw_status_bar_path(struct editor_state *state);
void clear_box(struct clear_box_data *clear_box);
void draw_line_status(struct editor_state *state,WINDOW *win);
void update_screen(struct editor_input_context *ctx);
void request_clear_box(struct editor_state *state, struct box box);
void draw_line_jump(struct editor_state *state);
void draw_make_file_dialog(struct editor_input_context *ctx);
void draw_file_browse(struct editor_input_context *ctx);
int set_clear_box(struct clear_box_data *clear_box_data,struct box box);
int clear_status_bar_outline(struct editor_state *state);
int clear_box_area(struct editor_input_context *ctx,struct box b);
struct box complete_box_screen(struct editor_state *state);
int draw_editor_complete_word_box(struct editor_input_context *ctx);
int draw_edit_complete_world(struct editor_state *state);
int redraw_write_area_line_str(struct editor_state *state,uint16_t line,uint16_t size);

#endif
