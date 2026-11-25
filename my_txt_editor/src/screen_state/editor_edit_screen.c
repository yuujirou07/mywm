#include <stdlib.h>
#include <string.h>
#include "editor_func.h"
#include "editor_screen.h"
#include "editor_types.h"

static void send_lsp_did_change(struct editor_input_context *ctx);

static void editor_mouse_click_event(struct editor_input_context *ctx,MEVENT *mev);

// 通常編集画面のキー入力を処理する。
// 引数: ctx=編集画面の描画・状態更新に必要なcontext、input_result=get_wch()の結果、ch=入力文字またはKEY_*。 返り値: 入力ループを続けるならtrue、qで終了するならfalse。
bool handle_edit_screen_input(struct editor_input_context *ctx, int input_result, wint_t ch){
    struct editor_state *state = ctx->state;
    WINDOW *win = ctx->win;

    if(state->file_data.now_open_path_name[0] != NUL && state->settings_data->show_status_bar){
        state->render_flags |= RENDER_EDIT_SCREEN_BASE;
        state->render_flags |= RENDER_FILETREE;
    }
    if (input_result == KEY_CODE_YES && ch == KEY_BACKSPACE && state->cursor.is_cur_show) {
        reduce_edit_comp_candidacy_part_data(state);
        handle_backspace(ctx);
        if(state->settings_data->lsp.lsp_use){
            send_lsp_did_change(ctx);
        }
        if(state->settings_data->built_in_syntax){
            set_syntax_data(&ctx->syntax_data,ctx);
        }
        return true;
    }
    else if(ch == CTRL('h')){
        editor_set_screen_state(state, line_jump_mode);
        reset_jump_mode(state);
        state->render_flags |= RENDER_LINE_JUMP;
        return true;
    }
    else if (ch == CTRL('f')) {
        my_cur_set(state,false);
        editor_set_screen_state(state, file_browse_screen_mode);
        state->render_flags |= RENDER_FILE_BROWSE;
        return true;
    }
    else if(ch == CTRL('s')){
        file_save_motion(ctx);
        return true;
    }
    else if(ch == CTRL('n')){
        editor_set_screen_state(state,filetree_screen_mode);
        // 編集領域をツリーの幅だけ右へ寄せる。
        int x;
        int y;
        getmaxyx(win, y, x);
        struct box tmp_ft_box = 
            {(struct pos){0,0},x/10,y};
        show_filetree(ctx,tmp_ft_box);
        my_cur_set(state,false);
    }
    else if(ch == CTRL('u')){
        editor_set_screen_state(state,editor_command_mode);
        cursor_set_command_mode_position(state);

        return true;
    }
    //デバッグ用
    else if(ch == CTRL(' ')){
       lsp_send_completion(ctx->lsp_data->to_server_fd,state);
    }
    if (ch == KEY_MOUSE) {
        handle_mouse(ctx, 0);
        state->render_flags |= RENDER_LINE;

        return true;
    }
    if (state->cursor.is_cur_show) {

        if (ch == KEY_ENTER || ch == '\n' || ch == '\r') {
            bool is_new_line = true;
            //入力補完設定
            if(state->settings_data->auto_complete_settings_data.auto_complete_enabled){
                if(state->edit_input_complete_data.
                    comp_world_candidacy_part_data.
                    comp_w_cand_part_d_count > 0 &&
                    state->edit_input_complete_data.show){
                    is_new_line = false;
                    editor_input_str(
                            state,
                            state->edit_input_complete_data.
                                word_data.world[state->edit_input_complete_data.
                                    now_select_complete_world_num]
                        );
                }
                init_edit_comt_candidacy_part_data(state);
            }

            if(is_new_line){
                handle_newline(ctx);
            }
            
        } else if (ch == '\t') {
            init_edit_comt_candidacy_part_data(state);
            handle_tab(win, state);
        } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN){
            handle_input_allow(ctx,ch);
        //通常可視文字入力
        } else if (input_result == OK && iswprint((wint_t)ch)) {
            //デバッグ用
            if (ch == 'q') {
                return false;

            }
            handle_char_input(win, (wchar_t)ch, state);
            if(state->settings_data->lsp.lsp_use){
                send_lsp_did_change(ctx);
            }
        }
        //struct pos write_area_pos = editor_cursor_write_area_pos(state);
        if(state->settings_data->built_in_syntax){
            set_syntax_data(&ctx->syntax_data,ctx);
        }
        state->render_flags |= RENDER_LINE_STATUS;
        
    } else {
        if (input_result == OK && iswprint((wint_t)ch)) {
            // カーソル非表示中は、論理行を見える位置へ戻してから入力する。
            if(editor_line_limit(state) == 0){
                return true;
            }
            move_view_to_line(state, state->cursor.file_pos.y, state->cursor.file_pos.x);
            my_cur_set(state,true);

            handle_char_input(win, (wchar_t)ch, state);
            if(state->settings_data->lsp.lsp_use){
                send_lsp_did_change(ctx);
            }
            if(state->settings_data->built_in_syntax){
                set_syntax_data(&ctx->syntax_data,ctx);
            }
            state->render_flags |= RENDER_LINE_STATUS;
        }
        if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP || ch == KEY_DOWN){
            // nは現在の論理行。move_view_to_line()がstateを書き換える前に保持する。
            int n = editor_cursor_logical_line_pos(state);
            my_cur_set(state,true);
            move_view_to_line(state, n - 1, 0);

            if(state->settings_data->built_in_syntax){
                set_syntax_data(&ctx->syntax_data,ctx);
            }
            
            if(state->settings_data->auto_complete_settings_data.auto_complete_enabled){
                state->edit_input_complete_data.show = true;
                state->render_flags |= RENDER_FILE_DATA;
            }

        }
    }
    if(ch == 'q') {
        return false;
    }

    return true;
}

// 編集バッファ全体をUTF-8化してLSPへ変更通知を送る。
// 引数: ctx=編集状態とLSP通信状態を持つ入力context。 返り値: なし。LSP未使用・未初期化・送信準備失敗時は通知しない。 所有権: 送信成功時のUTF-8文字列はstateが保持し、失敗時はこの関数が解放する。
static void send_lsp_did_change(struct editor_input_context *ctx){


    struct editor_state *state = ctx->state;
    struct lsp_process *lsp = ctx->lsp_data;
    char uri[(PATH_MAX * 3) + sizeof("file://")];
    char *text;
    int next_version;

    if(!state->settings_data->lsp.lsp_use || lsp == NULL ||
       !lsp->initialized || lsp->to_server_fd < 0 ||
       lsp->update_data.file_update_counter < 1 ||
       strcmp(lsp->update_data.path_name, state->file_data.now_open_path_name) != 0 ||
       lsp_path_to_file_uri(uri, sizeof(uri), state->file_data.now_open_path_name) == -1){
        return;
    }

    // 危険: 文字入力ごとに全文を確保・UTF-8化し、blockingなpipeへ同期送信する。
    // ファイルやLSP応答が重いと入力処理そのものが長時間停止する。
    text = editor_buffer_to_utf8(state);
    if(text == NULL){
        return;
    }

    next_version = lsp->update_data.file_update_counter + 1;
    if(lsp_send_did_change(lsp->to_server_fd, uri, next_version, text) == 0){
        lsp->update_data.file_update_counter = next_version;
        free(state->str.chr_file_all_str_data);
        state->str.chr_file_all_str_data = text;
        return;
    }

    free(text);
}


// ctx->state->status_barへ画面上の矩形boxを値コピーする。
// 返り値: 常に0。各ポインタは有効であること。範囲検査や再描画要求は行わない。
int set_status_bar_size(struct editor_input_context *ctx,struct box box){
    *ctx->state->status_bar = box;
    return 0;
}


// NUL終端のワイド文字列strをstateのカーソル位置から1文字ずつ入力する。
// 返り値: 空文字列なら-1、それ以外0。各文字の挿入失敗は返り値へ反映しない。
int editor_input_str(struct editor_state *state,wchar_t *str){
    int str_len = wcslen(str);
    if(str_len <= 0)return -1;
    for(int i = 0;i < str_len;i++){
        handle_char_input(stdscr,str[i],state);
    }
    return 0;
}

// stateの行番号と、有効な区切り線・ステータスバーを描画する。
// start_pos/end_posは画面座標の区切り線端点、winは下位描画関数へ渡す。返り値: なし。
void draw_edit_screen_base(struct editor_state *state,WINDOW *win,struct pos start_pos,struct pos end_pos){
    draw_line_numbers(state);
    if(state->settings_data->draw_split_line){
        draw_line(start_pos,end_pos, all_draw_mode);
    }
    if(state->settings_data->show_status_bar){
        // draw_status_bar_path()はステータスバー行を丸ごと塗り直すため、
        // 行ステータスより先に描かないと"N/M"が消える。
        draw_status_bar_line(state,*state->status_bar);
        draw_status_bar_path(state);
        draw_line_status(state,win);
    }
}

// 読み込んだファイル内容のうち、現在画面に見える範囲を描画する。
// 引数: state=表示開始行・書き込み領域・読み込み済み文字バッファ。 返り値: なし。
void draw_file_data(struct editor_state *state){
    for(int i = 0; i < state->write_area.h; i++){
        int line = state->scr.scr_start_num + i;
        draw_editor_buffer_line(state, line, i);
    }
}

// 表示開始行がmoved行(正で下方向)動いた分だけ構文情報をずらし、新しく見える行を解析し直す。
// 引数: ctx=構文情報と画面状態、moved=実際に動いた行数(0なら何もしない)。 返り値: なし。組み込み構文が無効なら何もしない。
static void sync_syntax_after_scroll(struct editor_input_context *ctx,int moved){
    struct editor_state *state = ctx->state;
    if(!state->settings_data->built_in_syntax || moved == 0)return;

    const int h = state->write_area.h;
    int amount = moved > 0 ? moved : -moved;
    // 構文情報は画面相対行で持つ。下へ動くと既存分は上(負)へ、上へ動くと下(正)へずれる。
    scroll_syntax_pos_data(&ctx->syntax_data,-moved,h);

    // 下へ動けば最下行側、上へ動けば最上行側のamount行が新しく見える。
    if(amount > h)amount = h;
    int first_line = moved > 0 ? h - amount : 0;
    for(int line = first_line;line < first_line + amount;line++){
        update_line_syntax_data(ctx,line);
    }
}

// マウスホイール1回分と、溜まっている同方向のホイール入力をまとめてスクロールする。
// 引数: ctx=編集画面の状態、scroll_up=trueで上、falseで下。 返り値: なし。
// 別種の入力が来たら押し戻してループを抜け、nodelayは必ず元(ブロッキング)へ戻す。
static void scroll_editor_by_wheel(struct editor_input_context *ctx,bool scroll_up){
    struct editor_state *state = ctx->state;
    const mmask_t same_dir_ev = scroll_up
        ? (BUTTON4_PRESSED | BUTTON4_CLICKED)
        : (BUTTON5_PRESSED | BUTTON5_CLICKED);
    const int sensitivity = state->settings_data->editor_scroll_sensitivity;
    wint_t chr;
    MEVENT tmp_msev;

    //スクロール入力は溜まりやすいため、一時的にノンブロッキングにして溜まった分をループで消化する
    nodelay(stdscr,true);
    do{
        const int before_scr_start_num = state->scr.scr_start_num;
        editor_scroll_logical_line(state,scroll_up ? -sensitivity : sensitivity);
        sync_syntax_after_scroll(ctx,state->scr.scr_start_num - before_scr_start_num);

        if(get_wch(&chr) == ERR){
            break;
        }
        if(chr == KEY_MOUSE){
            // 同方向のホイールなら続けて消化し、それ以外のマウス入力は戻して終了する。
            if(getmouse(&tmp_msev) == OK && !(tmp_msev.bstate & same_dir_ev)){
                ungetmouse(&tmp_msev);
                break;
            }
        }
        else{
            unget_wch(chr);
            break;
        }
    }while(1);
    nodelay(stdscr,false);
}

// 編集画面でのマウスクリックを処理する。クリック位置に応じてカーソル移動などを行う。
// 返り値: なし。
static void editor_mouse_click_event(struct editor_input_context *ctx,MEVENT *mev){
    
    struct editor_state *state = ctx->state;

    const int x = mev->x;
    const int y = mev->y;
    
    const struct box tmp_write_area_box = 
        {(struct pos){
            ctx->state->write_area.x_start,
            ctx->state->write_area.y_start
        },
            ctx->state->write_area.w,
            ctx->state->write_area.h
        };
    if(box_contains_point(tmp_write_area_box,(struct pos){x,y})){
        struct pos write_area_pos;
        write_area_pos.y = y - state->write_area.y_start;
        write_area_pos.x = x - state->write_area.x_start;
        int line_num = state->scr.scr_start_num + write_area_pos.y;
        if(state->file_data.description_line_end < line_num){
            line_num = *state->str.line;
        }
        editor_set_cursor(state,line_num,write_area_pos.x);
    }
    return;
}


// ctxの取得済みマウスイベントでスクロール・本文クリック・ツリーへの移行を処理する。
// 返り値: なし。ホイールは表示開始行だけ、本文クリックは論理カーソルを動かし、最後にカーソル表示可否を更新する。
void editor_screen_mouse_event(struct editor_input_context *ctx){
    MEVENT *event = &ctx->mouse_event;
    struct editor_state *state = ctx->state;

    //ファイルツリー領域でのマウスイベント判定
    //ファイルツリー内でマウスがスクロールや
    // クリックを行えばファイルツリーにフォーカスを移す
    if(state->file_tree_data.is_show){
        if(box_contains_point(
            state->file_tree_data.ft_box,
            (struct pos){event->x,event->y})){
            editor_set_screen_state(state,filetree_screen_mode);
            my_cur_set(state,false);
            return;
        }
    }

    //ホイールの向き(上下)ごとのイベントマスク
    const mmask_t button5_ev = BUTTON5_CLICKED | BUTTON5_PRESSED;
    const mmask_t button4_ev = BUTTON4_CLICKED | BUTTON4_PRESSED;

    //ホイールスクロール処理(上下共通)。上下同時に立っている場合は上を優先する。
    if(event->bstate & (button5_ev | button4_ev)){
        scroll_editor_by_wheel(ctx,(event->bstate & button4_ev) != 0);
    }

    //マウスクリック処理
    if(event->bstate &
       (BUTTON1_PRESSED |
        BUTTON1_CLICKED  | 
        BUTTON1_DOUBLE_CLICKED)){
        editor_mouse_click_event(ctx,event);
    }
    else{
        return;
    }

    // カーソル行が編集領域から出たら隠す。戻ってきたらまた出す。
    my_cur_set(state,editor_cursor_is_visible(state));
}
