#ifndef LANGUAGE_SERVER_COMMUNICATION_H_
#define LANGUAGE_SERVER_COMMUNICATION_H_

#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/epoll.h>
#include <sys/types.h>

#define MSG_BUFF_SIZE_MAX_ 2048 // LSP送受信ログ用の固定バッファ長。
#define MAX_ID_STRAGE_SIZE_ 512 // 事前生成して保持するJSON-RPC IDの個数。
#define initialize_id_num_ 1 // initialize要求へ割り当てるJSON-RPC ID。
#define NONE_ -1 // 有効なIDやメソッドが無い状態を表す値。

struct editor_state;

// この実装が生成できるLSP要求の種類。
typedef enum{
    lsp_method_completion, // textDocument/completion要求。
    lsp_method_none, // 生成対象のメソッドが無い状態。
}lsp_method;

// LSPの0始まりUTF-16位置。現在の送信元ではカーソル位置から設定する。
typedef struct{
    int line; // 文書先頭を0とする行番号。
    int character; // 行頭を0とする文字位置。
}lsp_pos;

// 1件のJSON-RPC要求を生成するための借用データ。
typedef struct{
    int id; // 要求と応答を対応付けるJSON-RPC ID。
    lsp_method lsp_method; // 生成するLSPメソッド。
    char *uri; // 対象文書のfile URI。文字列を所有しない。
    lsp_pos pos; // 要求を行う文書内位置。
}lsp_send_msg_data;

// 送信に使用するJSON-RPC ID列と、次に使う位置。
struct lsp_send_receve_id_data{
    int used_id_history[MAX_ID_STRAGE_SIZE_]; // initialize_id()が連番で初期化するID配列。
    int id_storage_counter; // 次に送信へ使用する配列添字。
};

// didChange通知に使う文書パスと版番号。
struct txt_update_data{
    // 更新対象ファイルのパス。now_open_path_nameと同じ相対パスまたは絶対パス。
    char path_name[PATH_MAX];
    int file_update_counter; // 文書変更ごとに増やすLSPのversion値。
};

// 起動中の言語サーバーの可変長配列。各要素のlsp_processはこの配列が所有する。
// lsp_proc_numが使用数、lsp_proc_allocate_numが確保数。
typedef struct {
    struct lsp_process **lsp_proc_list;
    size_t lsp_proc_num;
    size_t lsp_proc_allocate_num;
}lsp_server_data;

// 言語サーバープロセスと双方向パイプの所有状態。
struct lsp_process {
    pid_t pid; // 起動した言語サーバーのプロセスID。未起動時は負値。
    int to_server_fd; // 言語サーバーの標準入力へ書き込むファイルディスクリプタ。
    int from_server_fd; // 言語サーバーの標準出力から読むファイルディスクリプタ。
    bool initialized; // initialize応答を正常に受信済みならtrue。
    int epfd; // from_server_fdを監視するepollインスタンスのfd。未作成・クローズ済みは-1。
    struct epoll_event ev; // from_server_fdをepollへ登録する際のイベント情報。
    struct txt_update_data update_data; // didChange用の文書状態。
    struct lsp_send_receve_id_data id_data; // 送信要求へ割り当てるID列。
    char from_server_msg_buff[MSG_BUFF_SIZE_MAX_]; // 受信内容のログ・確認用NUL終端バッファ。
    char to_server_msg_buff[MSG_BUFF_SIZE_MAX_]; // 送信内容のログ・確認用NUL終端バッファ。
    char lsp_language_id[32]; // didOpenで送る言語IDのNUL終端文字列。
    char *lsp_root_uri; // initializeへ渡したルートURI(ヒープ所有)。未設定はNULL。
};



void lsp_process_init(struct lsp_process *lsp);
int lsp_start_server(struct lsp_process *lsp, const char *command, char *const argv[]);
void lsp_close_server(struct lsp_process *lsp);
int lsp_path_to_file_uri(char *uri, size_t uri_size, const char *path);
int lsp_send(int fd, const char *json);
int lsp_send_initialize(int fd, int id, pid_t process_id, const char *root_uri);
char *lsp_read_message(int fd);
void lsp_handle_message(struct lsp_process *lsp, char *msg);
void initialize_id(struct lsp_send_receve_id_data *id_data);
void set_lsp_use_language(struct lsp_process *lsp,char *language);
int lsp_send_did_open(int fd, const char *uri,
                      const char *language_id, const char *text);
int lsp_send_did_change(int fd, const char *uri,
                        int version, const char *text);

int lsp_make_msg(lsp_send_msg_data msg_data, char **msg);
int lsp_send_completion(int lsp_fd,struct editor_state *state);

#endif
