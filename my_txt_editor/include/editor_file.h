#ifndef TXT_EDITOR_FILE_H
#define TXT_EDITOR_FILE_H

#include <stdbool.h>
#include <wchar.h>
#include "editor_types.h"

struct editor_state;
struct editor_input_context;
struct editor_settings;
struct dir_entry;
struct dir_table;
struct file_browse_select_state;
struct file_select_line;

// editor_file.c
int load_dir_table(struct editor_state *state,struct dir_entry **table,int *table_rows,char *path_name,int start_num,int *dir_num,int *table_num);
void load_file(struct editor_state *state,struct dir_entry *table,int table_num,char *path_name,struct file_browse_select_state *select_state);
bool editor_alloc_text_buffer(struct editor_state *state, int line_count, long total_capacity);
void editor_free_text_buffer(struct editor_state *state);
bool editor_ensure_line_cap(struct editor_state *state, int line, int need);
bool editor_ensure_row_capacity(struct editor_state *state, int need_rows);
void set_line_memory(struct editor_state *state);
void load_string_data(struct editor_state *state,long load_start_line,int load_size);
void load_all_lines(struct editor_state *state);
char *editor_buffer_to_utf8(struct editor_state *state);
void save_file(struct editor_state *state);
void load_screen_size(struct editor_state *state);
void load_default_editor_settings(struct editor_settings *settings_data);
void load_custom_editor_settings(struct editor_settings *settings_data);
void file_select_line_update(struct file_select_line *file_select_line,int line);
void input_mode_tmp_path(char *path,enum flags flags);
const wchar_t *now_open_path_name(struct dir_table *path,enum flags flags);
int check_dir_mem(struct dir_table *dir_table,int size);
int now_input_path_open(struct editor_state *state,struct editor_input_context *ctx);
int file_browser_show_mem_start_num(int start_num,enum flags flags);

#endif
