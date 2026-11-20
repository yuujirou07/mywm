#ifndef FILETREE_H
#define FILETREE_H

#include <stdbool.h>
#include <time.h>
#include"editor_types.h"

#define FILETREE_DEFAULT_SIZE_W 10 // ファイルツリーの既定幅（端末セル数）。
#define FILETREE_MIN_W 5

//ファイルツリーのダブルクリックでのリサイズ設定falseならシングルっクリックでリサイズイベントが発生する
#define FILETREE_ENABLE_OUTLINE_DOUBLE_CLICK_RESIZE false

#define FILE_TREE_ICON_CLOSE ">"
#define FILE_TREE_ICON_OPEN "\u2304"


struct ft_path_click_data;
struct editor_input_context;

// ツリー内の1項目について、開閉状態・階層・直近の描画位置を保持する。
typedef struct{
    struct table *table_ptr; // root_node配下の項目への借用ポインタ。
    bool is_open; // ディレクトリの子要素を展開中ならtrue。
    int layer; // ルート直下を0とするツリー階層。
    int screen_y; // 直近の描画先となる画面y座標。非表示なら-1。
    int screen_x; // 直近の描画先となる画面x座標(項目先頭)。非表示なら-1。
}ft_item_view_data;

// 枠線クリックの回数と最初のクリック時刻。ダブルクリックでのリサイズ判定に使う。
typedef struct{
    int outline_click_count; // 判定中の連続クリック回数。
    struct timespec first_click_time; // 連続クリックの1回目の時刻。
}ft_outline_click_data;

// ファイルツリー画面の形状、ツリー本体、項目ごとの表示状態。
typedef struct{
    struct box ft_search_box; // 検索欄として予約する画面上の矩形。
    struct box ft_box; // ファイルツリー全体の外枠。
    struct root_node *root_node; // create_tree()で確保し、close_tree()で解放するツリーの根。
    ft_item_view_data *item_view_data; // この構造体が所有する項目状態の動的配列。
    
    int item_view_count_num; // item_view_dataに格納済みの有効要素数。
    int item_view_data_allocate_num; // item_view_dataの確保済み要素数。item_view_count_num以上。
    int drawing_data_num;//ファイルツリーに表示しているメンバ数
    int draw_file_member_start_num;//ファイルツリーに表示するメンバの開始数
    int scroll_y;//ファイルツリーのスクロール行数。0以上
    bool is_show; // ファイルツリーを表示中ならtrue。編集領域を右へ寄せる幅の計算に使う。
    bool is_grabed; // 枠線をマウスで掴み、幅を変更中ならtrue。
    ft_outline_click_data outline_click_data;//枠線のクリック判定 ダブルクリックなどマウスでのサイズ変更イベントに使う
    side ft_side; // ファイルツリーを接続する画面端。
}file_tree_data;


int get_root_file_tree_data(file_tree_data *file_tree,char *path);

ft_item_view_data *get_filetree_item_data(file_tree_data *file_tree,
                                                struct table *table_ptr);

int set_filetree_box(file_tree_data *filetree_data,struct box filetree_box);

int draw_filetree(struct editor_input_context *ctx,file_tree_data *filetree_data);
int change_file_tree_width(struct editor_input_context *ctx,int width);
void show_filetree(struct editor_input_context *ctx,struct box ft_box);
void hide_filetree(struct editor_input_context *ctx);


int ft_get_same_layer_item_view_data(file_tree_data *file_tree_data,int layer,
                                    ft_item_view_data ***item_view_data,int *item_view_num);
int ft_get_sorted_screen_y_item_view_data(ft_item_view_data *item_view_data,int item_view_num,
                                    ft_item_view_data ***sorted_data);

int ft_draw_filetree_data(struct editor_input_context *ctx,file_tree_data *file_tree_data);



int ft_get_max_layer_item_view_data(ft_item_view_data *item_view_data,int item_view_num);


#endif
