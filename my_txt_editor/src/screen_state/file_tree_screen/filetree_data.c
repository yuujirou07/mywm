#include <ncurses.h>
#include <stdlib.h>
#include <math.h>
#include <wctype.h>

#include "editor.h"
#include "editor_log.h"
#include "filetree.h"
#include "ftj.h"
#include "editor_screen.h"


int ft_init_file_tree(file_tree_data *file_tree){
    if(file_tree == NULL){
        return -1;
    }

    file_tree->ft_box = (struct box){(struct pos){0,0},0,0};
    file_tree->ft_search_box = (struct box){(struct pos){0,0},0,0};
    file_tree->ft_item_box = (struct box){(struct pos){0,0},0,0};
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
    file_tree->drawing_item_num = 0;
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


int ft_set_file_tree_item_select_line(file_tree_data *file_tree_data,int line_num){
    if(file_tree_data == NULL || line_num < 0){
        return -1;
    }

    if(file_tree_data->ft_box.h <= line_num || 
        file_tree_data->drawing_item_num <= line_num){
        return -1;
    }

    file_tree_data->ft_item_select_line = line_num;
    return 0;
}


// filetree_dataの枠を画面上の矩形filetree_boxへ値コピーし、項目領域ft_item_boxも枠から検索欄の高さを除いて求め直す。
// 返り値: 常に0。範囲検査・描画・編集領域の更新は行わない。検索欄の高さが枠以上なら項目領域の高さは0になる。
int set_filetree_box(file_tree_data *filetree_data,struct box filetree_box){
    int search_h = filetree_data->ft_search_box.h;
    if(search_h < 0)search_h = 0;
    if(search_h > filetree_box.h)search_h = filetree_box.h;

    filetree_data->ft_box = filetree_box;
    filetree_data->ft_item_box = filetree_box;
    filetree_data->ft_item_box.pos.y += search_h;
    filetree_data->ft_item_box.h -= search_h;
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

// item_dataの項目を開く。ディレクトリなら開閉を切り替え、ファイルなら読み込んで編集画面の表示を更新する。
// 引数: ctx=編集状態・構文情報を持つcontext、item_data=開く項目(table_ptrは有効であること)。 返り値: なし。必要な再描画フラグを立てる。
void ft_item_open_event(struct editor_input_context *ctx,ft_item_view_data *item_data){

    ft_item_view_data *item = item_data;
    struct editor_state *state = ctx->state;

    enum select_state path_state =
        get_path_state(item->table_ptr->absolute_path);
        
    if(path_state == folder){
        item->is_open = !item->is_open;
        state->render_flags |= RENDER_EDIT_SCREEN_BASE;
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
    return;
}
