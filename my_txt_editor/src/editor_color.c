

#include<stdlib.h>

#include"editor_color.h"

// 固定の色ペア(PAIR_xxx)をncursesへ登録し、初期属性を本文色(白on黒)にする。
// 引数: state=NULL検査のみに使う。 返り値: stateがNULLなら-1、それ以外は0(カラー非対応・ペア数不足なら登録せず0)。
int init_curses_color(struct editor_state *state){

    if(state == NULL){
        return -1;
    }
    start_color();

    //カラー非対応か、固定ペア+構文色を置けるだけのペア数がなければカラーをつけない
    if (!has_colors() || COLOR_PAIRS <= PAIR_SYNTAX_BASE) {
        return 0;
    }

    // 薄い白は256色端末だけ。それ以外は基本色の白で代用する。
    const short light_gray = (COLORS >= 256) ? COLOR_LIGHT_GRAY : COLOR_WHITE;

    init_pair(PAIR_TEXT,COLOR_WHITE,COLOR_BLACK);
    init_pair(PAIR_ERROR,COLOR_BLACK,COLOR_RED);
    init_pair(PAIR_SELECT,COLOR_BLACK,COLOR_WHITE);
    init_pair(PAIR_ACCENT,COLOR_CYAN,COLOR_BLACK);
    init_pair(PAIR_FILETREE_SELECT,COLOR_BLACK,light_gray);

    attrset(COLOR_PAIR(PAIR_TEXT));
    return 0;
}

// (x,y)から右へnセルを色ペアpairへ変える。各セルの文字・文字属性は保つ。
// 引数: pair=PAIR_xxxの色ペア番号、x/y=開始セルの画面座標、n=変更するセル数。 返り値: 成功0、nが0以下または読み取り失敗なら-1。
int set_cell_color_pair(short pair,int x,int y,int n){
    if(n <= 0)return -1;
    // mvinchnstrは読み取った末尾に終端の0を書くため、n+1個の領域が要る。
    chtype cht[n + 1];
    if(mvinchnstr(y,x,cht,n) == ERR)return -1;

    for(int i = 0;i < n;i++){
        // 元の色ペアのビットは外し、文字属性だけ残す。
        attr_t attr = cht[i] & A_ATTRIBUTES & ~A_COLOR;
        mvchgat(y,x + i,1,attr,pair,NULL);
    }
    return 0;
}
