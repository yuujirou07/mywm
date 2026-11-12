#include <assert.h>
#include <dirent.h>
#include <ncurses.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>




#include "editor_types.h"
#include "filetree.h"
#include "input_complete.h"
#include "settings_screen.h"
#include "editor.h"
#include"editor_syntax.h"
#include"editor_color.h"



// 線の向きから描画範囲・移動量・罫線文字を決める。
// 引数: start_pos/end_pos=線の端点、range/step_x/step_y/line_ch=計算結果の書き込み先。 返り値: 縦線または横線ならtrue、斜め線ならfalse。
static bool line_draw_info(struct pos start_pos, struct pos end_pos,
                int *range, int *step_x, int *step_y, chtype *line_ch){
    if(start_pos.x == end_pos.x){
        *range = abs(start_pos.y - end_pos.y);
        *step_x = 0;
        *step_y = 1;
        *line_ch = ACS_VLINE;
        return true;
    }
    if(start_pos.y == end_pos.y){
        *range = abs(start_pos.x - end_pos.x);
        *step_x = 1;
        *step_y = 0;
        *line_ch = ACS_HLINE;
        return true;
    }
    return false;
}

// 指定方向へrangeセル分の罫線を描画する。
// 引数: start_pos=開始座標、range=描画セル数、step_x/step_y=1セルごとの移動量、line_ch=描画文字。 返り値: なし。
static void draw_full_line(struct pos start_pos, int range, int step_x, int step_y, chtype line_ch){
    for(int i = 0; i < range; i++){
        move(start_pos.y + i * step_y, start_pos.x + i * step_x);
        addch(line_ch);
    }
}

// 指定セルの罫線が壊れていれば描き直す。
// 引数: y/x=確認する座標、line_ch=期待する罫線文字。 返り値: 既に正しい罫線だったら1、描き直したら0。
static int fix_line_cell(int y, int x, chtype line_ch){
    if(mvinch(y, x) == line_ch){
        return 1;
    }
    move(y, x);
    addch(line_ch);
    return 0;
}

// 両端から罫線を確認し、壊れたセルだけ補修する。
// 引数: start_pos=開始座標、range=確認セル数、step_x/step_y=1セルごとの移動量、line_ch=期待する罫線文字。 返り値: なし。
static void fix_line_damage(struct pos start_pos, int range, int step_x, int step_y, chtype line_ch){
    int is_fixed_all = 0;

    for(int i = 0; i < range; i++){
        is_fixed_all += fix_line_cell(start_pos.y + i * step_y,
                                      start_pos.x + i * step_x,
                                      line_ch);
        is_fixed_all += fix_line_cell(start_pos.y + (range - i) * step_y,
                                      start_pos.x + (range - i) * step_x,
                                      line_ch);

        // fix_line_cellは「既に正しい」で1を返す。両端から1セルずつ見て正常なセルが
        // 2つ続いたら、それ以上内側は無事とみなして走査を打ち切る(毎回全セルを確認しない)。
        if(is_fixed_all >= 2){
            break;
        }
    }
}

// 指定した論理行を画面上の1行へ描画する。
// 引数: state=文字バッファと書き込み領域、line=描画する論理行、write_y=描画先の編集領域内y座標(write_area上端が0)。 返り値: なし。
void draw_editor_buffer_line(struct editor_state *state, int line, int write_y){
    int screen_y = state->write_area.y_start + write_y;
    //描画に使うのは可視幅だけ。バッファ側の容量とは無関係。
    int col_limit = editor_view_cols(state);
    if(screen_y < state->write_area.y_start || screen_y >= state->write_area.y_end || col_limit <= 0){
        return;
    }

    mvhline(screen_y, state->write_area.x_start, ' ', col_limit);
    if(line < 0 || line >= editor_line_limit(state)) return;
    if(state->str.line[line] <= 0) return;

    // wint_line_str_dataは画面セル位置に合わせて格納している。
    // addwstrで詰めて描くと2桁幅文字の後ろでズレるため、
    // セルごとのx座標へ1文字ずつ描く。
    wint_t *cells = editor_line_cells(state, line);
    if(cells == NULL) return;

    int max_col = editor_line_len(state, line);
    if(max_col > col_limit) max_col = col_limit;

    for(int col = 0; col < max_col; col++){
        wint_t cell = cells[col];
        if(cell == 0) continue;

        wchar_t ch = (wchar_t)cell;
        mvaddnwstr(screen_y, state->write_area.x_start + col, &ch, 1);
    }
}

// stateの表示開始行に対応する行番号を描き、描画後の端末カーソル位置は復元しない。
// 返り値: なし。stateは画面寸法・編集領域を設定済みとし、全描画後にカーソルを同期する。
void draw_line_numbers(struct editor_state *state) {
    struct scr_data *scr_data = &state->scr;
    struct write_possible_area *area = &state->write_area;
    int line_number_space = (area->x_start > 0) ? area->x_start - 1 : 0;
    
    for (int i = 0; i < area->h; i++) {
        char num_str[6];

        int size = snprintf(num_str, 6, "%d", (scr_data->scr_start_num + i) + 1);
        int draw_x = line_number_space - size;
        if(draw_x < 0){
            draw_x = 0;
        }
        mvhline(area->y_start + i, 0, ' ', line_number_space);
        mvprintw(area->y_start + i, draw_x, "%s", num_str);
    }
}

// 画面のstart_posからend_pos方向の水平・垂直罫線を、modeに応じて全描画または補修する。winは未使用。
// 返り値: なし。全描画は終点を除き、座標の増加方向に描くため始点は左または上に置く。斜線は描かない。
void draw_line(struct pos start_pos,struct pos end_pos,WINDOW *win,enum line_mode mode){
    (void)win;

    int range;
    int step_x;
    int step_y;
    chtype line_ch;

    if(!line_draw_info(start_pos, end_pos, &range, &step_x, &step_y, &line_ch)){
        return;
    }

    switch(mode){
        case all_draw_mode:
            draw_full_line(start_pos, range, step_x, step_y, line_ch);
            break;

        case fix_scr_line_damage:
            fix_line_damage(start_pos, range, step_x, step_y, line_ch);
            break;
    }
}

// 指定セルに既に描かれている文字だけを取り出す。mvinch()は色やA_ALTCHARSETも一緒に返すため、文字の部分だけを残す。
// 引数: y/x=確認する座標。 返り値: その位置の文字。何も無ければ空白。
static chtype box_existing_chr(int y, int x){
    return (chtype)(mvinch(y, x) & A_CHARTEXT);
}

// cornerに重なる既存罫線existingに合わせ、接続用のT字または元の角文字を返す。
// 引数はncursesの罫線文字。状態や画面は変更しない。
static chtype box_joint_chr(chtype corner, chtype existing){
    if(existing == ACS_HLINE){
        //上側の角なら下へ、下側の角なら上へ伸びる
        if(corner == ACS_ULCORNER || corner == ACS_URCORNER)return ACS_TTEE;
        if(corner == ACS_LLCORNER || corner == ACS_LRCORNER)return ACS_BTEE;
        return corner;
    }
    if(existing == ACS_VLINE){
        //左側の角なら右へ、右側の角なら左へ伸びる
        if(corner == ACS_ULCORNER || corner == ACS_LLCORNER)return ACS_LTEE;
        if(corner == ACS_URCORNER || corner == ACS_LRCORNER)return ACS_RTEE;
        return corner;
    }
    return corner;
}

// 画面上のboxの枠と四隅を描き、既存罫線に重なる角は接続用文字へ置き換える。
// 返り値: なし。幅・高さが0以下なら描かず、winにかかわらず標準画面へ描く。
void draw_box(struct box box, WINDOW *win){

    int x = box.pos.x;
    int y = box.pos.y;
    int w = box.w;
    int h = box.h;

    if(w <= 0 || h <= 0)return;
    

    struct pos top_left     = {x,     y};
    struct pos top_right    = {x + w - 1, y};
    struct pos bottom_left  = {x,         y + h - 1};
    struct pos bottom_right = {x + w - 1, y + h - 1};

    //角に重なる罫線は、線を引く前に読んでおく。
    chtype corner_ul = box_joint_chr(ACS_ULCORNER,box_existing_chr(y,         x));
    chtype corner_ur = box_joint_chr(ACS_URCORNER,box_existing_chr(y,         x + w - 1));
    chtype corner_ll = box_joint_chr(ACS_LLCORNER,box_existing_chr(y + h - 1, x));
    chtype corner_lr = box_joint_chr(ACS_LRCORNER,box_existing_chr(y + h - 1, x + w - 1));

    draw_line(top_left,    bottom_left,  win, all_draw_mode);
    draw_line(top_right,   bottom_right, win, all_draw_mode);
    draw_line(top_left,    top_right,    win, all_draw_mode);
    draw_line(bottom_left, bottom_right, win, all_draw_mode);

    mvaddch(y,         x,         corner_ul);
    mvaddch(y,         x + w - 1, corner_ur);
    mvaddch(y + h - 1, x,         corner_ll);
    mvaddch(y + h - 1, x + w - 1, corner_lr);

}

// queueの矩形全体を消去してから枠を順に描き、件数を0へ戻す。
// 引数: ctx=色表を持つcontext、queue=有効な枠キュー、win=枠描画関数へ渡すウィンドウ。返り値: なし。
static void flush_box_queue(struct editor_input_context *ctx,struct box_queue *queue, WINDOW *win){

    for(int i = 0; i < queue->count; i++){
        clear_box_area(ctx,queue->box[i]);
        draw_box(queue->box[i], win);
    }
    queue->count = 0;
}

// 次回更新で描く枠を描画要求キューの末尾へ追加する。
// 引数: state=描画要求の保存先、box=枠線を含む描画領域。 返り値: なし。キューが満杯なら追加しない。
void request_draw_box(struct editor_state *state,struct box box){
    struct box_queue *queue = &state->draw_box_queue;

    if(queue->count >= DRAW_BOX_REQUEST_MAX)return;
    queue->box[queue->count++] = box;
    state->render_flags |= RENDER_BOX;
}







// ctxの表示開始行と論理カーソル行をnum行だけ同時に動かし、再描画を要求する。
// 返り値: なし。移動先が有効な場合だけ更新し、桁は移動先行へ丸める。呼び出し側で行を二重加算しない。
void editor_screen_move_line(struct editor_input_context *ctx,int num){
    struct editor_state *state = ctx->state;
    int line_limit = state->settings_data->default_load_line_size;
    int next_cursor_line = state->cursor.file_pos.y + num;
    int next_scr_start = state->scr.scr_start_num + num;
    if(line_limit <= 0 || next_cursor_line < 0 || next_cursor_line >= line_limit || next_scr_start < 0){
        return;
    }

    state->cursor.file_pos.y = next_cursor_line;
    state->cursor.file_pos.x = editor_cursor_col_boundary(state, next_cursor_line,
        state->cursor.file_pos.x);
    state->scr.scr_start_num = next_scr_start;

    // 表示がnum行ずれるので構文情報は逆向きにずらし、新しく見える端の1行だけ解析し直す。
    if(state->settings_data->built_in_syntax){
        scroll_syntax_pos_data(&ctx->syntax_data,-num,state->write_area.h);
        int update_line = num > 0 ? state->write_area.h - 1 : 0;
        update_line_syntax_data(ctx,update_line);
    }

    state->render_flags |= RENDER_EDIT_SCREEN_BASE;
    state->render_flags |= RENDER_FILE_DATA;
}



// ステータスバーの横線と区切り接続部を描画する。
// 引数: state=書き込み領域、status_bar=描画するバー領域、win=描画先ウィンドウ。 返り値: なし。
void draw_status_bar_line(struct editor_state *state,struct box status_bar,WINDOW *win){
    struct pos end_pos = (struct pos){status_bar.pos.x + status_bar.w - 1,status_bar.pos.y};
    clear_status_bar_outline(state);
    draw_line(status_bar.pos,end_pos,win,all_draw_mode);
    mvaddch(status_bar.pos.y,state->write_area.x_start-1,ACS_TTEE);
}

// ステータスバー中央に現在開いているファイル名を描画する。
// 引数: state=ファイルパスとステータスバー設定、win=描画先ウィンドウ。 返り値: なし。
void draw_status_bar_path(struct editor_state *state, WINDOW *win){
    if(!state->settings_data->show_status_bar){
        return;
    }
    
    int status_y = (state->settings_data->bar_side_state == top)
        ? state->status_bar->pos.y - 1 : state->status_bar->pos.y;
    
    char *draw_path = NULL; 
    if(state->file_data.now_open_path_name[0] == '\0'){ 
        draw_path = "Unknown file";
    }
    else{
        
        draw_path = strrchr(state->file_data.now_open_path_name, '/');
        if(draw_path == NULL){
            draw_path = state->file_data.now_open_path_name;
        }

        int path_len = strlen(draw_path);
        int draw_len = path_len;
        int max_len = state->status_bar->w - 2;

        if(max_len <= 0){
            return;
        }
        if(draw_len > max_len){
            draw_path += draw_len - max_len;
            draw_len = max_len;
        }
    }
    size_t draw_len = strlen(draw_path);
    int draw_x = state->status_bar->pos.x + (state->status_bar->w - draw_len) / 2;
    if(state->settings_data->bar_side_state == top){
        mvhline(status_y, state->status_bar->pos.x, ' ', state->status_bar->w);
    }
    else{
        draw_status_bar_line(state, *state->status_bar, win);
    }
    mvaddnstr(status_y, draw_x, draw_path, draw_len);
}

// 登録済み矩形を空白で消し、消去要求件数を0へ戻す。
// 引数: clear_box=消去対象の矩形配列と件数。 返り値: なし。ncursesの描画失敗は通知しない。
void clear_box(struct clear_box_data *clear_box){
    for(int f = 0;f < clear_box->clear_box_counter;f++){
        struct box box = clear_box->clear_box[f];
        for(int i = box.pos.y;i < box.pos.y + box.h;i++){
            mvhline(i,box.pos.x,' ',box.w);
        }
    }
    clear_box->clear_box_counter = 0;
}

// ステータスバー右端へ現在行と総行数を描画する。
// 引数: state=カーソル・行数・ステータスバー設定、win=描画先。現在は標準画面へ描くため未使用。 返り値: なし。ステータスバー非表示時は何もしない。
void draw_line_status(struct editor_state *state,WINDOW *win){
    if(!state->settings_data->show_status_bar){
        return;
    }

    (void)win;
    char line_status_str[32];
    snprintf(line_status_str, sizeof(line_status_str), "%d/%ld",
             state->cursor.file_pos.y+1, state->file_data.description_line_end);
    int total_line_len = strlen(line_status_str);
    struct pos write_start_pos;
    write_start_pos.y = (state->settings_data->bar_side_state == top)
        ? state->status_bar->pos.y - 1 : state->status_bar->pos.y;
    if(total_line_len > state->status_bar->w){
        total_line_len = state->status_bar->w;
    }
    write_start_pos.x = state->status_bar->pos.x + state->status_bar->w - total_line_len;

    int clear_len = (state->status_bar->w < (int)sizeof(line_status_str))
        ? state->status_bar->w : (int)sizeof(line_status_str);
    mvhline(write_start_pos.y,
            state->status_bar->pos.x + state->status_bar->w - clear_len,
            ' ', clear_len);
    mvaddnstr(write_start_pos.y, write_start_pos.x, line_status_str, total_line_len);
}



// ctxのrender_flagsにある描画要求を順に処理し、予約済みカーソルを反映してフラグをRENDER_NONEに戻す。
// 返り値: なし。ctxの状態・描画先は必須。現実装のRENDER_ALLは空処理で、個別描画も行わない。
void update_screen(struct editor_input_context *ctx){
    unsigned int flags = ctx->state->render_flags;
    struct editor_state *state = ctx->state;
    WINDOW *win = ctx->win;    

    if(flags & RENDER_ALL){
        
    }
    else{
        if(flags & RENDER_CLEAR_BOX){
            clear_box(&state->clear_box_data);
        }
        if(flags & RENDER_STATUS_BAR_LINE){
            draw_status_bar_line(state, *state->status_bar, win);
        }

        if(flags & RENDER_LINE_STATUS){
            draw_line_status(state, win);
        }
        if(flags & RENDER_SETTINGS){
            bool is_cur_move = false;
            struct pos mouse_pos;
            draw_settings_screen(ctx);   
            if(state->settings_screen_data.value_input_mode){
                draw_settings_search_box(ctx);
                getyx(ctx->win,mouse_pos.y,mouse_pos.x);
                is_cur_move = true;
            }
            draw_settings_explanation_box(ctx);
            if(is_cur_move)cur_pos_push(mouse_pos,state);
        }

        // 順序に意味がある。本文を描いた後で枠・ブラウザなどを重ねる(後から描く方が上に出る)。
        if(flags & RENDER_FILE_DATA){
            draw_file_data(state);
        }
        if(flags & RENDER_LINE){
            draw_line(ctx->edit_screen.line_start_pos,
                      ctx->edit_screen.line_end_pos, win, all_draw_mode);
        }
        if(flags & RENDER_SELECT_DIR_SCENE_COLOR){
            draw_select_dir_scene_color(state,state->file_browse.dir_name_table_num,get_color_pair(ctx,COLOR_BLACK,COLOR_WHITE));
        }
        if(flags & RENDER_EDIT_SCREEN_BASE){
            draw_edit_screen_base(state, win, ctx->edit_screen.line_start_pos,
                                  ctx->edit_screen.line_end_pos);
            // 行番号の消去は画面左端から始まりツリーの枠まで及ぶため、
            // ツリーを表示中は必ずこの後で描き直す。
            if(state->file_tree_data.is_show){
                draw_filetree(ctx,&state->file_tree_data);
            }
        }
        if(flags & RENDER_FILE_BROWSE){
            draw_file_browse(ctx);
        }
        if(flags & RENDER_BOX){
            flush_box_queue(ctx,&state->draw_box_queue, win);
        }
        if(flags & RENDER_LINE_JUMP){
           draw_line_jump(state);
           
        } 
        if(flags & RENDER_MAKE_FILE){
            draw_make_file_dialog(ctx);
        }
        if(flags & RENDER_EDIT_COMPLETE_WINDOW){
            draw_editor_complete_word_box(ctx);
            draw_edit_complete_world(state);

        }
        if(flags & RENDER_COMMAND_MODE){
                 
        }
    }

    set_cur_pos(state);
    ctx->state->render_flags = RENDER_NONE;
}

// 次回更新で消す矩形を消去要求配列へ追加する。
// 引数: state=消去要求の保存先、box=消去対象領域。 返り値: なし。要求配列が満杯ならエラー画面へ遷移する。
void request_clear_box(struct editor_state *state, struct box box){
    if(state->clear_box_data.clear_box_counter >= box_retention_max){
        editor_error_screen(state,"clear box over flow ");
        return;
    }
    int *counter = &state->clear_box_data.clear_box_counter;
    state->clear_box_data.clear_box[(*counter)++] = box; 
    state->render_flags |= RENDER_CLEAR_BOX;
}



// 消去対象の矩形をclear_box_dataの末尾へ追加する。
// 引数: clear_box_data=追加先、box=消去対象領域。 返り値: 成功時0、引数がNULLまたは配列が満杯なら-1。
int set_clear_box(struct clear_box_data *clear_box_data,struct box box){
        if(clear_box_data == NULL){
            return -1;
        }
        if((int)(sizeof(clear_box_data->clear_box)/sizeof(struct box)) <= clear_box_data->clear_box_counter){
            return -1;   
        }

        clear_box_data->clear_box[clear_box_data->clear_box_counter++] = box;
        return 0;
}





// stateのステータスバー領域の上辺と左右端を空白で消す。幅は非負であること。
// 返り値: 常に0。現実装は下辺を横一列に消す処理を行わず、描画失敗も通知しない。
int clear_status_bar_outline(struct editor_state *state){
    struct box tmp_status_bar_box = *state->status_bar;
    char outline_str[tmp_status_bar_box.w + 1];
    memset(outline_str,' ',tmp_status_bar_box.w);
    outline_str[tmp_status_bar_box.w] = '\0';
    for(int i = 0;i < tmp_status_bar_box.h;i++){
        if(i == 0 || i == tmp_status_bar_box.h){
            mvaddnstr(tmp_status_bar_box.pos.y + i,
                tmp_status_bar_box.pos.x,
                outline_str,
                tmp_status_bar_box.w);
        }
        else{
            char wall_outline[2] = {' ','\0'};
            mvaddnstr(tmp_status_bar_box.pos.y + i,
                tmp_status_bar_box.pos.x,
                wall_outline,sizeof(wall_outline));
             mvaddnstr(tmp_status_bar_box.pos.y + i,
                tmp_status_bar_box.pos.x + tmp_status_bar_box.w,
                wall_outline,sizeof(wall_outline));
        }
    }
    return 0;
}


// 補完候補枠(編集領域内座標)を画面座標へ変換して返す。
struct box complete_box_screen(struct editor_state *state){
    struct box b = state->edit_input_complete_data.box;
    b.pos.x += state->write_area.x_start;
    b.pos.y += state->write_area.y_start;
    return b;
}

// stateの補完表示設定とカーソル位置から候補枠を配置・保存し、内部消去と枠描画を行う。
// 枠の位置は編集領域内座標で保存する(描画時に画面座標へ変換する)。
// 返り値: 常に0。縦の配置余地が足りない場合はエラー画面へ遷移する。
int draw_editor_complete_word_box(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;

    struct pos tmp_mouse_pos;
    state->edit_input_complete_data.show = true;
    struct pos tmp_pos = editor_cursor_write_area_pos(state);

    // 枠は入力のたびに位置が変わるため、動く前に旧枠を消し、下に隠れていた本文は後で描き直す。
    //補完ウィンドウが動く前に消しておく
    if(state->edit_input_complete_data.box.h > 0 && 
        state->edit_input_complete_data.box.w > 0){
        clear_box_area(ctx,complete_box_screen(state));
    }

    struct box old_box = {0};
    if(state->edit_input_complete_data.show_data.pos_mode == EDIT_COMP_TRACKING){
        old_box = state->edit_input_complete_data.box;
    }

    switch(state->edit_input_complete_data.show_data.pos_mode){
        case EDIT_COMP_TRACKING:
        case EDIT_COMP_UNKNOWN:
            tmp_mouse_pos = tmp_pos;
            tmp_mouse_pos.y += 1;
            break;

        case EDIT_COMP_FIXED_TOP_LEFT:
            tmp_mouse_pos = (struct pos){0,0};
            break;

        case EDIT_COMP_FIXED_TOP_RIGHT:
            tmp_mouse_pos = (struct pos){state->write_area.w,0};
            break;

        case EDIT_COMP_FIXED_BOTTOM_LEFT:
            tmp_mouse_pos = (struct pos){0,state->write_area.h};
            break;

        case EDIT_COMP_FIXED_BOTTOM_RIGH:
            tmp_mouse_pos = (struct pos){state->write_area.w,state->write_area.h};
            break;

        default:
            tmp_mouse_pos = tmp_pos;
            tmp_mouse_pos.y += 1;
    }


    struct box tmp_comp_box = (struct box){
        tmp_mouse_pos,
        state->edit_input_complete_data.show_data.size.x,
        state->edit_input_complete_data.show_data.size.y
    };

    /* 入力候補ウィンドウ画面内収束処理*/
    if(tmp_comp_box.pos.x < 0 ){
        tmp_comp_box.pos.x = 0;
    }

    else if(tmp_comp_box.pos.x + tmp_comp_box.w >= state->write_area.w){
        tmp_comp_box.pos.x = state->write_area.w - tmp_comp_box.w;
    }   

    // 下に収まらないときはカーソルの上側へ反転表示する(上にも収まらなければ画面が小さすぎる)。
    //編集画面下とカーソルの間がウィンドウの縦サイズより小さくなった場合上に表示する
    if(state->write_area.h - tmp_comp_box.pos.y < tmp_comp_box.h){
        if(tmp_comp_box.pos.y < tmp_comp_box.h){
            editor_error_screen(state,"youre screen is so small resize bigger");
            return 0;
        }
        tmp_comp_box.pos.y = tmp_pos.y - tmp_comp_box.h;
    }
    state->edit_input_complete_data.box = tmp_comp_box;

    //補完ウィンドウ移動後の下の文字の再描画(未表示なら旧枠はw,hが0)
    if(old_box.w > 0 && old_box.h > 0){
        redraw_write_area_line_str(state,old_box.pos.y,old_box.h);
        if(state->settings_data->built_in_syntax){
            set_syntax_color_line(ctx,old_box.pos.y,old_box.h);
        }
    }
    clear_box_area(ctx,complete_box_screen(state));
    draw_box(complete_box_screen(state),stdscr);
    state->render_flags |= RENDER_BOX;
    return 0;
}


// 画面上を空白で消す。
// 返り値: 幅・高さが2以下なら-1、それ以外0。ncursesの描画失敗は通知しない。
int clear_box_area(struct editor_input_context *ctx,struct box b){
    if(b.h <= 2 || b.w <= 2)return -1;
    attr_t clear_attr = COLOR_PAIR(get_color_pair(ctx,COLOR_WHITE,COLOR_BLACK));
    for(int i = 0;i < b.h;i++){
        // 本文色で塗る。b.w-1にして右端の枠線列は残す。
        mvhline(b.pos.y + i,b.pos.x,L' '|clear_attr,b.w - 1);
    }
    return 0;
}


// stateの補完候補を候補枠の高さ・幅に合わせて描く。表示行数分の候補配列を確保しておく。
// 返り値: 常に0。候補数が0以下なら描画せず、空文字の候補行は飛ばす。
int draw_edit_complete_world(struct editor_state *state){
    complete_world_data *cmp_data = &state->edit_input_complete_data.word_data;
    struct box tmp_cmp_box = complete_box_screen(state);

    if(state->edit_input_complete_data.word_data.world_num <= 0)return 0;
    
    for(int i = 0;i < state->edit_input_complete_data.box.h - 2;i++){
        if(cmp_data->world[i][0] == L'\0')continue;
        mvaddnwstr(tmp_cmp_box.pos.y + i,tmp_cmp_box.pos.x + 1,cmp_data->world[i],tmp_cmp_box.w - 2);
    }
    return 0;
}

// stateの編集領域のline行目からsize行分を、ファイル文字列から再描画する。sizeは編集領域の高さで切り詰める。
// 引数: state=描画対象、line=再描画を始める編集領域内の相対行、size=再描画する行数。
// 返り値: 成功なら0、ファイル未オープンまたはlineが表示・ファイル範囲外なら-1。
int redraw_write_area_line_str(struct editor_state *state,uint16_t line,uint16_t size){

    if(line > state->write_area.h){
        return -1;
    }

    size = (size > state->write_area.h - line)?state->write_area.h - line:size;
    for(int i = 0;i < size;i++){
        //行の範囲外はdraw_editor_buffer_line()が空行として扱う
        draw_editor_buffer_line(state,state->scr.scr_start_num + line + i,line + i);
        
    }



    return 0;
}
