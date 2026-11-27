#include "editor_mouse.h"
#include "editor.h"
#include "editor_log.h"







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
        case edit_screen_mode:{
            int old_scr_start_num = state->scr.scr_start_num;
            editor_screen_mouse_event(ctx);
            if(old_scr_start_num != state->scr.scr_start_num){
                state->render_flags |= RENDER_EDIT_SCREEN_BASE;
                state->render_flags |= RENDER_FILETREE;
                state->render_flags |= RENDER_FILE_DATA;
            }
            break;
        }
        case file_browse_screen_mode:{
            file_browse_screen_mouse_event(win,event,state,dir_num);
            break;
        }
        case filetree_screen_mode:{
            ft_mouse_event(ctx);
            break;
        }

        default:
            break;
    }
}


// 指定座標がボックスの範囲内か判定する。
// 引数: b=画面座標と幅・高さを持つボックス、p=判定する画面座標。 返り値: pが左上から右下の境界を含む範囲内ならtrue、それ以外はfalse。
bool box_contains_point(struct box b,struct pos p){
    // 右端・下端も含める(<=)。枠線を含む箱でクリック判定をするための仕様。
    return(
        b.pos.x <= p.x && 
        b.pos.x + b.w >= p.x && 
        b.pos.y <= p.y && 
        b.pos.y + b.h >= p.y
    );
}

// マウスの画面座標を編集領域の左上基準の座標へ変換する。
// 引数: state=編集領域の画面上の位置と大きさ、mouse_pos=変換する画面座標。 返り値: 編集領域内なら領域左上を(0,0)とする座標、範囲外なら(-1,-1)。 スクロール開始行は加算しない。
struct pos editor_mouse_to_buffer_pos(struct editor_state *state,
                                    struct pos mouse_pos){

    struct write_possible_area write_area = state->write_area;
    struct box write_area_box = 
        create_box(
            write_area.x_start,
            write_area.y_start,
            write_area.w,
            write_area.h
        );                        

    struct pos tmp_pos = (struct pos){-1,-1};  
    if(screen_pos_to_box_pos(write_area_box,mouse_pos,&tmp_pos)){
        return tmp_pos;
    }
    return tmp_pos;  
}


// 画面座標p1をb1左上からの相対座標へ変換して非NULLのrp1へ格納する。右下境界も範囲に含む。
// 返り値: 成功false、範囲外true。失敗時は*rp1を変更しない。
bool screen_pos_to_box_pos(struct box b1,struct pos p1,struct pos *rp1){
    if(!box_contains_point(b1,p1)){return 1;}
    *rp1 = (struct pos){p1.x - b1.pos.x,p1.y - b1.pos.y};
    return 0;
}

// stateの表示開始行をeditor_pos.yへ加え、編集領域の相対位置から論理位置を返す。
// 返り値: xが列、yが論理行のpos。範囲検査は行わない。
struct pos editor_pos_to_buffer_pos(
                struct editor_state *state,
                struct pos editor_pos){
    return(struct pos){editor_pos.x,state->scr.scr_start_num + editor_pos.y};
}

// 画面状態に合わせてカーソルの形状と色を設定する。
// 編集画面では入力用カーソル(白)、それ以外は端末既定にする。常に0を返す。
int SetCursorModeScreen(struct editor_state *state,screen_state screen_state){
    switch(screen_state){
        case edit_screen_mode:
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


// カーソル形状を設定して端末へ出力し、stateへ保存する。
// 返り値: 常に0。
int SetCursorMode(struct editor_state *state,CURSOR_MODE CurMode){
    state->cursor.CurMode = CurMode;
    fputs(CURSOR_SHAPE[state->cursor.CurMode],stdout);
    fflush(stdout);
    return 0;
}

// カーソル色を設定して端末へ出力し、stateへ保存する。
// 返り値: 成功時0、colorが範囲外なら-1。
int SetCursorColor(struct editor_state *state,CURSOR_COLOR color){

    if((sizeof(CURSOR_COLOR_ESC)/sizeof(CURSOR_COLOR_ESC[0])) <= color){
        error_log_("cursor color error");
        return -1;
    }

    state->cursor.Color = color;
    fputs(CURSOR_COLOR_ESC[color],stdout);
    fflush(stdout);
    return 0;
}
