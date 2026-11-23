#include <ncurses.h>
#include <stdlib.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>

#include "editor.h"
#include "editor_func.h"
#include "editor_log.h"
#include "filetree.h"
#include "ftj.h"
#include "editor_screen.h"
#include"editor_color.h"


// ctxのツリーを画面上のft_boxに表示する状態へ変更し、編集領域・ステータスバー・カーソルを合わせる。
// 返り値: なし。本文と枠の再描画を要求し、ツリーの内容は解放しない。
void show_filetree(struct editor_input_context *ctx,struct box ft_box){
    struct editor_state *state = ctx->state;

    set_filetree_box(&state->file_tree_data,ft_box);
    state->file_tree_data.is_show = true;

    if(state->settings_data->show_status_bar){
        //ステータスバーが上または下に設定されている場合ファイルツリーと重なってしまうため、
        // ステータスバーのボックスデータを重ならない大きさに変更する
        //(左右にステータスバーを設置する機能は実装していない)
        if(state->settings_data->bar_side_state == top ||
            state->settings_data->bar_side_state == bottom){
            int x = getmaxx(stdscr);
            //ステータスバーの範囲変更
            struct box tmp_status_bar_box =
                {(struct pos){ft_box.pos.x + ft_box.w,state->status_bar->pos.y},
                x - (ft_box.pos.x + ft_box.w),
                state->status_bar->h};
            //変更前の枠を消す
            clear_status_bar_outline(state);

            set_status_bar_size(ctx,tmp_status_bar_box);
            state->render_flags |= RENDER_STATUS_BAR_LINE;
        }
    }
    // 編集領域と区切り線を新しい左端へ合わせ、新しい位置で描き直す。
    editor_apply_write_area(state);
    editor_sync_split_line(ctx);
    editor_sync_cursor(state);
    state->render_flags |= RENDER_EDIT_SCREEN_BASE;
    state->render_flags |= RENDER_FILETREE;
    state->render_flags |= RENDER_FILE_DATA;
}


// ファイルツリーを閉じ、編集領域を元の位置へ戻す。
// 引数: ctx=画面サイズ・編集領域・ファイルツリーを持つ入力context。 返り値: なし。
void hide_filetree(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;

    state->file_tree_data.is_show = false;
    state->file_tree_data.ft_box = (struct box){(struct pos){0,0},0,0};
    state->file_tree_data.ft_item_box = (struct box){(struct pos){0,0},0,0};
    editor_apply_write_area(state);
    editor_sync_split_line(ctx);

    // ツリーの枠が残らないよう画面全体を消してから編集画面へ戻す。
    restore_edit_screen(state);

    //ステータスバーのサイズを戻す
    if(state->settings_data->show_status_bar){
        if(state->settings_data->bar_side_state == top || 
            state->settings_data->bar_side_state == bottom){
            int status_bar_y = (state->settings_data->bar_side_state == top)?1:getmaxy(ctx->win) - 1;
            state->status_bar->pos = (struct pos){0,status_bar_y};
            state->status_bar->w = getmaxx(ctx->win);
            state->status_bar->h = 3;
        }
        state->render_flags |= RENDER_STATUS_BAR_LINE;
    }
}


// ctxのツリー画面でchの開閉・幅変更・マウス操作・行ジャンプを処理する。input_resultは未使用。
// 返り値: qによる終了要求はfalse、それ以外はtrue。ctxの各画面状態とウィンドウは設定済みとする。
bool handle_filetree_screen_input(struct editor_input_context *ctx,wint_t ch,int input_result){
    (void)input_result;
    struct editor_state *state = ctx->state;
    file_tree_data *tmp_file_tree = &state->file_tree_data;
    if(state->cursor.is_cur_show && 
        !tmp_file_tree->ft_search_data.is_search_mode){
        my_cur_set(state,false);
    }

    
    if(ch == (wchar_t)L'j' || ch == (wchar_t)L'k'){
        if(tmp_file_tree->item_view_count_num > 
            tmp_file_tree->ft_item_select_line){
            int next_select_line = tmp_file_tree->ft_item_select_line+1;
            if(ch == (wchar_t)L'k'){
                next_select_line -= 2;
            }
            
            ft_set_file_tree_item_select_line(
                tmp_file_tree,
                next_select_line
            );
            state->render_flags |= RENDER_FILETREE;
        }
    }

    if(ch == KEY_ENTER || ch == L'\n'){
        int item_view_num = state->file_tree_data.item_view_count_num;
        //選択行(項目領域内の0始まりの行)を画面のy座標へ変換する。
        //項目領域の先頭1行は項目を置かない(項目はview_h=1から並ぶ)ため+1する。
        const struct box item_box = state->file_tree_data.ft_item_box;
        int select_line = item_box.pos.y + 1 + state->file_tree_data.ft_item_select_line;
        ft_item_view_data *tmp_item_data_list = state->file_tree_data.item_view_data;

        ft_item_view_data *tmp_item = NULL;
        for(int i = 0;i < item_view_num;i++){
            if(tmp_item_data_list[i].screen_y == select_line){
                tmp_item = &tmp_item_data_list[i];
                break;
            }
        }
        
        if(tmp_item != NULL){
            enum select_state state = ft_item_open_event(ctx,tmp_item);
            //通常ファイルなら編集画面に移動する
            if(state == file){
                editor_set_screen_state(ctx->state,edit_screen_mode);
                set_cell_color_pair(PAIR_TEXT,item_box.pos.x,tmp_item->screen_y,item_box.w - 1);
                my_cur_set(ctx->state,true);
                ctx->state->render_flags |= RENDER_EDIT_SCREEN_BASE;
                ctx->state->render_flags |= RENDER_FILETREE;
            }
        }
    }

    if(ch == (wchar_t)L'i'){
        struct pos cur_pos = 
            (struct pos){0,0};

        cur_pos_push(cur_pos,state);
        my_cur_set(state,true);
        tmp_file_tree->ft_search_data.is_search_mode = true;
    }


    if(ch == CTRL('n')){
        // ツリーを出したキーと同じキーで閉じる。
        hide_filetree(ctx);
        return true;
    }
    else if(ch == CTRL('h')){
        editor_set_screen_state(state,line_jump_mode);
        reset_jump_mode(state);
        state->render_flags |= RENDER_LINE_JUMP;
    }
    if(ch == 'q')return false;
    if(ch == KEY_MOUSE){
        handle_mouse(ctx,0);
    }
    
    //ファイルツリーの横幅変更処理
    if(ch == '>' || ch == '<' || ch == KEY_LEFT || ch == KEY_RIGHT){
        int size_fiff = (ch == '<')?-1:(ch == KEY_LEFT)?-1:1;
        //画面外にいくかの判定をつける
        int ft_w = ctx->state->file_tree_data.ft_box.w + size_fiff;
        change_file_tree_width(ctx,ft_w);
    }

    return true;
}


// ctxの既存ツリー幅が画面内なら、幅をwidthへ更新して編集領域と再描画要求を設定する。
// 返り値: 更新0、既存幅が0以下または画面幅以上なら-1。新しいwidthの範囲は呼び出し側で保証する。
int change_file_tree_width(struct editor_input_context *ctx,int width){
    int x = getmaxx(stdscr);
    
    file_tree_data *tmp_ft_data = &ctx->state->file_tree_data;
    if(tmp_ft_data->ft_box.w <= 0 || x <= tmp_ft_data->ft_box.w){
        return -1;
    }
    tmp_ft_data->ft_box.w = width;
    show_filetree(ctx,tmp_ft_data->ft_box);
    return 0;
}

// item_dataの項目を開く。ディレクトリなら開閉を切り替え、ファイルなら読み込んで編集画面の表示を更新する。
// 引数: ctx=編集状態・構文情報を持つcontext、item_data=開く項目(table_ptrは有効であること)。 返り値: なし。必要な再描画フラグを立てる。
enum select_state ft_item_open_event(struct editor_input_context *ctx,ft_item_view_data *item_data){

    ft_item_view_data *item = item_data;
    struct editor_state *state = ctx->state;

    enum select_state path_state =
        get_path_state(item->table_ptr->absolute_path);
        
    if(path_state == folder){
        item->is_open = !item->is_open;
        state->render_flags |= RENDER_FILETREE;
    }
    else if(path_state == file){
        struct file_browse_select_state select_state;
        load_file(state,NULL,0
            ,item->table_ptr->absolute_path,
            &select_state);
        
        if(select_state.select_state == file){
            load_screen_size(state);
            editor_set_cursor(state,0,0);
            state->render_flags |= RENDER_FILE_DATA;
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
            state->render_flags |= RENDER_FILETREE;
            if(state->settings_data->built_in_syntax){
                set_syntax_data(&ctx->syntax_data,ctx);
            }
        }
    }
    return path_state;
}
