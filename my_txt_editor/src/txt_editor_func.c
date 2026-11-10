#include <limits.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include "default_settings.h"
#include "editor_types.h"
#include "editor_log.h"
#include "filetree.h"
#include "input_complete.h"
#include "txt_editor.h"
#include "txt_editor_screen.h"
#include"txt_editor_syntax.h"
#include"editor_color.h"
#include"editor_types.h"

// stateの画面寸法からブラウザの枠・入力欄・内側領域を再計算し、高さ変更時に一覧を再読込する。
// 返り値: なし。一覧の容量不足は再確保し、失敗時は表示行数を既存容量へ制限する。
void resize_file_browser(struct editor_state *state){
    struct file_browse_state *browse = &state->file_browse;

    int old_h = browse->area.h;

    // main.cの初期化と同じ比率で作り直す
    struct box box;
    box.w     = state->scr.scr_size.x / 3;
    box.h     = state->scr.scr_size.y / 2;
    box.pos.x = (state->scr.scr_size.x / 2) - box.w / 2;
    box.pos.y = state->scr.scr_size.y / 4;

    // 枠の実体はstateが持つので、ここを書き換えれば描画側にもそのまま反映される。
    browse->box = box;
    browse->search_box = (struct box){
        .pos = {box.pos.x, box.pos.y + box.h - 1},
        .w = box.w,
        .h = 3,
    };

    //内側は枠の分だけ1セット内へ寄せる
    browse->area.pos.x = box.pos.x + 1;
    browse->area.pos.y = box.pos.y + 1;
    browse->area.w     = (box.w - 2 > 0) ? box.w - 2 : 0;
    browse->area.h     = (box.h - 2 > 0) ? box.h - 2 : 0;

    if(browse->area.h > browse->dir_name_table_rows){
        struct dir_entry *table =
            realloc(browse->dir_name_table,
                    (size_t)browse->area.h * sizeof(*table));
        if(table != NULL){
            browse->dir_name_table      = table;
            browse->dir_name_table_rows = browse->area.h;
        }
        else{
            // 確保できないときは既存テーブルに収まる行数まで削って範囲外書き込みを防ぐ
            browse->area.h = browse->dir_name_table_rows;
        }
    }

    // 幅が変わっても行の内容は有効なままなので、表示件数が変わる高さの変化のときだけ
    // ディレクトリを走査し直す(ドラッグ中に毎回走らせない)。
    if(browse->area.h != old_h){
        load_dir_table(state, &browse->dir_name_table,
            &browse->dir_name_table_rows,
            browse->path_name,
            browse->select_line.now_logical_line,
            &browse->dir_num,
            &browse->dir_name_table_num);
    }
}

// winの寸法からctxの編集領域・各画面配置・カーソル列を更新し、再描画を要求する。
// 返り値: なし。幅61セル・高さ25行未満では、サイズが足りるまで入力を待ち続ける。
void handle_resize(WINDOW *win, struct editor_input_context *ctx){

    struct editor_state *state = ctx->state;
    bool is_cur_show = state->cursor.is_cur_show;
    int right_margin = state->scr.scr_size.x - state->write_area.x_end;
    int bottom_margin = state->scr.scr_size.y - state->write_area.y_end;

    getmaxyx(win, state->scr.scr_size.y, state->scr.scr_size.x);

    // 最小サイズ未満の間は編集画面を組み立てず、警告だけ出して十分な広さになるまで待つ。
    const char *resize_msg = "Please set the dimensions to at least 60 by 20 cells.";
    int resize_msg_len = strlen(resize_msg);

    while(state->scr.scr_size.x <= 60 || state->scr.scr_size.y < 25){
        my_cur_set(state,false);

        // 画面幅に収まらない場合は右端で切り詰めてから中央へ寄せる
        int draw_len = (resize_msg_len < state->scr.scr_size.x)
            ? resize_msg_len : state->scr.scr_size.x;
        int msg_x = (state->scr.scr_size.x - draw_len) / 2;
        int msg_y = state->scr.scr_size.y / 2;

        clear();
        attr_t error_attr = COLOR_PAIR(get_color_pair(ctx,COLOR_BLACK,COLOR_RED));
        attron(error_attr);
        mvaddnstr(msg_y, msg_x, resize_msg, draw_len);
        attroff(error_attr);
        refresh();

        // リサイズ以外の入力は捨て、サイズが変わるたびに新しい中央へ描き直す
        if(getch() == KEY_RESIZE){
            getmaxyx(win, state->scr.scr_size.y, state->scr.scr_size.x);
        }
    }
    my_cur_set(state,is_cur_show);

    state->write_area.x_end = state->scr.scr_size.x - right_margin;
    state->write_area.y_end = state->scr.scr_size.y - bottom_margin;
    if(state->settings_data->show_status_bar){
        state->status_bar->w = state->scr.scr_size.x;
        state->status_bar->h = 1;
        if(state->settings_data->bar_side_state == top){
            state->status_bar->pos.y = state->write_area.y_start - state->status_bar->h;
        }
        else{
            state->status_bar->pos.y = state->write_area.y_end;
        }
    }
    // ファイルツリーを表示中なら、新しい画面幅でも寄せた左端を保つ。
    editor_apply_write_area(state);
    state->write_area.h = state->write_area.y_end - state->write_area.y_start;

    editor_sync_split_line(ctx);

    // 幅が縮むと桁が可視範囲外へ出るため、新しい可視幅で丸め直す。
    // 行番号は変わらないので、行方向はupdate_screen_ratio()側の判定に任せる。
    state->cursor.file_pos.x = editor_cursor_col_boundary(state, state->cursor.file_pos.y,
        state->cursor.file_pos.x);

    //各画面の枠やカーソル位置を新しい画面サイズの比率へ合わせ、再描画を要求する
    update_screen_ratio(ctx);
}

// ctxのカーソル左の1文字をセル幅ごと削除する。行頭では前行へ結合して結合位置へ移動する。
// 返り値: なし。不正な行・セルや結合失敗では処理を打ち切り、変更時は本文の再描画を要求する。
void handle_backspace(struct editor_input_context *ctx) {
    struct editor_state *state = ctx->state;
    int line = state->cursor.file_pos.y;
    if(line < 0 || line >= editor_line_limit(state)){
        return;
    }
    state->cursor.file_pos.x = editor_cursor_col_boundary(state, line,
        state->cursor.file_pos.x);

    if (state->cursor.file_pos.x > 0) {
        // 行内の1文字削除。列容量が要るのはこちらの経路だけ。
        int col_limit = editor_col_limit(state, line);
        if(col_limit <= 0){
            return;
        }
        int del_pos = editor_previous_char_col(state, line, state->cursor.file_pos.x);
        if(del_pos < 0 || del_pos >= col_limit || del_pos >= state->str.line[line]){
            return;
        }
        wint_t *cells = editor_line_cells(state, line);
        if(cells == NULL){
            return;
        }
        int old_len = editor_line_len(state, line);
        int char_width = wcwidth((wchar_t)cells[del_pos]);
        if(char_width < 1){
            char_width = 1;
        }
        if(del_pos + char_width > old_len){
            char_width = old_len - del_pos;
        }
        int new_len = old_len - char_width;
        state->str.line[line] = new_len;

        //画面外にある桁も含めて行末まで詰める
        int move_count = old_len - del_pos - char_width;
        if (move_count > 0)
            memmove(&cells[del_pos], &cells[del_pos + char_width],
                    move_count * sizeof(wint_t));

        memset(&cells[new_len], 0, char_width * sizeof(wint_t));
        state->cursor.file_pos.x = del_pos;


    } else if (line > 0) {
        // 結合位置(=上の行の元の長さ)を結合前に控えておく。
        // 結合後はここがカーソル位置になる。上の行が5文字・下の行が3文字なら、
        // 8文字になった行の6文字目(桁5)へ置く。
        int join_col = editor_line_len(state, line - 1);

        if(remove_line_join_str_data(state,line) < 0){
            return;
        }

        // 移動先の行が画面内に残るなら行だけ動かし、画面先頭より上へ出るならスクロールする。
        if(line > state->scr.scr_start_num){
            editor_move_cursor_line(state, -1);
            state->render_flags |= RENDER_LINE_STATUS;
        }
        else{
            //関数内でcursor.file_pos.yの値も変更される
            editor_screen_move_line(ctx,-1);
        }
        //どちらの経路も桁は触らないため、ここで結合位置へ寄せる
        state->cursor.file_pos.x = editor_clamp_col(state, state->cursor.file_pos.y, join_col);
        state->render_flags |= RENDER_EDIT_SCREEN_BASE;
    }
    state->render_flags |= RENDER_FILE_DATA;
}

// カーソル位置で現在行を分割し、右側の文字列を下の新しい行へ移す。その後、カーソルを次の行の行頭(col=0)へ進める。
// 引数: ctx=ウィンドウ、カーソル行、書き込み領域を持つ入力context。 返り値: なし。
void handle_newline(struct editor_input_context *ctx) {
    struct editor_state *state = ctx->state;
    int line = state->cursor.file_pos.y;
    if(line < 0 || line >= editor_line_limit(state)){
        return;
    }

    // カーソルから右側を新しい行へ切り出す。行スロットの伸長もこの中で行うため、
    // editor_line_limit()が広がる可能性がある。カーソルを動かす前に呼ぶ必要がある
    // (分割位置は現在のカーソル桁から決まるため)。
    if(make_new_line_space(state, line) < 0){
        return;
    }

    // 次の行が編集領域の中に収まるならカーソル行だけ進め、下端なら画面をスクロールする。
    if (line - state->scr.scr_start_num + 1 < state->write_area.h){
        editor_move_cursor_line(state, 1);
    }
    else{
        // editor_screen_move_line()がcursor.file_pos.yの+1も行うため、ここでは動かさない
        editor_screen_move_line(ctx,1);
    }
    state->cursor.file_pos.x = 0;

    if(state->cursor.file_pos.y >= state->file_data.description_line_end){
        //行カウントは0から始まるので1足す
        state->file_data.description_line_end = state->cursor.file_pos.y + 1;
    }
    state->render_flags |= RENDER_EDIT_SCREEN_BASE;
    state->render_flags |= RENDER_FILE_DATA;
}

// stateのカーソル位置へindent_range個の空白を、handle_char_inputを通して順に挿入する。
// 引数: win=文字入力関数へ渡すウィンドウ、state=編集状態。返り値: なし。挿入できない文字があっても通知しない。
void handle_tab(WINDOW *win, struct editor_state *state) {
    int line = state->cursor.file_pos.y;
    if(line < 0 || line >= editor_line_limit(state)){
        return;
    }

    for(int i = 0; i < state->settings_data->indent_range; i++){
        handle_char_input(win, L' ', state);
    }
}

// stateのカーソル位置へワイド文字chを挿入し、後続セル・行長・カーソル・補完状態を更新する。winは未使用。
// 返り値: なし。行が無効、表示幅不足、容量確保失敗なら挿入しない。全角の継続セルは0を格納する。
void handle_char_input(WINDOW *win, wchar_t ch, struct editor_state *state){
    (void)win;

    int line = state->cursor.file_pos.y;
    if(line < 0 || line >= editor_line_limit(state)){
        return;
    }

    int char_width = wcwidth(ch);
    if(char_width < 1){
        char_width = 1;
    }

    int view_cols = editor_view_cols(state);
    if(view_cols <= 0){
        return;
    }

    state->cursor.file_pos.x = 
        editor_clamp_int(
            state->cursor.file_pos.x,
            0,
             view_cols - 1
        );

    state->cursor.file_pos.x = 
        editor_cursor_col_boundary(
        state,
        line,
        state->cursor.file_pos.x
        );

    int writing_area = state->cursor.file_pos.x;

    //上のまるめ作業でほぼ成立しないが、仕様変更などの保険で判定している
    if(writing_area < 0 || writing_area >= view_cols){
        return;
    }

    // 挿入後に必要となる桁数を先に確保する。行末追記なら書き込み位置+文字幅、
    // 途中挿入なら既存の行長+文字幅まで伸びる。
    int line_len = editor_line_len(state, line);

    //入力後そのセルが何セル必要かの処理。
    //(writing_area < line_len)入力途中
    //だと右にline_len(行の保存セル数) + char_width(今回の入力の必要なセル数)
    //になり、その行のセル数を延長するときに使う
    int need = (writing_area < line_len) ? 
        line_len + char_width : writing_area + char_width;
        
    if(!editor_ensure_line_cap(state, line, need)){
        return;
    }

    int col_limit = editor_col_limit(state, line);
    if(col_limit <= 0 || writing_area + char_width > col_limit){
        return;
    }

    wint_t *cells = editor_line_cells(state, line);
    if(cells == NULL){
        return;
    }

    if(writing_area < line_len){
        //画面外にある桁も含めて右へずらす
        int insert_count = line_len - writing_area;
        if (insert_count > 0)
            memmove(&cells[writing_area + char_width],
                &cells[writing_area],
                insert_count * sizeof(wint_t));
    }

    cells[writing_area] = ch;
    for(int i = 1; i < char_width; i++){
        cells[writing_area + i] = 0;
    }
    if(state->str.line[line] <= writing_area){
        state->str.line[line] = writing_area + char_width;
    }
    else if(state->str.line[line] + char_width <= editor_line_cap(state, line)){
        state->str.line[line] += char_width;
    }

    // 保存対象はdescription_line_endまでなので、それより後ろの行へ書き込んだら伸ばす。
    // これが無いと、矢印キーで下へ移動して打った内容が丸ごと保存されない。
    if(line >= state->file_data.description_line_end){
        //行カウントは0から始まるので1足す
        state->file_data.description_line_end = line + 1;
    }

    state->cursor.file_pos.x = writing_area + char_width;
    //自動で改行する仕様。判定は挿入前の桁で行う(可視幅の右端に居たかどうか)。
    if (writing_area >= view_cols - 1 && line + 1 < editor_line_limit(state)){
        editor_move_cursor_line(state, 1);
        state->cursor.file_pos.x = 0;
        if(state->cursor.file_pos.y >= state->file_data.description_line_end){
            //行カウントは0から始まるので1足す
            state->file_data.description_line_end = state->cursor.file_pos.y + 1;
        }
    }

    //入力候補ウィンドウ処理
    if(state->settings_data->auto_complete_settings_data.auto_complete_enabled){
        if(ch != L' '){
            state->edit_input_complete_data.show = true;
            set_edit_comp_candidacy_part_data(state,ch);
        }
        else{
            init_edit_comt_candidacy_part_data(state);
        }
    }
    state->render_flags |= RENDER_FILE_DATA;
}

// 矢印キー入力を処理し、行長を超えない位置へカーソルを移動する。画面端ではスクロールしながら表示内容を補う。
// 引数: ctx=カーソルと表示位置を持つ入力context、ch=KEY_UP/DOWN/LEFT/RIGHT。 返り値: なし。
void handle_input_allow(struct editor_input_context *ctx,wchar_t ch){
    struct editor_state *state = ctx->state;
    int line_limit = state->settings_data->default_load_line_size;
    if(line_limit <= 0)return;
    

    // 画面内に留まったまま動けるかどうかの判定。画面座標ではなく
    // 「カーソル行が表示範囲のどこにいるか」で決める。
    int line = state->cursor.file_pos.y;
    bool can_move_up_in_view   = (line > state->scr.scr_start_num);
    bool can_move_down_in_view = (line - state->scr.scr_start_num + 1 < state->write_area.h);
    state->cursor.file_pos.x = editor_cursor_col_boundary(state, line,
        state->cursor.file_pos.x);

    switch(ch){
        case KEY_UP:{
            if (can_move_up_in_view && line > 0) {
                editor_move_cursor_line(state, -1);
            }
            else if(state->scr.scr_start_num > 0){
              editor_screen_move_line(ctx,-1);
            }
            break;
        }
        case KEY_DOWN:{
            if(line + 1 >= line_limit)break;
            if (can_move_down_in_view)editor_move_cursor_line(state, 1);
            else editor_screen_move_line(ctx,1);
            break;
        }
        case KEY_LEFT:{
            if (state->cursor.file_pos.x > 0){
                state->cursor.file_pos.x = editor_previous_char_col(state, line,
                    state->cursor.file_pos.x);
            }
            else if (can_move_up_in_view && line > 0) {
                editor_move_cursor_line(state, -1);
                state->cursor.file_pos.x = editor_clamp_col(state, state->cursor.file_pos.y,
                    editor_line_len(state, state->cursor.file_pos.y));
            }
            break;
        }
        case KEY_RIGHT:{
            if(line < 0 || line >= line_limit){
                break;
            }
            if (state->cursor.file_pos.x < editor_clamp_col(state, line, editor_line_len(state, line))){
                state->cursor.file_pos.x = editor_next_char_col(state, line,
                    state->cursor.file_pos.x);
            }
            else if (can_move_down_in_view && line + 1 < line_limit) {
                editor_move_cursor_line(state, 1);
                state->cursor.file_pos.x = 0;
            }
            break;
        }
    }
    state->render_flags |= RENDER_LINE_STATUS;
}

// stateのremove_line_num行（1以上）を直前の行へ結合し、後続の行情報を前へ詰める。
// 返り値: 結合後のセル数、状態・行・セルが不正なら-1。結合した行が削除行の容量も引き継ぐ。
int remove_line_join_str_data(struct editor_state *state,long remove_line_num){
    if(state == NULL || state->str.line_offset == NULL || state->str.line_cap == NULL ||
       state->str.line == NULL || remove_line_num < 1 ||
       remove_line_num >= state->str.line_capacity){
        return -1;
    }

    int prev   = (int)remove_line_num - 1;
    int target = (int)remove_line_num;

    wint_t *prev_cells   = editor_line_cells(state, prev);
    wint_t *target_cells = editor_line_cells(state, target);
    if(prev_cells == NULL || target_cells == NULL){
        return -1;
    }

    int prev_len = state->str.line[prev];
    int join_len = state->str.line[target];

    // 前の行の実データ末尾へ、削除行の実データをそのまま詰める
    if(join_len > 0){
        memmove(&prev_cells[prev_len], target_cells, (size_t)join_len * sizeof(wint_t));
    }

    // 削除行の領域終端までを前の行が吸収する
    long region_end = state->str.line_offset[target] + state->str.line_cap[target];
    int joined_len = prev_len + join_len;
    int joined_cap = (int)(region_end - state->str.line_offset[prev]);

    // memmoveは移動元を消さないため、結合部より後ろに古いセルが残る。
    // line[]までしか通常は読まないが、行が伸びたときに露出しないよう0で埋めておく。
    if(joined_cap > joined_len){
        memset(&prev_cells[joined_len], 0,
               (size_t)(joined_cap - joined_len) * sizeof(wint_t));
    }
    state->str.line[prev]     = joined_len;
    state->str.line_cap[prev] = joined_cap;

    // 削除行より後ろの行情報を1つ前へ詰めて、境界を1つ消す
    int move_count = state->str.line_capacity - target - 1;
    if(move_count > 0){
        memmove(&state->str.line_offset[target], &state->str.line_offset[target + 1],
                sizeof(long) * (size_t)move_count);
        memmove(&state->str.line_cap[target], &state->str.line_cap[target + 1],
                sizeof(int) * (size_t)move_count);
        memmove(&state->str.line[target], &state->str.line[target + 1],
                sizeof(int) * (size_t)move_count);
    }

    // 詰めた分だけ空いた末尾スロットを、未確保として扱われる状態にしておく
    int last = state->str.line_capacity - 1;
    state->str.line[last]        = 0;
    state->str.line_cap[last]    = 0;
    state->str.line_offset[last] = state->str.total_capacity;

    if(state->file_data.description_line_end > 0){
        state->file_data.description_line_end--;
    }
    if(state->file_data.file_line_start_num_counter > 0){
        state->file_data.file_line_start_num_counter--;
    }

    return joined_len;
}


// stateのmake_space_line_num行を現在のカーソル列で分割し、後半を直後の新しい行とする。
// 返り値: 新しい行のセル数、行・状態不正や行情報確保失敗は-1。本文の所有権はstateに残る。
int make_new_line_space(struct editor_state *state,long make_space_line_num){
    if(state == NULL || state->str.line == NULL || state->str.line_offset == NULL ||
       state->str.line_cap == NULL || make_space_line_num < 0 ||
       make_space_line_num >= editor_line_limit(state)){
        return -1;
    }

    int line    = (int)make_space_line_num;
    int old_len = state->str.line[line];

    // 分割位置はカーソルの現在桁。端末へ問い合わせず、モデルの値をそのまま使う。
    // 行長を超えないよう丸める。
    int col = editor_clamp_int(state->cursor.file_pos.x, 0, old_len);

    // 分割後、行make_space_line_num+1が実データとして加わる分だけ、
    // 「実際に使用中の行数」を伸ばす必要がある。
    long used_rows = state->file_data.description_line_end;
    if(used_rows < make_space_line_num + 1){
        used_rows = make_space_line_num + 1;
    }
    if(used_rows > INT_MAX - 2){
        return -1;
    }

    // 挿入先(line+1)を空けるための空き行スロットが末尾に無ければ、行情報配列を伸ばす
    if(!editor_ensure_row_capacity(state, (int)used_rows + 1)){
        return -1;
    }

    int target = line + 1;

    // targetより後ろの実データ行(target..used_rows-1)を1つ後ろへずらし、
    // targetの位置を新しい行のために空ける
    int move_count = (int)used_rows - target;
    if(move_count > 0){
        memmove(&state->str.line_offset[target + 1], &state->str.line_offset[target],
                sizeof(long) * (size_t)move_count);
        memmove(&state->str.line_cap[target + 1], &state->str.line_cap[target],
                sizeof(int) * (size_t)move_count);
        memmove(&state->str.line[target + 1], &state->str.line[target],
                sizeof(int) * (size_t)move_count);
    }

    // lineが持っていた容量域を2つに割り直す。物理セル(wint_line_str_data)は動かさない。
    long old_offset = state->str.line_offset[line];
    int  old_cap    = state->str.line_cap[line];
    int  new_len    = old_len - col;

    state->str.line[line]     = col;
    state->str.line_cap[line] = col;

    state->str.line_offset[target] = old_offset + col;
    state->str.line_cap[target]    = old_cap - col;
    state->str.line[target]        = new_len;

    // 元の行に余白が無い状態で行末分割すると、割り当てる容量が0の行ができてしまう。
    // 容量0の行は編集経路の列容量チェックに引っかかって操作を受け付けなくなるため、
    // 最低限の余白を持たせておく(伸長は後続行のoffsetまで面倒を見てくれる)。
    if(state->str.line_cap[target] <= 0){
        editor_ensure_line_cap(state, target, EDITOR_LINE_COL_SLACK);
    }
    if(state->str.line_cap[line] <= 0){
        editor_ensure_line_cap(state, line, EDITOR_LINE_COL_SLACK);
    }

    state->file_data.description_line_end = used_rows + 1;
    if(state->file_data.file_line_start_num_counter <
       state->settings_data->default_load_line_size){
        state->file_data.file_line_start_num_counter++;
    }

    return new_len;
}

// ファイルブラウザのパス入力モードを設定する。
// 引数: file_browse=更新対象、flag=設定する有効状態。 返り値: なし。file_browseがNULLなら何もしない。
void set_file_browse_path_input_mode(struct file_browse_state *file_browse,bool flag){
    if(file_browse == NULL)return;
    file_browse->path_input_mode = flag;
}

// ファイルブラウザのパス入力モードを取得する。
// 引数: file_browse=取得元。 返り値: 現在の有効状態。file_browseがNULLならfalse。
bool get_file_browse_path_input_mode(struct file_browse_state *file_browse){
    if(file_browse == NULL)return 0;
    return file_browse->path_input_mode;
}

// エディタ状態とncursesのカーソル表示状態を同時に更新する。
// 引数: state=更新対象、set=trueで表示、falseで非表示。 返り値: なし。stateがNULLなら何もしない。curs_set()の失敗は通知しない。
void my_cur_set(struct editor_state *state,bool set){
    if(state == NULL)return;
    state->cursor.is_cur_show = set;
    curs_set(set);
}

// 次回ncursesへ反映する画面カーソル座標をstateへ保存する。
// 引数: pos=保存する画面座標、state=保存先のエディタ状態。posは値コピーされる。 返り値: 常に0。stateがNULLの場合の動作は未定義。
int cur_pos_push(struct pos pos, struct editor_state *state){
    // 描画中にカーソルを動かすと表示がちらつくため、座標は貯めておき最後にset_cur_posで反映する。
    state->cursor.show_cur_pos_queue = pos;
    return 0;
}

// stateに保存された画面カーソル座標をncursesへ反映する。
// 引数: state=反映する座標を持つエディタ状態。 返り値: 常に0。move()の失敗は呼び出し元へ通知しない。
int set_cur_pos(struct editor_state *state){
    move(state->cursor.show_cur_pos_queue.y,state->cursor.show_cur_pos_queue.x);
    return 0;
}



// 言語を保存し、有効な構文着色と補完データを初期化する。
// 引数: ctx=stateとsettings_dataが設定済みの入力context、lang=新しい言語。 返り値: 常に0。現実装では補完データの初期化失敗を返さない。
int editor_set_env_lang(struct editor_input_context *ctx,language lang){
    if(ctx->state->settings_data->built_in_syntax){
        init_syntax(&ctx->syntax_data);
        set_syntax_language(lang,&ctx->syntax_data);
    }
    env_language_ctl(&lang,set);
    if(ctx->state->settings_data->auto_complete_settings_data.auto_complete_enabled){
        init_edit_complete_data(ctx);
    }

    return 0;
}



// ctxを0クリアし、stateの借用先とキーマップ配列を設定する。
// win・lsp_data・画面配置は値が決まってから呼び出し側が設定する。
// 返り値: 成功0、キーマップ配列の確保失敗-1。stateはctxより長く生きること。
int init_input_context(struct editor_input_context *ctx,struct editor_state *state){
    memset(ctx,0,sizeof(*ctx));
    ctx->state       = state;
    state->input_ctx = ctx;

    ctx->input_manager.key_maps.key_map_allocate_num = 16;
    ctx->input_manager.key_maps.key_mapp_list =
        malloc(sizeof(key_map_data) * ctx->input_manager.key_maps.key_map_allocate_num);
    if(ctx->input_manager.key_maps.key_mapp_list == NULL){
        error_log("malloc");
        return -1;
    }
    return 0;
}

// ctx->state->settings_dataへ既定値を設定してから、設定JSONの有効な項目で上書きする。
// 返り値: 常に0。ctx・state・settings_dataは呼び出し前に接続しておく。
int init_settings_data(struct editor_input_context *ctx){
    //最初にデフォルト設定を読み込みユーザーが設定している項目だけ更新する
    load_default_editor_settings(ctx->state->settings_data);
    load_custom_editor_settings(ctx->state->settings_data);
    return 0;
}


// env_language_ctl()に保存された現在の言語を取得する。
// 返り値: 未設定ならUNKNOWN、設定済みなら最後に保存した言語。
language get_env_language(){
    language tmp_lang;
    env_language_ctl(&tmp_lang,get);
    return tmp_lang;
}


// getなら保存済み言語をlangへ書き、setならlangの値を保存する。
// 引数: lang=読み書き先の有効なポインタ、flags=getまたはset。 返り値: 現実装ではflagsにかかわらず0。
int env_language_ctl(language *lang,enum flags flags){
    static language static_env_lang = UNKNOWN;
    if(flags == get){
        *lang = static_env_lang;
        return 0;
    }
    else if(flags == set){
        static_env_lang = *lang;
        return 0;
    }
    return 0;
}



int get_write_screen_pos_chr(struct editor_state *state,struct pos pos,char *chr){
    if(state->file_data.is_open_file == false){
        chr = NULL;
        return -1;
    }

    //write_area.wは0からカウントしないため-1して判定
    if(pos.x < 0 || pos.y < 0 ||
        state->write_area.w-1 <= pos.x ||
        state->write_area.h-1 <= pos.y){
            chr = NULL;
            return -1;
    }

    int logical_line_pos = state->scr.scr_start_num + pos.y;
    int line_start_num = state->file_data.file_line_start_num[logical_line_pos];
    char *str_line_start_ptr = state->file_data.file_str_data[line_start_num];
    if((int)strlen(str_line_start_ptr) < pos.x){
        chr = NULL;
        return -1;
    }
    
    *chr = str_line_start_ptr[pos.x];
    return pos.x;
}





int SetCursorModeScreen(struct editor_state *state,screen_state screen_state){
    switch(screen_state){
        case edit_screen:
            SetCursorMode(state,CURSOR_INPUT_MODE);
            SetCursorColor(state,CURSOR_COLOR_WHITE);
            break;
        default:
            SetCursorMode(state,CURSOR_DEFAULT_MODE);
            SetCursorColor(state,CURSOR_COLOR_DEFAULT);
            break;
    }
    return 0;
}


int SetCursorMode(struct editor_state *state,CURSOR_MODE CurMode){
    state->cursor.CurMode = CurMode;
    fputs(CURSOR_SHAPE[state->cursor.CurMode],stdout);
    fflush(stdout);
    return 0;
}

int SetCursorColor(struct editor_state *state,CURSOR_COLOR color){

    if((sizeof(CURSOR_COLOR_ESC)/sizeof(CURSOR_COLOR_ESC[0])) <= color){
        error_log("cursor color error");
        return -1;
    }

    state->cursor.Color = color;
    fputs(CURSOR_COLOR_ESC[color],stdout);
    fflush(stdout);
    return 0;
}


double comb_time_interval_ms(struct timespec t1,struct timespec t2){
    return (double)((t2.tv_sec * 1000) - (t1.tv_sec * 1000)) + 
        (((double)t2.tv_nsec / 1000000) - ((double)t1.tv_nsec / 1000000));
}
