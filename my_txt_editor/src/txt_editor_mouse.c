#include "txt_editor_mouse.h"
#include <bits/time.h>
#include <math.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>
#include "default_settings.h"
#include "editor_log.h"
#include "editor_types.h"
#include "filetree.h"
#include "ftj.h"
#include "txt_editor.h"
#include "txt_editor_syntax.h"

// getmouseで取得したイベントをctxの現在画面へ振り分ける。dir_numはブラウザの表示項目数。
// 返り値: なし。イベント取得失敗や未対応画面では何もしない。
void handle_mouse(struct editor_input_context *ctx,int dir_num) {
    WINDOW *win = ctx->win;
    MEVENT *event = &ctx->mouse_event;
    struct editor_state *state = ctx->state;

    

    if (getmouse(event) != OK){
        return;
    }
    screen_state scr_state = editor_get_screen_state(state);

    switch(scr_state){
        case edit_screen:{
            int old_scr_start_num = state->scr.scr_start_num;
            editor_screen_mouse_event(ctx);
            if(old_scr_start_num != state->scr.scr_start_num){
                state->render_flags |= RENDER_EDIT_SCREEN_BASE;
                state->render_flags |= RENDER_FILE_DATA;
            }
            break;
        }
        case file_browse_screen:{
            file_browse_screen_mouse_event(win,event,state,dir_num);
            break;
        }
        case filetree_screen:{
            filetree_mouse_event(ctx);
            break;
        }

        default:
            break;
    }
}

// ctxの取得済みマウスイベントでスクロール・本文クリック・ツリーへの移行を処理する。
// 返り値: なし。ホイールは表示開始行だけ、本文クリックは論理カーソルを動かし、最後にカーソル表示可否を更新する。
void editor_screen_mouse_event(struct editor_input_context *ctx){
    MEVENT *event = &ctx->mouse_event;
    struct editor_state *state = ctx->state;

    int line_limit = editor_line_limit(state);
    bool can_scroll_down = state->scr.scr_start_num + state->write_area.h < line_limit;

    //エディター画面下スクロール処理
    if (event->bstate & BUTTON5_PRESSED && can_scroll_down){

        
        bool is_cur_on_edit_screen = true;
        //ファイルツリーが表示されている場合
        // スクロール時のマウスポインターがファイルツリー上にあるか判断する
        if(state->file_tree_data.is_show){
            struct box tmp_write_area_box = 
                {(struct pos){state->write_area.x_start,state->write_area.y_start},
                    state->write_area.w,state->write_area.h};    
            if(!box_contains_point(tmp_write_area_box,(struct pos){event->x,event->y})){
                editor_set_screen_state(state,filetree_screen);
                is_cur_on_edit_screen = false;
            }
        }
        if(is_cur_on_edit_screen){
            
            wint_t chr;
            MEVENT tmp_msev;

            nodelay(stdscr,true);
            //エディタースクロールスタック解消ループ処理
            do{
                int before_scr_start_num = state->scr.scr_start_num;
                //文字のスクロール処理 
                editor_scroll_logical_line(
                    state,state->settings_data->editor_scroll_sensitivity);
                int scrolled_num = state->scr.scr_start_num - before_scr_start_num;
                // 構文情報は画面相対行で持つため、実際にスクロールした行数だけずらし、
                // 新しく見える最下行からscrolled_num行分を解析する。
                if(state->settings_data->built_in_syntax && scrolled_num > 0){
                    scroll_syntax_pos_data(
                        &ctx->syntax_data,-scrolled_num,state->write_area.h);
                    int first_line = state->write_area.h - scrolled_num;
                    if(first_line < 0)first_line = 0;
                    for(int line = first_line;line < state->write_area.h;line++){
                        update_line_syntax_data(ctx,line);
                    }
                }

                if(get_wch(&chr) == ERR){
                    break;
                }


                if(chr == KEY_MOUSE){
                    if(getmouse(&tmp_msev) == OK && 
                        !(tmp_msev.bstate & (BUTTON5_PRESSED | BUTTON5_CLICKED))){
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
        
    //エディター画面上スクロール処理
    }else if ((event->bstate & BUTTON4_PRESSED) && state->scr.scr_start_num > 0) {
        bool is_cur_on_edit_screen = true;
         if(state->file_tree_data.is_show){

            struct box tmp_write_area_box = 
                {(struct pos){state->write_area.x_start,state->write_area.y_start},
                    state->write_area.w,state->write_area.h};    

            if(!box_contains_point(tmp_write_area_box,(struct pos){event->x,event->y})){
                editor_set_screen_state(state,filetree_screen);
                is_cur_on_edit_screen = false;
            }
        }

        if(is_cur_on_edit_screen){ 
            wint_t chr;
            
            nodelay(stdscr,true);
            MEVENT tmp_msev;
            //スクロール処理はスタックがたまるため一時的にノンブロッキングにして
            //溜まっているスクロールスタックをループで処理する
            do{
                int before_scr_start_num = state->scr.scr_start_num;
                editor_scroll_logical_line(
                    state,-state->settings_data->editor_scroll_sensitivity);
                int scrolled_num = before_scr_start_num - state->scr.scr_start_num;
                // 下スクロールの逆。実際にスクロールした行数だけ既存分を下げ、
                // 新しく見える最上行からscrolled_num行分を解析する。
                if(state->settings_data->built_in_syntax && scrolled_num > 0){
                    scroll_syntax_pos_data(
                        &ctx->syntax_data,scrolled_num,state->write_area.h);
                    int last_line = scrolled_num;
                    if(last_line > state->write_area.h)last_line = state->write_area.h;
                    for(int line = 0;line < last_line;line++){
                        update_line_syntax_data(ctx,line);
                    }
                }

                if(get_wch(&chr) == ERR){
                    break;
                }
                
                if(chr == KEY_MOUSE){
                    if(getmouse(&tmp_msev) == OK && 
                        !(tmp_msev.bstate & (BUTTON4_PRESSED | BUTTON4_CLICKED))){
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
    }
    //マウスクリック処理
    else if(event->bstate &
            (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)){
        int x = event->x;
        int y = event->y;

      
        if(box_contains_point(ctx->state->file_tree_data.ft_box,(struct pos){event->x,event->y})){
            editor_set_screen_state(state,filetree_screen);
            filetree_mouse_event(ctx);
            return;
        }
        else{
            struct box tmp_write_area_box = 
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
        }
    }
    else{
        return;
    }

    // カーソル行が編集領域から出たら隠す。戻ってきたらまた出す。
    my_cur_set(state,editor_cursor_is_visible(state));
}

// ホイール入力でファイルブラウザの選択行を循環移動する。
// 引数: win=描画先。現在は未使用、event=マウスイベント、state=選択状態、dir_num=表示項目数。 返り値: なし。選択行の強調表示が無効なら何もしない。
void file_browse_screen_mouse_event(WINDOW *win, MEVENT *event, struct editor_state *state,int dir_num){
    //ホイールで選択行を動かすだけなので描画先ウィンドウは使わない
    (void)win;

    if(event->bstate & BUTTON1_DOUBLE_CLICKED){
        if(box_contains_point(state->file_browse.box,(struct pos){event->x,event->y})){

            
        

        }
    }

    if(state->settings_data->file_select_scene_lighting){
        if(event->bstate & BUTTON4_PRESSED){
             // next_lineはハイライトを移す先。端では上下に循環させる。
            int next_line = (state->file_browse.select_line.now_line <= 0) 
                ? dir_num - 1:state->file_browse.select_line.now_line - 1;
            set_file_select_line(state, dir_num, next_line);
            
        }  
        if(event->bstate & BUTTON5_PRESSED){
            // next_lineはハイライトを移す先。端では上下に循環させる。
            int next_line = (state->file_browse.select_line.now_line  >= dir_num - 1)
                ?0:state->file_browse.select_line.now_line + 1;
            set_file_select_line(state, dir_num, next_line);
        }      
    }
}

// ctxのマウスイベントでツリーの開閉・ファイル読込・境界のドラッグ開始・編集画面への移行を処理する。
// 返り値: 常に0。イベントはgetmouseで取得済みとし、必要な再描画と構文情報更新を要求する。
int filetree_mouse_event(struct editor_input_context *ctx){
    MEVENT *ev = &ctx->mouse_event;
    struct editor_state *state = ctx->state;

    //ファイルツリーの枠線をダブルクリックした場合にリサイズ処理に移る
    if(state->file_tree_data.is_show == true){
        //枠線のダブルクリック判定
        //ダブルクリックかつボタンを離してい
        if((ev->bstate & BUTTON1_PRESSED) && state->file_tree_data.is_grabed == false){
            //枠線判定
            if(state->file_tree_data.ft_box.pos.x + 
                state->file_tree_data.ft_box.w == ev->x){

                state->file_tree_data.
                    outline_click_data.
                        outline_click_count++;   
                //クリックが1っ回目かの判定
                if(state->file_tree_data.
                        outline_click_data.outline_click_count == 1){
                    //ダブルクリックリサイズが無効だった場合シングルクリックでリサイズを開始する
                    if(FILETREE_ENABLE_OUTLINE_DOUBLE_CLICK_RESIZE == false){
                        state->file_tree_data.is_grabed = true;
                    }
                    else{
                        struct timespec *tmp_click_time = 
                            &state->file_tree_data.outline_click_data.
                                                            first_click_time;
                        //ダブルクリック判定一回目のクリックの時間取得
                        // (2回目のクリックとの時間差でダブルクリックか判定する)
                        timespec_get(tmp_click_time,CLOCK_MONOTONIC);
                    }                    
                }
                else if(state->file_tree_data.outline_click_data.
                                            outline_click_count == 2){
                    
                    struct timespec *first_click_time = 
                        &state->file_tree_data.outline_click_data.first_click_time;
                        
                    struct timespec last_click_time;
                    timespec_get(&last_click_time,CLOCK_MONOTONIC);
        
                    double click_interval = 
                        comb_time_interval_ms(*first_click_time,last_click_time);
                    //小数点以下切り捨て処理
                    click_interval = trunc(click_interval);
                    
                    //クリック感覚が既定のダブルクリック間隔以内かの判定
                    //間隔がダブルクリックの間隔以上なら初期化する
                    if(click_interval < MOUSE_DOUBLE_CLICK_INTERVAL){
                        state->file_tree_data.is_grabed = true;
                        state->file_tree_data.outline_click_data.outline_click_count = 0;
                    }
                    else{
                        state->file_tree_data.outline_click_data.outline_click_count = 0;
                    }                        
                }  
            }
            else if(state->file_tree_data.outline_click_data.outline_click_count > 0){
                state->file_tree_data.outline_click_data.outline_click_count = 0;
            }
        }

        if((ev->bstate & BUTTON1_RELEASED) && 
            state->file_tree_data.is_grabed == true){
            state->file_tree_data.is_grabed = false;
        }
    }

    //ファイルツリーリサイズ処理
    if(state->file_tree_data.is_grabed && ev->bstate & REPORT_MOUSE_POSITION){
        // 溜まったマウスイベントを読み捨て、最新の座標をevへ取得する
        wint_t chr;
        int result;
        nodelay(stdscr,true);
        while((result = get_wch(&chr)) != ERR){
            if(result != KEY_CODE_YES || chr != KEY_MOUSE){
                unget_wch(chr);
                break;
            }
            if(getmouse(ev) != OK || !(ev->bstate & REPORT_MOUSE_POSITION)){
                break;
            }
        }
        nodelay(stdscr,false);

        int screen_width = getmaxx(ctx->win);
        int new_ft_width = ev->x;

        //ファイルツリー横幅制限
        if(ev->x < FILETREE_DEFAULT_SIZE_W){
            new_ft_width = FILETREE_DEFAULT_SIZE_W;
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
            for(int i = 0;i < state->file_tree_data.open_count_num;i++){
                ft_path_open_check_data *item =
                    &state->file_tree_data.open_check_data[i];
                if(item->screen_y != ev->y)continue;

                enum select_state path_state =
                    get_path_state(item->table_ptr->absolute_path);
                if(path_state == folder){
                    item->is_open = !item->is_open;
                    state->render_flags |= RENDER_EDIT_SCREEN_BASE;
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
                        if(state->settings_data->built_in_syntax){
                            set_syntax_data(&ctx->syntax_data,ctx);
                        }
                    }
                }
                break;
            }
        }
        else{
            struct box tmp_write_area_box = 
                {(struct pos){state->write_area.x_start,state->write_area.y_start},
                    state->write_area.w,state->write_area.h};

            if(box_contains_point(tmp_write_area_box,(struct pos){ev->x,ev->y})){
                struct pos write_area_cur_pos = 
                    editor_mouse_to_buffer_pos(state,(struct pos){ev->x,ev->y});
                //カーソル論理座標
                struct pos cur_logical_pos = 
                    editor_pos_to_buffer_pos(state,write_area_cur_pos);
                //表示されている行数内に収める
                if(*state->str.line <= cur_logical_pos.y){
                    cur_logical_pos.y = *state->str.line;
                }
                editor_set_cursor(state,cur_logical_pos.x,cur_logical_pos.y);
                editor_set_screen_state(state,edit_screen);
                my_cur_set(state,true);
            }
        }
    }
    //ファイルツリーの枠線を右クリックするとファイルツリーサイズを変更する
    if(ev->bstate & BUTTON1_DOUBLE_CLICKED ){
        //枠線クリック判定
        if(state->file_tree_data.ft_box.pos.x + state->file_tree_data.ft_box.w == ev->x){
            state->file_tree_data.is_grabed = true;
        }
    }
    //スクロール判定
    else if(ev->bstate == BUTTON5_PRESSED || ev->bstate == BUTTON4_PRESSED){
        struct box tmp_write_area_box = 
            {(struct pos){state->write_area.x_start,state->write_area.y_start},
                state->write_area.w,state->write_area.h};
        if(box_contains_point(tmp_write_area_box,(struct pos){ev->x,ev->y})){
            editor_set_screen_state(state,edit_screen);
            my_cur_set(state,true);
        }
    }
    return 0;
}

// 指定座標がボックスの範囲内か判定する。
// 引数: b=画面座標と幅・高さを持つボックス、p=判定する画面座標。 返り値: pが左上から右下の境界を含む範囲内ならtrue、それ以外はfalse。
bool box_contains_point(struct box b,struct pos p){
    // 右端・下端も含める(<=)。枠線を含む箱でクリック判定をするための仕様。
    if(b.pos.x <= p.x && b.pos.x + b.w >= p.x && 
        b.pos.y <= p.y && b.pos.y + b.h >= p.y)return true;
    return false;
}

// マウスの画面座標を編集領域の左上基準の座標へ変換する。
// 引数: state=編集領域の画面上の位置と大きさ、mouse_pos=変換する画面座標。 返り値: 編集領域内なら領域左上を(0,0)とする座標、範囲外なら(-1,-1)。 スクロール開始行は加算しない。
struct pos editor_mouse_to_buffer_pos(struct editor_state *state,
                                    struct pos mouse_pos){
    struct box write_area = 
        (struct box){(struct pos){state->write_area.x_start,
            state->write_area.y_start},
            state->write_area.w,
            state->write_area.h};
    struct pos tmp_pos = (struct pos){-1,-1};  
    if(screen_pos_to_box_pos(write_area,mouse_pos,&tmp_pos)){
        error_log("error");
        return tmp_pos;
    }
    return tmp_pos;  
}

// 画面座標p1をb1左上からの相対座標へ変換して非NULLのrp1へ格納する。右下境界も範囲に含む。
// 返り値: 成功false、範囲外true。失敗時は*rp1を変更しない。
bool screen_pos_to_box_pos(struct box b1,struct pos p1,struct pos *rp1){
    if(!box_contains_point(b1,p1))return 1;
    *rp1 = (struct pos){p1.x - b1.pos.x,p1.y - b1.pos.y};
    return 0;
}

// stateの表示開始行をeditor_pos.yへ加え、編集領域の相対位置から論理位置を返す。
// 返り値: xが論理行、yが列のpos。通常の座標と順序が逆で、範囲検査は行わない。
struct pos editor_pos_to_buffer_pos(
                struct editor_state *state,
                struct pos editor_pos){
    return(struct pos){state->scr.scr_start_num + editor_pos.y,editor_pos.x};
}
