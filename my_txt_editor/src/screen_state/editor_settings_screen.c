#include <ncurses.h>
#include<stdlib.h>
#include<string.h>
#include<wctype.h>
#include "settings_screen.h"
#include "editor.h"
#include "editor_color.h"
#include "editor_screen.h"
#include"editor_log.h"



#define SETTINGS_EXPLATAION_BOX_MIN_W 12 // 設定説明欄へ確保する最小幅（端末セル数）。

static void draw_explanation_str(const char *str,struct box box);
settings_items_data *get_now_select_settings_item(settings_screen_data *screen_data);

// ctxの設定画面でchとget_wch結果input_resultを処理し、選択移動・値入力・Tabでの復帰を行う。
// 返り値: 入力モード外のqはfalse、それ以外true。入力値の確定は入力モードを閉じるだけで、設定本体へは反映しない。
bool handle_settings_screen_input(struct editor_input_context *ctx,wint_t ch,int input_result){
    struct editor_state *state = ctx->state;
    settings_screen_data *screen_data = &state->settings_screen_data;

    if(screen_data->value_input_mode){

        if(ch == KEY_ENTER || ch == '\n' || ch == '\r'){
            screen_data->value_input_mode = false;
            my_cur_set(state,false);
        }
        else if(ch == KEY_BACKSPACE){
            if(screen_data->input_value_len > 0){
                screen_data->input_value[--screen_data->input_value_len] = L'\0';
            }
        }
        else if(input_result == OK && iswprint(ch) &&
            screen_data->input_value_len < SETTINGS_INPUT_MAX - 1){

            screen_data->input_value[screen_data->input_value_len++] = ch;
            screen_data->input_value[screen_data->input_value_len] = L'\0';
        }
        state->render_flags |= RENDER_SETTINGS;
        return true;
    }
    else if(input_result == OK && iswprint(ch)){
        for(int i = 0;i < screen_data->settings_item_data_num;i++){
            if(screen_data->item_data[i].key_code == ch){
                screen_data->value_input_mode = true;
                screen_data->input_value_len = 0;
                screen_data->input_value[0] = L'\0';
                my_cur_set(state,true);
                state->render_flags |= RENDER_SETTINGS;
                int select_item_delta = i - screen_data->select_line;
                move_settings_select_line(screen_data,select_item_delta);
            }
        }
    }

    if(ch == 'q')return false;
    if(input_result == ERR)editor_error_screen(ctx->state,"key input error");
    if(ch == '\t'){
        enum screen_state old_state = editor_get_screen_state_log(state,1);
        if(old_state == screen_state_log_error_mode)editor_error_screen(state,"editor screen error");
        else editor_set_screen_state(state,old_state);
    }
    if(ch == KEY_UP || ch == 'k'){
        move_settings_select_line(screen_data,-1);
        state->render_flags |= RENDER_SETTINGS;
    }
    if(ch == KEY_DOWN || ch == 'j'){
        move_settings_select_line(screen_data,1);
        state->render_flags |= RENDER_SETTINGS;
    }
    if(ch == KEY_ENTER || ch == '\n' || ch == '\r'){
        screen_data->value_input_mode = true;
        screen_data->input_value_len = 0;
        screen_data->input_value[0] = L'\0';
        my_cur_set(state,true);
        state->render_flags |= RENDER_SETTINGS;
    }
    return true;
}

// 設定画面の選択行をdelta分だけ動かす。項目の範囲外へは出さず、端ではそのまま止める。
// 引数: settings_screen_data=選択行と項目数を持つ設定画面データ、delta=移動量。 返り値: なし。
void move_settings_select_line(settings_screen_data *settings_screen_data,int delta){
    if(settings_screen_data == NULL)return;

    int line_limit = settings_screen_data->settings_item_data_num;
    if(line_limit <= 0)return;

    int next_line = settings_screen_data->select_line + delta;
    if(next_line < 0)next_line = 0;
    if(next_line >= line_limit)next_line = line_limit - 1;
    settings_screen_data->select_line = next_line;
}


// 設定項目配列を必要に応じて拡張し、末尾へ1項目追加する。
// 引数: settings_screen_data=追加先、item_data=値コピーする項目。 返り値: 成功時0、malloc()またはrealloc()失敗時-1。 所有権: item_data内の文字列ポインタは複製せず、成功後はsettings_screen_dataが保持する。
int add_settings_screen_item(settings_screen_data *settings_screen_data,settings_items_data item_data){
    if(settings_screen_data->item_data == NULL){
        settings_screen_data->settings_item_data_allocate_num = 16; 
        settings_screen_data->item_data = malloc(sizeof(settings_items_data) *
            settings_screen_data->settings_item_data_allocate_num);
        if(settings_screen_data->item_data == NULL){
            error_log("item data arry allocate error");
            return -1;
        }
    }
    if(settings_screen_data->settings_item_data_num >= settings_screen_data->settings_item_data_allocate_num){
        settings_items_data *tmp_item_data = realloc(settings_screen_data->item_data
            ,sizeof(settings_items_data) * (settings_screen_data->settings_item_data_num * 2));
        if(tmp_item_data == NULL){
            error_log("settings_item_data realloc error");
            return -1;
        }
        settings_screen_data->item_data = tmp_item_data;
        settings_screen_data->settings_item_data_allocate_num *= 2;
    }
    settings_screen_data->item_data[settings_screen_data->settings_item_data_num] = item_data;
    settings_screen_data->settings_item_data_num++;
    return 0;
}

// 現在選択中の設定項目を返す。
// 引数: screen_data=項目配列と選択位置を持つ設定画面データ。 返り値: 選択項目への借用ポインタ。引数や選択位置が不正な場合の動作は未定義。
settings_items_data *get_now_select_settings_item(settings_screen_data *screen_data){
    return &screen_data->item_data[screen_data->select_line];
}

// NUL終端の項目名nameを最大255ワイド文字へ変換し、その部分の表示セル数を返す。
// 返り値: 表示幅、NULL・変換失敗・幅を求められない場合は-1。文字変換は現在のロケールに従う。
static int settings_item_name_width(const char *name){
    if(name == NULL)return -1;

    wchar_t wide_name[256];
    size_t converted = mbstowcs(wide_name,name,sizeof(wide_name)/sizeof(wide_name[0]) - 1);
    if(converted == (size_t)-1)return -1;
    wide_name[converted] = L'\0';

    int width = wcswidth(wide_name,converted);
    return (width < 0) ? -1 : width;
}

// 設定画面のbox上辺中央へタイトルを重ねる。draw_boxの後に呼ぶ。
// 返り値: なし。タイトルと左右余白が枠幅に収まらなければ描画しない。
static void draw_settings_title(struct editor_input_context *ctx,struct box box){
    const char *title = " Settings ";
    int title_len = (int)strlen(title);
    if(box.w < title_len + 2)return;

    int title_x = box.pos.x + (box.w - title_len) / 2;
    attr_t accent_attr = COLOR_PAIR(get_color_pair(ctx,COLOR_CYAN,COLOR_BLACK)) | A_BOLD;
    attron(accent_attr);
    mvaddstr(box.pos.y,title_x,title);
    attroff(accent_attr);
}

// stateの項目名の表示幅・項目数・画面寸法から、中央寄せした設定画面の枠を決める。
// 返り値: なし。高さに収まらない項目は複数列に配置する幅を確保し、実際の描画は行わない。
void set_settings_screen_box(struct editor_state *state){
    if(state == NULL)return;
    settings_screen_data *st_scr_data = &state->settings_screen_data;

    //枠の上限。画面の端まで届かせず、上下左右に余白を残す。
    int limit_w = state->scr.scr_size.x - state->scr.scr_size.x / SETTINGS_SCREEN_MARGIN_DIV;
    int limit_h = state->scr.scr_size.y - state->scr.scr_size.y / SETTINGS_SCREEN_MARGIN_DIV;

    //一番長い項目名にキーと余白を足して、1列分の幅を決める。
    int name_width_max = 0;
    for(int i = 0;i < st_scr_data->settings_item_data_num;i++){
        int name_width = settings_item_name_width(st_scr_data->item_data[i].name);
        if(name_width > name_width_max)name_width_max = name_width;
    }
    int col_w = name_width_max + SETTINGS_ITEM_KEY_WIDTH + 5;
    if(col_w < SETTINGS_ITEM_COLUMN_MIN_WIDTH)col_w = SETTINGS_ITEM_COLUMN_MIN_WIDTH;
    if(col_w > SETTINGS_ITEM_COLUMN_MAX_WIDTH)col_w = SETTINGS_ITEM_COLUMN_MAX_WIDTH;

    //枠の内側に並べられる行数。上下の枠線の分を引く。
    int rows = limit_h - 2;
    if(rows < 1)rows = 1;

    //縦に収まらない分は列を増やして横へ送る。
    int col_count = (st_scr_data->settings_item_data_num + rows - 1) / rows;
    if(col_count < 1)col_count = 1;

    int box_w = col_w * col_count + 2;
    int box_h = st_scr_data->settings_item_data_num + 2;
    //項目が少なくても小さくなりすぎないよう、最小サイズまで広げる。
    if(box_w < SETTINGS_SCREEN_MIN_W)box_w = SETTINGS_SCREEN_MIN_W;
    if(box_h < SETTINGS_SCREEN_MIN_H)box_h = SETTINGS_SCREEN_MIN_H;
    if(box_w > limit_w)box_w = limit_w;
    if(box_h > limit_h)box_h = limit_h;

    //残った余白は左右と上下へ均等に割って中央へ寄せる。
    st_scr_data->box.pos = (struct pos){
        (state->scr.scr_size.x - box_w) / 2,
        (state->scr.scr_size.y - box_h) / 2
    };
    st_scr_data->box.w = box_w;
    st_scr_data->box.h = box_h;
}

// ctxの設定項目を枠内に複数列で描き、項目名とキーを左右へ揃えて選択項目を強調する。
// 返り値: なし。ctxには設定画面の枠・項目配列・描画先が必要で、枠とタイトルも描画する。
void draw_settings_screen(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    settings_screen_data st_scr_data = state->settings_screen_data;

    draw_box(st_scr_data.box,ctx->win);
    draw_settings_title(ctx,st_scr_data.box);

    int item_num = st_scr_data.settings_item_data_num;
    int inner_w = st_scr_data.box.w - 2;
    //内側に並べられるのは上下の枠線を除いた分だけ。
    int inner_h = st_scr_data.box.h - 2;
    if(item_num <= 0 || inner_w <= 0 || inner_h <= 0)return;

    //1列に入りきらない分を次の列へ回す。端数は最終列に入る。
    int col_count = (item_num + inner_h - 1) / inner_h;
    //狭い枠で列を増やしすぎると1列が潰れるため、最小幅で頭打ちにする。
    int col_count_max = inner_w / SETTINGS_ITEM_COLUMN_MIN_WIDTH;
    if(col_count > col_count_max)col_count = col_count_max;
    if(col_count < 1)col_count = 1;

    //余りは切り捨てる。はみ出した分は使わないだけで、枠線へは被らない。
    int col_w = inner_w / col_count;
    //枠が広いときはキーが右端まで離れてしまうため、列幅に上限を設ける。
    if(col_w > SETTINGS_ITEM_COLUMN_MAX_WIDTH)col_w = SETTINGS_ITEM_COLUMN_MAX_WIDTH;
    //使わなくなった幅は左右へ均等に割って、項目の並びを中央へ寄せる。
    int items_pos_x = st_scr_data.box.pos.x + 1 + (inner_w - col_w * col_count) / 2;

    //列の外側に前回の描画が残らないよう、内側を一度消してから並べる。
    for(int y = 0; y < inner_h;y++){
        mvhline(st_scr_data.box.pos.y + y + 1,st_scr_data.box.pos.x + 1,' ',inner_w);
    }

    for(int c = 0; c < col_count;c++){
        //列の左端。マーカーはこの位置、名前はその2つ右に置く。
        int col_pos_x = items_pos_x + c * col_w;
        int mark_pos_x = col_pos_x;
        int item_pos_x = col_pos_x + 2;
        //キーは列の右端から桁数と余白1つ分だけ内側へ寄せる。
        int key_pos_x = col_pos_x + col_w - 1 - SETTINGS_ITEM_KEY_WIDTH - 1;

        for(int i = 0; i < inner_h;i++){
            int item_index = c * inner_h + i;
            if(item_index >= item_num)break;

            settings_items_data item = st_scr_data.item_data[item_index];
            int scr_pos_y = st_scr_data.box.pos.y + i + 1;
            bool is_select = (item_index == st_scr_data.select_line);

            if(is_select){
                mvaddnwstr(scr_pos_y,mark_pos_x,L"\u276F",1);
            }

            attron(A_BOLD);
            my_mvaddstr((struct pos){item_pos_x,scr_pos_y},(char *)item.name);
            attroff(A_BOLD);

            //名前の後ろからキーの手前まで空白で埋める。
            int name_width = settings_item_name_width(item.name);
            if(name_width < 0)name_width = 0;
            for(int blank = item_pos_x + name_width;blank < key_pos_x;blank++){
                mvaddch(scr_pos_y,blank,' ');
            }

            wchar_t key_code[SETTINGS_ITEM_KEY_WIDTH + 1] =
                {L'[',(wchar_t)item.key_code,L']',L'\0'};
            attr_t accent_attr = COLOR_PAIR(get_color_pair(ctx,COLOR_CYAN,COLOR_BLACK)) | A_BOLD;
            attron(accent_attr);
            mvaddnwstr(scr_pos_y,key_pos_x,key_code,SETTINGS_ITEM_KEY_WIDTH);
            attroff(accent_attr);

            //反転は最後に重ねる。名前とキーの色より選択表示を優先する。
            if(is_select){
                mvchgat(scr_pos_y,col_pos_x,col_w,A_NORMAL,get_color_pair(ctx,COLOR_BLACK,COLOR_WHITE),NULL);
            }
        }
    }
}

// 選択項目の型名と入力中の値を設定画面上部へ描画する。
// 引数: ctx=設定画面状態と描画先を持つ入力context。 返り値: 成功時0、入力枠を配置できない場合は-1。
int draw_settings_search_box(struct editor_input_context *ctx){

    struct editor_state *state = ctx->state;
    settings_screen_data st_scr_data = state->settings_screen_data;

    if(st_scr_data.box.pos.y <= 0)return -1;

    struct pos search_box_pos = (struct pos){st_scr_data.box.pos.x,st_scr_data.box.pos.y - 3};
    struct box search_box = {search_box_pos,st_scr_data.box.w,3};
    draw_box(search_box,ctx->win);
    int input_w = search_box.w - 2;
    if(input_w <= 0)return -1;

    mvhline(search_box.pos.y + 1,search_box.pos.x + 1,' ',input_w);

    settinge_value_type value_type = st_scr_data.item_data[st_scr_data.select_line].value_type;
    wchar_t value_type_str[32];
    switch(value_type){
        case VALUE_TYPE_BOOL:
            wcscpy(value_type_str,L"BOOL:");
            break;
        case VALUE_TYPE_INT:
            wcscpy(value_type_str,L"INT:");
            break;
        case VALUE_TYPE_STR:
            wcscpy(value_type_str,L"STR:");
            break;
        case VALUE_TYPE_UNKNOWN:
            wcscpy(value_type_str,L"UNKNOWN:");
            break;
    }

    int input_start = st_scr_data.input_value_len - input_w;
    if(input_start < 0)input_start = 0;
    int value_type_str_len = wcslen(value_type_str);
    mvaddwstr(search_box.pos.y + 1,search_box.pos.x + 1,value_type_str);
    if(st_scr_data.input_value_len > input_start){
        mvaddnwstr(search_box.pos.y + 1,search_box.pos.x + 1 + value_type_str_len,
                   &st_scr_data.input_value[input_start],input_w);
    }
    return 0;
}

// 選択中設定項目の説明枠を設定画面の右側へ描画する。
// 引数: ctx=設定項目・画面寸法・描画先を持つ入力context。 返り値: なし。必要な幅または説明文が無い場合は描画しない。
void draw_settings_explanation_box(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;
    settings_screen_data st_scr_data = state->settings_screen_data;

    int set_mouse_pos_x;
    int set_mouse_pos_y;
    getyx(ctx->win,set_mouse_pos_y,set_mouse_pos_x);

    bool is_cur_showed = false;
    if(state->cursor.is_cur_show){
        my_cur_set(state,false);
        is_cur_showed = true;
    }
    int x;
    int y;
    getmaxyx(ctx->win,y,x);

    int explanation_box_pos_x = st_scr_data.box.pos.x + st_scr_data.box.w;
    int explanation_box_max_w = x - explanation_box_pos_x;
    if(explanation_box_max_w < SETTINGS_EXPLATAION_BOX_MIN_W)return;
    

    struct pos explanation_pos = {explanation_box_pos_x,st_scr_data.box.pos.y};
    size_t explanation_str_len = strlen(st_scr_data.item_data[st_scr_data.select_line].explanation);
    if(explanation_str_len <= 0)return;
    //説明ウィンドウの座標から画面下までのセル数
    int explanation_box_max_h = (y - st_scr_data.box.pos.y);
    // 説明枠の最小幅で説明文を折り返した場合の必要行数。これを満たす最小の枠サイズを下で探す。
    //説明ウィンドウの横幅に説明文を入れきれるかの計算
    int explanation_str_h = (explanation_str_len/(SETTINGS_EXPLATAION_BOX_MIN_W - 2)) + 1;
    
    int area_w = 0;
    int area_h = 0;
    for(int w = SETTINGS_EXPLATAION_BOX_MIN_W;w < explanation_box_max_w;w++){
        for(int h = st_scr_data.box.h; h < explanation_box_max_h;h++){
            int explanation_box_area = w * h;
            if(explanation_box_area < explanation_str_h)continue;
            area_w = w;
            area_h = h;
            break;
        }
        if(area_h != 0)break;
    }

    struct box explanation_box = {explanation_pos,area_w,area_h};
    draw_box(explanation_box,ctx->win);
    draw_explanation_str(st_scr_data.item_data[st_scr_data.select_line].explanation,
        explanation_box);
    move(set_mouse_pos_y,set_mouse_pos_x);
    if(is_cur_showed)my_cur_set(state,true);
}

// 説明文をboxの内側幅で分割し、上から順に描画する。
// 引数: str=描画するNUL終端文字列、box=枠線を含む描画領域。 返り値: なし。
// 空文字列なら何も描画しない。strがNULLまたはbox.wが2以下の場合の動作は未定義。
static void draw_explanation_str(const char *str,struct box box){
    int str_len = strlen(str);
    if(str_len <= 0)return;

    // 枠線の左右1セルずつを除いた内側幅(box.w-2)で割り、切り上げた行数が必要行数になる。
    int loop_h = (str_len + box.w - 3) / (box.w - 2);
    int offset = 0;
    for(int h = 0;h < loop_h;h++){
        struct pos explanation_str_pos = {box.pos.x + 1,box.pos.y + h+1};
        int split_len = (str_len > box.w)?box.w - 2:str_len;
        char splited_explanation_str[split_len + 1];
        memcpy(splited_explanation_str,&str[offset],sizeof(char) * split_len);
        splited_explanation_str[split_len] = '\0';
        my_mvaddstr(explanation_str_pos,splited_explanation_str);
        offset += split_len;
    }
    return;
}
