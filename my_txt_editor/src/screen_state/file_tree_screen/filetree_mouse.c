#include <ncurses.h>
#include <stdlib.h>
#include <math.h>
#include <wctype.h>

#include "editor.h"
#include "editor_color.h"
#include "editor_log.h"
#include "filetree.h"
#include "ftj.h"
#include "editor_screen.h"

static bool file_tree_is_on_outline(struct editor_state *state,MEVENT *ev);
static bool is_within_double_click(struct timespec st);
static double get_time_elapsed_MS(struct timespec time);
static int file_tree_outline_first_click_process(struct editor_state *state);
static bool editor_click_move_cursor(struct editor_state *state, const MEVENT *ev);
static void ft_mouse_click_event(syntax *syntax,struct editor_state *state,MEVENT mev);

// ctxのマウスイベントでツリーの開閉・ファイル読込・境界のドラッグ開始・編集画面への移行を処理する。
// 返り値: 常に0。イベントはgetmouseで取得済みとし、必要な再描画と構文情報更新を要求する。
int ft_mouse_event(struct editor_input_context *ctx){
    MEVENT *ev = &ctx->mouse_event;
    struct editor_state *state = ctx->state;
    
    //ファイルツリーの枠線をダブルクリックした場合にリサイズ処理に移る
    if(state->file_tree_data.is_show == true){
        file_tree_data *tmp_ft_data = &state->file_tree_data;
        //枠線のダブルクリック判定
        //ダブルクリックかつボタンを離しているか
        if((ev->bstate & (BUTTON1_PRESSED|BUTTON1_DOUBLE_CLICKED)) &&
                                            tmp_ft_data->is_grabed == false){

            ft_outline_click_data *tmp_outline_click_data =
                        &state->file_tree_data.outline_click_data;
            //枠線判定
            if(file_tree_is_on_outline(state,ev) == true){
                tmp_outline_click_data->outline_click_count++;   
                //クリックが1っ回目かの判定
                if(tmp_outline_click_data->outline_click_count == 1){
                    file_tree_outline_first_click_process(state);
                }
                //二回目のクリックで決めたダブルクリックの
                // 間隔内で2回クリックされているかの判定をし、
                //間隔内であればリサイズシグナルをtrueにする
                else if(tmp_outline_click_data->outline_click_count == 2){
                    struct timespec st_time = tmp_outline_click_data->first_click_time;
                    tmp_ft_data->is_grabed = is_within_double_click(st_time);
                    tmp_outline_click_data->outline_click_count = 0;                     
                }  
            }
            //ファイルツリーの枠線外をクリックしていれば
            // 枠線クリックカウントを0に初期化する
            else if(tmp_outline_click_data->outline_click_count > 0){
                tmp_outline_click_data->outline_click_count = 0;
            }
        }
        if((ev->bstate & BUTTON1_RELEASED) && tmp_ft_data->is_grabed == true){
            tmp_ft_data->is_grabed = false;
            tmp_ft_data->outline_click_data.outline_click_count = 0;
        }
    }

    //ファイルツリーリサイズ処理
    if(state->file_tree_data.is_grabed && 
        ev->bstate & REPORT_MOUSE_POSITION){

        // 溜まったマウスイベントを読み捨て、最新の座標をevへ取得する
        wint_t chr;
        int result;
        nodelay(stdscr,true);
        while((result = get_wch(&chr)) != ERR){
            if(result != KEY_CODE_YES || chr != KEY_MOUSE){
                unget_wch(chr);
                break;
            }
            if(getmouse(ev) != OK || 
                !(ev->bstate & REPORT_MOUSE_POSITION)){
                break;
            }
        }
        nodelay(stdscr,false);

        const int screen_width = getmaxx(ctx->win);
        int new_ft_width = ev->x;

        //ファイルツリー横幅制限
        if(ev->x < FILETREE_DEFAULT_SIZE_W_){
            new_ft_width = FILETREE_DEFAULT_SIZE_W_;
        }
        else if(screen_width < ev->x){
            new_ft_width = screen_width;
        }

        change_file_tree_width(ctx,new_ft_width);
    }

    //左クリック
    if(ev->bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED)){
        struct box ft_box = ctx->state->file_tree_data.ft_box;
        //ファイルツリーボックスは枠線も含むサイズのため、枠線のクリックで
        //ファイルなどが開かないよう横幅を−1する
        ft_box.w--;
        // ファイルツリー内クリック時の処理
        if(box_contains_point(ft_box,(struct pos){ev->x,ev->y})){
           ft_mouse_click_event(&ctx->syntax_data, state,*ev); 

        }
        else{
            editor_click_move_cursor(state,ev);
        }
    }
    //ファイルツリーの枠線をダブルクリックするとファイルツリーサイズを変更する
    if(ev->bstate & BUTTON1_DOUBLE_CLICKED ){
        //枠線クリック判定
        if(state->file_tree_data.ft_box.pos.x + 
                state->file_tree_data.ft_box.w == ev->x){
            state->file_tree_data.is_grabed = true;
        }
    }
    //スクロール判定
    else if(ev->bstate & 
        (BUTTON5_PRESSED | BUTTON5_CLICKED | 
            BUTTON4_CLICKED| BUTTON4_PRESSED)){

        struct box tmp_write_area_box = 
            comb_write_possible_area_to_box(state->write_area);

        //編集画面内でのスクロール
        if(box_contains_point(tmp_write_area_box,(struct pos){ev->x,ev->y})){
            editor_set_screen_state(state,edit_screen_mode);
            my_cur_set(state,true);
        }
        //ファイルツリー内でのスクロール
        else if(box_contains_point(
            state->file_tree_data.ft_box,
            (struct pos){ev->x,ev->y})){

        

            file_tree_data *tmp_file_tree = &state->file_tree_data;

            nodelay(stdscr,true);

            MEVENT msev = *ev;
            wint_t wint;

            do{
                //手前へのスクロールイベント
                if(msev.bstate & (BUTTON5_CLICKED | BUTTON5_PRESSED)){
                    
                    //表示するアイテム数がファイルツリー内に収まらず、かつ末尾まで表示しきっていない場合にのみスクロールする
                    if(tmp_file_tree->item_view_count_num > tmp_file_tree->ft_item_box.h && 
                        tmp_file_tree->item_view_count_num > 
                            tmp_file_tree->scroll_y + tmp_file_tree->ft_item_box.h){

                        tmp_file_tree->scroll_y++;
                    }
                }
                else if(msev.bstate & (BUTTON4_CLICKED | BUTTON4_PRESSED)){
                    //先頭まで戻っていない場合にのみスクロールする
                    if(tmp_file_tree->scroll_y > 0){
                                
                        tmp_file_tree->scroll_y--;
                    }
                }
                
                //マウスのスクロールキューの解消
                get_wch(&wint);
                if(wint == KEY_MOUSE){
                    if(getmouse(&msev) == OK){
                        if(msev.bstate &
                            (BUTTON5_PRESSED | 
                             BUTTON5_CLICKED | 
                             BUTTON4_CLICKED | 
                             BUTTON4_PRESSED)){
                            
                            continue;       
                        }
                        else{
                            ungetmouse(&msev);
                            break;
                        }
                    }
                }    
                else{
                    unget_wch(wint);
                    break;
                }           
            }
            while(1);

            nodelay(stdscr,false);
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
            state->render_flags |= RENDER_FILETREE;
        }
    }
    return 0;
}


// 1回目のクリック時刻stからの経過時間がダブルクリック判定間隔未満かを返す。
// 返り値: 間隔未満ならtrue。
static bool is_within_double_click(struct timespec st){
    //st_timeから関数実行時までの時間差取得
    return (trunc(get_time_elapsed_MS(st)) < MOUSE_DOUBLE_CLICK_INTERVAL_);
}

// 編集領域内のクリックなら、その位置へ論理カーソルを移して編集画面に切り替える。
// 返り値: 移動したらtrue、編集領域外ならfalse。行・列の範囲への丸めはeditor_set_cursorが行う。
static bool editor_click_move_cursor(struct editor_state *state, const MEVENT *ev){
    struct pos mouse_pos = {ev->x,ev->y};
    //editor_mouse_to_buffer_posは領域外でエラーログを出すため先に範囲判定する
    struct box write_area_box = comb_write_possible_area_to_box(state->write_area);
    if(!box_contains_point(write_area_box,mouse_pos)){
        return false;
    }

    struct pos rel_pos = editor_mouse_to_buffer_pos(state,mouse_pos);

    //論理座標 (x=列、y=行の順で返る)
    struct pos logical_pos = editor_pos_to_buffer_pos(state,rel_pos);
    editor_set_cursor(state,logical_pos.y,logical_pos.x);
    editor_set_screen_state(state,edit_screen_mode);
    my_cur_set(state,true);
    return true;
}

//マウス座標がファイルツリーの枠線の上にあるかの判定
static bool file_tree_is_on_outline(struct editor_state *state,MEVENT *ev){
    int out_line_pos_x = state->file_tree_data.ft_box.pos.x + state->file_tree_data.ft_box.w;
    return (out_line_pos_x == ev->x || out_line_pos_x - 1 == ev->x);
}

// ファイルツリー外枠の1回目のクリックを処理する。
// ダブルクリックリサイズが無効なら即リサイズ開始、有効なら2回目判定用に時刻を記録する。常に0を返す。
static int file_tree_outline_first_click_process(struct editor_state *state){
    //ダブルクリックリサイズが無効だった場合シングルクリックでリサイズを開始する
    if(FILETREE_ENABLE_OUTLINE_DOUBLE_CLICK_RESIZE_ == false){
        state->file_tree_data.is_grabed = true;
    }
    else{
        struct timespec *tmp_click_time = 
            &state->file_tree_data.outline_click_data.first_click_time;
        //ダブルクリック判定一回目のクリックの時間取得
        // (2回目のクリックとの時間差でダブルクリックか判定する)
        clock_gettime(CLOCK_MONOTONIC,tmp_click_time);
    } 
    return 0;
}

// 時刻timeから現在までの経過時間をミリ秒で返す。
// 返り値: 経過ミリ秒(CLOCK_MONOTONIC基準)。
static double get_time_elapsed_MS(struct timespec time){
    struct timespec last_click_time;
    clock_gettime(CLOCK_MONOTONIC,&last_click_time);
    //ダブルクリックのミリ秒の差取得
    return comb_time_interval_ms(time,last_click_time);
}



// ファイルツリー内のマウスクリックを処理する。クリック行の項目を探して選択・操作する。
// 返り値: なし。
static void ft_mouse_click_event(syntax *syntax,struct editor_state *state,MEVENT mev){


    
    const int item_num = state->file_tree_data.item_view_count_num;
    ft_item_view_data *tmp_item_data = state->file_tree_data.item_view_data;
    //クリックした位置にアイテムがあるか調べる
    int j = -1;
    for(int i = 0;i < item_num;i++){
        if(tmp_item_data[i].screen_y == mev.y){
            j = i;
            break;
        }
    }

    if(j < 0){
        return;
    }

    ft_item_view_data *const item = &tmp_item_data[j];
    
    ft_item_open_event(syntax, state,item);
    // 選択行は項目領域内の0始まりの行で持つ(項目はft_item_boxの先頭から1行下に並ぶ)。
    state->file_tree_data.ft_item_select_line =
        item->screen_y - (state->file_tree_data.ft_item_box.pos.y + 1);
    return;
}
