/* ソケット層: クライアント接続の管理と送受信バッファ
 *
 * 1つの接続は 0 から MAX_CLIENT-1 までの id で管理する。
 * 各接続は読み込みバッファ (RBUFSIZE) と書き込みバッファ (WBUFSIZE) を
 * 持ち、メインループは connect_check() を呼んで select() による
 * 送受信をまとめて行う。
 *
 * 接続には2つのフラグがある:
 *   connecting … ソケットが生きている (物理的な接続)
 *   reference  … ゲーム側がこの接続を使っている (論理的な参照)
 * nfclose() は reference だけを落とし、書き込みバッファが空になるまで
 * 切断を遅らせる (遅延切断)。nclose() は即座に切断する。
 * 切断後も読み込みバッファに残っている行は read_line() で読める。
 *
 * このファイルの関数・変数は、tcp.h で宣言された公開 API を除き
 * すべて無名名前空間に閉じている。他のモジュールは tcp.h 経由でしか
 * この層に触れない。
 */

#if defined(__CYGWIN32__) || defined(__MINGW32__)
#define __USE_W32_SOCKETS
#endif

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <time.h>

#ifdef __USE_W32_SOCKETS
//#include <windows.h>
#include <winsock.h>
#include <sys/time.h>
#define SOCK_ERR	WSAGetLastError()
#define CLOSE_SOCKET(s)	closesocket(s)
#define IS_VALID_SOCKET(s)	((s)!=INVALID_SOCKET)

#define NTTYS FD_SETSIZE
#else

#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#define SOCK_ERR	errno
#define CLOSE_SOCKET(n)	close(n)
typedef int SOCKET;
#define INVALID_SOCKET -1
#define IS_VALID_SOCKET(s)	((s)>=0)

#ifdef AIX
#include <sys/select.h>
#endif
#include <sys/time.h>
#include <unistd.h>

#define NTTYS getdtablesize()
#endif

#include "tcp.h"
#ifndef MAXHOSTNAMELEN		/* 誰かちゃんと定義して */
#define MAXHOSTNAMELEN 255
#endif

namespace {

constexpr int kReadBufSize = 1024;	/* 読み込みバッファの大きさ */
constexpr int kWriteBufSize = 65536;	/* 書き込みバッファの大きさ (16KB では大量行で溢れやすいため拡大、2026-11-17) */
constexpr int kWriteTimeout = 600;	/* 書き込めない回数の上限 (超えたら切断) */
constexpr int kWriteChunkSize = 2048;	/* 1回の write() で送る上限 (EL's idea) */

/* 1つの接続の状態。バッファは接続の開始時に確保し、切断時に解放する。 */
struct Client {
  bool connecting = false;	/* ソケットが生きているか */
  bool reference = false;	/* ゲーム側が参照しているか (偽だと遅延切断) */
  int errnum = 0;		/* 切断時のエラー番号 */
  SOCKET fd = INVALID_SOCKET;
  char ip_address[32] = {};	/* 接続元の IP アドレス文字列 */
  char ip_domain[256] = {};	/* 接続元のドメイン名 (add for domain display 20020218) */
  std::unique_ptr<char[]> rbuf;	/* 読み込みバッファ */
  int rbuf_len = 0;		/* 読み込みバッファ内の有効バイト数 */
  std::unique_ptr<char[]> wbuf;	/* 書き込みバッファ */
  int wbuf_len = 0;		/* 書き込みバッファ内の有効バイト数 */
  int wtimeout = 0;		/* 書き込むデータがあって書き込めない回数 */
  bool closing_err = false;	/* nfclose() 後に異常切断した */
};

std::array<Client, MAX_CLIENT> clients;

/* 新規接続受け付け用の待ち受けソケット。set_server() で作る。 */
SOCKET listen_fd = INVALID_SOCKET;

/* バッファの先頭 n バイトを消費し、残りを前に詰める。 */
void shift_left(char *buf, int &buf_len, int n) {
  if (n <= 0) return;
  std::memmove(buf, buf + n, buf_len - n);
  buf_len -= n;
}

/* 空いている接続 id を探す。1 から順に見て、最後に 0 を使う。
 * 空きが無ければ -1 を返す。 */
int new_client_id() {
  int n;
  for (n = 1; n < MAX_CLIENT; n++)
    if (!(clients[n].connecting || clients[n].reference)) break;
  if (n == MAX_CLIENT) {
    if (clients[0].connecting || clients[0].reference) return -1;
    n = 0;
  }
  return n;
}

/* 送受信バッファを確保する。確保できなければ偽を返す。 */
bool allocate_buffers(Client &c) {
  c.rbuf.reset(new (std::nothrow) char[kReadBufSize]);
  if (!c.rbuf) return false;
  c.wbuf.reset(new (std::nothrow) char[kWriteBufSize]);
  if (!c.wbuf) {
    c.rbuf.reset();
    return false;
  }
  return true;
}

/* 送受信バッファを解放する。 */
void release_buffers(Client &c) {
  c.rbuf.reset();
  c.wbuf.reset();
  c.rbuf_len = 0;
  c.wbuf_len = 0;
}

/* 接続スロットを初期化し、ソケットとアドレスを結び付ける。
 * 呼び出し前に allocate_buffers() でバッファを確保しておくこと。 */
void setup_client(int n, SOCKET fd, const char *addr) {
  Client &c = clients[n];
  c.rbuf_len = 0;
  c.wbuf_len = 0;
  c.wtimeout = 0;
  c.fd = fd;
  c.connecting = c.reference = true;
  c.closing_err = false;
  CPYSTR(c.ip_address, addr);
}

/* ソケットを閉じて切断する。書き込みバッファは破棄するが、
 * 読み込みバッファは残し、未読の行を読み切れるようにする。
 * 既に切断済みなら 0 を返す。 */
int close_client(int n, int errnum) {
  Client &c = clients[n];
  if (!c.connecting) return 0;
  c.connecting = false;
  c.errnum = errnum;
  c.wbuf.reset();
  c.wbuf_len = 0;
  return CLOSE_SOCKET(c.fd);
}

/* select() に渡す fd_set を組み立てる。
 * 書き込み待ちのある接続は書き込み集合へ、読み込み余地のある接続は
 * 読み込み集合へ、全接続を例外集合へ入れる。
 * 1つでも接続があれば真を返す。 */
bool collect_fd_sets(fd_set *rfdsp, fd_set *wfdsp, fd_set *efdsp) {
  bool any = false;
  for (int n = 0; n < MAX_CLIENT; n++) {
    const Client &c = clients[n];
    if (c.connecting) {
      if (c.wbuf_len > 0) FD_SET(c.fd, wfdsp);
      if (c.reference && c.rbuf_len < kReadBufSize)
        FD_SET(c.fd, rfdsp);
      FD_SET(c.fd, efdsp);
      any = true;
    }
  }
  return any;
}

/* 例外が起きた接続を切断する。 */
void close_exceptional(const fd_set &efds) {
  for (int n = 0; n < MAX_CLIENT; n++)
    if (clients[n].connecting && FD_ISSET(clients[n].fd, &efds))
      close_client(n, CONNECT_EXCEPT);
}

/* 書き込み可能になった接続のバッファを吐き出す。
 * 吐き出し切って参照も無ければ (遅延切断の完了として) 切断する。 */
void flush_writable(const fd_set &wfds) {
  for (int n = 0; n < MAX_CLIENT; n++) {
    Client &c = clients[n];
    if (c.connecting && FD_ISSET(c.fd, &wfds)) {
      const int l = (c.wbuf_len > kWriteChunkSize) ? kWriteChunkSize : c.wbuf_len;
      const int size =
#ifdef __USE_W32_SOCKETS
        send(c.fd, c.wbuf.get(), l, 0)
#else
        write(c.fd, c.wbuf.get(), l)
#endif
        ;
      if (size < 0) {
        /* write err */
        close_client(n, SOCK_ERR);
        if (!c.reference) c.closing_err = true;
      } else {
        shift_left(c.wbuf.get(), c.wbuf_len, size);
        c.wtimeout = 0;
        if (c.wbuf_len == 0 && !c.reference) close_client(n, 0);
      }
    }
  }
}

/* 読み込み可能になった接続からデータを読み込む。
 * EOF (size == 0) なら切断する。非ブロック読みで「まだ来ていない」
 * だけの場合は切断しない。 */
void fill_readable(const fd_set &rfds) {
  for (int n = 0; n < MAX_CLIENT; n++) {
    Client &c = clients[n];
    if (c.connecting && c.reference
        && c.rbuf_len < kReadBufSize && FD_ISSET(c.fd, &rfds)) {
      const int size =
#ifdef __USE_W32_SOCKETS
        recv(c.fd, c.rbuf.get() + c.rbuf_len,
             kReadBufSize - c.rbuf_len, 0)
#else
        read(c.fd, c.rbuf.get() + c.rbuf_len,
             kReadBufSize - c.rbuf_len)
#endif
        ;
      if (size <= 0) {
        if (size == 0) {
          close_client(n, CONNECT_DETECT_EOF);
        } else {
          const int err = SOCK_ERR;
          bool transient = false;  /* 後で来るかもしれない一時的な失敗 */
#ifdef EAGAIN
          transient = transient || err == EAGAIN;
#endif
#ifdef EWOULDBLOCK
          transient = transient || err == EWOULDBLOCK;
#endif
#ifdef WSAEWOULDBLOCK
          transient = transient || err == WSAEWOULDBLOCK;
#endif
          if (!transient) close_client(n, err);
        }
      } else {
        c.rbuf_len += size;
      }
    }
  }
}

/* 書き込みバッファを持ったまま書き込めない回数が上限を超えた
 * 接続を切断する。connect_check() の最後に呼ぶ。 */
void drop_write_timed_out() {
  for (int n = 0; n < MAX_CLIENT; n++) {
    Client &c = clients[n];
    if (c.connecting && c.wbuf_len > 0
        && (c.wtimeout++) >= kWriteTimeout)
      close_client(n, CONNECT_TIMEOUT);
  }
}

/* select() に渡す残り時間を timeval にする。
 * 0 未満は 0 に切り詰める (即時復帰のポーリングになる)。 */
timeval to_timeval(std::chrono::steady_clock::duration d) {
  using namespace std::chrono;
  if (d < steady_clock::duration::zero()) d = steady_clock::duration::zero();
  const auto sec = duration_cast<seconds>(d);
  const auto usec = duration_cast<microseconds>(d - sec);
  timeval tv{};
  tv.tv_sec = static_cast<time_t>(sec.count());
  tv.tv_usec = static_cast<long>(usec.count());
  return tv;
}

}  /* namespace */

void   tcp_initialize(){ /* まずこれをやってください(やらなくとも一応平気) */
#ifdef __USE_W32_SOCKETS
  static int initialized = 0;
  static WSADATA data;
#endif
  for (int n = 0; n < MAX_CLIENT; n++) {
    clients[n].connecting = false;
    clients[n].reference = false;
    clients[n].closing_err = false;
  }

#ifdef __USE_W32_SOCKETS
  if (initialized == 0) {
    if (WSAStartup(MAKEWORD(1, 1), &data)) {
      printf("WSAStartup failed (%d)\n", SOCK_ERR);
      exit(-1);
    }
    initialized = 1;
    atexit((void (*)(void))WSACleanup);
  }
#endif
}

void   get_ip_address(int n, char *p, int len){ /* 自分の IP アドレスを取得 */
  /* socketから取得するように変更 20021016 */
  struct sockaddr_in sa{};
  socklen_t sasize;

  sasize = sizeof(sa);
  if (getsockname(fd_check(n), (struct sockaddr *)&sa, &sasize) < 0) {
    strlcpy(p, "127.0.0.1", len); /* しょうがないのでlocalで… */
    return;
  }
  strlcpy(p, inet_ntoa(sa.sin_addr), len);
  /* p[len-1]='\0' ;*/ /* 一応 */
  return;
}

int    read_line(int n, char *buf, int len)
/* len 以上の長さの文字列は破棄する。*/
/* 馬鹿正直に渡しても、それを表示する他のクライアントに迷惑なだけ */
{
  Client &c = clients[n];
  if (!c.reference) return 0;
  int rs;
  for (rs = 0; rs < c.rbuf_len; rs++)
    if (c.rbuf[rs] == '\n') break;
  if (rs == c.rbuf_len) { /* '\n' が見つからない */
    if (!c.connecting || c.rbuf_len == kReadBufSize)
      /* 異常終了待ち、*/
      /* もしくは読み込み量が大きすぎなため破棄(ゴミが後に出るが:-P) */
      c.rbuf_len = 0;
    return 0;
  }
  int f = 0;
  if (++rs < len - 1) {
    c.rbuf[rs - 1] = '\0';
    f = 1;
    strlcpy(buf, c.rbuf.get(), rs);
  } /* else 破棄 */
  /* 読み込み用バッファの移動 */
  shift_left(c.rbuf.get(), c.rbuf_len, rs);
  return f;
}

int    peek_protocol(int n, char *buf, int len)
/* buf中 #で始まる行(phiプロトコル)を優先して読み出す */
{
  Client &c = clients[n];
  if (!c.reference) return 0;
  for (int rs = 0; rs < c.rbuf_len; rs++) {
    /* phiプロトコル */
    if (c.rbuf[rs] == '#') {
      for (int i = rs + 1; i < c.rbuf_len; i++) {
        if (c.rbuf[i] == '\n') {
          int f = 0;
          if ((++i) - rs < len) {
            strlcpy(buf, c.rbuf.get() + rs, i - rs);
            f = 1;
          } /* else 破棄 */
          /* 取り出した行を除いて後ろを詰める */
          const int rest = c.rbuf_len - i;
          std::memmove(c.rbuf.get() + rs, c.rbuf.get() + i, rest);
          c.rbuf_len = rs + rest;
          return f;
        }
      }
      if (rs == 0 && (!c.connecting || c.rbuf_len == kReadBufSize))
        /* 異常終了待ち、*/
        c.rbuf_len = 0;
    } else {
      /* '#' で始まらない行は丸ごと読み飛ばす。'\n' の直後から
       * 走査を続けるが、for の増分と合わせて1文字進むため、
       * 実際にはバッファ先頭の '#' 行だけが拾われる。
       * 残りは read_line() が消費した後の呼び出しで拾われる。
       * この挙動は従来通りとし、ここでは変えない。 */
      for (; rs < c.rbuf_len; rs++) {
        if (c.rbuf[rs] == '\n') {
          rs++;
          break;
        }
      }
    }
  }
  return 0;
}

int set_server(int port){
  struct sockaddr_in saddr{};
#ifdef TESTOPT
  long	 l;
#endif
  if (!IS_VALID_SOCKET(listen_fd = socket(AF_INET, SOCK_STREAM, 0))) return 0;
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  saddr.sin_port = htons(port);
#ifdef TESTOPT
  l = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &l, sizeof (l));
#endif
  if (bind(listen_fd, (struct sockaddr *)&saddr, sizeof(saddr))
#ifdef __USE_W32_SOCKETS
   == SOCKET_ERROR
#else
   < 0
#endif
   ){
    CLOSE_SOCKET(listen_fd); listen_fd = INVALID_SOCKET; return 0;
  }
  listen(listen_fd, 5);
  return 1;
}

void close_server(){ if (IS_VALID_SOCKET(listen_fd)) { CLOSE_SOCKET(listen_fd); listen_fd = INVALID_SOCKET; } }

int    connect_server(char *addr, int port){
#ifdef __USE_W32_SOCKETS
  unsigned long argp = 1;
#endif
  struct sockaddr_in saddr{};
  int n;
  SOCKET fd;
  if ((n = new_client_id()) < 0) return -1;
  saddr.sin_family = AF_INET;
  if ((saddr.sin_addr.s_addr = inet_addr(addr)) == INADDR_NONE) return -1;
  saddr.sin_port = htons(port);
  if (!IS_VALID_SOCKET(fd = socket(AF_INET, SOCK_STREAM, 0))) return -1;
#ifdef __USE_W32_SOCKETS
  ioctlsocket( fd , FIONBIO , &argp );
#else
  fcntl(fd, F_SETFL, O_NDELAY);
#endif
  if (!allocate_buffers(clients[n])) { CLOSE_SOCKET(fd);  return -1; }
  if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr))
#ifdef __USE_W32_SOCKETS
   == SOCKET_ERROR
#else
   < 0
#endif
   ) {
#ifdef EINPROGRESS
    if (
      SOCK_ERR != EINPROGRESS
#ifdef __USE_W32_SOCKETS
      && SOCK_ERR != WSAEWOULDBLOCK
#endif
      ){ CLOSE_SOCKET(fd); release_buffers(clients[n]); return -1; }
#endif
  }
  setup_client(n, fd, addr);
  return n;
}

int    nfclose(int n){
  Client &c = clients[n];
  if (!c.reference) return 0;
  c.reference = false;
  c.rbuf.reset();
  c.rbuf_len = 0;
  if (c.connecting && c.wbuf_len == 0) /* 空バッファは即閉じ */
    return close_client(n, 0);
  /* 書き込みバッファがあるときは、吐き出し後閉じる */
  return 1;
}

int nclose(int n){
  Client &c = clients[n];
  if (!c.reference) return 0;
  c.reference = false;
  c.rbuf.reset();
  c.rbuf_len = 0;
  if (c.connecting) return close_client(n, 0);
  return 1;
}

void	 write_line(int n, char *buf, int len, int t){
  /* バッファがあふれる時は t ms の間だけフラッシュしてみる */
  Client &c = clients[n];
  if (!c.reference || !c.connecting) return;
  if (c.wbuf_len + len > kWriteBufSize) {
    /* t=0 でも、溢れた時だけは少し待って吐き出しを試みる。
     * 掲示板の検索結果のような大量行の途中で、クライアントの
     * 受信が一瞬滞っただけで切断されるのを防ぐ (2026-11-17)。
     * 本当に死んだ相手は、この待ちの後で従来通り切断される。 */
    nfflush(n, t > 0 ? t : 200);
    if (c.wbuf_len + len > kWriteBufSize) {
      close_client(n, CONNECT_BUFFULL); return ;
    }
  }
  strlcpy(c.wbuf.get() + c.wbuf_len, buf, len + 1); /* buffer over runは起きないはず… */
  c.wbuf_len += len;
}

int new_connect_check(void){
#ifdef __USE_W32_SOCKETS
  unsigned long argp = 1;
#endif
  /* 新規接続がある場合は、バッファを用意し、その id を返す。*/
  /* 新規接続が無い場合は -1 を返す。*/
  fd_set rfds;
  struct timeval zero_timeout;
  SOCKET fd;
  struct sockaddr_in caddr{};
#ifdef DOMAIN_DISPLAY
  struct hostent *hst;
#endif
  socklen_t size;
  int n;
  if (!IS_VALID_SOCKET(listen_fd)) return -1;
  if ((n = new_client_id()) < 0) return -1;
  FD_ZERO(&rfds); FD_SET(listen_fd, &rfds);
  zero_timeout.tv_sec  = 0; zero_timeout.tv_usec = 0;
  if (select(NTTYS, &rfds, NULL, NULL, &zero_timeout) <= 0) return -1;
  size = sizeof(caddr);
  if (!allocate_buffers(clients[n])) return -1;
  if (!IS_VALID_SOCKET(fd = accept(listen_fd, (struct sockaddr *)&caddr, &size))) {
    release_buffers(clients[n]); return -1;
  }
#ifdef __USE_W32_SOCKETS
  ioctlsocket( fd , FIONBIO , &argp );
#else
  fcntl(fd, F_SETFL, O_NDELAY);
#endif
  setup_client(n, fd, inet_ntoa(caddr.sin_addr));
#ifdef DOMAIN_DISPLAY
  hst = gethostbyaddr((char *)&(caddr.sin_addr), sizeof(struct in_addr), AF_INET);
  CPYSTR(clients[n].ip_domain, hst->h_name);
#else
  CPYSTR(clients[n].ip_domain, clients[n].ip_address);
#endif
  return n;
}

void connect_check(int t){ /* バッファの書き込み／読み込み */
  /* 終了期限まで select() を繰り返す。期限の判定には単調時計を使い、
   * 壁時計のずれに影響されないようにしている (従来は gettimeofday)。 */
  const auto deadline = std::chrono::steady_clock::now()
    + std::chrono::milliseconds(t);
  fd_set rfds, wfds, efds;
  do {
    FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
    /* vv change for anti cpu exhausted 20050802 */
    if (collect_fd_sets(&rfds, &wfds, &efds) && IS_VALID_SOCKET(listen_fd)) {
      timeval timeout = to_timeval(deadline - std::chrono::steady_clock::now());
      if (select(NTTYS, &rfds, &wfds, &efds, &timeout) > 0) {
        close_exceptional(efds); fill_readable(rfds); flush_writable(wfds);
      }
    }
    else {
      usleep(1);
    }
    /* ^^ end */
  } while (std::chrono::steady_clock::now() < deadline);
  drop_write_timed_out();
}

int nfflush(int n, int t){
  /* 書き込みバッファの強制吐き出し */
  /* 完全に吐き出せた場合に限り真を返す */
  Client &c = clients[n];
  if (!c.reference || !c.connecting) return 0;
  if (c.wbuf_len == 0) return 1;
  const auto deadline = std::chrono::steady_clock::now()
    + std::chrono::milliseconds(t);
  fd_set wfds;
  do {
    FD_ZERO(&wfds); FD_SET(c.fd, &wfds);
    timeval timeout = to_timeval(deadline - std::chrono::steady_clock::now());
    if (select(NTTYS, NULL, &wfds, NULL, &timeout) <= 0) break;
    flush_writable(wfds);
    if (c.wbuf_len == 0) return 1;
    if (!c.connecting) break;
  } while (std::chrono::steady_clock::now() < deadline);
  return 0;
}

char *ip_address_check(int n){	return clients[n].ip_address; }
char *ip_domain_check(int n){  return clients[n].ip_domain; } /* add for domain display 20020218 */
int fd_check(int n){			return clients[n].fd; }
int lost_connect(int n){ /* 見掛けのクライアント切断情報 */
  if (!clients[n].reference) return 1;
  if (!clients[n].connecting && clients[n].rbuf_len == 0) return 1;
  return 0;
}
int connect_error_check(int n){ return clients[n].errnum; }
int connect_full(){ return new_client_id() < 0; }
int nfclose_error_check(){
  int n;
  for (n = 0; n < MAX_CLIENT; n++) if (clients[n].closing_err) {
    clients[n].closing_err = false;
    return n;
  }
  return -1;
}

#ifdef __MINGW32__
int gettimeofday (struct timeval *tp, void *tz)
{
  if (tz)
    abort ();
  tp->tv_usec = 0;
  if (time (&tp->tv_sec) == (time_t) -1)
    return -1;
  return 0;
}
#endif
