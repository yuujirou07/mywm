#ifndef EDITOR_INIT_H
#define EDITOR_INIT_H



#include"editor.h"

// 256色端末で使う色番号。256色未満の端末ではinit_curses_colorが近い基本色へ置き換える。
#define COLOR_LIGHT_GRAY 252 // 薄い白(明るい灰色)。

// 色ペア番号。init_curses_colorがncursesへ登録済みで、COLOR_PAIR(PAIR_xxx)やmvchgatの色引数へそのまま渡す。
// 0は端末の既定色で変更できないため使わない。
#define PAIR_TEXT            1  // 本文・通常表示(白on黒)。
#define PAIR_ERROR           2  // エラー表示(黒on赤)。
#define PAIR_SELECT          3  // 選択行の反転表示(黒on白)。
#define PAIR_ACCENT          4  // 設定画面などの強調(水色on黒)。
#define PAIR_FILETREE_SELECT 5  // ファイルツリーの選択行(黒on薄い白)。
#define PAIR_SYNTAX_BASE     16 // 構文色の先頭。PAIR_SYNTAX_BASE+syntax_typeが各分類のペア番号。


// 固定の色ペアを登録し、初期属性を本文色にする。
// 引数: state=NULL検査のみに使う。 返り値: stateがNULLなら-1、それ以外は0(カラー非対応・ペア数不足なら登録せず0)。
int init_curses_color(struct editor_state *state);

// (x,y)から右へnセルを色ペアpairへ変える。各セルの文字・文字属性は保つ。
// 引数: pair=PAIR_xxxの色ペア番号、x/y=開始セルの画面座標、n=変更するセル数。 返り値: 成功0、nが0以下または読み取り失敗なら-1。
int set_cell_color_pair(short pair,int x,int y,int n);

#endif