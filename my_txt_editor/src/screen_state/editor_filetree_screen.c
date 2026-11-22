
#include <ncurses.h>
#include<stdlib.h>


#include "editor.h"
#include "editor_log.h"
#include "filetree.h"
#include "ftj.h"
#include "editor_screen.h"
#include <math.h>
#include <wctype.h>

static bool file_tree_is_on_outline(struct editor_state *state,MEVENT *ev);
static bool is_within_double_click(struct timespec st);
static double get_time_elapsed_MS(struct timespec time);
static int file_tree_outline_first_click_process(struct editor_state *state);
static bool editor_click_move_cursor(struct editor_state *state, const MEVENT *ev);

static struct pos ft_get_item_name_pos(struct box ft_box,int file_h,int layer);



int ft_init_file_tree(file_tree_data *file_tree){
    if(file_tree == NULL){
        return -1;
    }

    file_tree->ft_box = (struct box){(struct pos){0,0},0,0};
    file_tree->ft_search_box = (struct box){(struct pos){0,0},0,0};
    file_tree->root_node = NULL;
    file_tree->is_show = false;
    file_tree->outline_click_data.outline_click_count = 0;
    file_tree->outline_click_data.first_click_time.tv_nsec = 0;
    file_tree->outline_click_data.first_click_time.tv_sec = 0;
    
    return 0;
}


// pathを起点とするツリーと直下項目の表示状態を、未確保のfile_treeへ作成する。
// 返り値: 成功0、作成・確保失敗-1。成功後は呼び出し側でroot_nodeをclose_treeし、item_view_dataをfreeする。
int get_root_file_tree_data(file_tree_data *file_tree,char *path){
    file_tree->root_node = create_tree(path);
    if(file_tree->root_node == NULL){
        error_log("can not create file tree");
        return -1;
    }
    int mem_num = file_tree->root_node->c_table_num;
    file_tree->item_view_data = NULL;
    file_tree->item_view_count_num = 0;
    file_tree->item_view_data_allocate_num = 0;
    file_tree->is_grabed = false;
    file_tree->drawing_data_num = 0;
    file_tree->draw_file_member_start_num = 0;
    file_tree->scroll_y = 0;
    file_tree->ft_item_select_line = 0;
    if(mem_num == 0)return 0;

    file_tree->item_view_data = malloc(sizeof(ft_item_view_data) * mem_num);
    if(file_tree->item_view_data == NULL){
        close_tree(file_tree->root_node);
        file_tree->root_node = NULL;
        return -1;
    }
    file_tree->item_view_count_num = mem_num;
    file_tree->item_view_data_allocate_num = mem_num;

    for(int i = 0;i < mem_num;i++){
        file_tree->item_view_data[i].table_ptr = &file_tree->root_node->c_table[i];
        file_tree->item_view_data[i].is_open = false;
        file_tree->item_view_data[i].layer = 0;
        file_tree->item_view_data[i].screen_y = -1;
        file_tree->item_view_data[i].screen_x = -1;
    }

    return 0;
}

// file_treeからtable_ptrの表示状態を探し、なければ配列を拡張して閉じた状態で追加する。
// 返り値: 内部要素の借用ポインタ、確保失敗NULL。後の追加で再確保されると以前の返値は無効になり得る。
ft_item_view_data *get_filetree_item_data(file_tree_data *file_tree,
                                                struct table *table_ptr){

    for(int i = 0;i < file_tree->item_view_count_num;i++){
        if(file_tree->item_view_data[i].table_ptr == table_ptr){
            return &file_tree->item_view_data[i];
        }
    }

    int new_count = file_tree->item_view_count_num + 1;
    ft_item_view_data *new_data = file_tree->item_view_data;

    // 確保数が足りないときだけ2倍に拡張する(確保数0からは1にする)。
    if(file_tree->item_view_data_allocate_num < new_count){

        int new_allocate_num = (file_tree->item_view_data_allocate_num > 0)
            ?file_tree->item_view_data_allocate_num * 2 : 1;

        new_data 
            = realloc(
            file_tree->item_view_data,
            sizeof(ft_item_view_data) * new_allocate_num
            );
            
        if(new_data == NULL)return NULL;
        file_tree->item_view_data_allocate_num = new_allocate_num;
    }

    file_tree->item_view_data = new_data;
    file_tree->item_view_count_num = new_count;
    new_data[new_count - 1].table_ptr = table_ptr;
    new_data[new_count - 1].is_open = false;
    new_data[new_count - 1].layer = 0;
    new_data[new_count - 1].screen_y = -1;
    new_data[new_count - 1].screen_x = -1;
    return &new_data[new_count - 1];
}

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
    state->render_flags |= RENDER_FILE_DATA;
}

// ファイルツリーを閉じ、編集領域を元の位置へ戻す。
// 引数: ctx=画面サイズ・編集領域・ファイルツリーを持つ入力context。 返り値: なし。
void hide_filetree(struct editor_input_context *ctx){
    struct editor_state *state = ctx->state;

    state->file_tree_data.is_show = false;
    state->file_tree_data.ft_box = (struct box){(struct pos){0,0},0,0};
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
    if(state->cursor.is_cur_show)my_cur_set(state,false);

    if(ch == L'j'){
        if(tmp_file_tree->item_view_count_num > 
            tmp_file_tree->ft_item_select_line){

            tmp_file_tree->ft_item_select_line++;
            state->render_flags |= RENDER_EDIT_SCREEN_BASE;
        }
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

int ft_set_file_tree_item_select_line(file_tree_data *file_tree_data,int line_num){
    if(file_tree_data == NULL || line_num < 0){
        return -1;
    }

    



}


// filetree_dataの枠を画面上の矩形filetree_boxへ値コピーする。
// 返り値: 常に0。範囲検査・描画・編集領域の更新は行わない。
int set_filetree_box(file_tree_data *filetree_data,struct box filetree_box){
    filetree_data->ft_box = filetree_box;
    return 0;
}



// tree_tableの子の名前を、filetree_box左上にlayer列・pos_y行を加えた位置へ順に描く。
// 返り値: 常に0。現実装は各項目の座標を進めず、同じ場所へ重ねて描く。
int set_tree_item(struct box filetree_box,int layer,int pos_y,struct table *tree_table){
    int x = filetree_box.pos.x + layer;
    int y = filetree_box.pos.y + pos_y;

    for(int i = 0; i < tree_table->c_table_num;i++){
        mvaddstr(y,x,tree_table->c_table[i].s_data.d_name);
    }
    return 0;
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

// filetree_dataのtable_num件のtableに表示座標と階層を再帰的に割り当てる(描画はしない)。layerは深さ、*file_hは次の相対行。
// 返り値: なし。*file_h・各項目の画面座標・drawing_data_numを更新し、枠の下端・項目情報の確保失敗で打ち切る。
static void ft_update_filetree_data(struct editor_input_context *ctx,
                                file_tree_data *filetree_data,
                                struct table *table,int table_num,
                                int layer,int *file_h){
                                    
    struct box box = filetree_data->ft_box;

    for(int i = 0;i < table_num;i++){
        // スクロール分ずらした枠内の相対行
        int view_h = *file_h - filetree_data->scroll_y;
        //ファイルツリー外描画ガード
        if(view_h >= box.h - 1)return;
        //現在のレイヤーのi番目のテーブルのデータを取得する
        ft_item_view_data *item_data =
            get_filetree_item_data(filetree_data,&table[i]);
        if(item_data == NULL)return;

        item_data->layer = layer;
        // 上へスクロールアウトした項目はscreen_y=-1のまま残す
        if(view_h >= 1){
            // i番目の要素の名前を描く画面座標
            struct pos item_name_pos = ft_get_item_name_pos(box,view_h,layer);
            //取得した名前の座標をitemに代入する
            item_data->screen_y = item_name_pos.y;
            item_data->screen_x = item_name_pos.x;
        }
        

        //表示されているデータの個数カウント
        filetree_data->drawing_data_num = (*file_h)++;

        //iの要素がディレクトリかつ、is_openで展開判定になっていたら
        //i番目のテーブルにそのディレクトリ内の要素のデータをつける
        if(table[i].s_data.d_type == DT_DIR && item_data->is_open){
            if(load_child_dir(&table[i]) == 0){
                ft_update_filetree_data(ctx,filetree_data,table[i].c_table,
                                    table[i].c_table_num,layer + 1,file_h);
            }
        }
    }
}



// filetree_dataの枠内を消して境界線を描き、項目の画面座標を振り直してからft_draw_filetree_dataで項目を描く。
// 返り値: 常に0。枠が小さい、またはルート未設定なら描画しない。
int draw_filetree(struct editor_input_context *ctx,file_tree_data *filetree_data){
    struct box box = filetree_data->ft_box;

    if(box.w <= 2 || box.h <= 2 || filetree_data->root_node == NULL){
        return 0;
    }

    //前に表示されていたツリーをクリアする
    clear_box_area(ctx,box);


    //開発用
    filetree_data->ft_side = LEFT;

    //ファイルツリーの位置が左右だった場合位置におおじて枠線を設定する
    // (ファイルツリーが左側にあった場合右に線を設定する)
    if(filetree_data->ft_side == LEFT || filetree_data->ft_side == RIGHT){
        int wall_pos_x = (filetree_data->ft_side == LEFT)
            ?(box.pos.x + box.w):box.pos.x;  

        for(int i = box.pos.y;i < box.pos.y + box.h;i++){
            mvaddch(i,wall_pos_x,ACS_VLINE);
        }
    }
    
    // 画面行は描画のたびに振り直す。-1のまま残った項目は画面外で、クリック判定に当たらない。
    for(int i = 0;i < filetree_data->item_view_count_num;i++){
        filetree_data->item_view_data[i].screen_y = -1;
        filetree_data->item_view_data[i].screen_x = -1;
    }

    int file_h = 1;
    ft_update_filetree_data(
        ctx,filetree_data,
        filetree_data->root_node->c_table,
        filetree_data->root_node->c_table_num,
        0,
        &file_h
    );

    ft_draw_filetree_data(ctx,filetree_data);
    
    return 0;
}

// ft_boxの枠内で、相対行file_h・階層layerの項目名を描く画面座標を求める(階層ごとに2列字下げ)。
// 返り値: 画面上の座標。範囲検査はしない。
static struct pos ft_get_item_name_pos(struct box ft_box,int file_h,int layer){
    struct pos tmp_pos;
    tmp_pos.x = ft_box.pos.x + 1 + (layer * 2);
    tmp_pos.y = ft_box.pos.y + file_h;
    return tmp_pos;
}



// 項目1件を、記録済みのscreen_y/screen_xへアイコン(ディレクトリは開閉、ファイルは拡張子別)と名前の順に描く。
// 引数: ctx=アイコン設定を持つcontext、ft_box=名前を切り詰める基準の枠、view_data=描画する項目(screen_y/xは設定済み)。 返り値: なし。名前の描画幅が残っていなければ名前は描かない。
static void ft_draw_item_data(struct editor_input_context *ctx,struct box ft_box,
                         ft_item_view_data *view_data){

    const char *item_name = view_data->table_ptr->s_data.d_name;
    const char *icon;

    //ディレクトリは開閉状態、それ以外は拡張子に応じたアイコンを使う
    if(view_data->table_ptr->s_data.d_type == DT_DIR){
        icon = view_data->is_open ? FILE_TREE_ICON_OPEN : FILE_TREE_ICON_CLOSE;
    }
    else{
        icon = get_file_ext_code(item_name,&ctx->state->settings_data->icon_data);
    }

    //アイコンの使用セル数
    const int item_icon_len = str_display_width(icon);
    mvaddstr(view_data->screen_y,view_data->screen_x,icon);

    //アイテム名の描画文字数制限。0以下はmvaddnstrで「全文字」の意味になるため描かない。
    int name_len_max = (ft_box.w - 1) - (view_data->screen_x + 1 + item_icon_len);
    if(name_len_max <= 0)return;

    mvaddnstr(view_data->screen_y,view_data->screen_x + item_icon_len + 1,
              item_name,name_len_max);
}

// file_tree_dataの先頭から枠の高さ-1件までの項目を、記録済みscreen_y/screen_xへアイコン付きで描く。
// 返り値: 常に0。座標は事前にft_update_filetree_dataで設定しておく。
int ft_draw_filetree_data(struct editor_input_context *ctx,file_tree_data *file_tree_data){
       
    struct box ft_box = file_tree_data->ft_box;
    ft_item_view_data **sorted_item_view_data;
    //y座標の昇順にソートした配列を取得する
    int sorted_num = ft_get_sorted_screen_y_item_view_data(
            file_tree_data->item_view_data,
            file_tree_data->item_view_count_num,
            &sorted_item_view_data
        );

    if(sorted_num <= 0){
        return -1;
    }

    int i = 0;
    while(i < sorted_num){
        ft_item_view_data *tmp_view_data = sorted_item_view_data[i];

        if(tmp_view_data->screen_y < 1){
            i++;
            continue;
        }
        
        // screen_y=-1は画面外の項目。表示行はft_update_filetree_dataで枠内に制限済み。
        if(tmp_view_data->screen_y < 0){
            i++;
            continue;
        }

        ft_draw_item_data(ctx,ft_box,tmp_view_data);
      
        i++;
    }

    free(sorted_item_view_data);
    
    return 0;
}


// file_tree_dataのdraw_file_member_start_num以降からlayer階層の項目を集め、ポインタ配列を*item_view_dataへ確保する。
// 返り値: 成功で件数(0件なら*item_view_data=NULL)、引数不正・確保失敗-1。配列は呼び出し側でfreeし、要素は借用。
int ft_get_same_layer_item_view_data(file_tree_data *file_tree_data,int layer,
                                    ft_item_view_data ***item_view_data,int *item_view_num){

    if(layer < 0 || file_tree_data == NULL || 
        item_view_data == NULL || item_view_num == NULL){
        return -1;
    }


    //仮配列
    ft_item_view_data *tmp_item_view_data_list[file_tree_data->item_view_count_num];
    int tmp_item_view_data_count = 0;

    for(int i = file_tree_data->draw_file_member_start_num;i < file_tree_data->item_view_count_num;i++){
        ft_item_view_data *tmp_item_view_data = &file_tree_data->item_view_data[i];

        if(tmp_item_view_data->layer == layer){
            tmp_item_view_data_list[tmp_item_view_data_count] = tmp_item_view_data;
            tmp_item_view_data_count++;
        }
    }
    
    if(tmp_item_view_data_count <= 0){
        *item_view_num = 0;
        *item_view_data = NULL;
        return 0;
    }

    ft_item_view_data **layer_item_view_data = 
        malloc(sizeof(ft_item_view_data *) * tmp_item_view_data_count);

    if(layer_item_view_data == NULL){
        *item_view_num = 0;
        *item_view_data = NULL;
        error_log("malloc error");
        return -1;
    }

    
    for(int i = 0;i < tmp_item_view_data_count;i++){
        layer_item_view_data[i] = tmp_item_view_data_list[i];
    }

    *item_view_data = layer_item_view_data;
    *item_view_num = tmp_item_view_data_count;

    return tmp_item_view_data_count;
}


// qsort用: ft_item_view_data *の配列要素a,bをscreen_yで比較する。
// 返り値: aが小さければ負、等しければ0、大きければ正。
static int ft_cmp_screen_y(const void *a,const void *b){
    const ft_item_view_data *item_a = *(ft_item_view_data * const *)a;
    const ft_item_view_data *item_b = *(ft_item_view_data * const *)b;
    return (item_a->screen_y > item_b->screen_y) - (item_a->screen_y < item_b->screen_y);
}

// 構造体配列item_view_dataのitem_view_num件をscreen_y昇順に並べたポインタ配列を*sorted_dataへ確保して返す。
// 返り値: 成功で件数、引数不正・確保失敗-1。*sorted_dataは呼び出し側でfreeする(要素自体は借用)。
int ft_get_sorted_screen_y_item_view_data(
    ft_item_view_data *item_view_data,
    int item_view_num,
    ft_item_view_data ***sorted_data){

    if(item_view_data == NULL || item_view_num < 0 || sorted_data == NULL){
        return -1;
    }
    *sorted_data = NULL;
    if(item_view_num == 0)return 0;

    ft_item_view_data **tmp_sorted_data = 
        malloc(sizeof(ft_item_view_data *) * item_view_num);
        
    if(tmp_sorted_data == NULL){
        error_log("malloc error");
        return -1;
    }
    for(int i = 0;i < item_view_num;i++){
        tmp_sorted_data[i] = &item_view_data[i];
    }
    qsort(tmp_sorted_data,item_view_num,sizeof(ft_item_view_data *),ft_cmp_screen_y);

    *sorted_data = tmp_sorted_data;
    
    return item_view_num;
}


int ft_get_max_layer_item_view_data(ft_item_view_data *item_view_data,int item_view_num){
    if(item_view_data == NULL){
        return -1;
    }

    if(item_view_num <= 0){
        return 0;
    }

    int tmp_layer = 0;
    for(int i = 0;i < item_view_num;i++){
        if(item_view_data[i].layer > tmp_layer){
            tmp_layer = item_view_data[i].layer;
            continue;
        }        
    }
    return tmp_layer;
}

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
            for(int i = 0;i < state->file_tree_data.item_view_count_num;i++){
                ft_item_view_data *item =
                    &state->file_tree_data.item_view_data[i];
                if(item->screen_y != ev->y){
                    continue;
                }

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
                    if(tmp_file_tree->item_view_count_num > tmp_file_tree->ft_box.h && 
                        tmp_file_tree->item_view_count_num > 
                            tmp_file_tree->scroll_y + tmp_file_tree->ft_box.h){

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
        }
    }
    return 0;
}

static bool is_within_double_click(struct timespec st){
    //st_timeから関数実行時までの時間差取得
    return (trunc(get_time_elapsed_MS(st)) < MOUSE_DOUBLE_CLICK_INTERVAL);
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

static int file_tree_outline_first_click_process(struct editor_state *state){
    //ダブルクリックリサイズが無効だった場合シングルクリックでリサイズを開始する
    if(FILETREE_ENABLE_OUTLINE_DOUBLE_CLICK_RESIZE == false){
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

static double get_time_elapsed_MS(struct timespec time){
    struct timespec last_click_time;
    clock_gettime(CLOCK_MONOTONIC,&last_click_time);
    //ダブルクリックのミリ秒の差取得
    return comb_time_interval_ms(time,last_click_time);
}
