#ifndef TCP_H
#define TCP_H

#define MAX_CLIENT       64  /* 接続の最大数 */

/* エラー番号に付加。負の数を用いる */
#define CONNECT_EXCEPT     -1 /* 例外処理待ちを検出 */
#define CONNECT_DETECT_EOF -2 /* EOF を検出 */
#define CONNECT_TIMEOUT    -3 /* 書き込み不可状態が WRITE_TIMEOUT 回を超えた */
#define CONNECT_BUFFULL    -4 /* 書き込みバッファがあふれた */

#include "phi_str.h"

void   tcp_initialize();
/* 先ず、これをやる */

void   get_ip_address(int n, char *buf, int buf_size);
/* 自分の IP アドレスを取得 */

int    read_line(int id, char *buf , int buf_size);
/* 接続先から情報を一行読み込む。*/
/* 終端の '\n' は省かれていない。*/

int    peek_protocol(int id, char *buf , int buf_size);
/* バッファから 先頭が#の行(phiプロトコル)を優先して読み出す*/
/* 終端の '\n' は省かれていない。*/

int    set_server(int port);

void   set_tcp_nodelay(int id);
/* 高速移動 (fastmove) を申告したセッションのソケットで Nagle
 * アルゴリズムを無効化する (PHIDM NEO extension)。 */
/* ポート番号 port を新規接続受け付け用ポートとする。*/
/* すでにそのポートが使われている等、しっぱいすると偽を返す。*/

void   close_server();
/* 新規接続受け付け用ポートを閉じる */

int    connect_server(char* ip_addr, int port);
/* IP アドレス ip_addr ポート番号 port へ接続要求を出し、*/
/* その接続の id を返す。接続に失敗すると、偽を返す。*/

int    nfclose(int id);
/* その接続用の id を解放し、(接続されているならば)接続を終了する。*/
/* 実際は書き込みバッファが空でない場合、切断は遅延される。 */

int    nclose(int id);
/* 接続用の id を解放し、(接続されているならば)接続を */
/* (書き込みバッファが空であるかどうかに関わらず)直ちに終了する。*/

void   write_line(int id, char *buf, int buf_size, int timeout);
/* (バッファを通して)接続先へ書き込む。*/
/* もし、バッファが一杯になった場合、*/
/* 最大 timeout (ms) の間バッファをフラッシュしてから書き込む。*/
/* それでもバッファの空きができなかった場合、接続を切断する。*/

int    new_connect_check();
/* 新規接続要求がある場合、その接続用の id を返す。*/
/* 新規接続が無い場合は、負を返す。*/

void   tcp_mark_authenticated(int id);
/* ログイン等の認証が完了した接続であることを記録する。
 * 未認証のまま一定時間経過した接続は切断される。 */

void   connect_check(int timeout);
/* 接続先へのバッファの吐き出し、バッファへの読み込みを */
/* timeout (ms) 行う。*/

int    nfflush(int n, int timeout);
/* その接続に対して、最大 timeout (ms) の間、優先的にバッファを書き出す。 */
/* 完全に吐き出せた場合の限り真を返す。*/

char   *ip_address_check(int id);
/* その接続の IP アドレスを返す。*/

char   *ip_domain_check(int id);
/* その接続の ドメイン名を返す。*/

int    fd_check(int id);
/* その接続の記述子を返す。*/

int    lost_connect(int id);
/* 接続が異常切断しているかどうかを判定する */

int    connect_error_check(int id);
/* 接続が異常切断した場合、そのエラー番号を返す。*/

int    connect_full();
/* 接続数が最大のとき、真を返す */

int    nfclose_error_check();
/* connect_check() により、nfclose() 後に異常切断した接続の id を返す。*/
/* (new_connect_check() でその id が新たに使用されるまで有効。)*/
/* 繰り返し呼び出すことによりそのような id を全て得られる。*/
/* そのような id が無くなると(もしくは最初からないと) -1 を返す。*/
/* ip_address_check(), fd_check(), connect_error_check() のみが */
/* その id に対して(まだ)安全に正しい結果が得られる。*/

#ifdef __MINGW32__
#include <sys/time.h>
/* かなり暫定 */
int gettimeofday (struct timeval *, void *);
#endif

#endif
