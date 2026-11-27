#ifndef TXT_EDITOR_FUNC_H_
#define TXT_EDITOR_FUNC_H_

#include <ncurses.h>
#include <stdbool.h>
#include <wchar.h>
#include <time.h>
#include "editor_types.h"
#include "editor_syntax.h"

struct editor_state;
struct editor_input_context;
struct file_browse_state;

// editor_func.c
void resize_file_browser(struct editor_state *state);
void handle_resize(WINDOW *win, struct editor_input_context *ctx);
void handle_backspace(syntax *syntax,struct editor_state *state);
void handle_newline(syntax *syntax,struct editor_state *state);
void handle_tab(WINDOW *win, struct editor_state *state);
void handle_char_input(WINDOW *win, wchar_t ch, struct editor_state *state);
void handle_input_allow(syntax *syntax,struct editor_state *state,wchar_t ch);
int remove_line_join_str_data(struct editor_state *state,long remove_line_num);
int make_new_line_space(struct editor_state *state,long make_space_line_num);
void set_file_browse_path_input_mode(struct file_browse_state *file_browse,bool flag);
bool get_file_browse_path_input_mode(struct file_browse_state *file_browse);
void my_cur_set(struct editor_state *state,bool set);
int init_input_context(struct editor_input_context *ctx,struct editor_state *state);
int init_settings_data(struct editor_state *state);
int editor_set_env_lang(syntax *syntax,struct editor_state *state,language lang);
int env_language_ctl(language *lang,enum flags flags);
language get_env_language();


int str_display_width(const char *str);

double comb_time_interval_ms(struct timespec t1,struct timespec t2);

#endif
