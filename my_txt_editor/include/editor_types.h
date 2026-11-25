#ifndef EDITOR_TYPES_H
#define EDITOR_TYPES_H

#ifndef NUL
    #define NUL '\0'
#endif

#ifndef WNUL
    #define WNUL L'\0'
#endif

// 解析対象の言語。現在はコメント開始文字列の切り替えに使用する。
typedef enum{
    C, // C。
    CPP, // C++。
    PY, // Python。
    TS, // TypeScript。
    UNKNOWN, // 未対応言語。構文着色を適用しない。
}language;


// 上下左右の方向・辺を表す列挙。
// 配置や位置指定(ステータスバーの表示側など)に使う。
typedef enum{
    TOP,
    BOTTOM,
    LEFT,
    RIGHT,
}side;


// 呼び出し側が定める座標系上の2次元位置。
struct pos {
    int x; // 横方向の位置。基準は使う構造体のコメントに従う(画面座標なら左端が0)。
    int y; // 縦方向の位置。基準は使う構造体のコメントに従う(画面座標なら上端が0)。
};

// 左上位置と、そこから右・下へ広がる矩形の大きさ。
struct box {
    struct pos pos; // 矩形の左上座標。
    int w; // 横幅。枠付きUIでは枠線を含む端末セル数。
    int h; // 高さ。枠付きUIでは枠線を含む端末行数。
};


// 値を保存する操作と取得する操作の指定。
enum flags{
    set, // 値を保存する。
    get, // 保存済みの値を取得する。
};


// 現在表示している画面・入力モード。
enum screen_state{
    edit_screen_mode,        // 通常の編集画面。
    file_browse_screen_mode, // ファイルブラウザ画面。
    filetree_screen_mode,    // ファイルツリー画面。
    start_menu_screen_mode,  // start menu pluginの画面。
    error_screen_mode,       // エラー表示画面。
    line_jump_mode,     // 行ジャンプ番号入力中。
    ask_make_file_mode, // 新規ファイル作成確認中。
    setting_screen_mode,     // 設定画面。
    editor_command_mode,           //コマンド入力モード
    screen_state_log_error_mode, // 指定された履歴位置が範囲外。
};

typedef enum screen_state screen_state;

// ファイルブラウザで判定した項目種別。
enum path_type{
    PATH_TYPE_FILE,  // 通常ファイル。
    PATH_TYPE_DIR,// ディレクトリ。
    PATH_TYPE_CHR, // キャラクタデバイス(S_ISCHR)。
    PATH_TYPE_BLK, // ブロックデバイス(S_ISBLK)。
    PATH_TYPE_FIFO,// 名前付きパイプ(S_ISFIFO)。
    PATH_TYPE_LNK, // シンボリックリンク(S_ISLNK)。statはリンク先を見るため、判定にはlstatが必要。
    PATH_TYPE_SOCK,// ソケット(S_ISSOCK)。
    PATH_TYPE_UNKOWN,// 種別を判定できない項目。
    PATH_TYPE_ERROR, // stat等の判定処理に失敗した状態。
};


#endif
