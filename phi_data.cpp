
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38  */

#include  <cstdio>
#include  <cstring>
#include  <cctype>
#include  <csignal>
#include  <ctime>
#ifndef Mips
#include  <cstdlib>
#endif
#include  <unistd.h>
#include  <climits>
#include  <dirent.h>
#include  <array>

#include  "tcp.h"
#include  "phi_dm.h"
#include  "phi_attack.h"
#include  "phi_data.h"
#include  "phi_calc.h"
#include  "phi_enemy.h"
#include  "phi_use.h"
#include  "phi_str.h" /* add for dmprintf 20000907 */
#include  "phi_class.h" /* PHIDM NEO extension: phi.class (Lua) */
#include  "phi_obj.h"
#include  "phi_m57.h"
#include  "ext_data.h"
#include  "ext_item.h"
#include  "new_command.h"
#include  "expression_op.h"
#include  "ext_skills.h"
#ifdef __CYGWIN32__
#include <process.h>
#endif

/* Single definitions of the globals declared extern in phi_data.h.
   (C++ stage 0: the C build relied on -fcommon merging the
   tentative definitions from the header.) */
ItemData putbuf;
int    gfbuf[GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX];
int    gfbuf2[GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX];
int    gfn;
int    gfn2;
int    match_n;
int	flr_max;
int	menun;
MENUData  *menud;
CMDData cmdd[CMD_MAX];

#define  chartoint(x) ((int)(unsigned int)(unsigned char)(x))

extern time_t abstime;
extern struct tm *sttime;

#ifdef DEBUG
char debug_buf[256];
#endif

extern char timestr[32];
CHROData *ed_o;

extern int px[]; /* added by Qajin 19991103 */
extern int py[]; /* 権限判定の為に追加(^^;) */

/* add new fn 20000324 */
namespace {
constexpr std::array<char, 10> kFnc = {'n', 'e', 'l', 'a', 's', 'w', 'f', 'm', 'p', 'r'};
} // namespace
#ifndef EPHI_FastBind_CD_NUM
/* cfile / savfile は efopen(char *, char *)（phi_data.h の共有宣言）へ
   渡すため char * のまま */
static char *cfile[2] = {"phi.cd", "phi.bd"};
static char *savfile = "phi.sav";
#else
static char *cfile[2] = {"phi_cdi/cd.%i", "phi.bd"};
static char *savfile = "phi_cdi/sav.%i";
#endif
int    enmax;     /* クリーチャーののべ数 */

extern int MLine[3];
extern int MHit;
extern int MExplode[3][3];
extern int MHeal;
extern int MCoat;
extern int MProtect;
extern int MIllusion;
extern int MDown;

extern int log_send;

/* add for spear 20000601 */
int  fsign(int x)
{
    if (x == 0)
        return 0;
    else if (x > 0)
        return 1;
    else
        return -1;
}

/*----------------------------------
  ps%のゆらぎを発生させる
----------------------------------*/
int    fsps(int ps, int v, int mx)
{
  v += frand(v * ps / 100 + 1) - v * ps / 200;
  if (v < 0)
    v = 0;
  if (v > mx)
    v = mx;
  return (v);
}

/*----------------------------------
  strstr 代替
----------------------------------*/
char  *fstrstr(char *p1, char *p2)
{
  int    i, j, l2;

  l2 = std::strlen(p2);
  j = std::strlen(p1) - l2 + 1;

  for (i = 0; i < j; i++) {
    if (!std::strncmp(p1, p2, l2))
      return (p1);
    p1++;
  }
  return (nullptr);
}

/*----------------------------------
  toupper 代替
----------------------------------*/
char   ftoupper(char c)
{
  if (c >= 'a' && c <= 'z')
    c -= 'a' - 'A';
  return (c);
}

/*----------------------------------
  tolower 代替
----------------------------------*/
char   ftolower(char c)
{
  if (c >= 'A' && c <= 'Z')
    c += 'a' - 'A';
  return (c);
}

/*----------------------------------
  isspace 代替
----------------------------------*/
int    fisspace(char c)
{
  if (c == ' ' || c == '\t')
    return (1);
  return (0);
}

/*----------------------------------
  IDの乱数部を発生する
----------------------------------*/
void   fecrypt(char *p, int n)
{
  int    i, j;

  for (i = 0; i < n; i++) {
    j = frand(26 * 2 + 10);
    if (j < 10) {
      p[i] = (char) ('0' + j);
    } else if (j < 36) {
      p[i] = (char) (j - 10 + 'a');
    } else {
      p[i] = (char) (j - 36 + 'A');
    }
  }
}

/*----------------------------------
  アイテムデータの整列
----------------------------------*/
void   item_gc(int n)
{
  int    i, ds, af, wf, of;

  ds = af = wf = of = 0;
  for (i = 0; i < MAX_ITEMS; i++) {
    if (cd[n].id[i].type != DT_NONE) {
      if (ds < i) {
  std::memcpy(&(cd[n].id[ds]), &(cd[n].id[i]), sizeof (ItemData));
  if (cd[n].weq == &(cd[n].id[i])) {
    wf = -1;
    cd[n].weq = &(cd[n].id[ds]);
  }
  if (cd[n].aeq == &(cd[n].id[i])) {
    af = -1;
    cd[n].aeq = &(cd[n].id[ds]);
  }
  if (cd[n].oeq == &(cd[n].id[i])) {
    of = -1;
    cd[n].oeq = &(cd[n].id[ds]);
  }
      } else {
  if (cd[n].weq == &(cd[n].id[i]))
    wf = -1;
  if (cd[n].aeq == &(cd[n].id[i]))
    af = -1;
  if (cd[n].oeq == &(cd[n].id[i]))
    of = -1;
      }
      ds++;
    }
  }
  for (; ds < MAX_ITEMS; ds++) {
    delete_item(&(cd[n].id[ds]));
  }
  if (wf == 0) {
    cd[n].weq = nullptr;
    cd[n].wsep = 0;
  }
  if (af == 0) {
    cd[n].aeq = nullptr;
    cd[n].asep = 0;
  }
  if (of == 0) {
    cd[n].oeq = nullptr;
    cd[n].at = AT_NONE;
    cd[n].osep = 0;
  } else {
    cd[n].at = cd[n].oeq->AT;
  }
}

void   swap_item(ItemData * id1, ItemData * id2)
{
  ItemData idtmp;

  std::memcpy(&idtmp, id1, sizeof (ItemData));
  std::memcpy(id1, id2, sizeof (ItemData));
  std::memcpy(id2, &idtmp, sizeof (ItemData));
}

/*----------------------------------
 ファイルチェック用簡易ルーチン
----------------------------------*/
/* pfname は CPYSTR の宛先のため C 配列のまま */
/*static*/ char pfname[128];
int lcnt = -1;

char  *efgets(char *buf, int size, FILE * fp)
{
  lcnt++;
  return (fgets(buf, size, fp));
}

FILE  *efopen(char *fnm, char *mod)
{
  FILE *fp;

  lcnt = 0;
  CPYSTR(pfname, fnm);
  fp = fopen(fnm, mod);
#ifdef DEBUG
  if(fp!=nullptr){
    snprintf(debug_buf, sizeof(debug_buf),"efopen:  (%d)(name=%s)(mode=%s)",fileno(fp),fnm,mod);
    log_print(debug_buf);
  }
#endif
  return (fp);
}

void    efclose(FILE * fp)
{
  int i;
#ifdef DEBUG
  int j;
  j = fileno(fp);
#endif

  lcnt = -1;
  i=fclose(fp);
#ifdef DEBUG
  if(i==0){
    snprintf(debug_buf, sizeof(debug_buf),"efclose: (%d)",j);
    log_print(debug_buf);
  }
#endif
}

/*----------------------------------
 アイテム座標ハッシュへのデータの追加
----------------------------------*/
/* 床の上のアイテムは座標毎に集約される。                    */
/* 検索時間等の削減のため適当に割り振りが行われ格納される。  */
ItemPData *additempos(int x, int y)
{
  int hd;
  ItemPData *p;

  p = finditempos(x,y);

  if (p != nullptr)
    return (p);

  hd = (x + y) & HASHMASK;

  p = (ItemPData *) malloc(sizeof(ItemPData));
  p->x = x;
  p->y = y;
  p->fid = nullptr;
  p->next = iphash[hd];
  p->notouch = 0;

  iphash[hd] = p;

  return (p);
}

/* additemposと同様だがデータの追加はしない */
ItemPData *finditempos(int x, int y)
{
  int    hd;
  ItemPData **p;

  hd = (x + y) & HASHMASK;

  p = &(iphash[hd]);
  while (*p != nullptr) {
    if ((*p)->x == x && (*p)->y == y)
      break;
    p = &((*p)->next);
  }
  return (*p);
}

/* アイテムがなくなった座標のデータ開放 */
void delitempos(int x, int y)
{
  int    hd;
  ItemPData **p, *p2;
  std::array<char, 32> fname; /* サークルバックアップ by NEO 20060402 */

  hd = (x + y) & HASHMASK;

  p = &(iphash[hd]);
  while (*p != nullptr) {
    if ((*p)->x == x && (*p)->y == y)
      break;
    p = &((*p)->next);
  }
  if (*p != nullptr && (*p)->fid == nullptr) {
    p2 = *p;
    *p = (*p)->next;
    free(p2);
    snprintf(fname.data(),fname.size(),"phi_circle_b/%03i%03i.id",x,y); /* サークルバックアップ by NEO 20060528  */
	unlink(fname.data()); /* Add by NEO 20060528 for サークルが空の場合バックアップを削除  */
  }
}

/* add 20020807 */
#ifndef ITEM_NO_TOUCH
#define ITEM_NO_TOUCH 12 /* Change by NEO 20060331 サークルデータ破損軽減のため　original　36*/
#endif

/*----------------------------------
 アイテムハッシュへのデータの追加
----------------------------------*/
void   additem(int x, int y, ItemData * id, int gp)
/* 床の上のアイテムはここに集約される。                      */
/* 検索時間等の削減のため適当に割り振りが行われ格納される。  */
/* gp>0 のときは、お金を示す。                               */
/* gp=-1の時は罠を示す(20001215 拡張)                        */
{
  ItemPData *pp;
  ItemOData **p, *p2, *p3;

  if ((map[y][x].tip & TMASK) == T_STORE) {
    if (gp > 0) {
        tax += gp;
    }
    return;
  }
  pp = additempos(x, y);

  if (!gp && pp->notouch >= ITEM_NO_TOUCH)
    load_id_in_circle(x,y);

  p = &(pp->fid);
  while (*p != nullptr) {
    if (std::strcmp((*p)->id.name, id->name) > 0)
      break;
    p = &((*p)->next);
  }
  p2 = *p;
  p3 = (ItemOData *) malloc(sizeof (ItemOData));
  if (p3 == nullptr) {
    log_print("Insufficient memory for item data\n");
    signal_trap(SIGTERM);
  }
  std::memcpy(&(p3->id), id, sizeof(ItemData));
  p3->gp = gp;
  p3->next = p2;
  if(p3->id.active_time==ULONG_MAX) p3->id.active_time = 0;
  *p = p3;
}

/*----------------------------------
   アイテムデータの検索
----------------------------------*/
ItemOData **finditem(int x, int y, ItemOData ** id)
/* additem と似ているが、データを追加しない。*/
/* 該当するデータが無い場合は、NULL を返す。 */
{
  ItemPData *p;

  if (id == nullptr) {
    p = finditempos(x,y);
    if (p == nullptr || p->fid == nullptr)
      return (nullptr);
    return (&(p->fid));
  } else if (*id == nullptr || (*id)->next == nullptr) {
    return (nullptr);
  } else
    return (&((*id)->next));
}

/*----------------------------------
 文字列ハッシュへのデータの追加
----------------------------------*/
char  *findstr(char *str)
/* ゲーム中に生成や消失の可能性のある文字列は一括してここに集約される。*/
/* 検索時間等の削減のため、適当に振り分けられて格納される。            */
/* 定期的に行われる str_gc() により、使われていない文字列は解放される。*/
{
  int    hd, i;
  StringData **p, *p2, *p3;
  char  *ps;

  ps = str;     /* ハッシュ関数 */
  hd = 0;
  while (*ps != '\0') {
    hd += (int) (unsigned int) (unsigned char) *ps++;
  }
  hd &= HASHMASK;

  p = &(shash[hd]);
  while (*p != nullptr) {
    if ((i = std::strcmp((*p)->string, str)) > 0)
      break;
    if (i == 0)
      return ((*p)->string);
    p = &((*p)->next);
  }
  p2 = *p;
  if((p3 = (StringData *) malloc(sizeof (StringData)))==nullptr
     || (p3->string = strdup(str)) == nullptr){
    log_print("Insufficient memory for string data\n");
    signal_trap(SIGTERM);
  }
  p3->next = p2;
  *p = p3;
#ifdef DEBUG_STR_GC
snprintf(debug_buf,sizeof(debug_buf),"str add %s",p3->string);
log_print(debug_buf);
#endif
  return (p3->string);
}

/*----------------------------------
 文字列ハッシュの検索
----------------------------------*/
StringData *findstrdata(char *str)
/* findstr と似ているが、データは追加しない。*/
/* 該当する文字列が無かった場合は NULL をかえす。*/
{
  int    hd, i;
  StringData *p;
  char  *ps;

  ps = str;     /* ハッシュ関数 */
  hd = 0;
  while (*ps != '\0') {
    hd += (int) (unsigned int) (unsigned char) *ps++;
  }
  hd &= HASHMASK;

  p = shash[hd];
  while (p != nullptr) {
    if ((i = std::strcmp(p->string, str)) > 0)
      return (nullptr);
    if (i == 0)
#ifdef DEBUG_STR_GC
{
snprintf(debug_buf,sizeof(debug_buf),"find %s",p->string);
log_print(debug_buf);
#endif
      return (p);
#ifdef DEBUG_STR_GC
}
#endif
    p = p->next;
  }
  return (nullptr);
}

/*----------------------------------
   外貨の持ち込み
----------------------------------*/
void   importcir(int n)
{
  ItemData id;
  std::array<char, 32> buf;
  std::array<char, 256> buf2;
  int    i, f;

  if (cirate == 100)
    return;
  f = -1;
  if ((cd[n].gp > 0) && ciflag) {
    dmcheck_write(n, DMM_NOUSE_FOREIGN_CUR);
    f = 0;
    snprintf(buf.data(), buf.size(),"Foreign currency (%d gp)", cd[n].gp);
    id.name = findstr(buf.data());
    id.type = MT_SILVER | WT_FIST | IT_FOREIGN;
    id.wp = 1;
    id.fn = FN_NONE;
    id.ATP = 1;
    id.VALUE = cd[n].gp / 100;
    id.VALUE2 = cd[n].gp % 100;
    id.SE = SE_NONE;
    id.active_time = 0;
    id.extension = nullptr; /* bug fix 20000204 */
    cd[n].gp = 0;
    skillup(n,SKL_DETECT_IT,IT_FOREIGN,99,90,10); /* ... :-P */
  } else {
    cd[n].gp *= cirate;
    cd[n].gp /= 100;
    if (cd[n].gp > 9999){
      tax+= (cd[n].gp-9999);
      cd[n].gp = 9999;
    }
  }
  for (i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type == DT_NONE) && (f == 0) && ciflag) {
      std::memcpy(&(cd[n].id[i]), &id, sizeof (ItemData));
      f = -1;
    }
  }
  if ((f == 0) && ciflag) {
    additem(cd[n].x, cd[n].y, &id, 0);
    map[cd[n].y][cd[n].x].tip |= T_ITEM;
    dmprintf(n,0,buf2.data(), buf2.size(),DMM_APPEAR_SOMETHING, id.name);
    write_companion(n, 0, buf2.data(), 0);
  } else {
    item_gc(n);
  }
}

/*----------------------------------
   通貨の持ち出し
----------------------------------*/
void   exportcir(int n)
{
  ItemData id;
  std::array<char, 256> buf;
  int    i, k;

  if (cirate == 100)
    return;

  k = -1;
  if (cd[n].gp > 0 && ciflag) {
    snprintf(buf.data(), buf.size(),"Traveler's check (%d %s)", cd[n].gp, cirunit);
    tax += cd[n].gp;
    cd[n].gp *= 100;
    cd[n].gp /= cirate;  /* 標準レートに戻す */
    k = cd[n].gp;
    id.name = findstr(buf.data());
    id.type = MT_SILVER | WT_FIST | IT_JEWEL;
    id.wp = 1;
    id.fn = FN_NONE;
    id.ATP = 1;
    id.VALUE = cd[n].gp / 100;
    id.VALUE2 = cd[n].gp % 100;
    id.SE = SE_NONE;
    id.active_time = 0;
    id.extension = nullptr; /* bug fix 20000204 */
    cd[n].gp = 0;
  } else {
    cd[n].gp *= 100;
    cd[n].gp /= cirate;  /* 標準レートに戻す */
    if (cd[n].gp > 9999){
      tax+=(cd[n].gp-9999) * cirate / 100; /* 自国レートで TAX へ */
      cd[n].gp = 9999;
    }
  }
  for (i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type & IT_MASK) == IT_FOREIGN) {
      cd[n].gp += cd[n].id[i].VALUE * 100 + cd[n].id[i].VALUE2;
      delete_item(&(cd[n].id[i]));
    }
  }
  if (k > 0 && ciflag) {
    for (i = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE) {
  std::memcpy(&(cd[n].id[i]), &id, sizeof (ItemData));
  break;
      }
    }
  }
  item_gc(n);
}

/*----------------------------------
  メッセージスイッチ
20030501 スイッチとデータを分けた
----------------------------------*/
/* :A レコードを番号から引く (PHIDM NEO extension: y/s 選択用) */
static int mesg_index(int orgn)
{
  int i;
  for (i = 0; i < msgn; i++) {
    if (msgd[i].orgn == orgn)
      return i;
  }
  return -1;
}

void   load_mesg(int n)
{
  int    i, l;
  std::array<char, 256> message; /* change for dmprint 20000907 */

  /* bug fix for message-menu conflict 20050827 */
  while (cd[n].anmf && cd[n].st != ST_MENU) {
    for(i=0;i<msgn;i++) {
        if (msgd[i].orgn == cd[n].anmf)
            break;
    }
    if (i >= msgn) {
        cd[n].anmf = 0; /* bug fix for freeze 20050520 */
        break;
    }

    cd[n].anmf = msgd[i].next;
    cd[n].anmt = msgd[i].wait - 1;

    /* psp3 - added by aleph */
    switch(msgd[i].type) {
      case 0:                         /* 0 : message 表示 */
          dmprintf(n, 0, message.data(), message.size(),msgd[i].data.mesg);
          check_write(n, message.data());
          break;
      case 1:                         /* 1 : 拡張"x" 強制移動 */
          clear_chr(n);
          cd[n].x = msgd[i].data.pos.x;
          cd[n].y = msgd[i].data.pos.y;
          set_chr(n);
          print_map(n, 0);
          if (!trnf) {          /* :O T 指定時には 移動先のスイッチを動作させない */
            cd[n].bx = cd[n].x;
            cd[n].by = cd[n].y;
          }
          break;
      case 2:                         /* 2 : 拡張"f" flag ON */
          l = msgd[i].data.flag;
          change_flag(n,l,FLAG_ON,1);
          load_switch(n,cd[n].x,cd[n].y,10);
          break;
      case 3:                         /* 3 : 拡張"z" FX (PHIDM NEO extension) */
          fx_send_payload(n, msgd[i].data.mesg);
          break;
      case 4:                         /* 4 : 拡張"y" Yes/No選択 (PHIDM NEO extension, from MRDM) */
          if (msgd[i].data.mesg != nullptr) {
            dmprintf(n, 0, message.data(), message.size(), msgd[i].data.mesg);
            check_write(n, message.data());
          }
          check_write(n, "#list\n");
          check_write(n, "[ 1] はい\n");
          check_write(n, "[ 2] いいえ\n");
          check_write(n, "#end-list\n");
          cd[n].anmf = msgd[i].orgn;
          cd[n].anmt = 0;
          cd[n].st = ST_SELECT;
          check_write(n, "#s-edit\n");
          break;
      case 5:                         /* 5 : 拡張"s" 選択肢 (PHIDM NEO extension, from MRDM) */
          {
            std::array<char, 256> wbuf;
            int idx = i, cnt = 0;
            check_write(n, "#list\n");
            while (idx >= 0 && cnt < 8) {
              cnt++;
              if (msgd[idx].data.mesg != nullptr) {
                dmprintf(n, 0, message.data(), message.size(), msgd[idx].data.mesg);
                snprintf(wbuf.data(), wbuf.size(), "[%2i] %s", cnt, message.data());
              } else {
                snprintf(wbuf.data(), wbuf.size(), "[%2i] \n", cnt);
              }
              check_write(n, wbuf.data());
              if (msgd[idx].next == 0)
                break;
              idx = mesg_index(msgd[idx].next);
            }
            check_write(n, "#end-list\n");
          }
          cd[n].anmf = msgd[i].orgn;
          cd[n].anmt = 0;
          cd[n].st = ST_SELECT;
          check_write(n, "#s-edit\n");
          break;
      default:
          break;
        }
    /* end */
    if (cd[n].anmt < 0) {
      cd[n].anmt = 0;
    } else {
      break;
    }
  }
}


/* :A 拡張 y/s の回答処理 (PHIDM NEO extension, from MRDM) */
void   select_mesg(int n, int k)
{
  int i, idx, cnt;

  i = mesg_index(cd[n].anmf);
  if (i < 0) {
    cd[n].anmf = 0;
    cd[n].st = ST_COMMAND;
    return;
  }
  if (msgd[i].type == 4) {          /* Yes/No選択 */
    if (k == 1) {
      cd[n].anmf = msgd[i].next;    /* Yes分岐 */
    } else if (k == 2) {
      cd[n].anmf = msgd[i].wait;    /* No分岐 */
    } else {
      return;                       /* 1/2以外は選択待ちを続ける */
    }
    cd[n].anmt = 0;
    cd[n].st = ST_COMMAND;
    load_mesg(n);
  } else if (msgd[i].type == 5) {   /* 選択肢 */
    idx = i;
    for (cnt = k; cnt > 1; cnt--) {
      if (msgd[idx].next == 0) {
        idx = -1;
        break;
      }
      idx = mesg_index(msgd[idx].next);
      if (idx < 0)
        break;
    }
    if (idx >= 0)
      cd[n].anmf = msgd[idx].wait;  /* 選択時の分岐 */
    /* 範囲外の番号では anmf を変えずに選択肢を再表示する */
    cd[n].anmt = 0;
    cd[n].st = ST_COMMAND;
    load_mesg(n);
  } else {
    cd[n].st = ST_COMMAND;
  }
}


/* メニュー起動 */
/* add 20041212 */
void  display_menu(int n, int num)
{
    int i;

    cd[n].st = ST_COMMAND;
    cd[n].anmf = 0;
    for(i=0;i<menun;i++) {
        if (menud[i].menu_num == num) {
            if (menud[i].caption != nullptr)
                dmcheck_write(n,menud[i].caption);
            if (menud[i].sel != nullptr) {
                cd[n].st = ST_MENU;
                cd[n].anmf = num;
            }
        }
    }
}

/* メニュー選択 */
/* add 20041212 */
void  select_menu(int n, char *p)
{
    int i, menu;

    menu = cd[n].anmf;

    cd[n].st = ST_COMMAND;
    cd[n].anmf = 0;
    for(i=0;i<menun;i++) {
        if (menud[i].menu_num == menu && menud[i].sel != nullptr) {
            if (wildcmp(menud[i].sel,p)) {
                CPYSTR(cd[n].str, p);
                switch (menud[i].act_type) {
                    case MENU_ACT_FLAG:
                        change_flag(n,menud[i].act_param,FLAG_ON,1);
                        load_switch(n,cd[n].x,cd[n].y,10);
                        break;
                    case MENU_ACT_MESG:
                        cd[n].st = ST_MSGW;
                        cd[n].anmf = menud[i].act_param;
                        cd[n].anmt = 0;
                        load_mesg(n);
                        break;
                    case MENU_ACT_MENU:
                        display_menu(n,menud[i].act_param);
                        break;
                    case MENU_ACT_NULL:
                    default:
                        break;
                }
                break;
            }
        }
    }
}


/*----------------------------------
  世界間接続のパスワードチェック
----------------------------------*/
int    authorize(char *password)
{
  int    i, cn;

  i = 0;
  for (cn = 0; cn < gatemax; cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'P')
      continue;
    i++;
    if (!std::strcmp(gatedata[cn][1].dp, password))  /* match */
      return (gatedata[cn][2].di);    /* authorize it */
  }
  if (i == 0)
    return (-1);
  return (0);
}

/*----------------------------------
  ゲート、キャラ転送位置のシフト
----------------------------------*/
int    shift_gate(int *x, int *y, int authority, char *name, char *ip)
/* 返却値は -1(接続受理), 0(接続拒否) */
{
  int    i, cn;
  std::array<char, 256> buf;

  i = 0;
  for (cn = 0; cn < gatemax; cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'H')
      continue;
    i++;
    if (gatedata[cn][3].di == 'P' && !authority)  /* パスワードチェック */
      continue;
    if (gatedata[cn][3].di != '-' && gatedata[cn][3].di != authority)  /* パスワードチェック2 */
      continue;
    if (!wildcmp(gatedata[cn][4].dp, name)) /* ランド名チェック */
      continue;
    if (!wildcmp(gatedata[cn][5].dp, ip)) /* IP チェック */
      continue;
    /* とりあえずポート無視 */

    /* 条件すべて O.K. */
    if (gatedata[cn][1].di != 0)
      *x = gatedata[cn][1].di;  /* 指定座標にシフト */
    if (gatedata[cn][2].di != 0)
      *y = gatedata[cn][2].di;  /* 指定座標にシフト */
      if ((fld[map[*y][*x].area].flag) & FF_GATE) {
  snprintf(buf.data(), buf.size(),"shift-err: (%d,%d) %d [%s](%s)"
    ,*x, *y, authority, name, ip);
  log_print(buf.data());
  return (0);   /* 位置が悪いので却下 */
      }
    return (-1);
  }
  if (i == 0)
    /* change default acception 20040408 */
    if (!((fld[map[*y][*x].area].flag) & FF_GATE))
      return (-1);
  snprintf(buf.data(), buf.size(),"shift-err: (%d,%d) %d [%s](%s)"
    ,*x, *y, authority, name, ip);
  log_print(buf.data());
  return (0);     /* 接続拒否 */
}


/* psp4 - banned check - by aleph */
/* メモリ上のスイッチをなめるので、接続のたびに負荷がかかる。　DoS注意 */
/* オンメモリモード専用 */
/* 返値は　０：拒否　－１：許可 */
/*  専用配列を使うよう修正 changed by Qajin 19991020 */
int    ip_banned(int n)
{
  int   cn;
  char  *ip;
  char  *dn;
  long  now;

  now = time(nullptr);
  ip = ip_address_check(n);    /* クライアントのアドレス */
  dn = ip_domain_check(n);     /* add for domain display 20020221 */

  for (cn = 0; cn < BANNEDIPMAX; cn++) {
    if (ip_ban[cn].ip == nullptr)
      continue;   /* 何もないエントリ */
    if (ip_ban[cn].time < now) {    /* 時間切れしたエントリ */
    ip_ban[cn].ip = nullptr;
    ip_ban[cn].time = 0;
    continue;
    }

#ifdef DEBUG_BAN
    snprintf(debug_buf, sizeof(debug_buf),"banned check: %s <=> %s(%s)", ip_ban[cn].ip, dn, ip);
    log_print(debug_buf);
#endif

    if (wildcmp(ip_ban[cn].ip, ip) || wildcmp(ip_ban[cn].ip, dn))
      {
#ifdef DEBUG_BAN
        snprintf(debug_buf, sizeof(debug_buf),"banned filter matches: %s", ip_ban[cn].ip);
        log_print(debug_buf);
#endif
        return 0;  /* 接続拒否 */
      }
  }

  return -1; /* 接続可 */
}

/* 複数アクセスチェック */
int    ip_multi(int n)
{
  int   i,cnt;
  char  *ip;

  ip = ip_address_check(n);    /* クライアントのアドレス */
  cnt = 0;

  for (i = 1; i < MAX_NTTYS; i++) {
    if(cd[i].fd == -1)
      continue;
    if (!std::strcmp(ip,ip_address_check(i)))
      cnt++;
  }

    if (cnt>=multi_access)
      {
#ifdef DEBUG_MULTI
        snprintf(debug_buf, sizeof(debug_buf),"access count: %i", cnt);
        log_print(debug_buf);
#endif
        return 0;  /* 接続拒否 */
      }

  return -1; /* 接続可 */
}

/* add 20030722 */
int    id_banned(int n)
{
    int i;

    for(i=0; i < BANNEDIDMAX; i++) {
        if (!std::strncmp(id_ban[i],cd[n].uid,sizeof(cd[n].uid)))
            return (1);
    }
    return (0);
}

int id_ban_add(char *p)
{
    int i;

    for(i=0; i < BANNEDIDMAX && id_ban[i][0] != '\0';i++);
    if (i >= BANNEDIDMAX) {
        std::memmove(id_ban[0],id_ban[1],sizeof(id_ban[0])*(BANNEDIDMAX-1));
        i = BANNEDIDMAX-1;
    }
    loadstr(id_ban[i], &p, '\n', 16);
    return(0);
}

/*----------------------------------
 相手サーバに送るパスワードを選択（頭に P: 付いてる）
----------------------------------*/
char  *select_password(char *name, char *ip)
{
  char  *p;
  int    cn;

  p = findstr(" ");
  for (cn = 0; cn < gatemax; cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'H')
      continue;
    if (!wildcmp(gatedata[cn][4].dp, name)) /* ランド名チェック */
      continue;
    if (!wildcmp(gatedata[cn][5].dp, ip)) /* IP チェック */
      continue;
    p = gatedata[cn][6].dp;
    break;
  }
  return (p);
}

/* add for Chr Extension data 20000929 */
CHRExData **load_chrex(CHRExData **chrex,char *p)
{
    std::array<char, 256> buf;

    if (chrex == nullptr)
        return ((CHRExData **)nullptr);

    if ((*chrex = (CHRExData *)malloc(sizeof(CHRExData))) == nullptr) {
      log_print("Insufficient memory for chr extension data\n");
      signal_trap(SIGTERM);
    }
    loadstr(buf.data(),&p,'\n',buf.size());
    if (((*chrex)->ext_data = strdup(buf.data())) == nullptr) {
      log_print("Insufficient memory for chr extension data\n");
      signal_trap(SIGTERM);
    }

#ifdef DEBUG
     snprintf(debug_buf,sizeof(debug_buf),"add ChrEx:%s",buf.data());
     log_print(debug_buf);
#endif

    (*chrex)->next = nullptr;
    return(&((*chrex)->next));
}

void erase_chrex(CHRExData *chrex)
{
    CHRExData *n;

    while(chrex != nullptr) {

#ifdef DEBUG
     snprintf(debug_buf,sizeof(debug_buf),"del ChrEx:%s",chrex->ext_data);
     log_print(debug_buf);
#endif

        free(chrex->ext_data);
        n = chrex->next;
        free(chrex);
        chrex = n;
    }
}

/* add 20030726 */
int  init_cd(int n)
{
    int i;
    int **p;

    p = cd[n].skill;
    std::memset(&(cd[n]),'\0',sizeof(cd[n]));
    cd[n].skill = p;

      cd[n].formatA = nullptr;
      cd[n].by = cd[n].bx = -1;
      for (i = 0; i < MAX_CN; i++)
  cd[n].cn[i] = 0;
      cd[n].moral = 99;
      cd[n].st = ST_COMMAND;
      cd[n].wt = 0;
      cd[n].anmf = 0;
      cd[n].m_wait = cd[n].m_wait_c = 0;
      cd[n].s_wait = cd[n].s_wait_c = 0;
      cd[n].modef = MD_BOARD;
      cd[n].undetect = ID_NONE;
      cd[n].innate = IA_NONE;
      cd[n].wd = WD_FORE;
      cd[n].angry = -1;
      cd[n].damaged = -1;
      cd[n].cooldown = 0;
      cd[n].trace = -1;
      cd[n].afn = cd[n].dfn = cd[n].sfn = FN_NONE;
      cd[n].parse = 0; /* command parse off added by Qajin 19991203 */
      cd[n].sg = 0;
      /* キャラクターのパラメータ初期化はここに追加 */
      /* 初期化もれのないよう */
      CN_ALAG(n) = 0;
      cd[n].auth = nullptr; /* added by Qajin 19991023 */
      cd[n].party = 0; /* add 20020822 */
      cd[n].pflag = 0; /* add 20000804 */
      skill_clear(n); /* add for skills 20020307 */

      for (i=0 ; i < MAX_MAGICS; i++) {
  cd[n].md[i].spell = nullptr;
  cd[n].md[i].effect = EF_NONE;
  cd[n].md[i].fn = FN_NONE;
  cd[n].md[i].is_inherence = 0;
  cd[n].md[i].extension = nullptr;
      }
      for (i=0 ; i < MAX_ITEMS; i++)
  delete_item(&(cd[n].id[i]));
      cd[n].eye = 1;
      for (i=0; i<MAX_TALKS; i++) {
  cd[n].td[i].type = TT_NONE;
  cd[n].td[i].string = nullptr;
      }

    return (0);
}


int    load_cd(int n, int fl)
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 32> name;
  char  *p;
  int    f;
  int    i, j, k;
  CHRExData **chrex;

  for (f = 0; f < 2; f++) {
#ifndef EPHI_FastBind_CD_NUM
    if ((fp = efopen(cfile[f], "rb")) == nullptr)
#else
    for(i = j = 0; i < 16 && cd[n].uid[i] != '\0' && cd[n].uid[i] != ' '; i++)
      j += (short)(int)cd[n].uid[i];
    j = j % EPHI_FastBind_CD_NUM;
    snprintf(name.data(),name.size(),cfile[f],j);
    if ((fp = efopen(name.data(), "rb")) == nullptr)
#endif
      return (0);
    while (1) {
      if (efgets(buf.data(), 255, fp) == nullptr)
  break;
      if (*buf.data() != ':')
  continue;            /* change for data broken 20000929 */
      for (i = 0, p = buf.data() + 1; *p && cd[n].uid[i] && *p == cd[n].uid[i]; i++, p++);
      if (cd[n].uid[i] || !fisspace(*p)) {
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() == '.')
      break;
  }
  continue;
      }
      for (i = 1; i < MAX_NTTYS; i++)
  if (i != n && (cd[i].type || (cd[i].st > ST_TRANSSELECT && cd[i].st <= ST_RECEIVECOMPLETE))
      && !std::strcmp(cd[n].uid, cd[i].uid))
    break;
      if (i < MAX_NTTYS) {
  efclose(fp);
  return (-1);
      }
#ifdef DEBUG
      snprintf(debug_buf,sizeof(debug_buf),"load cd[%d]",n);
      log_print(debug_buf);
#endif
      SKIP_SPACE(p);
      for (i = 0; i < 31 && (!fisspace(*p) || !fisspace(*(p + 1)));
     name[i++] = *(p++));
      for (; i <= 31; name[i++] = 0);
      cd[n].name = findstr(name.data());
      cd[n].cg = cd[n].ocg = nullptr /*findstr("human")*/;
      cd[n].lv[LV_FIGHTER] = decimal_load(4, &p);
      cd[n].lv[LV_WIZARD] = decimal_load(4, &p);
      cd[n].lv[LV_MERCHANT] = decimal_load(4, &p);
      cd[n].lv[LV_CREATER] = decimal_load(4, &p);
      cd[n].align = decimal_load(2, &p);
      cd[n].mhp = decimal_load(4, &p);
      cd[n].hp = decimal_load(4, &p);
      cd[n].mmp = decimal_load(4, &p);
      cd[n].mp = decimal_load(4, &p);
      cd[n].gp = decimal_load(4, &p);
      if (f == 1 && fl) {
  cd[n].gp = 0;
/*        tax -= cd[n].gp;*/
      }
      cd[n].exp = decimal_load(8, &p);
      cd[n].x = decimal_load(3, &p);
      cd[n].y = decimal_load(3, &p);
      cd[n].d = decimal_load(1, &p);
      cd[n].last = hexadecimal_load(8, &p);
      if (offline_exp && cd[n].exp < 99999999) {
        cd[n].exp += (time(nullptr) - cd[n].last) / 60;
        if (cd[n].exp >= 99999999)
          cd[n].exp = 99999998;
      }
      SKIP_SPACE(p);
      CPYSTR(cd[n].adr, p);
      for (i = 0; i < sizeof(cd[n].adr)-1 && cd[n].adr[i] != '\n'; i++);
      for (; i <= sizeof(cd[n].adr)-1; cd[n].adr[i++] = 0);
      /* add for suport format-A 20000906 */
      cd[n].formatA = nullptr;
      if (cd[n].adr[0] != '\0' && std::strstr(cd[n].adr,"@") == nullptr) {
        cd[n].formatA = findstr(cd[n].adr);
        cd[n].adr[0] = '\0';
      }
      if (cd[n].x < 1 || cd[n].x >= MAX_X - 1 || cd[n].y < 1 || cd[n].y >= MAX_Y - 1)
  cd[n].x = cd[n].y = 1;
      cd[n].by = cd[n].bx = -1;
      cd[n].ix = cd[n].x;
      cd[n].iy = cd[n].y;
      for (i = 0; i < MAX_CN; i++)
  cd[n].cn[i] = 0;
      cd[n].moral = 99;
      cd[n].st = ST_COMMAND;
      cd[n].wt = 0;
      cd[n].anmf = 0;
      cd[n].m_wait = cd[n].m_wait_c = 0;
      cd[n].s_wait = cd[n].s_wait_c = 0;
      cd[n].modef = MD_BOARD;
      cd[n].undetect = ID_NONE;
      cd[n].innate = IA_NONE;
      cd[n].wd = WD_FORE;
      cd[n].angry = -1;
      cd[n].damaged = -1;
      cd[n].cooldown = 0;
      cd[n].trace = -1;
      cd[n].afn = cd[n].dfn = cd[n].sfn = FN_NONE;
      cd[n].parse = 0; /* command parse off added by Qajin 19991203 */
      cd[n].sg = 0;
      /* キャラクターのパラメータ初期化はここに追加 */
      /* 初期化もれのないよう */
      CN_ALAG(n) = 0;
      cd[n].auth = nullptr; /* added by Qajin 19991023 */
      cd[n].party = 0; /* add 20020822 */
      cd[n].pflag = 0; /* add 20000804 */
      chrex = &(cd[n].extension);
      skill_clear(n); /* add for skills 20020307 */
      for (i = 0, j = 0;;) {
  if (efgets(buf.data(), 255, fp) == nullptr)
    break;
  if (*buf.data() != ':')
    break;
  p = buf.data() + 1;
  switch (*(p++)) {
  case 'M':
    if (i >= MAX_MAGICS)
      break;
    SKIP_SPACE(p);
    load_spell(&(cd[n].md[i]), p);
    if(cd[n].exp < 99999999)
      cd[n].md[i].is_inherence = 0;
    i++;
    break;
  case 'I':
    if (f == 1)   /* phi.bd musi */
      break;
    if (j >= MAX_ITEMS)
      break;
    SKIP_SPACE(p);
    if (load_item(&(cd[n].id[j]), p) != 1) {
    }
    j++;
    break;
  case 'G': /* 自キャラグラ設定 added by Qajin 19991224 */
  case 'g':
      SKIP_SPACE(p);
      loadstr(name.data(),&p,'\n',16);
    for(k=15;k>0;k--)           /* bug fix 20000204 */
      if(std::isspace(name[k]))
        name[k]=0;
      if (buf[1] == 'G')
        cd[n].cg = findstr(name.data());
      else
        cd[n].ocg = findstr(name.data());
    break;
  case 'C': /* コンディション復帰 */
      SKIP_SPACE(p);
      k = decimal_load(2,&p);
      sscanf(p,"%i",&(cd[n].cn[k]));
      break;
  case 'A': /* 新メールアドレス 20000906 */
      SKIP_SPACE(p);
      loadstr(cd[n].adr,&p,'\n',sizeof(cd[n].adr)-1);
      break;
  case 'S': /* SKILL 20020307 */
      load_skills(n,p);
      break;
  default:
      p--;
  case '?': /* 認識不能拡張データ */
      chrex = load_chrex(chrex,p);
  }
      }
      for (; i < MAX_MAGICS; i++) {
  cd[n].md[i].spell = nullptr;
  cd[n].md[i].effect = EF_NONE;
  cd[n].md[i].fn = FN_NONE;
  cd[n].md[i].is_inherence = 0;
  cd[n].md[i].extension = nullptr;
      }
      for (; j < MAX_ITEMS; j++)
  delete_item(&(cd[n].id[j]));
      cd[n].eye = 1;
      for (i=0; i<MAX_TALKS; i++) {
  cd[n].td[i].type = TT_NONE;
  cd[n].td[i].string = nullptr;
      }

      efclose(fp);
#ifdef DEBUG
      snprintf(debug_buf,sizeof(debug_buf),"load cd[%d] complete",n);
      log_print(debug_buf);
#endif

      load_sav(n); /* add for saved flags 20010622 */

    /* マスターキャラの権限設定 added by Qajin 19991023 */
    /* exp max以外のmasterキャラ可能に 20010226 */
/*    if (cd[n].exp == 99999999) {*/
          switch (load_authority(n)) {
            /* エラー処理追加 added by Qajin 19991103 */
            case 1:
              dmcheck_write(n, DMM_MASTER_ACCESS_ERR1);
              break;
            case 2:
              dmcheck_write(n, DMM_MASTER_ACCESS_ERR2);
              break;
            case 0:
              break;
          }
/*    }*/
      return (1);
    }
    efclose(fp);
  }
  return (0);
}

int    save_cd(int n, int f)
{
  FILE  *rfp, *wfp;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char   buf[256];
  char  *p;
  int    i;
#ifdef EPHI_FastBind_CD_NUM
  int    hash;
#endif
  CHRExData *chrex;

  if (n >= MAX_NTTYS)
    return (1);
  if (f && (cd[n].st == ST_CONNECTSERVER || cd[n].st == ST_CONNECTSERVER_ ||
      cd[n].st == ST_SENDCHR || cd[n].st ==  ST_RESERVE)) /* bug fix by Qajin 19991230/20000710/20000728 */
    return (1);
#ifndef EPHI_FastBind_CD_NUM
  if ((rfp = efopen(cfile[0], "rb")) == nullptr)
#else
  hash = 0;
  for(i = 0; i < 16 && cd[n].uid[i] != '\0' && cd[n].uid[i] != ' '; i++)
    hash += (short)(int)cd[n].uid[i];
  hash = hash % EPHI_FastBind_CD_NUM;
  snprintf(buf,sizeof(buf),cfile[0],hash);
  if ((rfp = efopen(buf, "rb")) == nullptr)
#endif
    return (0);
  if ((wfp = efopen("phi.cd.bak", "wb")) == nullptr) {
    efclose(rfp);
    return (0);
  }
  while (1) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (*buf != ':')
      break;
    fputs(buf, wfp);
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
  break;
      fputs(buf, wfp);
      if (*buf == '.')
  break;
    }
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  efclose(wfp);
  efclose(rfp);
  if ((rfp = efopen("phi.cd.bak", "rb")) == nullptr)
    return (0);
#ifndef EPHI_FastBind_CD_NUM
  if ((wfp = efopen(cfile[0], "wb")) == nullptr)
#else
  snprintf(buf,sizeof(buf),cfile[0],hash);
  if ((wfp = efopen(buf, "wb")) == nullptr)
#endif
  {
    efclose(rfp);
    return (0);
  }
  while (1) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (*buf != ':')
      break;
    for (i = 0, p = buf + 1; *p && cd[n].uid[i] && *p == cd[n].uid[i]; i++, p++);
    if (!cd[n].uid[i] && fisspace(*p)) {
      while (1) {
  if (efgets(buf, 255, rfp) == nullptr)
    break;
  if (*buf == '.')
    break;
      }
      continue;
    }
    fputs(buf, wfp);
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
  break;
      fputs(buf, wfp);
      if (*buf == '.')
  break;
    }
  }
  if (f) {
    cd[n].last = time(nullptr);
    if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_CALL) {
      cd[n].ix = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].x;
      cd[n].iy = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].y;
    } else {
      cd[n].ix = cd[n].x;
      cd[n].iy = cd[n].y;
    }
    snprintf(buf, sizeof(buf),":%-15s %-31s %04i %04i %04i %04i %02i %04li %04li %04li %04li %04i %08i %03i %03i %01i %08lx %s\n"
      ,cd[n].uid, cd[n].name
      ,cd[n].lv[LV_FIGHTER], cd[n].lv[LV_WIZARD]
      ,cd[n].lv[LV_MERCHANT], cd[n].lv[LV_CREATER]
      ,cd[n].align, cd[n].mhp, cd[n].hp, cd[n].mmp, cd[n].mp
      ,cd[n].gp, cd[n].exp, cd[n].ix, cd[n].iy, cd[n].d
      ,cd[n].last, (cd[n].formatA != nullptr ? cd[n].formatA : ""));
    fputs(buf, wfp);
    for (i = 0; i < MAX_MAGICS; i++) {
      if (cd[n].md[i].effect == EF_NONE)
  continue;
      snprintf(buf, sizeof(buf),":M ");
      save_spell(&(cd[n].md[i]), buf + 2, sizeof(buf)-2);
      fputs(buf, wfp);
    }
    for (i = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
  continue;
      snprintf(buf, sizeof(buf),":I ");
      save_item(&(cd[n].id[i]), buf + 2, sizeof(buf)-2);
      fputs(buf, wfp);
    }
    if (cd[n].cg != nullptr) { /* 自キャラグラ保存 added by Qajin 19991224 */
        snprintf(buf,sizeof(buf),":G %-32.32s\n",cd[n].cg);
        fputs(buf,wfp);
    }
    if (cd[n].ocg != nullptr) {
        snprintf(buf,sizeof(buf),":g %-32.32s\n",cd[n].ocg);
        fputs(buf,wfp);
    }
    /* add for save condition :-P 20000728 */
    for (i = 0; i <= NORMAL_CN; i++) {
      if (std::abs(cd[n].cn[i])>=0x10000 || (save_condition && cd[n].cn[i])) { /* change for Pygmalion :-P */
        snprintf(buf, sizeof(buf),":C %02i %i\n",i,cd[n].cn[i]);
        fputs(buf,wfp);
      }
    }
    /* new mail address for suport format-A 20000906 change 0928 */
    if (std::strlen(cd[n].adr) > 0) {
      snprintf(buf, sizeof(buf),":A %s\n", cd[n].adr);
      fputs(buf,wfp);
    }
    /* add for skills 20020307 */
    save_skills_cd(n,wfp);
    /* add for Chr extension data 20000929 */
    chrex = cd[n].extension;
    while (chrex != nullptr) {
        snprintf(buf, sizeof(buf),":?%s\n",chrex->ext_data);
        fputs(buf,wfp);
        chrex = chrex->next;
    }
    CPYSTR(buf, ".\n");
    fputs(buf, wfp);
  }
/*  strcpy(buf, ".\n");
  fputs(buf, wfp);*/
  efclose(wfp);
  efclose(rfp);
  return (1);
}

int    backup_cd(int n)
{
  FILE  *rfp, *wfp;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char   buf[256];
  char  *p;
  int    i, j;
  CHRExData *chrex;

  if (n >= MAX_NTTYS)
    return (1);
  if (cd[n].st == ST_CONNECTSERVER || cd[n].st == ST_CONNECTSERVER_ ) /* bug fix by Qajin 19991230 */
    return (1);
  if ((rfp = efopen(cfile[1], "rb")) == nullptr)
    return (0);
  if ((wfp = efopen("phi.bd.bak", "wb")) == nullptr) {
    efclose(rfp);
    return (0);
  }
  while (1) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (*buf != ':')
      break;
    fputs(buf, wfp);
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
  break;
      fputs(buf, wfp);
      if (*buf == '.')
  break;
    }
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  efclose(wfp);
  efclose(rfp);
  if ((rfp = efopen("phi.bd.bak", "rb")) == nullptr)
    return (0);
  if ((wfp = efopen(cfile[1], "wb")) == nullptr) {
    efclose(rfp);
    return (0);
  }
  cd[n].last = time(nullptr);
  if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_CALL) {
    cd[n].ix = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].x;
    cd[n].iy = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].y;
  } else {
    cd[n].ix = cd[n].x;
    cd[n].iy = cd[n].y;
  }
  snprintf(buf,sizeof(buf), ":%-15s %-31s %04i %04i %04i %04i %02i %04li %04li %04li %04li %04i %08i %03i %03i %01i %08lx %s\n"
    ,cd[n].uid, cd[n].name
    ,cd[n].lv[LV_FIGHTER], cd[n].lv[LV_WIZARD]
    ,cd[n].lv[LV_MERCHANT], cd[n].lv[LV_CREATER]
    ,cd[n].align, cd[n].mhp, cd[n].hp, cd[n].mmp, cd[n].mp
    ,cd[n].gp, cd[n].exp, cd[n].ix, cd[n].iy, cd[n].d
    ,cd[n].last, cd[n].formatA != nullptr ? cd[n].formatA : "");
  fputs(buf, wfp);
  for (i = 0; i < MAX_MAGICS; i++) {
    if (cd[n].md[i].effect == EF_NONE)
      continue;
    snprintf(buf, sizeof(buf),":M ");
    save_spell(&(cd[n].md[i]), buf + 2, sizeof(buf)-2);
    fputs(buf, wfp);
  }
/* 悪用防止の為、一時停止 u5 de kaihou */
  for (i = 0; i < MAX_ITEMS; i++) {
    if (cd[n].id[i].type == DT_NONE)
      continue;
    snprintf(buf, sizeof(buf),":I ");
    save_item(&(cd[n].id[i]), buf + 2, sizeof(buf)-2);
    fputs(buf, wfp);
  }
/**/
    if (cd[n].cg != nullptr) { /* 自キャラグラ保存 added by Qajin 19991224 */
        snprintf(buf,sizeof(buf),":G %-32.32s\n",cd[n].cg);
        fputs(buf,wfp);
    }
    if (cd[n].ocg != nullptr) {
        snprintf(buf,sizeof(buf),":g %-32.32s\n",cd[n].ocg);
        fputs(buf,wfp);
    }
  /* new mail address for suport format-A 20000906 change 0928 */
  if (std::strlen(cd[n].adr) > 0) {
    snprintf(buf, sizeof(buf),":A %s\n", cd[n].adr);
    fputs(buf, wfp);
  }
  /* add for skills 20020307 */
  save_skills_cd(n,wfp);
  /* add for Chr extension data 20000929 */
  chrex = cd[n].extension;
  while (chrex != nullptr) {
    snprintf(buf, sizeof(buf),":?%s\n",chrex->ext_data);
    fputs(buf,wfp);
    chrex = chrex->next;
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  for (i = 1; i < C_BACKUP; i++) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (*buf != ':')
      break;
    for (j = 0, p = buf + 1; *p && cd[n].uid[j] && *p == cd[n].uid[j]; j++, p++);
    if (!cd[n].uid[j] && fisspace(*p)) {
      while (1) {
  if (efgets(buf, 255, rfp) == nullptr)
    break;
  if (*buf == '.')
    break;
      }
      i--;
      continue;
    }
    fputs(buf, wfp);
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
  break;
      fputs(buf, wfp);
      if (*buf == '.')
  break;
    }
  }
/*  strcpy(buf, ".\n");
  fputs(buf, wfp);*/
  efclose(wfp);
  efclose(rfp);
  return (1);
}

int    get_cd(int n, char *data, char *ip)
/* 他サーバから送られてきた文字列データを元に、*/
/* キャラクタを構築する。                      */
/* 返却値 0(構築完成),-1(エラー),1(構築途中),2(shift-gate で拒否) */
/* 3(master入国拒否 add 20000725 */
{
/*  static int sum; delete for multi chr in 20000929 */

  std::array<char, 256> buf;
  std::array<char, 32> name, password;
  char  *p;
  long    i;
  CHRExData **chrex;

  p = data;
  if (*p == 'C') {
    cd[n].vf = 0;
    p += 4;
    SKIP_SPACE(p);
    for (i = 0; i < 31 && (!fisspace(*p) || !fisspace(*(p + 1))); name[i++] = *(p++));
    for (; i <= 31; name[i++] = 0);
    cd[n].name = findstr(name.data());
    cd[n].cg = cd[n].ocg = nullptr /*findstr("human")*/;
    /* エラーチェック大量に(笑)追加 added by Qajin 19991119 */
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].lv[LV_FIGHTER] = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].lv[LV_WIZARD] = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].lv[LV_MERCHANT] = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].lv[LV_CREATER] = i;
    i = decimal_load(2, &p);
    if (i < 0) return (-1);
    cd[n].align = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].mhp = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].hp = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].mmp = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].mp = i;
    i = decimal_load(4, &p);
    if (i < 0) return (-1);
    cd[n].gp = i;
    i = decimal_load(8, &p);
    if (i < 0) return (-1);
    if (i >= 99999999) return (-3); /* 今頃追加… ;-P 20000725 */
    cd[n].exp = i;
    i = decimal_load(3, &p);
    if (i < 0) return (-1);
    cd[n].x = i;
    i = decimal_load(3, &p);
    if (i < 0) return (-1);
    cd[n].y = i;
    i = decimal_load(1, &p);
    if (i < 0) return (-1);
    cd[n].d = i;
    i = hexadecimal_load(8, &p);
    if (i < 0) return (-1);
    cd[n].last = i;
    SKIP_SPACE(p);
    CPYSTR(cd[n].adr, p);
    for (i = 0; i < sizeof(cd[n].adr)-1 && cd[n].adr[i] != '\n'; i++);
    for (; i <= sizeof(cd[n].adr)-1; cd[n].adr[i++] = 0);
    /* for suport format-A 20000906 */
    cd[n].formatA = nullptr;
    if (cd[n].adr[0] != '\0' && !std::strstr(cd[n].adr,"@")) {
        cd[n].formatA = findstr(cd[n].adr);
        cd[n].adr[0] = '\0';
    }
    cd[n].str[0] = '\0';
    if (cd[n].x < 1 || cd[n].x >= MAX_X - 1 || cd[n].y < 1 || cd[n].y >= MAX_Y - 1)
      cd[n].x = cd[n].y = 1;
    cd[n].by = cd[n].bx = -1;
    cd[n].ix = cd[n].x;
    cd[n].iy = cd[n].y;
    for (i = 0; i < MAX_CN; i++)
      cd[n].cn[i] = 0;
    cd[n].moral = 99;
    cd[n].anmf = 0;
    cd[n].m_wait = cd[n].m_wait_c = 0;
    cd[n].s_wait = cd[n].s_wait_c = 0;
    cd[n].modef = MD_BOARD;
    cd[n].undetect = ID_NONE;
    cd[n].innate = IA_NONE;
    cd[n].wd = WD_FORE;
    cd[n].angry = -1;
    cd[n].damaged = -1;
    cd[n].cooldown = 0;
    cd[n].trace = -1;
    cd[n].afn = cd[n].dfn = cd[n].sfn = FN_NONE;
    cd[n].sg = 0;
    cd[n].auth = nullptr; /* 念の為… add 20000719 */
    cd[n].party = 0; /* add 20020822 */
    cd[n].pflag = 0; /* add for personal flag 20000804 */
    /* キャラクターのパラメータ初期化はここに追加 */
    /* 初期化もれのないよう */
    for (i = 0; i < MAX_MAGICS; i++) {
      cd[n].md[i].spell = nullptr;
      cd[n].md[i].effect = EF_NONE;
      cd[n].md[i].fn = FN_NONE;
      cd[n].md[i].is_inherence = 0;
    }
    for (i = 0; i < MAX_ITEMS; i++)
      delete_item(&(cd[n].id[i]));
    for (i = 0; i < MAX_TALKS; i++){
      cd[n].td[i].type =0;
      cd[n].td[i].string = nullptr;
    }
    skill_clear(n); /* add for skills 20020507 */
    for (i = 0; *(data + i) != 0; i++)
      cd[n].vf += *(data + i);
    return (1);
  } else if (*p == 'M') {
    p++;
    i = decimal_load(2, &p);
    /* error check added by Qajin 19991119 add upper 20000930 */
    if (i < 0 || i >= MAX_MAGICS) return (-1);
    p += 2;
    load_spell(&(cd[n].md[i]), p);
    cd[n].md[i].is_inherence = 0; /* 念のため */
    for (i = 0; *(data + i) != 0; i++)
      cd[n].vf += *(data + i);
    return (1);
  } else if (*p == 'I') {
    p++;
    i = decimal_load(2, &p);
    /* error check added by Qajin 19991119 add upper 20000930 */
    if (i < 0 || i >= MAX_ITEMS) return (-1);
    p += 2;
    load_item(&(cd[n].id[i]), p);
    for (i = 0; *(data + i) != 0; i++)
      cd[n].vf += *(data + i);
    return (1);
  } else if (*p == 'G') { /* キャラグラデータ added by Qajin 19991224 */
    p++;
    i = decimal_load(2,&p);
    if (i < 0) return (-1);
    p+=2;
    loadstr(name.data(),&p,'\n',15);
    cd[n].cg = findstr(name.data());
    return (1);
  } else if (*p == 'g') { /* キャラグラデータ 20030911 */
    SKIP_TO_SPACE(p);
    SKIP_SPACE(p);
    loadstr(name.data(),&p,'\n',15);
    cd[n].ocg = findstr(name.data());
    return (1);
  } else if (*p == 'A') {
    p++;
    i = decimal_load(2,&p);
    if (i < 0) return (-1);
    p++;
    loadstr(cd[n].adr,&p,'\n',47);
    return (1);
  /* v5z extend 20020307 */
  } else if (!std::strncmp(p,"VX:S",4) && !load_skills(n,p+4)) {
    return (1);
  } else if (*p == 'L' && *(p + 1) == ':') {
    loadstr(cd[n].str, &p, '\n', 80);
/********** 古いサーバでチェックサムが合わなくなるので、計算しない
    for(i=0;*(data+i)!=0;i++)
      sum += *(data+i);
***********/
    return (1);
  } else if (*p == '.') {
    p++;
    i = hexadecimal_load(4, &p);
    cd[n].vf &= 0xffff;
    if (cd[n].vf != i || i < 0)
      return (-1);
    p = cd[n].str;
    name[0] = ' ';
    name[1] = '\0';
    password[0] = ' ';
    password[1] = '\0';
    if (*p == 'L' && *(p + 1) == ':') {
      p += 2;
      loadstr(name.data(), &p, DSPACE, 32);
      SKIP_SPACE(p);
    }
    if (*p == 'P' && *(p + 1) == ':') {
      p += 2;
      loadstr(password.data(), &p, DSPACE, 32);
    }
    if (!shift_gate(&(cd[n].x), &(cd[n].y), authorize(password.data()), name.data(), ip))
      return (-2);
    if (cd[n].ix == cd[n].x && cd[n].iy == cd[n].y) {
      snprintf(buf.data(), buf.size(),"%s (%s)", name.data(), ip);
    } else {
      snprintf(buf.data(), buf.size(), "%s (%s) shift (%d,%d)->(%d,%d)"
        ,name.data(), ip
        ,cd[n].ix, cd[n].iy, cd[n].x, cd[n].y);
    }
    /* add for import check 20001128 */
    if(!export_item_check(n, cd[n].x, cd[n].y, name.data(), ip,'g'))
      return (-1);
    CPYSTR(cd[n].str, buf.data());
    cd[n].ix = cd[n].x;
    cd[n].iy = cd[n].y;
  } else {
    /* add for Chr extension data 20000929 */
    chrex = &(cd[n].extension);
    while (*chrex != nullptr) {
        chrex = &((*chrex)->next);
    }
    chrex = load_chrex(chrex,p);
    return (1);
  }
  cd[n].weq = nullptr;
  cd[n].afn = FN_NONE;
  cd[n].wsep = 0;
  cd[n].aeq = nullptr;
  cd[n].dfn = FN_NONE;
  cd[n].asep = 0;
  cd[n].oeq = nullptr;
  cd[n].at = AT_NONE;
  cd[n].osep = 0;
  cd[n].eye = 1;
  CN_ALAG(n) = 0;
  cd[n].vf = 0;
  cd[n].st = ST_RECEIVECOMPLETE; /* add for checking abnomal process sequence 20000725 */
  return (0);
}

void   put_cd(int n, int m, char *p)
{
  int    sum;
  std::array<char, 256> buf;
  int    i, j;
  CHRExData *chrex;

  std::memcpy(&cd[0], &cd[n], sizeof (CHRData));
  cd[0].fd = -1;
  cd[0].vf = 0;
  cd[0].st = 0;
  cd[0].wt = 0;
  cd[0].type = CT_NOEXIST;
  exportcir(0);

  sum = 0;
  snprintf(buf.data(), buf.size(),"#chr-D %-15s C00:%-31s %04i %04i %04i %04i %02i %04li %04li %04li %04li %04i %08i %03i %03i %01i %08lx %s\n"
    ,cd[0].uid, cd[0].name
    ,cd[0].lv[LV_FIGHTER], cd[0].lv[LV_WIZARD]
    ,cd[0].lv[LV_MERCHANT], cd[0].lv[LV_CREATER]
    ,cd[0].align, cd[0].mhp, cd[0].hp, cd[0].mmp, cd[0].mp
    ,cd[0].gp, cd[0].exp, cd[0].ix, cd[0].iy, cd[0].d
    ,cd[0].last, (cd[0].formatA != nullptr ? cd[0].formatA : ""));
  for (j = 23; buf[j] != '\n'; j++)
    sum += buf[j];
  check_write(m, buf.data());
  for (i = 0; i < MAX_MAGICS; i++) {
    if (cd[0].md[i].effect == EF_NONE)
      continue;
    snprintf(buf.data(), buf.size(),"#chr-D %-15s M%02i:", cd[0].uid, i);
    save_spell(&(cd[0].md[i]), buf.data() + 27,buf.size()-27);
    /* 転送されるキャラクタには inherent な magic は持ってないはず */
    for (j = 23; buf[j] != '\n'; j++)
      sum += buf[j];
    check_write(m, buf.data());
  }
  for (i = 0; i < MAX_ITEMS; i++) {
    if (cd[0].id[i].type == DT_NONE)
      continue;
    snprintf(buf.data(), buf.size(),"#chr-D %-15s I%02i:", cd[0].uid, i);
    save_item(&(cd[0].id[i]), buf.data() + 27, buf.size()-27);
    for (j = 23; buf[j] != '\n'; j++)
      sum += buf[j];
    check_write(m, buf.data());
  }
  if (cd[0].cg != nullptr) { /* キャラグラデータ送信 added by Qajin 19991224 */
    snprintf(buf.data(),buf.size(),"#chr-D %-15s G00: %-15s\n",cd[0].uid,cd[0].cg);
    check_write(m,buf.data());
  }
  if (cd[0].ocg != nullptr) { /* キャラグラデータ送信 retake 20030911 */
    snprintf(buf.data(),buf.size(),"#chr-D %-15s g %-15s\n",cd[0].uid,cd[0].ocg);
    check_write(m,buf.data());
  }
  /* add for format-A 20000906 chenge 0928 */
  if (std::strlen(cd[0].adr) > 0 && !std::strncmp(cd[0].adr,"/*@NP:",6)) {
    snprintf(buf.data(),buf.size(),"#chr-D %-15s A00:%s\n",cd[0].uid,cd[0].adr);
    check_write(m,buf.data());
  }
  /* add for skills 20020307 */
  put_skills(n,m);
  /* add for Chr extension data 20000929 */
  chrex = cd[0].extension;
  while (chrex != nullptr) {
    snprintf(buf.data(), buf.size(),"#chr-D %-15s %s\n",cd[0].uid,chrex->ext_data);
    check_write(m,buf.data());
    chrex = chrex->next;
  }
  cd[0].extension = nullptr;
  snprintf(buf.data(),buf.size(),"#chr-D %-15s L:%s  %s\n", cd[0].uid, land_name, p);
/********** 古いサーバでチェックサムが合わなくなるので、計算しない
  for(j=23;buf[j]!='\n';j++)
    sum+=buf[j];
***********/
  check_write(m, buf.data());
  sum &= 0xffff; /* add for check sum over 20000929 */
  snprintf(buf.data(), buf.size(),"#chr-D %-15s .%04x \n", cd[0].uid, sum);
  check_write(m, buf.data());
  cd[0].uid[0]='\0'; /* 念のため消去しておく */
  return;
}

/* add for saved flags 20010622 */
int    load_sav(int n)
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 32> name;
  char  *p;
  int    i, j, k;

  if (!flagsave)
    return (1);
  cd[n].pflag = 0;
  for (i=0;i<SVAR_MAX;i++)
    cd[n].svar[i] = 0;
#ifndef EPHI_FastBind_CD_NUM
    if ((fp = efopen(savfile, "rb")) == nullptr)
#else
    for(i = j = 0; i < 16 && cd[n].uid[i] != '\0' && cd[n].uid[i] != ' '; i++)
      j += (short)(int)cd[n].uid[i];
    j = j % EPHI_FastBind_CD_NUM;
    snprintf(name.data(),name.size(),savfile,j);
    if ((fp = efopen(name.data(), "rb")) == nullptr)
#endif
      return (0);
    while (1) {
      if (efgets(buf.data(), 255, fp) == nullptr)
  break;
      if (*buf.data() != ':')
  continue;
      for (i = 0, p = buf.data() + 1; *p && cd[n].uid[i] && *p == cd[n].uid[i]; i++, p++);
      if (cd[n].uid[i] || *p != '\n') {
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() == '.')
      break;
  }
  continue;
      }

#ifdef DEBUG
      snprintf(debug_buf,sizeof(debug_buf),"load sav[%d]",n);
      log_print(debug_buf);
#endif
      SKIP_SPACE(p);
      cd[n].pflag = 0;
      while(1) {
  if (efgets(buf.data(), 255, fp) == nullptr)
    break;
  if (*buf.data() != ':')
    break;
  p = buf.data() + 1;
  switch (*(p++)) {
  case 'F':
      SKIP_SPACE(p);
      for(i=0;i<SFLAG_MAX;i++) {
        if (*(p++) == '1') {
            cd[n].pflag |= (1 << (16+i));
        }
      }
      break;
  case 'V':
    SKIP_SPACE(p);
    i = hexadecimal_load_arbitrarily(&p);
    if (i >= 0 && i < SVAR_MAX) {
        cd[n].svar[i] = decimal_load_arbitrarily(&p);
    }
    break;
  default:
      break;
  }
      }

      efclose(fp);
#ifdef DEBUG
      snprintf(debug_buf,sizeof(debug_buf),"load sav[%d] complete",n);
      log_print(debug_buf);
#endif

      return (1);
    }
    efclose(fp);
  return (0);
}

int    save_sav(int n)
{
  FILE  *rfp, *wfp;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char   buf[256];
  char  *p;
  int    i;
#ifdef EPHI_FastBind_CD_NUM
  int    hash;
#endif

  if (!flagsave)
    return (1);
  if (n >= MAX_NTTYS)
    return (1);
  if (cd[n].st == ST_CONNECTSERVER || cd[n].st == ST_CONNECTSERVER_ ||
      cd[n].st == ST_SENDCHR || cd[n].st ==  ST_RESERVE)
    return (1);

  if ((wfp = efopen("phi.sav.bak", "wb")) == nullptr) {
    return (0);
  }
#ifndef EPHI_FastBind_CD_NUM
  if ((rfp = efopen(savfile, "rb")) != nullptr)
#else
  hash = 0;
  for(i = 0; i < 16 && cd[n].uid[i] != '\0' && cd[n].uid[i] != ' '; i++)
    hash += (short)(int)cd[n].uid[i];
  hash = hash % EPHI_FastBind_CD_NUM;
  snprintf(buf,sizeof(buf),savfile,hash);
  if ((rfp = efopen(buf, "rb")) != nullptr)
#endif
  {
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
        break;
      if (*buf != ':')
        break;
      fputs(buf, wfp);
      while (1) {
        if (efgets(buf, 255, rfp) == nullptr)
          break;
        fputs(buf, wfp);
        if (*buf == '.')
          break;
      }
    }
    efclose(rfp);
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  efclose(wfp);
  if ((rfp = efopen("phi.sav.bak", "rb")) == nullptr)
    return (0);
#ifndef EPHI_FastBind_CD_NUM
  if ((wfp = efopen(savfile, "wb")) == nullptr)
#else
  snprintf(buf,sizeof(buf),savfile,hash);
  if ((wfp = efopen(buf, "wb")) == nullptr)
#endif
  {
    efclose(rfp);
    return (0);
  }
  while (1) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (*buf != ':')
      break;
    for (i = 0, p = buf + 1; *p && cd[n].uid[i] && *p == cd[n].uid[i]; i++, p++);
    if (!cd[n].uid[i] && *p == '\n') {
      while (1) {
  if (efgets(buf, 255, rfp) == nullptr)
    break;
  if (*buf == '.')
    break;
      }
      continue;
    }
    fputs(buf, wfp);
    while (1) {
      if (efgets(buf, 255, rfp) == nullptr)
  break;
      fputs(buf, wfp);
      if (*buf == '.')
  break;
    }
  }
  for(i=0;i<SVAR_MAX && !(cd[n].svar[i]);i++);
  if (((cd[n].pflag) & 0xffff0000) || i < SVAR_MAX) {
    snprintf(buf, sizeof(buf),":%.15s\n",cd[n].uid);
    fputs(buf, wfp);

    if (cd[n].pflag & 0xffff0000) {
        CPYSTR(buf, ":F ");
        p = buf + 3;
        for(i=0;i<SFLAG_MAX;i++) {
            *(p++) = (cd[n].pflag & (1 << (16+i))) ? '1' : '0';
        }
        *p++ = '\n';
        *p++ = '\0';
        fputs(buf, wfp);
    }

    for(i=0;i<SVAR_MAX;i++) {
        if (cd[n].svar[i]) {
            snprintf(buf, sizeof(buf),":V %x %i\n",i,cd[n].svar[i]);
            fputs(buf, wfp);
        }
    }

    CPYSTR(buf, ".\n");

    fputs(buf, wfp);
  }
  efclose(wfp);
  efclose(rfp);
  return (1);
}

/* マスター権限ファイルの読み込み    */
/* added by Qajin 19991023           */
/* エラー処理の為返値を変更 19991103 */
int    load_authority(int n)
{
  FILE  *fp;
  std::array<char, 256> buf;
  int    i,j;
  char  *p;
  std::array<char, 16> ip;
  int    ter[257];

  if ((fp = efopen("phi.ad", "rb")) == nullptr) {
    /* fprintf(stderr, "Warning: Can not find Authority data file. [phi.ad] \n"); */
    /* phi.adが存在しない時 過去環境との共通化の為 全権を与える */
    /* exp max以外のmasterキャラ可能に 20010226 */
    if (cd[n].exp == 99999999) {
      cd[n].auth = (MasterAuth *)malloc(sizeof(MasterAuth));
      if (cd[n].auth == nullptr) {
         /* エラー処理追加 added by Qajin 19991103 */
         log_print("Warning : Can not allocate memory for master authority .");
         return(1);
      }
      cd[n].auth->main = MS_ALL;
      cd[n].auth->sub  = 0;
      cd[n].auth->territory = nullptr;
    }
    return(0);
  }

  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      continue;
    for (i = 0, p = buf.data() + 1; *p && cd[n].uid[i] && *p == cd[n].uid[i]; i++, p++);
    if (cd[n].uid[i] || !fisspace(*p)) {
      while (1) {
        if (efgets(buf.data(), 255, fp) == nullptr)
          break;
        if (*buf.data() == '.')
          break;
      }
      continue;
    }
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"load ad[%d]",n);
    log_print(debug_buf);
#endif

    SKIP_SPACE(p);

    /* access address check */
    loadstr(ip.data(),&p,' ',15);
    /* 読み込み時エラーチェック追加  by Qajin 19991030 */
    if (*ip.data() == '\0') {
      log_print("Warning : parse error at IP address in Phi.ad .");
      break;
    }
    if (!wildcmp(ip.data(),ip_address_check(n)) && !wildcmp(ip.data(),ip_domain_check(n))) {
    /* 警告メッセージ追加 by Qajin 19991030 */
      log_print("Warning : IP address refuse to authorize master.");
      break;
    }

    /* set authorities */
    cd[n].auth = (MasterAuth *)malloc(sizeof(MasterAuth));
    if (cd[n].auth == nullptr) {
      /* エラー処理追加 added by Qajin 19991103 */
      log_print("Warning : Can not allocate memory for master authority .");
      efclose(fp);
      return(1);
    }
    cd[n].auth->main = 0;
    cd[n].auth->sub  = 0;
    cd[n].auth->territory = nullptr;

    for(;*p && *p != '\n' && *p != '\0';p++) {
      if (*p == 'A') {
        cd[n].auth->main  = MS_ALL;
        break;
      }
      switch(*p) {
        case 'B':
          cd[n].auth->main |= MS_BAN;
          break;
        case 'K':
          cd[n].auth->main |= MS_KICK;
          break;
        case 'U':
          cd[n].auth->main |= MS_UNLOCK;
          break;
        case 'D':
          cd[n].auth->main |= MS_DELETE;
          break;
        case 'R':
          cd[n].auth->main |= MS_RESOURCE;
          break;
        case 'I':
          cd[n].auth->main |= MS_ITEM;
          break;
        case 'L':
          cd[n].auth->main |= MS_LOAD;
          break;
        case 'S':
          cd[n].auth->main |= MS_SHUTDOWN;
          break;
        case 'u':
          cd[n].auth->sub  |= MS_UNLOCK;
          break;
        case 'd':
          cd[n].auth->sub  |= MS_DELETE;
          break;
        case 'r':
          cd[n].auth->sub  |= MS_RESOURCE;
          break;
        /* add new authority 20010618 */
        case 'T':
          cd[n].auth->main |= MS_TRANS;
          break;
        case 'Q':
          cd[n].auth->main |= MS_QUEST;
          break;
      }
    }

    for (;;) {
      if (efgets(buf.data(), 255, fp) == nullptr)
        break;
      if (*buf.data() != ':')
        break;
      p = buf.data() + 1;
      switch (*(p++)) {
        case 'A':
          SKIP_SPACE(p);
          i = analyze_hexnumbers(ter,256,p);
          cd[n].auth->territory = (int *)malloc(sizeof(int)*(i+1));
          if (cd[n].auth->territory == nullptr) {
            /* エラー処理追加 added by Qajin 19991103 */
            log_print("Warning : Can not allocate memory for sub master territory .");
            efclose(fp);
            return (2);
          }
          ter[i] = -1;
          std::memcpy(cd[n].auth->territory,ter,sizeof(int)*(i+1));

#ifdef DEBUG
    for(j=0;j<i;j++) {
      snprintf(debug_buf,sizeof(debug_buf),"territory[%i]",cd[n].auth->territory[j]);
      log_print(debug_buf);
    }
#endif

          break;
      }
    }
    efclose(fp);
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"load ad[%d] complete",n);
    log_print(debug_buf);
#endif

    return (0);
  }
  efclose(fp);
  return (cd[n].exp == 99999999 ? 1 : 0);
}


/* 権限の判定                                */
/* authority は define の各フラグ            */
/* added by Qajin 19991024                   */
int    have_authority(int n,int authority,int x,int y)
{
    int i;
    int fwd;

    if (cd[n].auth == nullptr)
        return (0);

    if (((cd[n].auth->main) & authority) == authority) /* bug fix 20000727 */
           return (1);
    /* BAN,KICK,ITEM,LOAD,SHUTDOWN */
    else if (authority & (MS_BAN | MS_KICK | MS_ITEM | MS_LOAD |
                          MS_SHUTDOWN | MS_TRANS | MS_QUEST)) {
        dmcheck_write(n,DMM_NOAUTHORITY);
           return (0);
    }

    if (((cd[n].auth->sub) & authority) != authority ||
        cd[n].auth->territory == nullptr) {
        if (authority & MS_RESOURCE)
            dmcheck_write(n,DMM_NORSCAUTHORITY);
        return (0);
    }

    /* 自分の領土か */
    fwd = 0;
    for(i=0;i < 256 && (cd[n].auth->territory[i] >= 0) ;i++) { /* bug fix 20000701 */
        /* DELETE */
        if (cd[n].auth->territory[i] == fld[map[y][x].area].area)
           fwd |= MS_DELETE | MS_UNLOCK | MS_RESOURCE;
    }

    if ((authority & MS_RESOURCE) & !(fwd & MS_RESOURCE))
        dmcheck_write(n,DMM_NORSCAUTHORITY);

    return((authority & fwd) == authority);
}


/*----------------------------------
   クリーチャーデータの読み込み
----------------------------------*/
void   load_ed_o()
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 32> nm;
  char  *p;
  int    f;
  long   i, j;
  int    en;
  CHRSData sbuf[100];   /* データバッファ */
  ItemData id;
  MagicData md;
  TalkData td;
  AppData ad;

  if ((fp = efopen("phi.ed", "rb")) == nullptr) {
    fprintf(stderr, " Can not find enemy data file [phi.ed]. ");
    exit(1);
  }
  for (max_enemy = 0;; max_enemy++) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;      /* 読み込み */
    if (*buf.data() == '.')
      break;
    if (*buf.data() != ':'){
      max_enemy--;
      continue;
    }
    while (1) {
      if (efgets(buf.data(), 255, fp) == nullptr)
  break;
      if (*buf.data() == '.')
  break;
    }
  }
  efclose(fp);

#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"load_ed_o:number of kind of NPC is %d",max_enemy);
  log_print(debug_buf);
#endif

  ed_o = (CHROData *) malloc(sizeof (CHROData) * max_enemy);
  if (ed_o == nullptr) {
    log_print("Insufficient Memory\n");
    exit(0);
  }
  if ((fp = efopen("phi.ed", "rb")) == nullptr)
    return;
  en = 0;

    enmax = MAX_NTTYS;
  /* enmax は MAX_NTTYS + NPC出現可能最大数 と設定する。 */
  /* max_enemy は NPC の種類(出現最大数ではない)        */
  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;      /* 読み込み */
    if (*buf.data() == '.')
      break;      /* デリミタ */
    if (*buf.data() != ':')
      continue;     /* :以外はスキップ */
    for (i = 0, p = buf.data() + 1;
   i < 31 && (!fisspace(*p) || !fisspace(*(p + 1)));
   ed_o[en].name[i++] = *(p++));  /* 名前読み取り */
    for (; i <= 31; ed_o[en].name[i++] = '\0');
    ed_o[en].cg[0] = '\0';
    ed_o[en].nm = decimal_load(2, &p); /* 発生数 */
    ed_o[en].nr = 0;    /* 実発生数 */
    ed_o[en].type = decimal_load(2, &p) + 1;  /* タイプ */
    ed_o[en].align = decimal_load(2, &p); /* カルマ */
    ed_o[en].moral = decimal_load(2, &p); /* モラル */
    ed_o[en].sp = decimal_load(2, &p);  /* スピード */
    ed_o[en].flv = decimal_load(4, &p); /* レベル */
    ed_o[en].wlv = decimal_load(4, &p);
    ed_o[en].mhp = decimal_load(4, &p); /* HP */
    ed_o[en].mmp = decimal_load(4, &p); /* MP */
    ed_o[en].gp = decimal_load(4, &p);  /* 所持金 */
    ed_o[en].ix = decimal_load(3, &p);  /* 発生位置 */
    ed_o[en].iy = decimal_load(3, &p);
    ed_o[en].d = decimal_load(2, &p);
    ed_o[en].st = decimal_load(2, &p);  /* 発生時刻 */
    ed_o[en].et = decimal_load(2, &p);
    f = -1;
    ed_o[en].apc = 1;   /* デフォルト再発生条件 */
    ed_o[en].anm = 99;
    ed_o[en].apt = 1200;
    ed_o[en].undetect = ID_NONE;/* デフォルトIdentifyマスク */
    ed_o[en].innate = IA_NONE;  /* 生来の能力は通常はなし */
    ed_o[en].nhp = 1;   /* MaxHp = x1 */
    ed_o[en].nmp = 1;   /* MaxMp = x1 */
    ed_o[en].cooldown = 50;     /* 怒りが収まる割合 */
    ed_o[en].big = 0;   /* 2x2 の大きな敵ではない */
    ed_o[en].fdn = -1;     /* フラグを操作する発生・消滅数 */
    ed_o[en].fbn = MAX_NTTYS + MAX_ENEMIES;
    ed_o[en].fdead = ed_o[en].fborn = -1; /* 操作するフラグNo */

    i = 0;
    for (;;) {
      if (efgets(buf.data(), 255, fp) == nullptr)
  break;
      if (*buf.data() == '<') {
  if (!std::strncmp(buf.data() + 1, "select", 6))
    f = 0;
  continue;
      } else if (*buf.data() == '>') {
  sbuf[i - f].sf = f;
  f = -1;
  continue;
  /* <select ...> があると、最初の所持データの sf に */
  /* 選択対象の個数が入る。*/
      } else if (*buf.data() == '.') {
  break;
      } else if (*buf.data() != ':') {
  continue;
      } else if (buf[1] == 'R') {
  p = buf.data() + 2;
  ed_o[en].apc = decimal_load(2, &p); /* 再発生条件 */
  ed_o[en].apt = decimal_load(5, &p) * 10;
  if (ed_o[en].apc <= 1) {/* 再発生が始まる数 */
    ed_o[en].anm = decimal_load(2, &p);
  } else if (ed_o[en].apc == 3) { /* 召喚パラメータ */
    ed_o[en].apcode = decimal_load(2, &p);
    ed_o[en].aplvl = decimal_load(4, &p);
    ed_o[en].apdm = ed_o[en].apnm = nullptr;
    SKIP_SPACE(p);
    if (*p == '\"') { /* " */
      p++;
      loadstr(nm.data(), &p, '\"', 32);  /* 召喚アイテム名 " */
      ed_o[en].apnm = findstr(nm.data());
      SKIP_SPACE(p);
      if (*p == '\"') { /* " */
        p++;
        loadstr(nm.data(), &p, '\"', 32);  /* 消滅アイテム名 " */
        ed_o[en].apdm = findstr(nm.data());
      }
    }
  } else if (ed_o[en].apc == 4) { /* 絶滅危惧種 */
    ed_o[en].threaten = decimal_load(5, &p);
  }
  continue;
      } else if (buf[1] == 'G') { /* 表示グラフィック名取得 */
  p = buf.data() + 2;
  SKIP_SPACE(p);
  for (j = 0; j < 15 && *p && !(*p == '\n'); ed_o[en].cg[j++] = *(p++));
  for (; j <= 15; ed_o[en].cg[j++] = '\0');
  continue;
      } else if (buf[1] == 'A') { /* Innately attributes */
  p = buf.data() + 2;
  ed_o[en].innate = hexadecimal_load(4, &p);
  continue;
      } else if (buf[1] == 'O') { /* options */
  for (p = buf.data() + 2; *p && !(*p == '\n');)
    switch (*p++) {
    case 'H':
      ed_o[en].nhp = decimal_load(5, &p);
      break;
    case 'M':
      ed_o[en].nmp = decimal_load(5, &p);
      break;
    case 'B':   /* 2x2 の大きな敵 */
      ed_o[en].big = 1;
      break;
    }
  continue;
      } else if (buf[1] == 'F') { /* flag on add 20000522 */
  for (p = buf.data() + 2; *p && !(*p == '\n');)
    switch (*p++) {
    case 'A':
      ed_o[en].fbn = decimal_load(2, &p);
      ed_o[en].fborn = flagno_load(&p);
      break;
    case 'D':
      ed_o[en].fdn = decimal_load(2, &p);
      ed_o[en].fdead = flagno_load(&p);
      break;
    }
  continue;
      } else if (buf[1] == 'D') { /* undetective status  and Innately */
  for (p = buf.data() + 2; *p && !(*p == '\n');)
    switch (*p++) {
    case 'k':
      ed_o[en].undetect |= ID_KARMA;
      break;
    case 'a':
      ed_o[en].undetect |= ID_AFN;
      break;
    case 'd':
      ed_o[en].undetect |= ID_DFN;
      break;
    case 'H':
      ed_o[en].undetect |= ID_HP;
      break;
    case 'M':
      ed_o[en].undetect |= ID_MP;
      break;
    case 'S':
      ed_o[en].undetect |= ID_STATUS;
      break;
    case 's':
      ed_o[en].undetect |= ID_SPEED;
      break;
    case 'D':
      ed_o[en].undetect |= ID_DEFMRP;
      break;
    case 'A':
      ed_o[en].undetect |= ID_ATP;
      break;
    case 'L':
      ed_o[en].undetect |= ID_LEVEL;
      break;
    case 'n':
      ed_o[en].undetect |= ID_SIMPLE;
      break;
    case '*':
      ed_o[en].undetect |= ID_ALL;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      ed_o[en].innate = ed_o[en].innate * 10
        + *p - '0';
      break;    /* old style IA */
    }
  continue;
      } else if (buf[1] == 'C') {
  p = buf.data() + 2;
  ed_o[en].cooldown = decimal_load(2, &p);
  continue;
      }

      p = buf.data() + 1;
      switch (*(p++)) {
      case 'M':   /* 魔法データ読み取り */
  sbuf[i].flg = 'M';
  sbuf[i].sf = 0;
  sbuf[i].nm = decimal_load(2, &p);
  load_spell(&md, p);
  std::memcpy(&(sbuf[i].data.md), &md, sizeof(MagicData));
  break;
      case 'I':   /* アイテムデータ読み取り */
  sbuf[i].flg = 'I';
  sbuf[i].sf = 0;
  sbuf[i].nm = decimal_load(2, &p);
  load_item(&id, p);
  std::memcpy(&(sbuf[i].data.id), &id, sizeof(ItemData));
  break;
      case 'T':   /* 会話データ読み取り */
  sbuf[i].flg = 'T';
  sbuf[i].sf = 0;
  sbuf[i].nm = decimal_load(2, &p);
  load_talk(&td, p);
  std::memcpy(&(sbuf[i].data.td), &td, sizeof(TalkData));
  break;
      case 'S':   /* 召喚データ読み取り */
  sbuf[i].flg = 'S';
  sbuf[i].sf = 0;
  sbuf[i].nm = decimal_load(2, &p);
  load_app(&ad, p);
  std::memcpy(&(sbuf[i].data.ad), &ad, sizeof(AppData));
  break;
      case 'Z':   /* Dummy add 20000522 */
  sbuf[i].flg = 'Z';
  sbuf[i].sf = 0;
  sbuf[i].nm = decimal_load(2, &p);
  break;
      default:
  continue;
      }
      i++;
      if (f >= 0)
  f++;
    }
    for (j = 0; j < sizeof (CHRSData); j++) {
      *((char *) (&(sbuf[i])) + j) = '\0';
    }
    /*    sbuf[i].flg = '\0';  */
    i++;
    ed_o[en].sd = (CHRSData *) malloc(sizeof (CHRSData) * i);
    if (ed_o[en].sd == nullptr) {
      log_print("Insufficient Memory\n");
      exit(0);
    }
    std::memcpy(ed_o[en].sd, sbuf, sizeof (CHRSData) * i);
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"load_ed_o:set sd(size:%d) for ed_o[%d]", i, en);
    log_print(debug_buf);
#endif
      /* 定数モードの場合、出現する可能性のある全ての NPC を */
      /* 予め cd に割り振る。                              */
      for (i = 0; i < ed_o[en].nm; i++) { /* 発生条件のセット */
  if (enmax >= MAX_ENEMIES + MAX_NTTYS)
    break;
  cd[enmax].et = en;
  cd[enmax].wt = (fsps(80, ed_o[en].apt, 900000) >> 1);
  if (ed_o[en].apc <= 1 || ed_o[en].apc == 4) {
    cd[enmax++].ec = 1;
  } else {
    cd[enmax++].ec = 0;
  }
      }
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"load_ed_o:set ed_o[%d](name:%s)",
      en, ed_o[en].name);
    log_print(debug_buf);
#endif
    en++;
  }
  efclose(fp);
}

void    free_ed_o(){
  int n;

  for(n = MAX_NTTYS; n<enmax ;n++){
    if(cd[n].type!=CT_NOEXIST){
      clear_chr(n);
      tax+=cd[n].gp;
    }
    cd[n].type = CT_NOEXIST;
    cd[n].gp = 0;
  }

  for(n = 0; n<max_enemy ;n++)
    free(ed_o[n].sd);
  free(ed_o);
}

/*----------------------------------
   クリーチャー発生
----------------------------------*/
static int big_spawn_ok(int x, int y)
/* 2x2 の大きな敵の発生位置として、4マスとも地形が通れて
   キャラを埋め込めるか */
{
  int i, j, xx, yy;

  for (i = 0; i < 4; i++) {
    xx = x + BIG_OFF[i][0];
    yy = y + BIG_OFF[i][1];
    if (xx < 1 || xx >= MAX_X - 1 || yy < 1 || yy >= MAX_Y - 1)
      return (0);
    if (map[yy][xx].tip & (wall_for_beast | T_EWALL))
      return (0);
    if ((map[yy][xx].tip & TMASK) == T_BOX)
      return (0);
    /* 体が重なるため、4マスとも誰もいないこと (通常の敵と違い、
       空きスロットがあるだけでは不十分) */
    for (j = 0; j < MAX_CHRS_INONE; j++)
      if (map[yy][xx].chr[j])
        return (0);
  }
  return (1);
}

void   load_ed(int n)
{
  int    en;
  int    i, j, k, l, o, s;

  if (cd[n].et < 0)
    return;     /* 定数モードで発生種が未定義 */

  if (cd[n].ec == 0) {    /* 発生の不許可 */
    return;
  }
  cd[n].ec = 0;

  en = cd[n].et;

  i = ed_o[en].st;    /* 発生時間のチェック */
  j = ed_o[en].et;
  k = (timestr[11] - '0') * 10 + (timestr[12] - '0') + tslide;
  if (k > 23)
    k -= 24;
  if (i < j) {
    if (k < i) {
      cd[n].ec = 1;
      cd[n].wt = 600;
      return;     /* 定数モードは発生種が固定されているため時間外はお休みモード */
    }
    if (j < k) {
      cd[n].ec = 1;
      cd[n].wt = 600;
      return;
    }
  } else {
    if (k < i && j < k) {
      cd[n].ec = 1;
      cd[n].wt = 600;
      return;
    }
  }

  cd[n].name = ed_o[en].name; /* 名前の設定 */
  cd[n].cg = cd[n].ocg = ed_o[en].cg;
  cd[n].type = ed_o[en].type;
  cd[n].big = ed_o[en].big;
  cd[n].align = ed_o[en].align;
  cd[n].moral = ed_o[en].moral;
  cd[n].sp = ed_o[en].sp;
  cd[n].lv[LV_FIGHTER] = ed_o[en].flv + PER_DICE;
  if (cd[n].lv[LV_FIGHTER] > 9999)
    cd[n].lv[LV_FIGHTER] = 9999;
  cd[n].lv[LV_WIZARD] = ed_o[en].wlv + PER_DICE;
  if (cd[n].lv[LV_WIZARD] > 9999)
    cd[n].lv[LV_WIZARD] = 9999;
  cd[n].hp = cd[n].mhp = fsps(50, ed_o[en].mhp, 9999);
  if (!ggflag) {     /* MHP 5000 を境界値とした巨大化に関すること */
    if (ed_o[en].mhp < 5000 && cd[n].mhp >= 5000)
      cd[n].hp = cd[n].mhp = 4999;
    else if (ed_o[en].mhp >= 5000 && cd[n].mhp < 5000)
      cd[n].hp = cd[n].mhp = 5000;
  }
  cd[n].mp = cd[n].mmp = fsps(50, ed_o[en].mmp, 9999);

  if (ed_o[en].nhp > 0 && ed_o[en].nhp < 100000) {
    cd[n].mhp *= ed_o[en].nhp;
    cd[n].hp = cd[n].mhp;
  }
  if (ed_o[en].nmp > 0 && ed_o[en].nmp < 100000) {
    cd[n].mmp *= ed_o[en].nmp;
    cd[n].mp = cd[n].mmp;
  }
  cd[n].ix = ed_o[en].ix;
  cd[n].iy = ed_o[en].iy;
  cd[n].d = ed_o[en].d;
  for (i = 0; i < 100; i++) { /* 発生位置の決定 */
    cd[n].x = frand(cd[n].d * 2 + 1) - cd[n].d + cd[n].ix;
    cd[n].y = frand(cd[n].d * 2 + 1) - cd[n].d + cd[n].iy;
    if (cd[n].x >= 1 && cd[n].x < MAX_X - 1
  && cd[n].y >= 1 && cd[n].y < MAX_Y - 1
  && !(map[cd[n].y][cd[n].x].tip & (wall_for_beast | T_EWALL)) /* change for cureature barrier 20000621 */
  && (map[cd[n].y][cd[n].x].tip & TMASK) != T_BOX
  && (!chr_big(n) || big_spawn_ok(cd[n].x, cd[n].y))) {
    /* change for variable multi characters in one space 20000721 */
      for (j = 0; j < MAX_CHRS_INONE; j++)
        if (!(map[cd[n].y][cd[n].x].chr[j]))
          break;
      if (j >= MAX_CHRS_INONE)
        continue;
      /* 2x2 の大きな敵の体の上には発生しない (体が重なるため。
         空きスロットの有無とは別に、占有者が大きな敵なら不可) */
      for (j = 0; j < MAX_CHRS_INONE; j++) {
        int occ = map[cd[n].y][cd[n].x].chr[j] - 1;
        if (occ >= 0 && chr_big(occ))
          break;
      }
      if (j >= MAX_CHRS_INONE)
        break;
    }
  }
  if (i >= 100) {   /* エラー */
    cd[n].type = CT_NOEXIST;
    cd[n].ec = 1;
    cd[n].wt = 600;   /* 60秒後にリトライ */
    return;
  }
  cd[n].d = PER_DICE / 25;  /* 向き */
  cd[n].bx = cd[n].by = -1; /* 直前に踏んでたマスは未定義 */
  cd[n].ix = cd[n].x;   /* ビーコンの設置 */
  cd[n].iy = cd[n].y;
  i = j = k = o = s = 0;
  for (l = 0;; l++) {   /* 所持魔法、アイテム、会話 */
    if (ed_o[en].sd[l].flg == '\0')
      break;
    if ((o = ed_o[en].sd[l].sf) > 0) {  /* select に関する処理 */
      o--;
      while (PER_DICE > ed_o[en].sd[l].nm) {
  if (--o < 0)
    break;
  l++;
      }
      if (o < 0)
  continue;
    } else {      /* 発生確率 */
      if (PER_DICE > ed_o[en].sd[l].nm)
  continue;
    }
    switch (ed_o[en].sd[l].flg) {
    case 'M':     /* 魔法 */
      if (i >= MAX_MAGICS)
  break;
      std::memcpy(&(cd[n].md[i]), &(ed_o[en].sd[l].data.md), sizeof(MagicData));
      i++;
      break;
    case 'I':     /* アイテム */
      if (j >= MAX_ITEMS)
  break;
      std::memcpy(&(cd[n].id[j]), &(ed_o[en].sd[l].data.id), sizeof(ItemData));
      j++;
      break;
    case 'T':     /* 会話 */
      if (k >= MAX_TALKS)
  break;
      std::memcpy(&(cd[n].td[k]), &(ed_o[en].sd[l].data.td), sizeof(TalkData));
      k++;
      break;
    case 'A':     /* 召喚 */
      if (s >= MAX_APPS)
  break;
      std::memcpy(&(cd[n].ad[s]), &(ed_o[en].sd[l].data.ad), sizeof(AppData));
      s++;
      break;
    case 'Z':     /* Dummy */
  break;
    }
    l += o;
  }
  for (; i < MAX_MAGICS; i++) {
    cd[n].md[i].spell = nullptr;
    cd[n].md[i].effect = EF_NONE;
    cd[n].md[i].fn = FN_NONE;
    cd[n].md[i].is_inherence = 0;
    cd[n].md[i].extension = nullptr;
  }
  for (; j < MAX_ITEMS; j++)
    delete_item(&(cd[n].id[j]));
  for (; k < MAX_TALKS; k++) {
    cd[n].td[k].type = TT_NONE;
    cd[n].td[k].string = nullptr;
  }
  for (; s < MAX_APPS; s++){
    cd[n].ad[s].type = 0;
    cd[n].ad[s].string = nullptr;
  }
  cd[n].weq = nullptr;
  cd[n].wsep = 0;
  cd[n].afn = FN_NONE;
  for (i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type & IT_MASK) == IT_WEAPON && cd[n].id[i].active_time !=ULONG_MAX) {
      if (cd[n].weq == nullptr || cd[n].weq->ATP < cd[n].id[i].ATP) {
  cd[n].weq = &(cd[n].id[i]);
  cd[n].afn = cd[n].weq->fn;
      }
    }
  }
  cd[n].oeq = nullptr;
  cd[n].at = AT_NONE;
  cd[n].asep = 0;
  select_accessory(n);    /* noroi bousi no tame, saki ni soubi simasu */
  select_armor(n);
  cd[n].exp = 99999998; /* change for vs exp-limit 20000929 */
  cd[n].last = time(nullptr); /* born time for threatened species 20020520 */
  for (i = 0; i < MAX_CN; i++)
    cd[n].cn[i] = 0;
  set_chr(n);     /* マップへの埋め込み */
  if (cd[n].aeq != nullptr && cd[n].aeq->DFP == 99)
    CN_FORCE(n) = 0x10000;
  if (cd[n].aeq != nullptr && cd[n].aeq->MRP == 99)
    CN_REFLEX(n) = 0x10000;
  if (cd[n].type == CT_ASTRAL)
    CN_FORCE(n) = 0x10000;
  cd[n].st = ST_COMMAND;
  cd[n].wt = 0;
  cd[n].anmf = 0;
  cd[n].think = TH_NONE;
  cd[n].angry = -1;
  cd[n].damaged = -1;
  cd[n].cooldown = ed_o[en].cooldown;
  cd[n].trace = -1;
  cd[n].fd = -1;
  cd[n].eye = 1;    /* クライアントにフォーカスがあっているか */
  CPYSTR(cd[n].uid, "?");
  cd[n].gp = fsps(100, ed_o[en].gp, 9999);
  cd[n].m_wait = cd[n].m_wait_c = 0;
  cd[n].s_wait = cd[n].s_wait_c = 0;
  cd[n].afn = cd[n].dfn = cd[n].sfn = FN_NONE;
  cd[n].undetect = ed_o[en].undetect;
  cd[n].innate = ed_o[en].innate;
  /* change for traitor summonee 20010221 */
  if (accept_master(n,cd[n].vf2)) {
      cd[n].target = cd[n].trace = cd[n].vf2;
      cd[n].party = cd[cd[n].vf2].party; /* add 20020822 */
      cd[n].think = TH_TRACE;
  } else {
    if (ed_o[en].apc != 3)
      cd[n].vf2 = -1;
      cd[n].party = 0;
  }
  cd[n].pflag = 0; /* add for personal flag 20000804 */
  for(i=0;i<PVAR_MAX;i++)
    cd[n].pvar[i]=0;
  for(i=0;i<SVAR_MAX;i++)
    cd[n].svar[i]=0;
  cd[n].formatA = nullptr;
  cd[n].extension = nullptr;
  /* キャラクターのパラメータ初期化はここに追加 */
  /* 初期化もれのないよう */

/* select_armor() からの set_fn() により wd の再設定は不要。*/

  if (tax > 0)
    cd[n].gp *= (10000 + tax) / 10000;
  if (cd[n].gp > 9999)
    cd[n].gp = 9999;
  if (cd[n].gp < 0)
    cd[n].gp = 0;
  tax -= cd[n].gp;
  ed_o[en].nr++;    /* 発生数 */

#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"NPC cd[%d](%d) is appeared.",n,en);
  log_print(debug_buf);
#endif

  /* flag on when NPC appeared added by Qajin 20000522 */
  if (ed_o[en].nr >= ed_o[en].fbn) {
    change_flag(n,ed_o[en].fborn,FLAG_ON,1);
    load_switch(n,cd[n].x,cd[n].y,10);
  }
}

/*----------------------------------
  アイテムデータの読み込み
----------------------------------*/
void   load_id()
{
  ItemData id;
  int    x, y, gp, i;
  FILE  *fp;
  char  *p, *p2;
  std::array<char, 256> buf;

  for (i = 0; i < HASHMAX; i++) { /* ハッシュ領域をクリア */
    iphash[i] = nullptr;
  }
  if ((fp = efopen("phi.id", "rb")) == nullptr) {
    fprintf(stderr, " Can not find item data file [phi.id]. ");
    exit(1);
  }
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      break;
    p = buf.data() + 2;
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (buf[1] == 'G') {
      gp = 0;
      SKIP_SPACE(p);
      p2 = p;
      while ('0'<= *p && *p <= '9') {
  gp *= 10;
  gp += (int) (unsigned int) (unsigned char) *(p++)-'0';
      }
      SKIP_SPACE(p);
      if ((std::strncmp(p, "gp", 2) == 0) || (std::strncmp(p, "Gp", 2) == 0)) {
  /* gp が落ちている場合は、現地レートに変換 */
  gp *= cirate;
  gp /= 100;
      }
      p = p2;
      while (*p != ' ' || p[1] != ' ')
  *p++ = ' ';
      for(i = 0; cirunit[i]!='\0'; i++)
  p2[i] = cirunit[i];
      p = p2;
    } else {
      gp = 0;
    }
    load_item(&id, p);
    if (x >= 1 && x < MAX_X - 1 && y >= 1 && y < MAX_Y - 1) {
      /* 罠 保存 20010226 */
      map[y][x].tip |= (buf[1] == 'T'? T_TRAP : T_ITEM);
      additem(x, y, &id, (buf[1] == 'T' ? -1 : gp));
    }
  }
  efclose(fp);
  check_id_in_circle();
}

void   check_id_in_circle()
{
  DIR *dp;
  struct dirent *ent;
  char *p;
  int  x,y;
  ItemPData *pp;

  if ((dp = opendir("phi_circle")) == nullptr) {
    log_print(" Can not read circle data directory [phi_circle/]. ");
    return;
  }
  while ((ent = readdir(dp)) != nullptr) {
    p = ent->d_name;
    if (std::strlen(p) == 9 && !std::strcmp(p+6,".id")) {
      x = decimal_load(3,&p);
      y = decimal_load(3,&p);
      /* bug fix for re-appear circle items 20031213 */
      if ((map[y][x].tip & TMASK) != T_BOX &&
          (map[y][x].tip & TMASK) != T_LBOX) {
        load_id_in_circle(x,y);
      } else {
        pp = additempos(x,y);
        pp->notouch = ITEM_NO_TOUCH;
      }
    }
  }

  closedir(dp);
}

/*----------------------------------
  アイテムデータのセーブ
----------------------------------*/
int    savef_id()
{
  ItemOData **ip;
  int    x, y;
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 32> buf2;

  if ((fp = efopen("phi.id", "wb")) == nullptr) {
    log_print("DM> Can not write item data file [phi.id].\n");
    return (-1);
  }
  for (x = 0; x < MAX_X; x++) {
    for (y = 0; y < MAX_Y; y++) {
      ip = nullptr;
      for (;;) {
  ip = finditem(x, y, ip);
  if (ip == nullptr)
    break;
  if (is_gp(*ip)) { /* お金 */
    snprintf(buf2.data(),buf2.size(),"%d %s",(*ip)->gp, cirunit);
    snprintf(buf.data(),buf.size(),
      ":G %03i %03i %-31s %04x %02i %c %02i %02i %02i %02i\n",
      x, y, buf2.data(), (*ip)->id.type, (*ip)->id.wp, kFnc[(*ip)->id.fn],
      (*ip)->id.op[0], (*ip)->id.op[1], (*ip)->id.op[2], (*ip)->id.op[3]);
    fputs(buf.data(), fp);
  } else {
    /* 罠 保存 20010226 */
    snprintf(buf.data(), buf.size(),":%c %03i %03i ",(is_trap(*ip)? 'T':'I'), x, y);
    save_item(&((*ip)->id), buf.data() + 10,buf.size()-10);
    fputs(buf.data(), fp);
  }
      }
    }
  }
  fputs(".\n", fp);
  efclose(fp);
  return (0);
}

/* サークル内アイテムのファイルへの追出し */
int  savef_id_in_circle()
{
  ItemPData *pp;
  ItemOData **ip,*ip2;
  FILE  *fp;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char   buf[256];
  std::array<char, 32> fname; /* added by NEO 20060402 for サークルデータバックアップ */
  int   i,j;

  for (i=0;i<HASHMAX;i++) {
    pp = iphash[i];
    while(pp != nullptr) {
    if (((map[pp->y][pp->x].tip & TMASK) == T_LBOX ||
         (map[pp->y][pp->x].tip & TMASK) == T_BOX) &&
        pp->notouch < ITEM_NO_TOUCH &&
        ++(pp->notouch) >= ITEM_NO_TOUCH) {
          snprintf(buf,sizeof(buf),"phi_circle/%03i%03i.id",pp->x,pp->y);
          if ((fp = efopen(buf, "wb")) == nullptr) {
            log_print("DM > Can not write item data file [phi_circle/].\n");
            return (-1);
          }
          j = 0;
          ip = &(pp->fid);
          while (*ip != nullptr) {
            /* 金/罠は追出さない(金はサークル内でも劣化する為) */
            /*                  (罠は乗っただけで起動する為)   */
            if (is_item(*ip)) {
              if (j > 0) { /* 1つ目のアイテムは残す(m57表示用) */
                  CPYSTR(buf,":I ");
                  save_item(&((*ip)->id), buf + 3,sizeof(buf)-3);
                  fputs(buf, fp);
                  ip2 = *ip;
                  *ip = (*ip)->next;
                  free(ip2);
              } else {
                j++;
                ip = &((*ip)->next);
              }
            } else {
              ip = &((*ip)->next);
            }
          }
          efclose(fp);
	snprintf(fname.data(),fname.size(),"phi_circle_b/%03i%03i.id",pp->x,pp->y); /* added by NEO 20060402 for サークルデータバックアップ */
	unlink(fname.data()); /* added by NEO 20060402 for サークルデータバックアップ */
        }
      pp = pp->next;
    }
  }
  return (0);
}

/* サークルファイルの読込み */
int  load_id_in_circle(int x,int y)
{
  int    hd;
  ItemPData **pp;
  char   *p;
  ItemData id;
  std::array<char, 256> buf;
  std::array<char, 32> fname;
  std::array<char, 32> fnameb;
  FILE   *fp;

  hd = (x + y) & HASHMASK;

  pp = &(iphash[hd]);
  while (*pp != nullptr) {
    if ((*pp)->x == x && (*pp)->y == y)
      break;
    pp = &((*pp)->next);
  }
  if (*pp == nullptr)
    return (0);
  if ((*pp)->notouch < ITEM_NO_TOUCH) {
    (*pp)->notouch = 0;
    return (0);
  }

  snprintf(fname.data(),fname.size(),"phi_circle/%03i%03i.id",x,y);
  if ((fp = efopen(fname.data(), "rb")) == nullptr) {
    log_print("DM > Can not find circle item file [phi_circle/].\n");
    return (-1);
  }

  (*pp)->notouch = 0;
  while(1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      break;
    p = buf.data() + 2;
    if (buf[1] == 'I') {
      load_item(&id, p);
      additem(x, y, &id, 0);
    }
  }
  efclose(fp);
  snprintf(fnameb.data(),fnameb.size(),"phi_circle_b/%03i%03i.id",x,y);
  rename(fname.data(), fnameb.data()); /* added by NEO 20060402 for サークルデータバックアップ */
/*  unlink(fname); deleted by NEO  20060402 for サークルデータバックアップ */

  return (0);
}


/*----------------------------------
  フロアデータの読み込み
----------------------------------*/
void   load_fd()
{
  int    x, y, i, j;
#ifndef AREA_SUB
  std::array<char, MAX_X * 2 + 2> buf2;
#else
  std::array<char, MAX_X * 3 + 2> buf2;
#endif
  std::array<char, 256> buf;
  std::array<char, 32> tip;
  FILE  *fp;
  char  *p;
  int ary[256];

  for (i = 0; i < 256; i++) {
    ary[i] = 0;
  }
  for (y = 0; y < MAX_Y; y++)
    for (x = 0; x < MAX_X; x++) {
        map[y][x].area = 0;
#ifdef AREA_SUB
        map[y][x].area_sub = 0;
#endif
    }
  if ((fp = efopen("phi.fd", "rb")) == nullptr) {
    fprintf(stderr, "Warning: Can not find floor data file. [phi.fd]\n");
  } else
  for (y = 0; y < MAX_Y; y++) { /* エリアデータ */
    if (efgets(buf2.data(), buf2.size(), fp) == nullptr) {
      break;
    }
    p = buf2.data();
    for (x = 0; x < MAX_X; x++) {
      map[y][x].area = (unsigned char) hexadecimal_load(2, &p);
#ifdef AREA_SUB
      map[y][x].area_sub = (unsigned char) hexadecimal_load(1, &p);
#endif
    }
  }
  for (y = 0; y < MAX_Y; y++)
    for (x = 0; x < MAX_X; x++)
      ary[map[y][x].area] = -1;
  j = 0;
  for (i = 0; i < 256; i++) { /* エリア番号変換テーブル */
    if (ary[i]) {
      ary[i] = j++;
    } else {
      ary[i] = -1;
    }
  }
  flr_max = j;
  for (y = 0; y < MAX_Y; y++) { /* エリア番号変換 */
    for (x = 0; x < MAX_X; x++) {
      map[y][x].area = (unsigned char) ary[map[y][x].area];
    }
  }
  fld = (FLRData *) malloc(sizeof (FLRData) * flr_max);
  if (fld == nullptr) {
    log_print("Insufficient memory for floor data\n");
    signal_trap(SIGTERM);
  }
  for (i = 0; i < flr_max; i++) {
    fld[i].floor = 0;
#ifndef AREA_SUB
    fld[i].name[0] = ' ';
    fld[i].name[1] = '\0';
#else
    for(j=0;j<0x10;j++)
      fld[i].name[j] = strdup(""); /* bug fix for no-use subarea 20060810 */
#endif
    fld[i].flag = 0;
    fld[i].teleport = 0;
    /** psp6 by aleph **/
    fld[i].mapset = nullptr; /* change for speed display maptip 20020502 */
    fld[i].bgm[0] = ' ';
    fld[i].bgm[1] = '\0';
    /** end **/
    fld[i].fin = -1;
    fld[i].fout = -1;
    fld[i].fexist = -1;
    fld[i].fmove = -1;
  }
  for (i = 0; i < 26; i++) {
    fldt[i].x = 1;
    fldt[i].y = 1;
  }
  if (fp != nullptr) {
  for (;;) {
    if (efgets(buf.data(), 256, fp) == nullptr)
      break;
    if (feof(fp))
      break;
    if (buf[0] == '.')
      break;
    if (buf[0] == ':') {

      /** psp6 by aleph **/
      /** cv(#version-cli) > 05100006以降で有効 **/

             if (buf[1] == 'S') { /* parts-set */
  p = buf.data() + 2;
  i = hexadecimal_load(2, &p);
  if (i >= 0 && ary[i] >= 0) { /* bug fix 20000802 */
    SKIP_SPACE(p);
    loadstr(tip.data(), &p, '\n', 32);
    fld[ary[i]].mapset = findstr(tip.data()); /* change for speed display maptip 20020502 */
  }

      } else if (buf[1] == 'M') { /* bgm */
  p = buf.data() + 2;
  i = hexadecimal_load(2, &p);
  if (i >= 0 && ary[i] >= 0) { /* bug fix 20000802 */
    SKIP_SPACE(p);
    loadstr(fld[ary[i]].bgm, &p, '\n', 32);
      /** end **/
  }
      } else if (buf[1] == 'F') { /* flag add 20000629 */
  p = buf.data() + 2;
  i = hexadecimal_load(2, &p);
  if (i >= 0 && ary[i] >= 0) { /* bug fix 20000802 */
    for(;*p != '\n' && *p != '\0';) {
      switch (*p++) {
        case 'I':
          fld[ary[i]].fin = flagno_load(&p);
          break;
        case 'O':
          fld[ary[i]].fout = flagno_load(&p);
          break;
        case 'E':
          fld[ary[i]].fexist = flagno_load(&p);
          break;
        case 'M':
          fld[ary[i]].fmove = flagno_load(&p);
          break;
      }
    }
  }

      } else if (buf[1] == 'T') {
  p = buf.data() + 3;
  i = *p - 'a';
  p = p + 2;
  fldt[i].x = decimal_load(3, &p);
  fldt[i].y = decimal_load(3, &p);
      } else if (buf[1] == 'E') {
  p = buf.data() + 2;
  i = hexadecimal_load(2, &p);
  if (i >= 0 && ary[i] >= 0) { /* bug fix 20000802 */
    fld[ary[i]].area = i;

#ifdef AREA_SUB
    x = hexadecimal_load(1, &p);
    if (x)
      hexadecimal_load(2,&p);
    else
#endif
    fld[ary[i]].floor = hexadecimal_load(2, &p);
    SKIP_SPACE(p);

#ifdef AREA_SUB
    for (j = 0; j < 31 && p[j] != '\n' &&
         (!fisspace(p[j]) || !fisspace(p[j+1]));j++);
    p[j] = '\0';
    fld[ary[i]].name[x] = strdup(p);
    p += j+1;
    if (!x) {
#else
    for (j = 0; j < 31 && *p != '\n' &&
         (!fisspace(*p) || !fisspace(*(p + 1)));
         fld[ary[i]].name[j++] = *(p++));
    fld[ary[i]].name[j] = '\0';
#endif

    SKIP_SPACE(p);
    for (; *p != '\0'; p++) {
      switch (*p) {
      case 'R':
        fld[ary[i]].flag |= FF_RESOURCE;
        break;
      case 'M':
        fld[ary[i]].flag |= FF_MAGIC;
        break;
      case 'N':
        fld[ary[i]].flag |= FF_MAGICEFFECT;
        break;
      case 'E':
        fld[ary[i]].flag |= FF_EAGLEEYE;
        break;
      case 'Q':
        fld[ary[i]].flag |= FF_RETURN;
        break;
      case 'T':
        fld[ary[i]].flag |= FF_GATE;
        break;
      case 'W':
        fld[ary[i]].flag |= FF_WRITE;
        break;
      case 'L':
        fld[ary[i]].flag |= FF_LOCK;
        break;
      case 'G':
        fld[ary[i]].flag |= FF_GASIFY;
        break;
      case 'A':
        fld[ary[i]].flag |= FF_ATTACK;
        break;
      case 'D':
        fld[ary[i]].flag |= FF_AREAMAGIC;
        break;
      case 'X':
        fld[ary[i]].flag |= FF_EXTENSION;
        break;
      case 'C':
        fld[ary[i]].flag |= FF_CHARGE;
        break;
      /* add for prohibition remote get/put 20000804 */
      case 'O':
        fld[ary[i]].flag |= FF_REMOTEGET;
        break;
      case 'P':
        fld[ary[i]].flag |= FF_REMOTEPUT;
        break;
      case '7':
        fld[ary[i]].flag |= FF_M57;
        break;
      default:
        if ((*p < 'a') || (*p > 'z'))
    break;
        fld[ary[i]].flag |= FF_CALL;
        fld[ary[i]].teleport = *p - 'a';
        break;
      }
    }

#ifdef AREA_SUB
    }
#endif

  }
      }
    }
  }
  efclose(fp);
  }
}

void   free_fd(){
#ifdef AREA_SUB
  int i,j;

  for(i=0;i<flr_max;i++)
    for(j=0;j<0x10;j++)
      if (fld[i].name[j] != nullptr)
        free(fld[i].name[j]);
#endif

  free(fld);
}

void   load_td_gate()
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 256> buf2;
  ATOM   abf[100];
  int    lpt, rsdn;
  char  *p;

  if ((fp = efopen("phi.td", "rb")) == nullptr) {
    fprintf(stderr, " Can not find transfer data file. [phi.td] ");
    exit(1);
  }
  gatemax = 0;
  for (;;) {      /* データ数調査 */
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (feof(fp))
      break;
    if (buf[0] != ':')
      continue;
    switch (buf[1]) {
    case 'G':
    case 'g':
    case 'P':
    case 'H':
      gatemax++;
    }
  }
  efclose(fp);

  gatedata = (ATOM **) malloc(sizeof(ATOM *) * gatemax);
  if(gatedata == nullptr) {
    log_print("Insufficient memory for switch data\n");
    exit(1);
  }

  if ((fp = efopen("phi.td", "r")) == nullptr) {
    fprintf(stderr, " Can not find transfer data file. [phi.td] ");
    exit(1);
  }
  rsdn = 0;
  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':')
      continue;
    lpt = 0;
    p = buf.data() + 2;
    switch (buf[1]) {
    case 'G':
    case 'g':
      abf[lpt++].di = chartoint(buf[1]);
      abf[lpt++].di = decimal_load(3, &p);
      abf[lpt++].di = decimal_load(3, &p);
      abf[lpt++].di = decimal_load(3, &p);
      abf[lpt++].di = decimal_load(3, &p);
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, DSPACE, 32);
      abf[lpt++].dp = findstr(buf2.data());
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, '\n', 128); /* IP and/or Port */
      abf[lpt++].dp = findstr(buf2.data());  /* and/or Password */
      /* dead gate check counter added by Qajin 20000116 */
      if (abf[0].di == 'G' && buf2[0] != '-')
         abf[lpt++].di = 0;
      break;
    case 'P':     /* 受け入れパスワード群 */
      abf[lpt++].di = 'P';
      p++;
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, '\n', 32);
      abf[lpt++].dp = findstr(buf2.data());
      /* add for security up 20020524 */
      abf[lpt++].di = (buf[2] == ' ' ? -1 : chartoint(buf[2]));
      break;
    case 'H':     /* 接続位置指定 */
      abf[lpt++].di = 'H';
      SKIP_SPACE(p);
      if (*p == '-') {
  abf[lpt++].di = 0;
  SKIP_TO_SPACE(p);
      } else {
  abf[lpt++].di = decimal_load(3, &p);
      }
      SKIP_SPACE(p);
      if (*p == '-') {
  abf[lpt++].di = 0;
  SKIP_TO_SPACE(p);
      } else {
  abf[lpt++].di = decimal_load(3, &p);
      }
      SKIP_SPACE(p);
  abf[lpt++].di = chartoint(*p);
  /* P:要パスワード */
  /* -:パス不要 */
  /* その他:指定パスワードのみ */
      p++;
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, DSPACE, 32);  /* 世界名 */
      abf[lpt++].dp = findstr(buf2.data());
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, ' ', 32); /* IP */
      abf[lpt++].dp = findstr(buf2.data());
      /* とりあえずポート無視…… */
      for (; *p != 'P' && *p != '\0'; p++);
      if (!std::strncmp(p, "P:", 2)) {
  loadstr(buf2.data(), &p, '\n', 32);
  abf[lpt++].dp = findstr(buf2.data());
      } else {
  abf[lpt++].dp = findstr(" ");
      }
      break;
    default:
      continue;
    }
    if (lpt > 0) {
      gatedata[rsdn] = (ATOM *) malloc(sizeof (ATOM) * lpt);
      if (gatedata[rsdn] == nullptr) { /* 領域確保 */
  log_print("Insufficient memory for switch data\n");
  exit(1);
      }
      std::memcpy(gatedata[rsdn], abf, sizeof (ATOM) * lpt);
    } else {
      gatedata[rsdn] = nullptr;
    }
    rsdn++;
  }
  efclose(fp);
  return;
}

void   free_gatedata()
{
  int cn;

  for(cn=0; cn<gatemax; cn++)
    if(gatedata[cn]!=nullptr)
      free(gatedata[cn]);
  free(gatedata);
}

/* add 20010206 */
/* 外行きゲートのみクリアする */
void   free_outgatedata()
{
  int cn;

  for(cn=0; cn<gatemax; cn++)
    if(gatedata[cn]!=nullptr) {
      if (gatedata[cn][0].di == 'G' && gatedata[cn][6].dp[0] != '-') {
        free(gatedata[cn]);
        gatedata[cn]=nullptr;
      }
    }
}

void   load_td_swt()
{
  fx_clear_image_cache();  /* FX 画像キャッシュは Load swt で破棄 (PHIDM NEO extension) */
  FILE  *fp;
  std::array<char, 256> buf;
  /* buf2 は CPYSTR の宛先のため C 配列のまま */
  char   buf2[256];
  ATOM   abf[ATOMBUF_SIZE];
  int    i, j, lpt, l, rsdn, rmdn, rndn;
  char  *p, *p2;
  ItemData id;
  MagicData md;

  rsdn = swtn = rmdn = msgn = rndn = menun = 0;
  if ((fp = efopen("phi.swt", "r")) == nullptr) {
    fprintf(stderr, "Warning: Can not find switch data file. [phi.swt]\n");
    return;
  }
  for (;;) {      /* データ数調査 */
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (feof(fp))
      break;
    if (buf[0] != ':')
      continue;
    switch (buf[1]) {
    case 'A':
      msgn++; /* change for quick message search 20030501 */
      break;
    case 'S':
      swtn++;
      break;
    case 'M': /* add for menu 20041211 */
      menun++;
      break;
    }
  }
  efclose(fp);

  swtd = (ATOM **) malloc(sizeof (ATOM *) * swtn);
  msgd = (MSGData *) malloc(sizeof(MSGData) * msgn); /* add for quick message search 20030501 */
  menud = (MENUData *) malloc(sizeof(MENUData) * menun); /* add for menu 20041211 */
  if (swtd == nullptr || msgd == nullptr || menud == nullptr) {   /* 領域確保 */
    log_print("Insufficient memory for switch data\n");
    exit(1);
  }

  if ((fp = efopen("phi.swt", "r")) == nullptr) {
    fprintf(stderr, "Warning: Can not find switch data file. [phi.swt]\n");
    exit(1);
  }

  /* add for menu 20041212 */
  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':' || buf[1] != 'M')
      continue;

    p = buf.data() + 2;
    /* Menu number */
    menud[rndn].menu_num = decimal_load(4, &p);

    /* Selection pattern */
    SKIP_SPACE(p);
    if (*p == '\"' || *p == '\'') {
      p++;
      loadstr(buf2, &p,*(p-1),256);
      menud[rndn].sel = findstr(buf2);
    } else if (*p == '-') {
      p++;
      menud[rndn].sel = nullptr;
    } else {
      snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
      log_print(buf2);
      menud[rndn].sel = nullptr;
    }

    /* Selection caption */
    SKIP_SPACE(p);
    if (*p == '\"' || *p == '\'') {
      p++;
      loadstr(buf2, &p,*(p-1),256);
      l = std::strlen(buf2);
      buf2[l++] = '\n';
      buf2[l] = '\0';
      menud[rndn].caption = findstr(buf2);
    } else if (*p == '-') {
      p++;
      menud[rndn].caption = nullptr;
    } else {
      snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
      log_print(buf2);
      menud[rndn].caption = nullptr;
    }

    /* action type & parameter*/
    SKIP_SPACE(p);
    menud[rndn].act_type = MENU_ACT_NULL;
    menud[rndn].act_param = 0;
    switch (*p++) {
        case 'F':
        case 'f':
          menud[rndn].act_type = MENU_ACT_FLAG;
          menud[rndn].act_param = flagno_load(&p);
          break;
        case 'A':
        case 'a':
          menud[rndn].act_type = MENU_ACT_MESG;
          menud[rndn].act_param = decimal_load(4, &p);
          break;
        case 'M':
        case 'm':
          menud[rndn].act_type = MENU_ACT_MENU;
          menud[rndn].act_param = decimal_load(4, &p);
          break;
        case '\0':
        case '\n':
          break;
        default:
          snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
          log_print(buf2);
          break;
    }

    rndn++;
  }
  efclose(fp);

  if ((fp = efopen("phi.swt", "r")) == nullptr) {
    fprintf(stderr, "Warning: Can not find switch data file. [phi.swt]\n");
    exit(1);
  }

  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':' || buf[1] != 'A')
      continue;
    p = buf.data() + 2;
/*      abf[lpt++].di = 'A'; */
      msgd[rmdn].orgn = decimal_load(4, &p); /* add for used boid number bug 20041117 */
      msgd[rmdn].next = decimal_load(4, &p);
      msgd[rmdn].wait = decimal_load(4, &p);
      while ((*p != '\0') && (*p != '\"') && (*p != 'x') && (*p != 'f') && (*p != 'z') && (*p != 'y') && (*p != 's')) /* psp3 */ /* add flag 20000522 */ /* z: FX extension (PHIDM NEO) */ /* y/s: 選択拡張 (PHIDM NEO extension, from MRDM) */
  p++;
      if (*p == '\"') {
  p++;
  CPYSTR(buf2, p);
  l = std::strlen(buf2);
  for (; l > 0; l--) {
    if (buf2[l] == '\"')
      break;
  }
  buf2[l++] = '\n';
  buf2[l] = '\0';
  /* psp3 -added by aleph */
  msgd[rmdn].type = 0;                       /* 拡張フラグ無し */
  /* end */
  msgd[rmdn].data.mesg = findstr(buf2); /* change sequence for less memory 20000627 */
      } else {

  /* psp3 - added by aleph */
  if (*p == 'x') {                         /* :A拡張　終了後移動処理 */
    p++;
    msgd[rmdn].type = 1;                     /* 拡張フラグ1 */
    msgd[rmdn].data.pos.x = decimal_load(3, &p);   /* :A MMMM NNNN TTTT x XXX YYY */
    msgd[rmdn].data.pos.y = decimal_load(3, &p);
  }
  else if (*p == 'f') {                         /* :A拡張 flag ON */
    p++;
    msgd[rmdn].type = 2;                     /* 拡張フラグ2 */
    msgd[rmdn].data.flag = flagno_load(&p);   /* :A MMMM NNNN TTTT f FF */
  }
  else if (*p == 'z') {                         /* :A拡張 FX (PHIDM NEO extension) */
    p++;
    SKIP_SPACE(p);
    msgd[rmdn].type = 3;                     /* 拡張フラグ3: 残り全部が FX ペイロード */
    CPYSTR(buf2, p);
    l = std::strlen(buf2);
    while (l > 0 && (buf2[l - 1] == '\n' || buf2[l - 1] == '\r'
                     || buf2[l - 1] == ' ' || buf2[l - 1] == '\t'))
      buf2[--l] = '\0';
    msgd[rmdn].data.mesg = findstr(buf2);
  }
  else if (*p == 'y') {                         /* :A拡張 Yes/No選択 (PHIDM NEO extension, from MRDM) */
    p++;
    msgd[rmdn].type = 4;                     /* 拡張フラグ4: next=Yes分岐, wait=No分岐 */
    SKIP_SPACE(p);
    if (*p == '\"') {
  p++;
  CPYSTR(buf2, p);
  l = std::strlen(buf2);
  for (; l > 0; l--) {
    if (buf2[l] == '\"')
      break;
  }
  buf2[l++] = '\n';
  buf2[l] = '\0';
  msgd[rmdn].data.mesg = findstr(buf2);
    } else {
  msgd[rmdn].data.mesg = nullptr;
    }
  }
  else if (*p == 's') {                         /* :A拡張 選択肢 (PHIDM NEO extension, from MRDM) */
    p++;
    msgd[rmdn].type = 5;                     /* 拡張フラグ5: next=次の選択肢行, wait=選択時の分岐 */
    SKIP_SPACE(p);
    if (*p == '\"') {
  p++;
  CPYSTR(buf2, p);
  l = std::strlen(buf2);
  for (; l > 0; l--) {
    if (buf2[l] == '\"')
      break;
  }
  buf2[l++] = '\n';
  buf2[l] = '\0';
  msgd[rmdn].data.mesg = findstr(buf2);
    } else {
  msgd[rmdn].data.mesg = nullptr;
    }
  }
  else {
  /* ここに他の拡張の認識を記述可能 */
    msgd[rmdn].type = -1;                     /* 拡張フラグ無し */
  }
  /* end */
      }
      rmdn++;
  }
  efclose(fp);

  if ((fp = efopen("phi.swt", "r")) == nullptr) {
    fprintf(stderr, "Warning: Can not find switch data file. [phi.swt]\n");
    exit(1);
  }

  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':' || buf[1] != 'S')
      continue;
    lpt = 0;
    p = buf.data() + 2;
/* delete for quick message search 20030501
    switch (buf[1]) {
    case 'S':
      abf[lpt++].di = 'S';
*/
      abf[lpt++].di = decimal_load(3, &p);  /* X */
      abf[lpt++].di = decimal_load(3, &p);  /* Y */
      i = abf[lpt++].di = decimal_load(2, &p);  /* 条件 */
      SKIP_SPACE(p);
      switch (i) {
      case 1:     /* あるレベルに達した時 */
      case 6:     /* 無言プレート */
      case 11:      /* あるレベルに達してない時 */
      case 16:      /* 無言プレート */
      case 25:      /* ADD スイッチデータ追加 2005/11/26 Savarl.F */
  abf[lpt++].di = decimal_load(2, &p);  /* レベル種類 */
      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      if (i == 25) {
		SKIP_TO_SPACE(p);
		SKIP_SPACE(p);
        abf[lpt++].dc = *p++; /* 条件式 */
      }
      /* ↑ADD */
  if (abf[lpt - 1].di == 5) { /* 経験値 */
    abf[lpt++].di = decimal_load(8, &p);
  } else {    /* レベル他 */
    abf[lpt++].di = decimal_load(4, &p);
  }
      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      if (i == 25) {
        abf[lpt++].di = decimal_load(1, &p);     /* ステータス条件 */
		SKIP_TO_SPACE(p);
		SKIP_SPACE(p);
        abf[lpt++].dc = *p++; /* 条件式 */
        abf[lpt++].di = decimal_load(4, &p);     /* チェック値 */
      }
      /* ↑ADD */
  break;
      case 3:     /* 特定のメッセージを話した時 */
      case 7:     /* NPCが特定の言葉を話した時 */
      case 13:    /* パターンに合致するメッセージを話した時 added 20000101 */
  if (*p == '\"') {
    p2 = buf2;
    for (i = 0, p++; i < 63 && *p != '\"'; i++, *(p2++) = *(p++));
    *p2 = '\0';
    p++;
    abf[lpt++].dp = findstr(buf2);
  } else {
    abf[lpt++].dp = nullptr;
  }
  break;
      case 4:     /* アイテムをその場においた時 */
      case 8:     /* アイテム所持 */
      case 18:    /* アイテムを拾った時 */
  if (*p == '\"' || *p == '\'') { /* change for Extend data 20010614 */
    p++;
    loadstr(buf2, &p,*(p-1),256);
    load_item(&id, buf2);
  } else {
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
  }
  abf[lpt++].dp = id.name;
  abf[lpt++].di = id.type;
  abf[lpt++].di = id.wp;
  abf[lpt++].di = id.fn;
  abf[lpt++].di = id.op[0];
  abf[lpt++].di = id.op[1];
  abf[lpt++].di = id.op[2];
  abf[lpt++].di = id.op[3];
  abf[lpt++].dp = id.extension;
  break;
      case 14:    /* 魔法を唱えた時 */
  if (*p == '\"' || *p == '\'') { /* change for Extend data 20010614 */
    p++;
    loadstr(buf2, &p,*(p-1),256);
    load_spell(&md, buf2);
  } else {
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
  }
  abf[lpt++].dp = md.spell;
  abf[lpt++].di = md.effect;
  abf[lpt++].di = md.fn;
  abf[lpt++].dp = md.extension;
  break;
      case 5:     /* お金をおいた時 */
  abf[lpt++].di = decimal_load_p(4, &p); /* Wildcard error 対策 added by NEO 20060707 */
  break;
      case 9:     /* 置かれたアイテムの総重量が規定値を越えた時 */
  abf[lpt++].di = decimal_load(4, &p);
  break;
      case 10:      /* 汎用フラグが ON の時 */
      case 12:
  if (*p == '\"') {
    p++;
    i = lpt++;
    abf[i].di = 0;
    while (*p != '\"') {
      j = flagno_load(&p);
      if (j < 0 || j >= GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX)
        continue;
      abf[lpt++].di = j;
      abf[i].di++;  /* チェックするフラグの個数 */
    }
    SKIP_TO_SPACE(p);
  } else {
    abf[lpt++].di = 0;
  }
  break;
      case 15:  /* turn check */
        switch (*p) {
            case 'N':
              j = 0;
              break;
            case 'E':
              j = 1;
              break;
            case 'S':
              j = 2;
              break;
            case 'W':
              j = 3;
              break;
            case 'l':
              j = 4;
              break;
            case 'r':
              j = 5;
              break;
            case 'b':
              j = 6;
              break;
            default:
              j = 7;
        }
        abf[lpt++].di = j;
        p++;
        break;
      case 20: /* variable check */
        if ((abf[lpt++].di=varno_load(&p))<0) {
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
        }
        SKIP_SPACE(p);
        if (!std::strncmp(p,"<=",2)) {
            *buf2 = 'l';
            p += 2;
        } else if (!std::strncmp(p,">=",2)) {
            *buf2 = 'g';
            j = 5;
            p += 2;
        } else {
            switch (*p) {
                case '=':
                case '!':
                case '<':
                case '>':
                case '&':
                case '^':
                case '_':
                    *buf2 = *p;
                    break;
                default:
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
            }
            p++;
        }
        SKIP_SPACE(p);
        if ((j=varno_load(&p))<0) {
            if ((abf[lpt++].di=decimal_load_arbitrarily(&p))<0) {
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
            }
            abf[lpt++].di = 0;
        } else {
            abf[lpt++].di = j;
            abf[lpt++].di = 1;
        }
        abf[lpt++].dc = *buf2;
        break;
      case 30: /* object on */
      case 31: /*       off */
        loadstr(buf2,&p,' ',15);
        abf[lpt++].dp = findstr(buf2);
  break;
      case 91: /* 実時間 (PHIDM NEO extension, from MRDM) */
  abf[lpt++].di = decimal_load(2, &p);  /* 月 (99=任意) */
  abf[lpt++].di = decimal_load(2, &p);  /* 日 (99=任意) */
  abf[lpt++].di = decimal_load(2, &p);  /* 時 (99=任意) */
  abf[lpt++].di = decimal_load(2, &p);  /* 分 (99=任意) */
  break;
      case 80: /* 職業・性別 (PHIDM NEO extension) */
  abf[lpt++].di = decimal_load(2, &p);  /* 00=性別 01=職業 */
  abf[lpt++].di = decimal_load(2, &p);  /* 値 (0=未設定) */
  break;
      case 92: /* ペット (PHIDM NEO extension) */
  abf[lpt++].di = decimal_load(2, &p);  /* タイプ (0=任意 1妖精 2猫 3犬 4鳥) */
  abf[lpt++].di = decimal_load(2, &p);  /* 状態 (0=任意 1行動可 2休止中) */
  break;
      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      case 21: /* 指定NPC殺害時 */
        SKIP_SPACE(p);
        if (*p == '\"') {
          p++;
          loadstr(buf2, &p, '\"', 32);
          abf[lpt++].dp = findstr(buf2);
        } else {
          loadstr(buf2, &p, '\n', 32);
          abf[lpt++].dp = findstr(buf2);
        }
        break;
      /* ↑ADD */
      case 0:
      case 2:
      case 17: /* add for hit switch 20020530 */
      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      case 22:
      case 23:
      case 24:
      /* ↑ADD */
      default:
  break;
      }
      abf[lpt++].di = decimal_load(3, &p);  /* X */
      abf[lpt++].di = decimal_load(3, &p);  /* Y */
      i = abf[lpt++].di = decimal_load(2, &p);  /* 動作 */
      SKIP_SPACE(p);
      switch (i) {
      case 23: /* change surface */
      case 1:
  j = ((int) (unsigned int) (unsigned char) *p++) << 8;
  j |= ((int) (unsigned int) (unsigned char) *p++);
  abf[lpt++].di = j;
  break;
      case 11:     /* 広範囲地形変化 added 20000102 */
  if (*p == '\"') {
    p++;
    loadstr(buf2, &p, '\"', 256);
    j = std::strlen(buf2);
    abf[lpt++].dp = findstr(buf2);
  } else {
    abf[lpt++].dp = nullptr;
    j = 0;
  }
  /* 矩形範囲地形変化 add 20001130 */
  SKIP_SPACE(p);
  if (*p != '\n' && *p != '\0') {
    l = decimal_load(2,&p);
    if (l)
        j = l;
  }
  abf[lpt++].di = j;
  break;
      case 16:          /* 特殊範囲メッセージ added by Qajin 20000102 */
      j = abf[lpt++].di = decimal_load(1, &p);
      if (j == 2) {
          SKIP_SPACE(p);
          abf[lpt++].di = hexadecimal_load(2, &p);
      } else if (j == 3) {
          SKIP_SPACE(p);
          abf[lpt++].di = decimal_load(2, &p);
      }
      SKIP_SPACE(p);
      case 3:
      case 90: /* add for recording log 20020112 */
  if (*p == '\"') {
    p++;
    loadstr(buf2, &p, '\"', 256); /* メッセージ */
    j = std::strlen(buf2);
    if (i != 90)
      buf2[j++] = '\n';
    buf2[j] = '\0';
    abf[lpt++].dp = findstr(buf2);
  } else {
    abf[lpt++].dp = nullptr;
  }
  break;
      case 4:     /* アイテム出現 */
      case 14:
      case 44:   /* add for キャラ位置アイテム出現　by NEO 2005/01/08　*/
  if (*p == '\"' || *p == '\'') { /* change for Extend data 20010614 */
    p++;
    loadstr(buf2, &p,*(p-1),256);
    if (!load_item(&id, buf2)) { /* bug(?) fix 20010816 */
      snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
      log_print(buf2);
      continue;
    }
  } else if (*p == '@') { /* add for duplicate item 20010816 */
    id.name = nullptr;
    id.extension = nullptr;
    id.type = id.wp = id.fn = id.op[0] = id.op[1] = id.op[2] = id.op[3] = 0;
    if (!std::strncmp(p+1,"item",4)) {
        id.type = 1;
    } else if (!std::strncmp(p+1,"wpn",3)) {
        id.type = 2;
    } else if (!std::strncmp(p+1,"arm",3)) {
        id.type = 3;
    } else if (!std::strncmp(p+1,"acc",3)) {
        id.type = 4;
    } else {
      snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
      log_print(buf2);
      continue;
    }
  } else {
    snprintf(buf2,sizeof(buf2),"phi.swt:line %d:Something wrong.",lcnt);
    log_print(buf2);
    continue;
  }
  abf[lpt++].dp = id.name;
  abf[lpt++].di = id.type;
  abf[lpt++].di = id.wp;
  abf[lpt++].di = id.fn;
  abf[lpt++].di = id.op[0];
  abf[lpt++].di = id.op[1];
  abf[lpt++].di = id.op[2];
  abf[lpt++].di = id.op[3];
  abf[lpt++].dp = id.extension;
  break;
      case 5:     /* お金が出現 */
      case 45:   /* add for キャラ位置お金出現　by NEO 2005/01/08　*/
      case 29:    /* メニュー起動 add 20041212 */
  abf[lpt++].di = decimal_load(4, &p);
  break;
      case 26:     /* 相対座標転送  */
      if (*p++ == '-')
        abf[lpt-3].di = -abf[lpt-3].di;
      SKIP_SPACE(p);
      if (*p++ == '-')
        abf[lpt-2].di = -abf[lpt-2].di;
      SKIP_SPACE(p);
      case 6:     /* 強制転送  */
  abf[lpt++].di = (int) (unsigned int) (unsigned char) *p;
  break;
      case 7:     /* クリーチャー発生スイッチ */
  SKIP_SPACE(p);
  if (*p == '\"') {
    p++;
    loadstr(buf2, &p, '\"', 32);
    abf[lpt++].dp = findstr(buf2);
  } else {
    loadstr(buf2, &p, '\n', 32);
    abf[lpt++].dp = findstr(buf2);
  }
  break;
      case 13:    /* 強制サーバ間転送 */
  abf[lpt++].di = decimal_load_arbitrarily(&p);
  break;
      case 15:     /* キャラグラ変更 added by Qajin 19991224 from psp10 */
  SKIP_SPACE(p);
  if (*p == '\"') {
    p++;
    loadstr(buf2, &p, '\"', 15);
    abf[lpt++].dp = findstr(buf2);
  } else {
    loadstr(buf2, &p, '\n', 15);
    abf[lpt++].dp = findstr(buf2);
  }
  break;
      case 17:    /* 強制 Shrink/Gigant */
        i = decimal_load(1,&p);
      switch (i) {
        case 0:
        case 1:
        case 2:
        case 5:
          abf[lpt++].di = i;
          break;
        case 3:
          abf[lpt++].di = 4;
          break;
        case 4:
          abf[lpt++].di = 8;
          break;
        case 6:
          abf[lpt++].di = 10;
          break;
      }
        break;
      case 8:     /* ダメージプレート */
  abf[lpt++].di = hexadecimal_load(1, &p); /* changed 20000102 */
  abf[lpt++].di = decimal_load(4, &p);
  break;
      case 60:    /* 刻印スイッチ (PHIDM NEO extension) */
      case 61:    /* 刻印表示スイッチ (PHIDM NEO extension) */
      case 62:    /* プログラム実行スイッチ (PHIDM NEO extension) */
      case 63:    /* FX スイッチ (PHIDM NEO extension) */
      case 64:    /* FX スイッチ (PHIDM NEO extension) */
      case 65:    /* FX スイッチ (PHIDM NEO extension) */
      case 66:    /* FX スイッチ (PHIDM NEO extension) */
      case 67:    /* FX スイッチ (PHIDM NEO extension) */
      case 68:    /* FX スイッチ (PHIDM NEO extension) */
  SKIP_SPACE(p);
  if (*p == '\"') {
    p++;
    loadstr(buf2, &p, '\"', 256);
    abf[lpt++].dp = findstr(buf2);
  } else {
    abf[lpt++].dp = nullptr;
  }
  break;
      case 9:     /* メッセージスイッチ */
      case 19:    /* メッセージスイッチ2 */
  abf[lpt++].di = decimal_load(4, &p);
  abf[lpt++].di = decimal_load(4, &p);
  break;
      case 20:      /* 確率変化 */
  abf[lpt++].di = decimal_load(2, &p);
      case 10:      /* 汎用フラグを変化 */
      case 81:      /* 汎用フラグを変化 (スイッチを起動しない) (PHIDM NEO extension, from JET) */
      case 82:      /* 汎用フラグを変化 (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
  abf[lpt++].di = decimal_load(1, &p);
  SKIP_SPACE(p);
  if (*p == '\"') {
    p++;
    i = lpt++;
    abf[i].di = 0;
    while (*p != '\"') {
      j = flagno_load(&p);
      if (j < 0 || j >= GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX)
        continue;
      abf[lpt++].di = j;
      abf[i].di++;  /* 変化させるフラグの個数 */
    }
  } else {
    abf[lpt++].di = 0;
  }
  break;
      case 21: /* オブジェクト設置 */
        loadstr(buf2,&p,' ',15);
        abf[lpt++].dp = findstr(buf2);
        SKIP_SPACE(p);
        switch(*p) {
            case 'N':
            case 'n':
                abf[lpt++].di = 0;
                break;
            case 'E':
            case 'e':
                abf[lpt++].di = 1;
                break;
            case 'S':
            case 's':
                abf[lpt++].di = 2;
                break;
            case 'W':
            case 'w':
                abf[lpt++].di = 3;
                break;
            case 'B':
            case 'b':
                abf[lpt++].di = 4;
                break;
            case 'R':
            case 'r':
                abf[lpt++].di = 5;
                break;
            case 'F':
            case 'f':
                abf[lpt++].di = 6;
                break;
            case 'L':
            case 'l':
                abf[lpt++].di = 7;
                break;
            default:
                abf[lpt++].di = -1;
        }
        break;
  break;
      case 22: /* オブジェクト撤去 */
        loadstr(buf2,&p,'\n',15);
        abf[lpt++].dp = findstr(buf2);
  break;
      case 18: /* コンディション設定 */
      if (!std::strncmp(p,"coat",4)) {
        abf[lpt++].di = CNN_COAT;
      } else if (!std::strncmp(p,"protect",7)) {
        abf[lpt++].di = CNN_PROTECT;
      } else if (!std::strncmp(p,"shell",5)) {
        abf[lpt++].di = CNN_SHELL;
      } else if (!std::strncmp(p,"haste",5)) {
        abf[lpt++].di = CNN_HASTE;
      } else if (!std::strncmp(p,"illusion",8)) {
        abf[lpt++].di = CNN_ILLUSION;
      } else if (!std::strncmp(p,"spell",5)) {
        abf[lpt++].di = CNN_SPELL;
      } else if (!std::strncmp(p,"regene",6)) {
        abf[lpt++].di = CNN_REGENE;
      } else if (!std::strncmp(p,"force",5)) {
        abf[lpt++].di = CNN_FORCE;
      } else if (!std::strncmp(p,"reflex",6)) {
        abf[lpt++].di = CNN_REFLEX;
      } else if (!std::strncmp(p,"mirror",6)) {
        abf[lpt++].di = CNN_MIRROR;
      } else if (!std::strncmp(p,"vanish",6)) {
        abf[lpt++].di = CNN_VANISH;
      } else if (!std::strncmp(p,"poison",6)) {
        abf[lpt++].di = CNN_POISON;
      } else if (!std::strncmp(p,"palsy",5)) {
        abf[lpt++].di = CNN_PALSY;
      } else if (!std::strncmp(p,"panic",5)) {
        abf[lpt++].di = CNN_PANIC;
      } else if (!std::strncmp(p,"confuse",7)) {
        abf[lpt++].di = CNN_CONFUSE;
      } else if (!std::strncmp(p,"berserk",7)) {
        abf[lpt++].di = CNN_BERSERK;
      } else if (!std::strncmp(p,"silence",7)) {
        abf[lpt++].di = CNN_SILENCE;
      } else if (!std::strncmp(p,"blind",5)) {
        abf[lpt++].di = CNN_BLIND;
      } else if (!std::strncmp(p,"passwall",8)) {
        abf[lpt++].di = CNN_PASSWALL;
      } else if (!std::strncmp(p,"raisedead",9)) {
        abf[lpt++].di = CNN_RAISEDEAD;
      } else {
        abf[lpt++].di = -1;
  snprintf(buf.data(), buf.size(),"%s at line %d: Invalid parameter \'%c\' appeared.(load_td_switch)\n", pfname, lcnt, *p);
  log_print(buf.data());
      }
      SKIP_TO_SPACE(p);
      SKIP_SPACE(p);
      abf[lpt++].dc = *p++;
      SKIP_SPACE(p);
      sscanf(p, "%i", &i);
      abf[lpt++].di = i;
      if (abf[lpt-3].di < MAX_NCN) {
        SKIP_TO_SPACE(p);
        SKIP_SPACE(p);
        if (*p++ == '*') {
          SKIP_SPACE(p);
          sscanf(p, "%i", &i);
          abf[lpt++].di = i;
        } else {
          abf[lpt++].di = 1;
        }
      }
  break;
      case 34: /* NPC効果 (PHIDM NEO extension, from MRDM npcaction) */
        abf[lpt++].di = decimal_load(2, &p);  /* 効果番号 */
        abf[lpt++].di = decimal_load(4, &p);  /* 威力/パラメータ */
  break;
      case 80: /* 職業・性別を設定 (PHIDM NEO extension) */
        abf[lpt++].di = decimal_load(2, &p);  /* 00=性別 01=職業 */
        abf[lpt++].di = decimal_load(2, &p);  /* 値 (0=未設定に戻す) */
  break;
      case 92: /* ペット (PHIDM NEO extension) */
        abf[lpt++].di = decimal_load(2, &p);  /* 動作 (0付与 1改名 2解放 3回復) */
        abf[lpt++].di = decimal_load(2, &p);  /* タイプ (動作0の時のみ: 1妖精 2猫 3犬 4鳥) */
  break;
      case 25: /* add for switch show 20010621 */
        loadstr(buf2,&p,'\n',256);
        abf[lpt++].dp = findstr(buf2);
  break;
      case 50: /* calculate expression */
      case 51: /* calculate expression (スイッチを起動しない) (PHIDM NEO extension, from JET) */
      case 52: /* calculate expression (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
        get_sentence(&p,&lpt,abf,'\0');
  break;
      case 33: /* コマンド実行 */
        if (*p == '\"' || *p == '\'') {
          p++;
          loadstr(buf2, &p,*(p-1),256);
          abf[lpt++].dp = findstr(buf2);
        }
        break;
      /* ↑ADD */
      case 0:
      case 12: /* #name再送スイッチ */
      case 24: /* remove surface */
      case 30: /* call */
      case 31:
      /* 何もなし */
      default:
  break;
      }
/*
      break;
    default:
      continue;
    }
*/
    if (lpt > 0) {
      swtd[rsdn] = (ATOM *) malloc(sizeof (ATOM) * lpt);
      if (swtd[rsdn] == nullptr) { /* 領域確保 */
  log_print("Insufficient memory for switch data\n");
  exit(1);
      }
      std::memcpy(swtd[rsdn], abf, sizeof (ATOM) * lpt);
    } else {
      swtd[rsdn] = nullptr;
    }
    rsdn++;
  }
  efclose(fp);

  return;
}

/* added by Qajin 19991021 */
void   load_td_ban()
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 256> buf2;
  int    i;
  char  *p;

  if ((fp = efopen("phi.td", "r")) == nullptr) {
    fprintf(stderr, " Can not find transfer data file. [phi.td] ");
    exit(1);
  }
  /* banned data clear added by Qajin 19991020 */
  for(i=0; i < BANNEDIPMAX;i++)
     ip_ban[i].ip = nullptr;

  std::memset(id_ban,0,sizeof(id_ban));

  for (;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':')
      continue;
    switch (buf[1]) {
      case 'B':
    p = buf.data() + 2;
      SKIP_SPACE(p);
      loadstr(buf2.data(), &p, '\n', buf2.size());
    for(i=0; i < BANNEDIPMAX && ip_ban[i].ip != nullptr;i++);
    if (i == BANNEDIPMAX) {
       fprintf(stderr, " Too many banned addresses. [phi.td] ");
       break;
    }
    ip_ban[i].ip = findstr(buf2.data());
    ip_ban[i].time = ULONG_MAX;
        break;
      case 'b': /* add for chrID ban 20030722 */
        p = buf.data() + 2;
        SKIP_SPACE(p);
        id_ban_add(p);
        break;
    }
  }
  efclose(fp);
  return;
}

/* add for ext command 20041215 */
void load_cmd()
{
  FILE  *fp;
  std::array<char, 256> buf;
  int    rcdn;
  char  *p;

    std::memset(cmdd,'\0',sizeof(cmdd));

    if ((fp = efopen("phi.cmd", "r")) == nullptr) {
      fprintf(stderr, "Warning: Can not find cmd data file. [phi.cmd]\n");
      return;
    }

    for (rcdn=0;rcdn<CMD_MAX;) {
        if (efgets(buf.data(), 255, fp) == nullptr)
            break;
        if (buf[0] != ':')
            continue;

        p = buf.data() + 2;
        switch(buf[1]) {
            case 'C':
            case 'M':
                cmdd[rcdn].type = buf[1];
                SKIP_SPACE(p);
                loadstr(buf.data(),&p,DSPACE,CMD_MAXLEN);
                CPYSTR(cmdd[rcdn].com,buf.data());
                SKIP_SPACE(p);
                switch(*p++) {
                    case 'A':
                    case 'M':
                        cmdd[rcdn].act = *(p-1);
                        cmdd[rcdn].param = decimal_load(4,&p);
                        break;
                    case 'F':
                        cmdd[rcdn].act = *(p-1);
                        cmdd[rcdn].param = flagno_load(&p);
                        break;
                    default:
                        cmdd[rcdn].act = '\0';
                        cmdd[rcdn].param = 0;
                        break;
                }
                rcdn++;
                break;
            default:
                break;
        }
    }
    efclose(fp);

}

/* add for ext command 20041215 */
int check_and_process_cmd(int n,char mode, char *com)
{
    int i;

    for (i=0;i<CMD_MAX;i++) {
        if (cmdd[i].type == mode &&
            wildcmp(cmdd[i].com,com)) {
                SKIP_TO_SPACE(com);
                SKIP_SPACE(com);
                CPYSTR(cd[n].str,com);
                switch (cmdd[i].act) {
                    case 'A':
                        cd[n].st = ST_MSGW;
                        cd[n].anmf = cmdd[i].param;
                        cd[n].anmt = 0;
                        load_mesg(n);
                        break;
                    case 'M':
                        display_menu(n,cmdd[i].param);
                        break;
                    case 'F':
                        change_flag(n,cmdd[i].param,FLAG_ON,1);
                        load_switch(n,cd[n].x,cd[n].y,10);
                        break;
                    default:
                        break;
                }
                return(1);
        }
    }
    return(0);
}

void   free_swtd()
{
  int cn;

  for(cn=0; cn<swtn; cn++)
    if(swtd[cn]!=nullptr)
      free(swtd[cn]);
  free(swtd);
  free(msgd); /* add for quick message search 20030501 */
  free(menud); /* add for menu 20041212 */
}

void   load_dd()
{
  int    x, y;
  std::array<char, MAX_X * 2 + 3> buf2;
  std::array<char, 257> buf;
  FILE  *fp;
  char  *p;
  int    i;

  for (i = 0; i < HASHMAX; i++) { /* ハッシュ領域をクリア */
    shash[i] = nullptr;
  }

  /* デフォルト条件 */
  adding_power_spell_flag = 0;  /* Power spell は乗算 */
  cirate = 100;     /* 通貨換算レート 1.00 */
  trnf = -1;      /* 連続移動許可 */
  ciflag = 0;     /* 通貨持ち込み可 */
  nciflag = 1;                  /* 税金の大きさ を GP 化 */
  ggflag = 1;                   /* NPC の乱数による巨大化境界の揺れを許す */
  addf = 0;     /* 自動登録不可 */
  wizeyef = 1;      /* PCアクセス時に Wizard light 結果を表示 */
  autoeqf = -1;     /* PCアクセス時に auto_equip 実行 */
  s_id = 1;     /* シリアルID */
  is_see_burst = 1;             /* バーストを画面上の他の PC に知らせるか */
  is_need_charge_time =0;       /* charged item に charge time を授けない */
  is_need_cast_time = 0;        /* 魔法名を記入した場合キャストタイムなし */
  is_single_use   = 0;        /* use で一度に複数個指定可能 */
  is_destroy_companion = 0;     /* destroy で何を壊したかを回りに知らせない */
  is_kill_master = 0;           /* マスターを特別視する */
  is_speed_degrade = 0;         /* 劣化の割合は以前の通り */
  is_degrade_charge = 0;        /* チャージアイテムの使用による劣化はなし */
  is_limit_mrp     = 0;        /* create による MRP を確率的にしない */
  yslide = 0;
  *yname = 0;
  /* 元は memcpy だったが、1 バイトずらした重なりコピーは未定義動作で
     ASan が起動時に異常終了する。yname は大域変数で load_dd は起動時に
     1 回だけ呼ばれ、この時点では全バイト 0 のため、memmove でも結果は
     変わらない（第13段階の判断。詳細は docs/cxx-port-notes.md） */
  std::memmove(yname+1,yname,31);
#if MAX_OBJECTS > 0
  is_see_magic_effects = 0;     /* 魔法視覚化 off */
#endif
  wall_for_beast = T_CWALL;     /* クリーチャーにとって知的生物バリアも壁 */
  magic_anti_degrade = 0;       /* 強化魔法を使っても装備劣化率は変化無し */
  spellup_only_time = 0;        /* SpellUpは効果も倍増する */
  limit_addusers = 0;           /* 制限無 */
  fighter_wizard = 0;           /* CoatとSpellUp 同時使用可能 */
  multi_access = 0;             /* 複数アクセス許可 */
  save_condition = 0;           /* コンディションは保存しない */
  lv_up_between_pc = 0;         /* PC間のレベル変動を行う */
  gift_tax = 0;                 /* payの税金は取得税 */
  offline_exp = 0;              /* 切断中の時間は経験値に加算しない */
  chr_pos_psp_compati = 0;      /* m57 turn時のキャラ位置はPSP非互換 */
  turn_permission = 0;          /* turn/solid 切替え可 */
  mapsize_only55 = 0;           /* M57 許可 */
  flagsave = 0;                 /* snフラグ非保存 */
  deny_pk = 0;                  /* PK行為許可 */
  death_penalty = 0;            /* 死亡ペナルティ有 */
  penalty_rate = 10;            /* 死亡時ペナルティ割合（デフォルト10%） add for デスペナ割合設定 by NEO 2008/03/27 */
  dp_limitter = 0;              /* デスペナリミッタ設定（デフォルトなし） add by NEO 2011/12/4 */
  dp_beginner = 0;              /* 初心者の加護設定（デフォルトなし） add by NEO 2011/12/4 */
  rebirth_life = 0;             /* 再生時HP=MAX */
  access_dm = 1;                /* アクセスメッセージは全世界放送 */
  init_graph_n = 0;             /* 初期選択キャラグラはなし */
  counter_effect = 1;           /* カウンター効果係数設定（デフォルト等倍） add by NEO 2011/12/6 */
  attack_effect = 0;            /* 新攻撃効果設定（デフォルトold） add by NEO 2011/12/6 */
  axe_blade_setting = 0;        /* 斧＆ブレード補正（デフォルトold） add by NEO 2011/12/8 */
  sys_magic_no_degrade = 0;     /* システム魔法で武器が欠けないようにするか（デフォルトする） add by NEO 2012/02/11 */
  mhp_up_for_attack = 0;        /* 通常攻撃により抽選で最大HPを増加させるか（デフォルトしない） add by NEO 2012/2/12 */
  mmp_up_for_attack = 0;        /* 魔法攻撃により抽選で最大MPを増加させるか（デフォルトしない） add by NEO 2012/2/12 */
  spell_learning = 0;           /* 魔法ラーニング設定（デフォルト） add by NEO 2012/2/12 */
  degrade_by_magic = 0;         /* 魔法ダメージでも鎧・アクセが欠けるか（デフォルトなし） add by NEO 2012/2/12 */

  fld = nullptr;
  for (i = 0; i < 23; roofoff[i++] = '\0');
  CPYSTR(cirunit, "gp");
  std::memset(init_graphic, '\0', sizeof(init_graphic));

  for (y = 0; y < MAX_Y; y++)
    for (x = 0; x < MAX_X; x++) {
      map[y][x].tip = (T_MWALL | T_ROOF);
#if MAX_OBJECTS > 0
      dobj[y][x] = 0;
#endif
    }

  if ((fp = efopen("phi.dd", "rb")) == nullptr)
    return;
  for (y = 0; y < MAX_Y; y++) {
    if (efgets(buf2.data(), MAX_X * 2 + 2, fp) == nullptr)
      break;
    p = buf2.data();
    for (x = 0; x < MAX_X; x++, p += 2) {
      map[y][x].tip = set_dd(p);
      map[y][x].height = dd_height_of(p);
    }
  }
  for (x = 0; x < MAX_X; x++)
    map[0][x].tip = map[MAX_Y - 1][x].tip = (T_MWALL | T_ROOF);
  for (y = 0; y < MAX_Y; y++)
    map[y][0].tip = map[y][MAX_X - 1].tip = (T_MWALL | T_ROOF);
  for (x = 0; x < MAX_X; x++)
    map[0][x].height = map[MAX_Y - 1][x].height = 0;
  for (y = 0; y < MAX_Y; y++)
    map[y][0].height = map[y][MAX_X - 1].height = 0;
  while(1) {
    if (efgets(buf.data(), buf.size(), fp) == nullptr)
      break;
    if (*buf.data() != ':' || buf[1] != 'T')
      continue;
    p = buf.data()+2;
    x = decimal_load(3,&p);
    y = decimal_load(3,&p);
    if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y) {
      SKIP_SPACE(p);
      map[y][x].surface = (unsigned char)(set_dd(p) & TMASK);
      map[y][x].tip |= T_SURFACE;
    }
  }
  efclose(fp);
  for (i = 0; i < MAX_POWER_SPELL; i++) {
    power_spell[i].pwmg.spell = nullptr;
    power_spell[i].pwmg.extension = nullptr;
  }
  if ((fp = efopen("phi.td", "rb")) == nullptr) {
    fprintf(stderr, " Can not find transfer data file. [phi.td] ");
    exit(1);
  }
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      continue;
    p = buf.data() + 2;
    switch (*(buf.data() + 1)) {
    case 'O':
      for (;;) {
  SKIP_SPACE(p);
  if (*p == '\0' || *p == '\n')
    break;
  switch (*(p++)) {
  case 'N':
    nciflag = 0;
    break;
  case 'G':
    ggflag = 0;
    break;
  case 'A':
    addf = -1;
    addx = decimal_load(3, &p);
    addy = decimal_load(3, &p);
    SKIP_SPACE(p);
    loadstr(addw, &p, ' ', 5);
    break;
  case 'T':
    trnf = 0;
    break;
  case 'O':   /* サーバのポート(既に読み込み済み) */
    SKIP_SPACE(p);
    SKIP_TO_SPACE(p);
    break;
  }
      }
      break;
    case 'U':
      for (;;) {
  SKIP_SPACE(p);
  if (*p == '\0' || *p == '\n')
    break;
  switch (*(p++)) {
  case 'A':
    SKIP_SPACE(p);
    if (*p == 'X')      autoeqf = 0;
    else if (*p == 'O') autoeqf = -1;
    else if (*p == 'D') access_dm = 2;
    if (*p == 'B') {  /* add for 斧＆ブレード補正 by NEO 2011/12/8 */
        p++;
        SKIP_SPACE(p);
	    if (*p == 'X')      axe_blade_setting = 0;
	    else if (*p == 'O') axe_blade_setting = 1;
    }
    if (*p == 'E') {  /* add for 新攻撃効果設定 by NEO 2011/12/6 */
        p++;
        SKIP_SPACE(p);
        switch (*p) {
            case '1':
                attack_effect = 1;
                break;
            case '2':
                attack_effect = 2;
                break;
            case '0':
            default:
                attack_effect = 0;
                break;
        }
    }
    p++;
    break;
  case 'B':
    SKIP_SPACE(p);
    if(*p=='X') is_see_burst =0;
    p++;
    break;
  case 'C':
    SKIP_SPACE(p);
    if(*p=='T')      is_need_charge_time =1;
    else if(*p=='D') is_degrade_charge = 1;
    if(*p=='E') {  /* add for カウンター効果係数設定 by NEO 2011/12/6 */
        p++;
        SKIP_SPACE(p);
        counter_effect = decimal_load(1,&p);
        break;
    }
    p++;
    break;
  case 'D':
    SKIP_SPACE(p);
    if(*p=='C') is_destroy_companion = 1;
    if(*p=='P') {
        p++;
        SKIP_SPACE(p);
        death_penalty = decimal_load(1,&p);
        break;
    }
    /* add for デスペナ割合設定 by NEO 2008/03/27 */
    if(*p=='R') {
        p++;
        SKIP_SPACE(p);
        switch (*p) {
            case '1':
                penalty_rate = 1;
                break;
            case '2':
                penalty_rate = 2;
                break;
            case '3':
                penalty_rate = 3;
                break;
            case '5':
                penalty_rate = 5;
                break;
            case '0':
            default:
                penalty_rate = 10;
                break;
        }
    }
    /* add for control re-birth life 20020304 */
    if(*p=='L') {
        p++;
        SKIP_SPACE(p);
        switch (*p) {
            case '1':
                rebirth_life = 1;
                break;
            case '2':
                rebirth_life = 2;
                break;
            case 'M':
            default:
                rebirth_life = 0;
                break;
        }
    }
    /* add for 初心者の加護設定 by NEO 2011/12/4 */
    if(*p=='S') {
        p++;
        SKIP_SPACE(p);
	    if (*p == 'X')      dp_beginner = 0;
	    else if (*p == 'O') dp_beginner = 1;
    }
    /* add for デスペナリミッタ設定 by NEO 2011/12/4 */
    if(*p=='T') {
        p++;
        SKIP_SPACE(p);
	    if (*p == 'X')      dp_limitter = 0;
	    else if (*p == 'O') dp_limitter = 1;
    }
    if(*p=='M') degrade_by_magic = 1; /* 魔法ダメージでも鎧・アクセが欠ける add by NEO 20120212 */
    p++;
    break;
  /* add for creature barrir 20000607 */
  case 'E':
    SKIP_SPACE(p);
    if(*p=='X') wall_for_beast = 0;
    else if(*p=='B') wall_for_beast = T_CWALL;
    p++;
    break;
  /* add for fighter/wizard option 20000713 */
  case 'F':
    SKIP_SPACE(p);
    if(*p=='W') {
        p++;
        SKIP_SPACE(p);
        if(*p=='X') fighter_wizard = -1;
        if(*p=='0') fighter_wizard = 1;
    }
    p++;
    break;
  case 'H':
    SKIP_SPACE(p);
    if(*p=='U') mhp_up_for_attack = 1; /* 通常攻撃により抽選で最大HPを増加させる add by NEO 2012/2/12 */
    p++;
    break;
  case 'I':
    SKIP_SPACE(p);
    if(*p=='S') is_speed_degrade = 1;
    p++;
    break;
  case 'K':
    SKIP_SPACE(p);
    if(*p=='M') is_kill_master = 1;
    p++;
    break;
  case 'L':
    SKIP_SPACE(p);
    if(*p=='M') is_limit_mrp = 1;
    if(*p=='U') limit_addusers = 1;
    if(*p=='V') {
        p++;
        if(*p=='K') { lv_up_between_pc = 4; p++; }
        if(*p=='P') lv_up_between_pc |= 1;
        if(*p=='2') lv_up_between_pc |= 2;
    }
    p++;
    break;
  case 'M':
    SKIP_SPACE(p);
    if(*p=='T') is_need_cast_time = 1;
#if MAX_OBJECTS > 0
    if(*p=='S') is_see_magic_effects = 1;
    if(*p=='X') is_see_magic_effects = 0;
#endif
    if(*p=='D') magic_anti_degrade = 1;
    if(*p=='U') mmp_up_for_attack = 1; /* 魔法攻撃により抽選で最大MPを増加させる add by NEO 2012/2/12 */
    if(*p=='A') {
        p++;
        SKIP_SPACE(p);
        multi_access = decimal_load(1,&p);
        break;
    }
    p++;
    break;
  case 'P':
    SKIP_SPACE(p);
    if(*p=='A') adding_power_spell_flag=1;
    if(*p=='K') {
        p++;
        if(*p=='X') deny_pk = 1;
    }
    p++;
    break;
  case 'R':
    SKIP_SPACE(p);
    if (*p == '\"') {
      for (i = 0, p++; i < 22; i++, p++) {
        if (*p == '\"' || *p == '\n' || *p == '\0')
    break;
        roofoff[i] = *p;
      }
      for (; i < 23; i++)
        roofoff[i] = '\0';
    }
    if (*p != '\n' && *p != '\0')
      p++;
    break;
  case 'S':
    SKIP_SPACE(p);
    if(*p=='U') is_single_use = 1;
    if(*p=='T') spellup_only_time = 1;
    if(*p=='C') save_condition = 1;
    if(*p=='V') flagsave = 1;
    if(*p=='D') sys_magic_no_degrade = 1; /* システム魔法で武器が欠けないようにする added by NEO 20120211 */
    if(*p=='L') { /* 魔法ラーニング設定 added by NEO 20120212 */
        p++;
        if(*p=='D') spell_learning = 1; /* 魔法ラーニング完全禁止 */
        if(*p=='N') spell_learning = 2; /* NPCからのみ魔法ラーニング可能 */
        }
        /* ここまで by NEO 20120212 */
    p++;
    break;
  case 'U':
    SKIP_SPACE(p);
    if(*p=='O') npc_use_charged_item=1;
    p++;
    break;
  case 'W':
    wizeyef = 0;
    SKIP_SPACE(p);
    if (*p == 'E')      wizeyef = 0;
    else if (*p == 'L') wizeyef = 1;
    p++;
    break;
  case 'T': /* add for gift tax 20001204 */
    SKIP_SPACE(p);
    if(*p=='G') gift_tax = 1;
    p++;
    break;
  case 'O': /* add for offline exp 20001206 */
    SKIP_SPACE(p);
    if(*p=='E') offline_exp = 1;
    p++;
    break;
  case '7': /* add for m57 compatible as psp */
    SKIP_SPACE(p);
    if(*p=='C') chr_pos_psp_compati = 1;
    if(*p=='T') turn_permission = 1;
    if(*p=='S') turn_permission = -1;
    if(*p=='W') turn_permission = 0;
    if(*p=='X') mapsize_only55 = 1;
    p++;
    break;
  }
      }
      break;
    case 'N':
      SKIP_SPACE(p);
      CPYSTR(land_name, p);
      for (i = 0; i < 32 && land_name[i] >= ' '; i++);
      land_name[i] = 0;
      break;
    case 'T':
      SKIP_SPACE(p);
      sscanf(p, "%i", &tslide);
      for (; tslide < 0; tslide += 24);
      for (; tslide > 23; tslide -= 24);
      break;
    case 't':  /* 西暦との差 */
       SKIP_SPACE(p);
       sscanf(p, "%i", &yslide);
       for (; *p && *p != ' '; p++);  /* スペースまでをスキップ */
       SKIP_SPACE(p);
       loadstr(yname, &p, '\n', 32);
       break;
/* delete for non-connected boot up
    case 'G':
      x = decimal_load(3, &p);
      y = decimal_load(3, &p);
      if (x >= 1 && x < MAX_X - 1 && y >= 1 && y < MAX_Y - 1) {
  map[y][x].tip &= TCLEAR;
  map[y][x].tip |= T_TGATE;
      }
      break;
*/
    case 'R':
      SKIP_SPACE(p);
      if (*p == 'X') {
  ciflag = -1;
  p++;
      }
      cirate = decimal_load(4, &p);
      if (cirate == 0) {
  cirate = 100;
  break;
      }
      SKIP_SPACE(p);
      for (i = 0; i < 19; i++) {
  cirunit[i] = *p;
  if (*p == '\n' || *p == '\0') {
    cirunit[i] = '\0';
    break;
  }
  p++;
      }
      break;
    case 'M':
      SKIP_SPACE(p);
      for (i = 0; i < MAX_POWER_SPELL && power_spell[i].pwmg.spell != nullptr; i++);
      if (i < MAX_POWER_SPELL) {
  sscanf(p, "%i", &(power_spell[i].multiplier));
  if(adding_power_spell_flag == 0 && power_spell[i].multiplier == -1)
        power_spell[i].multiplier = 1;
  SKIP_TO_SPACE(p);
  SKIP_SPACE(p);
  if (!load_spell(&(power_spell[i].pwmg),p))
    power_spell[i].pwmg.spell = nullptr;
#ifdef DEBUG
snprintf(debug_buf,sizeof(debug_buf),"power spell:%02i %s %02i %i * %i",i,
    power_spell[i].pwmg.spell,power_spell[i].pwmg.effect,
    power_spell[i].pwmg.fn,power_spell[i].multiplier);
log_print(debug_buf);
#endif
      }
      break;
    case 'F':
      if (init_graph_n < MAX_INIT_GRAPHIC) {
          SKIP_SPACE(p);
          loadstr(init_graphic[init_graph_n],&p,'\n',32);
          init_graph_n++;
      }
      break;
    }
  }
  efclose(fp);
  /* delete for hostname problem 20021016 */
  /* get_ip_address(buf, sizeof(buf)); */ /*change for BOF*/
  /*
  if((server_ip_address=strdup(buf)) == NULL){
    fprintf(stderr," Insufficient Memory\n");
    exit(1);
  }
  */
  /* change sequence for boot up on non-connected */
  load_td_gate();
  put_gaten(0,0);
  remap_gate_and_switch();
  remap_lock_and_board(0);
  gfn = 0;
  gfn2 = 0;
  for (i = 0; i < GFLAG_MAX; i++) { /* 汎用フラグ初期化 */
    generic_flag[i] = FLAG_OFF;
  }
  load_td_swt();
  load_td_ban();  /* added by Qajin 19991021 */
  load_id();
  load_ed_o();
  load_fd();
  load_crt();
  load_ex();
#if MAX_OBJECTS > 0
  load_obj();
#endif
  load_cmd();
  remap_gate_and_switch(); /* add 20000803 */
  skill_init(); /* add 20020307 */
  return;
}

/* 看板・ロック状態の初期化 */
/* added by Qajin 20000728  */
void   remap_lock_and_board(int f)
{
  int    x, y;
  std::array<char, 257> buf;
  FILE  *fp;
  char  *p;
  int    i;

  for (y = 0; y < MAX_Y; y++)
    for (x = 0; x < MAX_X; x++) {
      map[y][x].tip &= ~T_MESSAGE;
      switch (map[y][x].tip & TMASK) {
      case T_LBOX:
        map[y][x].tip &= ~T_LBOX;
        map[y][x].tip |= T_BOX;
        break;
      case T_LGATE:
        map[y][x].tip &= ~T_LGATE;
        map[y][x].tip |= T_GATE;
        break;
      case T_LSGATE:
        map[y][x].tip &= ~T_LSGATE;
        map[y][x].tip |= T_SGATE;
        break;
      case T_LMSGATE:
        map[y][x].tip &= ~T_LMSGATE;
        map[y][x].tip |= T_MSGATE;
        break;
      }
    }

#ifdef SDI_AREA
  for (i=0;i<0x100;i++) {
    snprintf(buf.data(),buf.size(),"phi_sdi/sd.%02x",i);
    if ((fp = efopen(buf.data(), "rb")) != nullptr) {
#else
  if ((fp = efopen("phi.sd", "rb")) == nullptr) {
    fprintf(stderr, " Can not find system data file [phi.sd]. ");
    exit(1);
  }
#endif

  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      break;
    p = buf.data() + 2;
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (x >= 1 && x < MAX_X - 1 && y >= 1 && y < MAX_Y - 1)
      switch (*(buf.data() + 1)) {
      case 'G':
  SKIP_SPACE(p);
  if (!f)
    sscanf(p, "%li", &tax);
  break;
      case 'T':
      case 'B':
  map[y][x].tip |= T_MESSAGE;
  break;
      case 'L':
  switch (map[y][x].tip & TMASK) {
  case T_BOX:
    map[y][x].tip &= ~T_BOX;
    map[y][x].tip |= T_LBOX;
    break;
  case T_GATE:
    map[y][x].tip &= ~T_GATE;
    map[y][x].tip |= T_LGATE;
    break;
  case T_SGATE:
    map[y][x].tip &= ~T_SGATE;
    map[y][x].tip |= T_LSGATE;
    break;
  case T_MSGATE:
    map[y][x].tip &= ~T_MSGATE;
    map[y][x].tip |= T_LMSGATE;
    break;
  }
  break;
      case 'I':
  if (!f)
    sscanf(p, "%li", &s_id);
  break;
      }
  }
  efclose(fp);
#ifdef SDI_AREA
  }
 }
#endif

}

void   extension_dd(int n)
/* n は マスター権限を持つ PC。*/
/* save_sd を利用するために必要 */
{
  FILE *fp;
  std::array<char, MAX_X * 2 + 3> buf2;
  std::array<char, 100> buf;
  int x,y;
  char  *p;

  if ((fp = efopen("phi.ddx", "rb")) == nullptr)
    return;
  for (y = 0; y < MAX_Y-1; y++) {
    if (efgets(buf2.data(), MAX_X * 2 + 2, fp) == nullptr)
      break;
    if(y == 0) continue;
    for (x = 0, p = buf2.data();
   x < MAX_X-1 && *p!='\0' && *p!='\n' && *(p+1)!='\0' && *(p+1)!='\n';
   x++, p += 2){
      if(x == 0) continue;
      if(fld[map[y][x].area].flag & FF_EXTENSION){
  if((map[y][x].tip & TMASK) == T_LBOX ||
     (map[y][x].tip & TMASK) == T_LGATE ||
     (map[y][x].tip & TMASK) == T_LSGATE ||
     (map[y][x].tip & TMASK) == T_LMSGATE)
    save_sd(n, x, y, 'L', nullptr, 1);
  map[y][x].tip = (map[y][x].tip & (TCLEAR ^ (T_ROOF | T_SURFACE))) | set_dd(p);
  map[y][x].height = dd_height_of(p);
      }
    }
  }
  while(1) {
    if (efgets(buf.data(), buf.size(), fp) == nullptr)
      break;
    if (*buf.data() != ':' || buf[1] != 'T')
      continue;
    p = buf.data()+2;
    x = decimal_load(3,&p);
    y = decimal_load(3,&p);
    if (x >= 0 && x < MAX_X && y >= 0 && y < MAX_Y) {
      SKIP_SPACE(p);
      map[y][x].surface = (unsigned char)(set_dd(p) & TMASK);
      map[y][x].tip |= T_SURFACE;
    }
  }
  efclose(fp);
  check_id_in_circle(); /* bug fix for re-appear circle items 20031213 */
}

/* dd 2バイト目の高さ印 (PHIDM NEO v1.13)。
 * '1'=+1段 '2'=+2段 '3'=+3段 '9'=-1段(窪地)。それ以外は 0。
 * 高さは見た目属性で、通行・魔法等の判定には使わない。 */
signed char dd_height_of(char *p)
{
  switch (*(p + 1)) {
  case '1':
    return 1;
  case '2':
    return 2;
  case '3':
    return 3;
  case '9':
    return -1;
  }
  return 0;
}

/* 高さ値 (-1..3) から印文字へ。0 は '0' を返す */
char dd_height_char(int h)
{
  switch (h) {
  case 1:
    return '1';
  case 2:
    return '2';
  case 3:
    return '3';
  case -1:
    return '9';
  }
  return '0';
}

unsigned short set_dd(char *p)
/* 地形種類 ファイルから内部記述子へ */
{
  unsigned long d;

  switch (*p) {
  default:
    d = T_SPACE;
    break;
  case '*':
    d = T_ANTIMAGIC;
    break;
  case 'o':
    d = T_PPLATE;
    break;
  case ':':
    d = T_PLANT;
    break;
  case '+':
    d = T_FLOWER;
    break;
  case '_':
    d = T_WATER;
    break;
  case '^': /* add 20000627 */
    d = T_BWATER;
    break;
  case 'x':
    d = T_BOX;
    break;
  case '/':
    d = T_MIST;
    break;
  case '.':
    d = T_EWALL;
    break;
  case '[':
    d = T_GATE;
    break;
  case 'Z':
    d = T_MSGATE;
    break;
  case '$':
    d = T_SGATE;
    break;
  case '!':
    d = T_CWALL;
    break;
  case 'a': /* add 20000711 */
    d = T_AWALL;
    break;
  case 'I':
    d = T_JAIL;
    break;
  case '|':
    d = T_WINDOW;
    break;
  case 'T':
    d = T_TREE;
    break;
  case '=':
    d = T_GLASS;
    break;
  case 'H':
    d = T_MWALL;
    break;
  case '#':
    d = T_WALL;
    break;
  case '@':
    d = T_ROCK;
    break;
  case '{':
    d = T_LGATE;
    break;
  case '%':
    d = T_LBOX;
    break;
  case 's': /* add for storehouse 20020823 */
    d = T_STORE;
    break;
  }
  if (*(p + 1) == '~')
    d |= T_ROOF;
  return (d);
}

int    save_dd()
{
  int    x, y;
  std::array<char, MAX_X * 2 + 2> buf;
  std::array<char, 100> buf2;
  FILE  *fp;
  char  *p;

  if ((fp = efopen("phi.dd", "wb")) == nullptr)
    return (1);
  for (y = 0; y < MAX_Y; y++) {
    for (x = 0, p = buf.data(); x < MAX_X; x++, p += 2) {
      switch (map[y][x].tip & TMASK) {
      default:
  *p = ' ';
  *(p + 1) = ' ';
  break;
      case T_ANTIMAGIC:
  *p = '*';
  *(p + 1) = '*';
  break;
      case T_PPLATE:
  *p = 'o';
  *(p + 1) = ' ';
  break;
      case T_PLANT:
  *p = ':';
  *(p + 1) = ':';
  break;
      case T_FLOWER:
  *p = '+';
  *(p + 1) = ':';
  break;
      case T_WATER:
  *p = '_';
  *(p + 1) = '_';
  break;
      case T_BWATER:
  *p = '^';
  *(p + 1) = '^';
  break;
      case T_BOX:
      case T_LBOX:
  *p = 'x';
  *(p + 1) = 'x';
  break;
      case T_MIST:
  *p = '/';
  *(p + 1) = '/';
  break;
      case T_EWALL:
      case T_TGATE:
  *p = '.';
  *(p + 1) = '.';
  break;
      case T_GATE:
      case T_LGATE:
  *p = '[';
  *(p + 1) = ']';
  break;
      case T_SGATE:
      case T_LSGATE:
  *p = '$';
  *(p + 1) = '$';
  break;
      case T_CWALL:
  *p = '!';
  *(p + 1) = '!';
  break;
      case T_AWALL:
  *p = 'a';
  *(p + 1) = ' ';
  break;
      case T_JAIL:
  *p = 'I';
  *(p + 1) = 'I';
  break;
      case T_WINDOW:
  *p = '|';
  *(p + 1) = '|';
  break;
      case T_TREE:
  *p = 'T';
  *(p + 1) = 'T';
  break;
      case T_GLASS:
  *p = '=';
  *(p + 1) = '=';
  break;
      case T_MWALL:
  *p = 'H';
  *(p + 1) = 'H';
  break;
      case T_MSGATE:
      case T_LMSGATE:
  *p = 'Z';
  *(p + 1) = 'Z';
  break;
      case T_WALL:
  *p = '#';
  *(p + 1) = '#';
  break;
      case T_ROCK:
  *p = '@';
  *(p + 1) = '@';
  break;
      case T_UNKNOWN:
  *p = '?';
  *(p + 1) = '?';
  break;
      case T_STORE: /* add for storehouse 20020823 */
  *p = 's';
  *(p + 1) = 's';
  break;
      }
      if (map[y][x].tip & T_ROOF)
  *(p + 1) = '~';
      /* 高さ印は屋根より優先 (2バイト目は 1文字だけ) */
      if (map[y][x].height != 0)
  *(p + 1) = dd_height_char(map[y][x].height);
    }
    *(p++) = '\n';
    *p = 0;
    fputs(buf.data(), fp);
  }
  for (y = 0; y < MAX_Y; y++) {
    for (x = 0; x < MAX_X; x++) {
      if (map[y][x].tip & T_SURFACE) {
        snprintf(buf2.data(),buf2.size(),":T %03d %03d   \n",x,y);
        p = buf2.data()+11;
      switch (map[y][x].surface) {
      default:
      case T_ANTIMAGIC:
      case T_EWALL:
      case T_CWALL:
      case T_AWALL:
  *p = ' ';
  *(p + 1) = ' ';
  break;
      case T_PPLATE:
  *p = 'o';
  *(p + 1) = ' ';
  break;
      case T_PLANT:
  *p = ':';
  *(p + 1) = ':';
  break;
      case T_FLOWER:
  *p = '+';
  *(p + 1) = ':';
  break;
      case T_WATER:
      case T_BWATER:
  *p = '_';
  *(p + 1) = '_';
  break;
      case T_BOX:
  *p = 'x';
  *(p + 1) = 'x';
  break;
      case T_LBOX:
  *p = '%';
  *(p + 1) = '%';
  break;
      case T_MIST:
  *p = '/';
  *(p + 1) = '/';
  break;
      case T_TGATE:
  *p = '>';
  *(p + 1) = '<';
  break;
      case T_GATE:
  *p = '[';
  *(p + 1) = ']';
  break;
      case T_LGATE:
  *p = '{';
  *(p + 1) = '}';
  break;
      case T_JAIL:
  *p = 'I';
  *(p + 1) = 'I';
  break;
      case T_WINDOW:
  *p = '|';
  *(p + 1) = '|';
  break;
      case T_TREE:
  *p = 'T';
  *(p + 1) = 'T';
  break;
      case T_GLASS:
  *p = '=';
  *(p + 1) = '=';
  break;
      case T_MWALL:
      case T_MSGATE:
      case T_LMSGATE:
  *p = 'H';
  *(p + 1) = 'H';
  break;
      case T_WALL:
      case T_SGATE:
      case T_LSGATE:
  *p = '#';
  *(p + 1) = '#';
  break;
      case T_ROCK:
  *p = '@';
  *(p + 1) = '@';
  break;
      case T_UNKNOWN:
  *p = '?';
  *(p + 1) = '?';
  break;
      case T_STORE: /* add for storehouse 20020823 */
  *p = 's';
  *(p + 1) = 's';
  break;
      }
    fputs(buf2.data(), fp);
      }
    }
  }
  efclose(fp);
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"save_dd complete");
  log_print(debug_buf);
#endif
  return (0);
}

void   list_gate(int n)
{
  int    x, y;
  std::array<char, 256> buf;
  int    i,cn;

  for (cn = 0,i = 0;cn<gatemax;cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'G')
      continue;
    x = gatedata[cn][1].di;
    y = gatedata[cn][2].di;
    if (x != cd[n].x || y != cd[n].y)
      continue;
    i++;
    snprintf(buf.data(),buf.size(), "[%2i] %-31s \n", i, gatedata[cn][5].dp);
    check_write(n, buf.data());
  }
  i++;
  snprintf(buf.data(), buf.size(),"[%2i] CANCEL \n", i);
  check_write(n, buf.data());
  return;
}

void   load_gate(int n, int gn)
{
  std::array<char, 256> wbuf;
  char   *pb;
  std::array<char, 32> nm;
  /* land は CPYSTR の宛先のため C 配列のまま */
  char   land[32];
  int    x, y;
  int    i, j, l, port;
  int    cn;

  if (n >= MAX_NTTYS)
    return;

  for (i = 0, cn = 0;cn < gatemax;cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'G' && gatedata[cn][0].di != 'g')
      continue;
    x = gatedata[cn][1].di;
    y = gatedata[cn][2].di;
    if (x != cd[n].x || y != cd[n].y)
      continue;
    if ((++i) < gn)
      continue;
    /* add area flag & bug fix 20000630 */
    if (gatedata[cn][6].dp[0] != '-') {
      if (map[y][x].tip & T_SWITCH)
        load_switch(n, x, y, 2);
      if ((j = fld[map[y][x].area].fout) >= 0) {
        change_flag(n,j,FLAG_ON,1);
        load_switch(n,cd[n].x,cd[n].y,10);
      }
    }
    clear_chr(n);
    x = gatedata[cn][3].di;
    y = gatedata[cn][4].di;
    pb = gatedata[cn][5].dp;
    loadstr(cd[n].str, &pb, DSPACE, 32);
    dmprintf(n,0,wbuf.data(), wbuf.size(),DMM_TRANS, cd[n].str);
    check_write(n, wbuf.data());
    if (gatedata[cn][6].dp[0] == '-') {
      if(!export_item_check(n, cd[n].x, cd[n].y, cd[n].str, "-",'G'))
  return;
      if(!export_item_check(n, x, y, cd[n].str, "-",'g')) /* add for import check 20001128 */
  return;
      cd[n].x = x;
      cd[n].y = y;
      set_chr(n);
      print_map(n, 0);
      dmcheck_write(n, DMM_TRANS_COMP);
      CN_GASIFY(n) = 0x10000;
      return;
    } else {
      if (cd[n].exp >= 99999999) { /* change for un-trans bug made master 20041117 */
  dmcheck_write(n, DMM_TRANS_NO_MASTER);
  return;
      }
      pb = gatedata[cn][6].dp;
      loadstr(nm.data(), &pb, ' ', 32);/* ip */
      if(!export_item_check(n, cd[n].x, cd[n].y, cd[n].str, nm.data(), 'G'))
  return;
      SKIP_SPACE(pb);
      port = std::atoi(pb);    /* port */
      if ((port > 0) && port != SERV_TCP_PORT) {
        /*　if(gatedata[cn][0].di != 'g')  bug fix 20050516 */
        /*  gatedata[cn][7].dl = 0;  非標準ポートは自動削除対象外 change 20020811 */
        if (cd[n].cv <= 0) {
  dmcheck_write(n, DMM_TRANS_NO_OLD_CLIENT);
  return;
      }
    }
      if (ciflag) {
  l = 0;
  item_gc(n); /* 念の為… */
  for (j = 0; j < MAX_ITEMS; j++) {
    if (cd[n].id[j].type == DT_NONE)
      break;
    if ((cd[n].id[j].type & IT_MASK) == IT_FOREIGN) {
      l += cd[n].id[j].VALUE * 100 + cd[n].id[j].VALUE2;
    }
  }
  if (l >= 10000) {
    dmcheck_write(n, DMM_TRANS_NO_TOO_CUR);
    return;
  }
  if (cd[n].gp > 0) {
    if (j == MAX_ITEMS && l == 0) {
      dmcheck_write(n, DMM_TRANS_NO_TOO_ITEMS);
      return;
    }
    j = cd[n].gp * 100 / cirate;
    if (j > 9999) {
      dmcheck_write(n, DMM_TRANS_NO_TOO_MONEY);
      return;
    }
  }
      }

      /**** バージョンチェック ****/
      if ((j = connect_server(nm.data(), ((port<=0)?SERV_TCP_PORT:port))) < 0) {
  /* 他サーバの接続に失敗 */
  dmcheck_write(n, DMM_TRANS_NO_MISS_DEST);
  snprintf(wbuf.data(), wbuf.size(),"trs-err  : dead gate : %s", cd[n].str);
  log_print(wbuf.data());
  /* dead gate close check added by Qajin 20000116 */
  if (gatedata[cn][0].di == 'G') {          /* 隠しゲートは閉じない */
     if (!(gatedata[cn][7].dl)) {
        gatedata[cn][7].dl = time(nullptr); /* エラー発生時間記録 */
     } else if (time(nullptr)-gatedata[cn][7].dl > 60*1){ /* ゲート削除経過時間を１分に変更　by NEO 20061014 old 60*60*24*30 */
        CPYSTR(land, cd[n].str);
        CPYSTR(cd[n].str, nm.data());
        snprintf(wbuf.data(),wbuf.size()," %03i %03i %03i %03i %-31s  %s %05i \n",
              gatedata[cn][1].di,gatedata[cn][2].di,x,y,land,cd[n].str,port);
        get_gate(wbuf.data(), 1, n);               /* phi.td からも消す */
        snprintf(wbuf.data(), wbuf.size(),"g-delete  : %s", land);
        log_print(wbuf.data());
        dmprintf(n,0,wbuf.data(),wbuf.size(),DMM_GATE_DELETE,land);
        write_companion(0, 0, wbuf.data(), 1);
        clear_gate_and_switch_on_map();
        remap_gate_and_switch();
     }
  }
  return;
      }

      if (j == 0) {
  /* 他サーバに繋いだは良いがソケットフル状態 */
  dmcheck_write(n, DMM_CONNECT_NOOPEN);
  log_print("trs-err  : no more connection");
  write_line(j, "#x\n", 3, 0);
  nfflush(j, 5000);
  nclose(j);
  return;
      }
      cd[j].fd = fd_check(j);
      cd[j].name = findstr(cd[n].str);
      cd[j].st = ST_CONNECTSERVER;
      cd[j].type = CT_NOEXIST;
      cd[j].wt = 100;
      cd[j].vf =n;
      cd[j].vf2 = cn; /* add 20000523 Error時のゲート削除の為 */
      cd[j].extension = nullptr; /* bug fix 20010216 */
      CPYSTR(cd[j].uid, cd[n].uid);
      check_write(j, "#version-cli 0\n"); /* 0 はダミー */
      if (port<=0)
  snprintf(cd[j].str, sizeof(cd[j].str),"#ch-srv %s\n", nm.data());
      else
  snprintf(cd[j].str, sizeof(cd[j].str),"#ch-srv %s %05i\n", nm.data(), port);

      while (!backup_cd(n));
      while (!save_sav(n)); /* for saved flags 20010622 */
      qstr_save(n); /* PHIDM NEO extension: phi.qst string store */
      pet_save(n);  /* PHIDM NEO extension: phi.pet pet store */
      while (!save_cd(n, 1));
      cd[n].type = CT_NOEXIST;
      cd[n].st = ST_TRYRESERVE; /* ST_TRANSFER; change against abnomal process sequence 20000725 */
      cd[n].wt = 600;
      cd[n].ix = x;
      cd[n].iy = y;
      cd[n].vf = j;
      return;
    }
  }
  return;
}

int    get_gate(char *p, int f, int n)
{
  FILE  *rfp, *wfp;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char   buf[256];
  std::array<char, 32> nm, nm1, nm2;
  char   *t;
  std::array<char, 32> password;
  int    x, y, xx, yy, xxx, yyy, i;

  t = p;
  x = decimal_load(3, &t);
  y = decimal_load(3, &t);
  xx = decimal_load(3, &t);
  yy = decimal_load(3, &t);
  SKIP_SPACE(t);
  loadstr(nm1.data(), &t, DSPACE, 32); /* land name */
  SKIP_SPACE(t);
  loadstr(nm.data(), &t, ' ', 32); /* ip address */
  SKIP_SPACE(t);
  i = std::atoi(t);      /* port */
  if (i <= 0)
    i = SERV_TCP_PORT;

  password[0] = '\0';
  for (; *t != 'P' && *t != '\0'; t++);
  if (!std::strncmp(t, "P:", 2)) { /* password */
    t += 2;
    loadstr(password.data(), &t, '\n', 32);
  }
  if (!f) {     /* #gate の場合 */
    if (!shift_gate(&x, &y, authorize(password.data()), nm1.data(), nm.data())) {
      /* 被ゲート指定に合わない */
      return (-2);
    }
  }
  if ((rfp = efopen("phi.td", "rb")) == nullptr)
    return (-1);
  if ((wfp = efopen("phi.td.bak", "wb")) == nullptr) {
    efclose(rfp);
    return (-1);
  }
  while (1) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (buf[0]=='.' && (buf[1]=='\n' || buf[1]=='\0'))
      continue;
    fputs(buf, wfp);
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  efclose(wfp);
  efclose(rfp);
  if ((rfp = efopen("phi.td.bak", "rb")) == nullptr)
    return (-1);
  if ((wfp = efopen("phi.td", "wb")) == nullptr) {
    efclose(rfp);
    return (-1);
  }
  for (;;) {
    if (efgets(buf, 255, rfp) == nullptr)
      break;
    if (buf[0] == '.' && (buf[1] == '\n' || buf[1] == '\0'))
      continue;
    if (buf[0] == ':' && buf[1] == 'G') {
      t = buf + 19;
      loadstr(nm2.data(), &t, DSPACE, 32);
      if (!std::strcmp(nm1.data(), nm2.data())) {
  SKIP_SPACE(t);
  loadstr(nm2.data(), &t, ' ', 32);
  if (!std::strcmp(cd[n].str, nm2.data())){
    t = buf + 2;
    xxx = decimal_load(3, &t);
    yyy = decimal_load(3, &t);
    if((map[yyy][xxx].tip & TMASK) == T_TGATE)
      map[yyy][xxx].tip = (map[yyy][xxx].tip & TCLEAR) | T_EWALL ;
    continue;
    /* IP が違う場合は消しません */
  }
      }
    }
    fputs(buf, wfp);
  }

  if (!f) {
    if (i == SERV_TCP_PORT) {
      snprintf(buf, sizeof(buf),":G %03i %03i %03i %03i %-31s  %s\n"
        ,x, y, xx, yy, nm1.data(), cd[n].str);
    } else {
      snprintf(buf, sizeof(buf),":G %03i %03i %03i %03i %-31s  %s %05i\n"
        ,x, y, xx, yy, nm1.data(), cd[n].str, i);
    }
    fputs(buf, wfp);
  }
  CPYSTR(buf, ".\n");
  fputs(buf, wfp);
  efclose(wfp);
  efclose(rfp);
    get_gaten(f,n,x,y,xx,yy,nm1.data(),cd[n].str,i);
  remap_gate_and_switch();
  return (0);
}

void  get_gaten(int f, int n,int x,int y,int xx,int yy,
                char *land,char *ip,int port)
{
  std::array<char, 256> buf;
  int    cn, i, j;
  ATOM **newdata;

  for(i=0, cn=0; cn<gatemax; cn++) {
    if(gatedata[cn]==nullptr)
      continue;
    if(gatedata[cn][0].di == 'G' &&
       !std::strcmp(land,gatedata[cn][5].dp) &&
       !std::strncmp(ip,gatedata[cn][6].dp,std::strlen(ip))) {
      free(gatedata[cn]);
      gatedata[cn] = nullptr;
      continue;
    }
    i++;
  }

  if (f)
    return;

  i++;
  if (gatemax < i) {
    newdata = (ATOM **) malloc(sizeof(ATOM *) * i);
    if(newdata == nullptr) {
      log_print("Insufficient memory for gate data\n");
      return;
    }
  } else {
    newdata = gatedata;
  }

  for(j=0, cn = 0; cn < gatemax ; cn++) {
    if(gatedata[cn] == nullptr)
      continue;
    newdata[j++] = gatedata[cn];
  }

  if (gatemax < i) {
    free(gatedata);
    gatedata = newdata;
    gatemax = i;
  }

  gatedata[j] = (ATOM *) malloc(sizeof(ATOM) * 8);

  gatedata[j][0].di = chartoint('G');
  gatedata[j][1].di = x;
  gatedata[j][2].di = y;
  gatedata[j][3].di = xx;
  gatedata[j][4].di = yy;
  gatedata[j][5].dp = findstr(land);
  snprintf(buf.data(), buf.size(),"%s %05i", cd[n].str, port);
  gatedata[j][6].dp = findstr(buf.data());
  gatedata[j][7].dl = 0;

  for(j++; j < gatemax ; j++) {
    gatedata[j] = nullptr;
  }

}

void   put_gaten(int n, int f)
/* gatedata をもとにゲートを開け閉め */
/* f==0 ... open, f!=0 ... close */
{
  int    sfn;
  char  *p;
  std::array<char, 256> nm, buf;
  int    x, y, xx, yy, i, cn;
  std::array<char, 256> server_ip_address; /* add for hostname problem 20021016 */

  for (cn = 0; cn < gatemax; cn++) {
    if (gatedata[cn] == nullptr)
      continue;
    if (gatedata[cn][0].di != 'G')
      continue;
    x = gatedata[cn][1].di;
    y = gatedata[cn][2].di;
    xx = gatedata[cn][3].di;
    yy = gatedata[cn][4].di;
    p = gatedata[cn][6].dp;   /* ip, port */
    SKIP_SPACE(p);
    if (*p == '-')
      continue;
    SKIP_SPACE(p);
    loadstr(nm.data(), &p, ' ', 32);
    i = std::atoi(p);
    if (i <= 0)
      i = SERV_TCP_PORT;
    if ((sfn = connect_server(nm.data(), i)) < 0){
      if(f==0){
  dmprintf(n,0,buf.data(),buf.size(),DMM_GATE_DEAD,
    gatedata[cn][5].dp);
  check_write(n,buf.data());
      }
      continue;
    }
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"connect server(%d)(put_gaten)",sfn);
    log_print(debug_buf);
#endif
    p = select_password(gatedata[cn][5].dp, nm.data());
    /* add for hostname problem 20021016 */
    get_ip_address(sfn,server_ip_address.data(),server_ip_address.size());
    if (sport == SERV_TCP_PORT) {
      snprintf(buf.data(), buf.size(),"%s %03i %03i %03i %03i %-31s %s %s\n"
        ,(f ? "#g-close" : "#gate")
        ,xx, yy, x, y, land_name, server_ip_address.data(), p);
    } else {
      snprintf(buf.data(), buf.size(),"%s %03i %03i %03i %03i %-31s %s %05i %s\n"
        ,(f ? "#g-close" : "#gate")
        ,xx, yy, x, y, land_name, server_ip_address.data(), sport, p);
    }
    write_line(sfn, buf.data(), std::strlen(buf.data()), 0);
    if(!nfflush(sfn, 5000) && f==0){
      dmprintf(n,0,buf.data(),buf.size(),DMM_GATE_DEAD,
        gatedata[cn][5].dp);
      check_write(n, buf.data());
      /* delete dead gate 20040408 */
      free(gatedata[cn]);
      gatedata[cn]=nullptr;
    }
    nclose(sfn);
  }
  return;
}

void   put_gate(int f)
/* phi.td をもとにゲートの開け閉め */
/* f==0 ... open, f!=0 ...close */
{
  FILE  *fp;
  int    sfn;
  std::array<char, 256> buf;
  char   *p;
  std::array<char, 32> nm, nm1;
  int    x, y, xx, yy, i;
  std::array<char, 256> server_ip_address; /* add for hostname problem 20021016 */

  if ((fp = efopen("phi.td", "rb")) == nullptr)
    return;
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      continue;
    if (*(buf.data() + 1) != 'G')
      continue;
    p = buf.data() + 2;
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    SKIP_SPACE(p);
    loadstr(nm1.data(), &p, DSPACE, 32);
    SKIP_SPACE(p);
    if (*p == '-')
      continue;
    loadstr(nm.data(), &p, ' ', 32);
    SKIP_SPACE(p);
    i = std::atoi(p);
    if (i <= 0)
      i = SERV_TCP_PORT;
    if ((sfn = connect_server(nm.data(), i)) < 0){
      if(f==0)
  fprintf(stderr, "Warning: Gate to %s is dead. \n", nm1.data());
      continue;
    }
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"connect server(%d:%d)(put_gate)",sfn, fd_check(sfn));
    log_print(debug_buf);
#endif
#ifdef DEBUG
    log_print("select passwd");
#endif
    p = select_password(nm1.data(), nm.data());
    /* add for hostname problem 20021016 */
    get_ip_address(sfn,server_ip_address.data(),server_ip_address.size());
    if (sport == SERV_TCP_PORT) {
      snprintf(buf.data(), buf.size(),"%s %03i %03i %03i %03i %-31s %s %s\n"
        ,(f ? "#g-close" : "#gate")
        ,xx, yy, x, y, land_name, server_ip_address.data(), p);
    } else {
      snprintf(buf.data(), buf.size(),"%s %03i %03i %03i %03i %-31s %s %05i %s\n"
        ,(f ? "#g-close" : "#gate")
        ,xx, yy, x, y, land_name, server_ip_address.data(), sport, p);
    }
#ifdef DEBUG
    snprintf(debug_buf,sizeof(debug_buf),"send %s",(f ? "g-close":"g-open"));
    log_print(debug_buf);
#endif
    write_line(sfn, buf.data(), std::strlen(buf.data()), 0);
    if(!nfflush(sfn, 5000) && f==0)
      fprintf(stderr, "Warning: Gate to %s is dead. \n", nm1.data());
    nclose(sfn);
  }
  efclose(fp);
  return;
}

void   send_down()
{
  FILE  *fp;
  std::array<char, 256> rbuf, wbuf;
  std::array<char, 32> nm, nm1;
  char   *p;
  int    i;
  int    sfn;

  if ((fp = efopen("phi.td", "rb")) == nullptr)
    return;
  while (1) {
    if (efgets(rbuf.data(), 255, fp) == nullptr)
      break;
    if (*rbuf.data() != ':')
      continue;
    if (*(rbuf.data() + 1) != 'G')
      continue;
    p = rbuf.data() + 19;
    loadstr(nm1.data(), &p, DSPACE, 32); /* 世界名称 */
    SKIP_SPACE(p);
    loadstr(nm.data(), &p, ' ', 32); /* IP アドレス */
    if (nm[0] == '-')
      continue;
    SKIP_SPACE(p);
    i = std::atoi(p);
    if (i <= 0)
      i = SERV_TCP_PORT;
    if ((sfn = connect_server(nm.data(), i)) < 0)
      continue;
    p = select_password(nm1.data(), nm.data());
    snprintf(wbuf.data(), wbuf.size(),"#shutdown %-31s  %s\n", land_name, p);
    write_line(sfn, wbuf.data(), std::strlen(wbuf.data()), 0);
    nfflush(sfn, 5000);
    nclose(sfn);
  }
  efclose(fp);
  return;
}

/* Hall of fame switch (PHIDM NEO extension): phi.hall records */
static int hall_parse_line(char *p, char *cat, char *uid, char *name, char *date)
{
  if (p[0] != ':' || p[1] != 'H')
    return (0);
  p += 2;
  SKIP_SPACE(p);
  if (*p != '\"')
    return (0);
  p++;
  loadstr(cat, &p, '\"', 128);
  SKIP_SPACE(p);
  loadstr(uid, &p, ' ', 16);
  SKIP_SPACE(p);
  if (*p != '\"')
    return (0);
  p++;
  loadstr(name, &p, '\"', 64);
  SKIP_SPACE(p);
  loadstr(date, &p, '\n', 16);
  return (1);
}

/*----------------------------------
  DM演出 (FX) 拡張 (PHIDM NEO extension)

  スイッチ動作 63〜68 / :A チェーン type 3 (本文 z) / Lua dm.fx.* の
  共通送信部。ペイロード (kind から始まる文字列) を "#fx <payload>"
  行にして、対象キャラが MD_FX を申告している時だけ送る。
  世界データ由来の文字列を dmprintf の書式として渡すことは絶対に
  しない (組み立てた行をそのまま check_write する)。

  画像 (kind = img) だけはサーバが phi_graphics/<name>.bmp を引き、
  FNV-1a 32bit ハッシュを計算して行に挿入する。ファイルは初回要求
  時に読んでメモリにキャッシュし、Load swt (load_td_swt) のたびに
  破棄する。転送は #ex-fx get への応答として行う。
----------------------------------*/

#define FX_IMG_MAX_SIZE 131072  /* 画像ファイル上限 128KB */
#define FX_IMG_CACHE_MAX 16     /* キャッシュする画像の種類数 */
#define FX_IMG_CHUNK 720        /* 1行に載せる生バイト数 (base64 で 960 文字) */

typedef struct fximg_t {
  char   name[16];
  unsigned char *data;
  long   size;
  unsigned long hash;
} FXImg;

static FXImg fx_img_cache[FX_IMG_CACHE_MAX];
static int   fx_img_cache_n = 0;

/* FNV-1a 32bit */
static unsigned long fx_fnv1a(const unsigned char *data, long size)
{
  unsigned long h = 2166136261UL;
  long i;

  for (i = 0; i < size; i++) {
    h ^= (unsigned long) data[i];
    h *= 16777619UL;
  }
  return (h & 0xffffffffUL);
}

/* 画像名の検査 (パス区切り等を許さない) */
static int fx_img_name_ok(const char *name)
{
  int i, len;

  if (name == nullptr)
    return (0);
  len = (int) std::strlen(name);
  if (len < 1 || len > 15)
    return (0);
  for (i = 0; i < len; i++) {
    char c = name[i];
    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
          || (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return (0);
  }
  return (1);
}

void fx_clear_image_cache(void)
{
  int i;

  for (i = 0; i < fx_img_cache_n; i++) {
    std::free(fx_img_cache[i].data);
    fx_img_cache[i].data = nullptr;
    fx_img_cache[i].name[0] = '\0';
    fx_img_cache[i].size = 0;
  }
  fx_img_cache_n = 0;
}

/* phi_graphics/<name>.bmp を読んでキャッシュする。無ければ nullptr
   (理由をログに 1 行残す)。 */
static FXImg *fx_img_load(const char *name)
{
  FILE *fp;
  char path[64];
  char logbuf[160];
  long size;
  unsigned char *data;
  int i;

  for (i = 0; i < fx_img_cache_n; i++)
    if (!std::strcmp(fx_img_cache[i].name, name))
      return (&fx_img_cache[i]);
  if (!fx_img_name_ok(name)) {
    snprintf(logbuf, sizeof(logbuf), "fx       : bad image name: %s",
             name != nullptr ? name : "(null)");
    log_print(logbuf);
    return (nullptr);
  }
  snprintf(path, sizeof(path), "phi_graphics/%s.bmp", name);
  fp = efopen(path, (char *) "rb");
  if (fp == nullptr) {
    snprintf(logbuf, sizeof(logbuf), "fx       : image not found: %s", path);
    log_print(logbuf);
    return (nullptr);
  }
  std::fseek(fp, 0, SEEK_END);
  size = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (size <= 0 || size > FX_IMG_MAX_SIZE) {
    snprintf(logbuf, sizeof(logbuf),
             "fx       : image size out of range: %s (%ld bytes)", path, size);
    log_print(logbuf);
    efclose(fp);
    return (nullptr);
  }
  data = (unsigned char *) std::malloc((size_t) size);
  if (data == nullptr
      || std::fread(data, 1, (size_t) size, fp) != (size_t) size) {
    if (data != nullptr)
      std::free(data);
    efclose(fp);
    return (nullptr);
  }
  efclose(fp);
  if (fx_img_cache_n >= FX_IMG_CACHE_MAX) {
    /* 一番古いものを捨てて空ける */
    std::free(fx_img_cache[0].data);
    for (i = 1; i < fx_img_cache_n; i++)
      fx_img_cache[i - 1] = fx_img_cache[i];
    fx_img_cache_n--;
  }
  i = fx_img_cache_n++;
  CPYSTR(fx_img_cache[i].name, name);
  fx_img_cache[i].data = data;
  fx_img_cache[i].size = size;
  fx_img_cache[i].hash = fx_fnv1a(data, size);
  return (&fx_img_cache[i]);
}

/* base64 エンコード (自前実装、外部依存なし) */
static void fx_base64(const unsigned char *in, long len, char *out)
{
  static const char tbl[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  long i;
  char *o = out;

  for (i = 0; i + 3 <= len; i += 3) {
    unsigned long v = ((unsigned long) in[i] << 16)
      | ((unsigned long) in[i + 1] << 8) | (unsigned long) in[i + 2];
    *o++ = tbl[(v >> 18) & 63];
    *o++ = tbl[(v >> 12) & 63];
    *o++ = tbl[(v >> 6) & 63];
    *o++ = tbl[v & 63];
  }
  if (len - i == 1) {
    unsigned long v = ((unsigned long) in[i] << 16);
    *o++ = tbl[(v >> 18) & 63];
    *o++ = tbl[(v >> 12) & 63];
    *o++ = '=';
    *o++ = '=';
  } else if (len - i == 2) {
    unsigned long v = ((unsigned long) in[i] << 16)
      | ((unsigned long) in[i + 1] << 8);
    *o++ = tbl[(v >> 18) & 63];
    *o++ = tbl[(v >> 12) & 63];
    *o++ = tbl[(v >> 6) & 63];
    *o++ = '=';
  }
  *o = '\0';
}

/* "#fx img <where> <name> <hash> <dur>" を送る。送れたら 1。 */
int fx_send_img(int n, const char *where, const char *name, long dur)
{
  FXImg *img;
  char buf[256];

  if (n < 0 || n >= MAX_NTTYS)
    return (0);
  if (!(cd[n].modef & MD_FX))
    return (0);
  img = fx_img_load(name);
  if (img == nullptr)
    return (0);
  snprintf(buf, sizeof(buf), "#fx img %s %s %08lx %ld\n",
           where, img->name, img->hash, dur);
  check_write(n, buf);
  return (1);
}

/* #ex-fx get への応答: 画像データを base64 分割で送る。 */
void fx_send_image_data(int n, const char *name)
{
  FXImg *img;
  char buf[MAX_MESSAGE_SIZE];
  char b64[FX_IMG_CHUNK / 3 * 4 + 1];
  long off;
  int seq;

  if (n < 0 || n >= MAX_NTTYS)
    return;
  if (!(cd[n].modef & MD_FX))
    return;
  img = fx_img_load(name);
  if (img == nullptr)
    return;
  snprintf(buf, sizeof(buf), "#fx imgdata %s %ld %08lx\n",
           img->name, img->size, img->hash);
  check_write(n, buf);
  off = 0;
  seq = 0;
  while (off < img->size) {
    long chunk = img->size - off;
    if (chunk > FX_IMG_CHUNK)
      chunk = FX_IMG_CHUNK;
    fx_base64(img->data + off, chunk, b64);
    snprintf(buf, sizeof(buf), "#fx imgchunk %s %d %s\n",
             img->name, seq, b64);
    check_write(n, buf);
    off += chunk;
    seq++;
  }
  snprintf(buf, sizeof(buf), "#fx imgend %s\n", img->name);
  check_write(n, buf);
}

/* ペイロード (kind から始まる文字列) を "#fx ..." 行で送る共通関数。
   送れたら 1、対象が FX 非対応なら 0。 */
int fx_send_payload(int n, const char *payload)
{
  char buf[MAX_MESSAGE_SIZE];

  if (n < 0 || n >= MAX_NTTYS || payload == nullptr)
    return (0);
  if (!(cd[n].modef & MD_FX))
    return (0);
  if (!std::strncmp(payload, "img ", 4)) {
    /* img だけはサーバが hash を付ける (fx_send_img 経由) */
    char tmp[256];
    char *p, where[8], name[32];
    long dur = 0;

    CPYSTR(tmp, payload + 4);
    p = tmp;
    loadstr(where, &p, ' ', (int) sizeof(where));
    SKIP_SPACE(p);
    loadstr(name, &p, ' ', (int) sizeof(name));
    SKIP_SPACE(p);
    dur = std::atol(p);
    return (fx_send_img(n, where, name, dur));
  }
  snprintf(buf, sizeof(buf), "#fx %s\n", payload);
  check_write(n, buf);
  return (1);
}

/* スイッチ動作 63〜68 の実行。引数は引用符文字列 1 つで、中身は
   対応する #fx 行の引数部と同文。 */
static void fx_switch_action(int n, int act, const char *arg)
{
  char buf[MAX_MESSAGE_SIZE];

  if (arg == nullptr)
    arg = "";
  switch (act) {
  case 63:  /* マップ文字: "text <pos> <size> <dur> <text...>" / "off ..." */
    if (!std::strncmp(arg, "off ", 4))
      snprintf(buf, sizeof(buf), "textoff %s", arg + 4);
    else if (!std::strcmp(arg, "off"))
      snprintf(buf, sizeof(buf), "textoff all");
    else
      CPYSTR(buf, arg); /* arg は "text ..." から始まる (#fx 引数部と同文) */
    fx_send_payload(n, buf);
    break;
  case 64:  /* 暗転: "<level> <dur> <mode>" */
    snprintf(buf, sizeof(buf), "dark %s", arg);
    fx_send_payload(n, buf);
    break;
  case 65:  /* ワイプ: "<pattern> <on|off>" */
    snprintf(buf, sizeof(buf), "wipe %s", arg);
    fx_send_payload(n, buf);
    break;
  case 66: {  /* 画像表示: "<M|L> <name> <dur>" (hash はサーバが付ける) */
    char tmp[256];
    char *p, where[8], name[32];
    long dur = 0;

    CPYSTR(tmp, arg);
    p = tmp;
    loadstr(where, &p, ' ', (int) sizeof(where));
    SKIP_SPACE(p);
    loadstr(name, &p, ' ', (int) sizeof(name));
    SKIP_SPACE(p);
    dur = std::atol(p);
    fx_send_img(n, where, name, dur);
    break;
  }
  case 67:  /* 画面振動: "<amp> <dur>" */
    snprintf(buf, sizeof(buf), "shake %s", arg);
    fx_send_payload(n, buf);
    break;
  case 68:  /* 放射発光: "<dur>" */
    snprintf(buf, sizeof(buf), "glow %s", arg);
    fx_send_payload(n, buf);
    break;
  default:
    break;
  }
}

/* 地図 C 行末尾の cn マスク (24 ビット)。
   バフ系はプレイヤーのみ、しかも魔法等で付いた有限時間の cn 本体だけ
   (装備由来の ecn と、DFP/MRP99 防具等による永続値 0x10000 は出さない)。
   状態異常は敵味方とも cn 本体だけを出す (装備由来は出さない)。 */
static int fx_timed(int v)
{
  return (v != 0 && v > -0x10000 && v < 0x10000);
}

unsigned int fx_cn_mask(int j)
{
  unsigned int mask = 0;

  if (j < MAX_NTTYS) {
    if (OCN_COAT(j) && fx_timed(CN_COAT(j)))       mask |= 1u << CNN_COAT;
    if (OCN_PROTECT(j) && fx_timed(CN_PROTECT(j))) mask |= 1u << CNN_PROTECT;
    if (OCN_SHELL(j) && fx_timed(CN_SHELL(j)))     mask |= 1u << CNN_SHELL;
    if (OCN_HASTE(j) && fx_timed(CN_HASTE(j)))     mask |= 1u << CNN_HASTE;
    if (OCN_ILLUSION(j) && fx_timed(CN_ILLUSION(j))) mask |= 1u << CNN_ILLUSION;
    if (fx_timed(CN_SPELL(j)))     mask |= 1u << CNN_SPELL;
    if (fx_timed(CN_REGENE(j)))    mask |= 1u << CNN_REGENE;
    if (fx_timed(CN_FORCE(j)))    mask |= 1u << CNN_FORCE;
    if (fx_timed(CN_REFLEX(j)))   mask |= 1u << CNN_REFLEX;
    if (fx_timed(CN_MIRROR(j)))   mask |= 1u << CNN_MIRROR;
    if (fx_timed(CN_VANISH(j)))   mask |= 1u << CNN_VANISH;
    if (fx_timed(CN_PASSWALL(j))) mask |= 1u << CNN_PASSWALL;
    if (fx_timed(CN_RAISEDEAD(j))) mask |= 1u << CNN_RAISEDEAD;
    if (CN_IDLE(j))                mask |= 1u << CNN_IDLE;
    if (CN_LAG(j))                 mask |= 1u << CNN_LAG;
    if (fx_timed(CN_AEGIS(j)))     mask |= 1u << CNN_AEGIS;
    if (fx_timed(CN_HAMMER(j)))    mask |= 1u << CNN_HAMMER;
  }
  if (CN_POISON(j))  mask |= 1u << CNN_POISON;
  if (CN_PALSY(j))   mask |= 1u << CNN_PALSY;
  if (CN_PANIC(j))   mask |= 1u << CNN_PANIC;
  if (CN_CONFUSE(j)) mask |= 1u << CNN_CONFUSE;
  if (CN_BERSERK(j)) mask |= 1u << CNN_BERSERK;
  if (CN_SILENCE(j)) mask |= 1u << CNN_SILENCE;
  if (CN_BLIND(j))   mask |= 1u << CNN_BLIND;
  return (mask);
}


static void hall_engrave(int n, const char *cat)
{
  FILE *fp;
  char buf[256], hcat[128], huid[16], hname[64], hdate[16];
  char wbuf[256];
  time_t t;
  struct tm *lt;

  if (n >= MAX_NTTYS)
    return;
  if (cat == nullptr)
    cat = "";
  if ((fp = efopen((char *) "phi.hall", (char *) "r")) != nullptr) {
    while (efgets(buf, sizeof(buf), fp) != nullptr) {
      if (hall_parse_line(buf, hcat, huid, hname, hdate)
          && !std::strcmp(hcat, cat) && !std::strcmp(huid, cd[n].uid)) {
        efclose(fp);
        dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_ALREADY, cd[n].name);
        check_write(n, wbuf);
        return;
      }
    }
    efclose(fp);
  }
  time(&t);
  lt = localtime(&t);
  snprintf(hdate, sizeof(hdate), "%04d-%02d-%02d",
                lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
  if ((fp = efopen((char *) "phi.hall", (char *) "a")) != nullptr) {
    std::fprintf(fp, ":H \"%s\" %s \"%s\" %s\n", cat, cd[n].uid, cd[n].name, hdate);
    efclose(fp);
  }
  dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_ENGRAVED, cd[n].name);
  check_write(n, wbuf);
}

static void hall_show(int n, const char *cat)
{
  FILE *fp;
  char buf[256], hcat[128], huid[16], hname[64], hdate[16];
  char wbuf[256];
  int cnt = 0;

  if (cat == nullptr)
    cat = "";
  dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_TITLE, cat);
  check_write(n, wbuf);
  if ((fp = efopen((char *) "phi.hall", (char *) "r")) != nullptr) {
    while (efgets(buf, sizeof(buf), fp) != nullptr) {
      if (hall_parse_line(buf, hcat, huid, hname, hdate)
          && !std::strcmp(hcat, cat)) {
        cnt++;
        dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_NAME, cnt, hname, hdate);
        check_write(n, wbuf);
      }
    }
    efclose(fp);
  }
  if (cnt == 0) {
    dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_EMPTY);
    check_write(n, wbuf);
  } else {
    dmprintf(n, 0, wbuf, sizeof(wbuf), DMM_HALL_END, cnt);
    check_write(n, wbuf);
  }
}

/*----------------------------------
  NPC効果 (MRDM の npcaction 効果表をスイッチ動作34として実装)
  n      : 起動したキャラクタ = 効果の対象 (聞き手)。
           0 の時は汎用変数/汎用フラグの操作 (94-99) のみ有効。
  sx, sy : 起動位置 (ノックバックの押し出し方向の基準)
  f      : 効果番号 (MRDM npcaction と同じ番号)
  l      : 威力/パラメータ (0000-9999)
 ----------------------------------*/
static void npc_no_effect(int j)
{
  std::array<char, 256> buf;
  dmprintf(j, j, buf.data(), buf.size(), DMM_MAGIC_EFFECT_NO);
  write_companion(j, 0, buf.data(), 0);
}

/* 状態異常をひとつかける (mdp は抵抗計算済みの威力) */
static void npc_inflict(int j, int f, int mdp)
{
  std::array<char, 256> buf;
  int lv;
  switch (f) {
  case 11:  /* 毒 */
    if (cd[j].at == AT_POISONPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_POISON_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_POISON(j) = lv;
    break;
  case 12:  /* 麻痺 */
    if (cd[j].at == AT_PALSYPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    if (CN_PALSY(j)) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_PALSY_ALREADY);
      write_companion(j, 0, buf.data(), 0);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_PALSY_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_PALSY(j) = lv;
    break;
  case 13:  /* パニック */
    if (cd[j].at == AT_MEPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_PANIC_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_PANIC(j) = lv;
    break;
  case 14:  /* 混乱 */
    if (cd[j].at == AT_MEPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    if (CN_CONFUSE(j)) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_CONFUSE_ALREADY);
      write_companion(j, 0, buf.data(), 0);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_CONFUSE_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_CONFUSE(j) = lv;
    break;
  case 15:  /* バーサク */
    if (cd[j].at == AT_MEPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    if (CN_BERSERK(j)) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_BERSERK_ALREADY);
      write_companion(j, 0, buf.data(), 0);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_BERSERK_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_BERSERK(j) = lv;
    break;
  case 16:  /* 沈黙 */
    if (cd[j].at == AT_MEPROOF || (cd[j].at == AT_IMMORTAL && frand(100) < 90)) {
      npc_no_effect(j);
      return;
    }
    if (CN_SILENCE(j)) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_SILENCE_ALREADY);
      write_companion(j, 0, buf.data(), 0);
      return;
    }
    lv = mdp / 50 + 1;
    dmprintf(j, j, buf.data(), buf.size(), DMM_SILENCE_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_SILENCE(j) = lv;
    break;
  case 18:  /* 盲目 */
    lv = mdp / 50 + 3;
    if (CN_BLIND(j) < 0) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_BLIND_ALREADY);
      write_companion(j, 0, buf.data(), 0);
      return;
    }
    dmprintf(j, j, buf.data(), buf.size(), DMM_BLIND_BY_ATTACK, lv);
    write_companion(j, 0, buf.data(), 0);
    CN_BLIND(j) = -lv;
    break;
  }
}

/* 能力値の増減 (MRDM 形式: 変化量は mdp/100+1) */
static void npc_stat_up(int j, int cnn, int mdp, char *fmt)
{
  std::array<char, 256> buf;
  int v = mdp / 100 + 1;
  dmprintf(j, j, buf.data(), buf.size(), fmt, v);
  write_companion(j, 0, buf.data(), 0);
  if (cd[j].cn[cnn] > 0)
    cd[j].cn[cnn] = v;
  else
    cd[j].cn[cnn] += v;
}

static void npc_stat_down(int j, int cnn, int mdp, char *fmt)
{
  std::array<char, 256> buf;
  int v = mdp / 100 + 1;
  dmprintf(j, j, buf.data(), buf.size(), fmt, v);
  write_companion(j, 0, buf.data(), 0);
  if (cd[j].cn[cnn] > 0)
    cd[j].cn[cnn] -= v;
  else
    cd[j].cn[cnn] = -v;
}

void npc_action_effect(int n, int sx, int sy, int f, int l)
{
  std::array<char, 256> buf;
  int j = n, mdp, mrr, k, m;

  /* 汎用変数/汎用フラグの操作 (対象キャラクタを必要としない) */
  if (f >= 94) {
    if (l < 0)
      return;
    switch (f) {
    case 94:  /* 汎用変数 = 0 */
    case 95:  /* 汎用変数 ++ */
    case 96:  /* 汎用変数 -- */
      if (l >= GVAR_MAX)
        return;
      if (f == 94)
        generic_variable[l] = 0;
      else if (f == 95)
        generic_variable[l]++;
      else
        generic_variable[l]--;
      /* 変更を条件10/20の再評価へ伝える (change_flag と同じ経路) */
      if (gfn2 < (int) (sizeof(gfbuf2) / sizeof(gfbuf2[0])))
        gfbuf2[gfn2++] = GFLAG_MAX + PFLAG_MAX + l;
      return;
    case 97:  /* 汎用フラグ反転 */
      if (l < GFLAG_MAX)
        change_flag(n, l, FLAG_REV, 0);
      return;
    case 98:  /* 汎用フラグ OFF */
      if (l < GFLAG_MAX)
        change_flag(n, l, FLAG_OFF, 0);
      return;
    case 99:  /* 汎用フラグ ON */
      if (l < GFLAG_MAX)
        change_flag(n, l, FLAG_ON, 0);
      return;
    default:
      return;
    }
  }
  if (f == 19) {  /* ランダム (MRDM: 1-7を抽選し 7 は 8 として処理) */
    f = frand(7) + 1;
    if (f == 7)
      f = 8;
    npc_action_effect(n, sx, sy, f, l);
    return;
  }
  if (j <= 0 || !cd[j].type)
    return;   /* 聞き手がいない */
  if (f == 0 || f == 5)
    return;   /* 0: 効果なし, 5: 化ける は動作形式では未対応 */

  mdp = (l * 2) / 3 + frand(l / 3) + 1;  /* 威力の基本値 */
  if (f == 1 || f == 2) {        /* HP回復 */
    if (cd[j].hp <= 0)
      return;
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_HEAL_HP, mdp);
    write_companion(j, 0, buf.data(), 0);
    (void) hp_dmg(j, mdp, 0);
    return;
  }
  if (f == 3 || f == 4) {        /* MP回復 */
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_HEAL_MP, mdp);
    write_companion(j, 0, buf.data(), 0);
    (void) mp_dmg(j, mdp, 0);
    return;
  }
  if (f == 6) {                  /* Vanish */
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_VANISH, mdp / 50);
    check_write(j, buf.data());
    CN_VANISH(j) = mdp / 50;
    return;
  }
  if (f == 7) {                  /* Mirror */
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_MIRROR, mdp / 50);
    check_write(j, buf.data());
    CN_MIRROR(j) = mdp / 50;
    return;
  }
  if (CN_GASIFY(j) >= 60) {      /* ガス化した相手には届かない */
    npc_no_effect(j);
    return;
  }
  /* 治療・補助 (抵抗なし) */
  switch (f) {
  case 71:  /* 解毒 */
    if (CN_POISON(j) > 0) {
      CN_POISON(j) -= mdp / 50 + 1;
      if (CN_POISON(j) > 0)
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_ANTIDOTE_SOFT);
      else {
        CN_POISON(j) = 0;
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_ANTIDOTE);
      }
      write_companion(j, 0, buf.data(), 0);
    }
    return;
  case 72:  /* 麻痺治療 */
    if (CN_PALSY(j) > 0) {
      CN_PALSY(j) -= mdp / 50 + 1;
      if (CN_PALSY(j) > 0)
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_REMOVE_SOFT);
      else {
        CN_PALSY(j) = 0;
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_REMOVE);
      }
      write_companion(j, 0, buf.data(), 0);
    }
    return;
  case 73:  /* 精神異常治療 */
    k = mdp / 50 + 1;
    if (CN_PANIC(j) > 0 || CN_CONFUSE(j) > 0 || CN_BERSERK(j) > 0) {
      if (k < CN_PANIC(j) || k < CN_CONFUSE(j) || k < CN_BERSERK(j)) {
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_AWAKEN_SOFT);
        write_companion(j, 0, buf.data(), 0);
        CN_PANIC(j) -= k;
        if (CN_PANIC(j) < 0)
          CN_PANIC(j) = 0;
        CN_CONFUSE(j) -= k;
        if (CN_CONFUSE(j) < 0)
          CN_CONFUSE(j) = 0;
        CN_BERSERK(j) -= k;
        if (CN_BERSERK(j) < 0)
          CN_BERSERK(j) = 0;
      } else {
        dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_AWAKEN);
        write_companion(j, 0, buf.data(), 0);
        CN_PANIC(j) = CN_CONFUSE(j) = CN_BERSERK(j) = 0;
      }
    }
    return;
  case 81: npc_stat_up(j, CNN_COAT, mdp, DMM_NPCA_COAT_UP); return;
  case 82: npc_stat_up(j, CNN_SPELL, mdp, DMM_NPCA_SPELL_UP); return;
  case 83: npc_stat_up(j, CNN_PROTECT, mdp, DMM_NPCA_PROTECT_UP); return;
  case 84: npc_stat_up(j, CNN_SHELL, mdp, DMM_NPCA_SHELL_UP); return;
  case 85: npc_stat_up(j, CNN_HASTE, mdp, DMM_NPCA_HASTE_UP); return;
  }
  /* 攻撃系: 10-26 は MRP 抵抗, 40-44 は DFP 抵抗 (MRDM 形式) */
  if (!((f >= 10 && f <= 26) || (f >= 40 && f <= 44)))
    return;
  mrr = 1;
  if (f < 40) {
    if (cd[j].aeq != nullptr)
      mrr = cd[j].aeq->MRP + 1;
    if (TCN_ILLUSION(j) < 0)
      mrr = mrr * (100 + TCN_ILLUSION(j)) / 100;
    else if (TCN_ILLUSION(j) > 0)
      mrr = mrr * (100 - TCN_ILLUSION(j)) / 100;
  } else {
    if (cd[j].aeq != nullptr)
      mrr = cd[j].aeq->DFP + 1;
    if (TCN_PROTECT(j) < 0)
      mrr = mrr * (100 + TCN_PROTECT(j)) / 100;
    else if (TCN_PROTECT(j) > 0)
      mrr = mrr * (100 - TCN_PROTECT(j)) / 100;
  }
  if (mrr > 400 && frand(100) < 50)
    mrr = 400 - frand(40);
  if (frand(100) < mrr / 4) {
    npc_no_effect(j);
    return;
  }
  mdp = mdp * (100 - mrr) / 100;
  switch (f) {
  case 10:  /* MPダメージ */
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_MPDMG, mdp);
    write_companion(j, 0, buf.data(), 0);
    (void) mp_dmg(j, -mdp, 1);
    break;
  case 11: case 12: case 13: case 14: case 15: case 16: case 18:
    npc_inflict(j, f, mdp);
    break;
  case 17:  /* ドレイン (レベルを奪う。動作形式では受け手はいない) */
    if (cd[j].at == AT_DRAINPROOF) {
      npc_no_effect(j);
      break;
    }
    k = frand(4);
    mdp = mdp / 100 + 1;
    if (cd[j].lv[k] < 1000)
      mdp = (mdp * cd[j].lv[k]) / 1000 + 1;
    if (mdp > cd[j].lv[k])
      mdp = cd[j].lv[k];
    cd[j].lv[k] -= mdp;
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_DRAIN, mdp);
    write_companion(j, 0, buf.data(), 0);
    break;
  case 20:  /* バッド (全状態異常) */
    npc_inflict(j, 11, mdp);
    npc_inflict(j, 12, mdp);
    npc_inflict(j, 13, mdp);
    npc_inflict(j, 14, mdp);
    npc_inflict(j, 15, mdp);
    npc_inflict(j, 16, mdp);
    npc_inflict(j, 18, mdp);
    break;
  case 21: npc_stat_down(j, CNN_COAT, mdp, DMM_NPCA_COAT_DOWN); break;
  case 22: npc_stat_down(j, CNN_SPELL, mdp, DMM_NPCA_SPELL_DOWN); break;
  case 23: npc_stat_down(j, CNN_PROTECT, mdp, DMM_NPCA_PROTECT_DOWN); break;
  case 24: npc_stat_down(j, CNN_SHELL, mdp, DMM_NPCA_SHELL_DOWN); break;
  case 25: npc_stat_down(j, CNN_HASTE, mdp, DMM_NPCA_HASTE_DOWN); break;
  case 26:  /* MP分割 */
  case 44:  /* HP分割 */
    m = mdp / 16 + 1;
    m = 25 + frand(25) + 100 / m;
    if ((f == 44 ? cd[j].mhp : cd[j].mmp) == 0 || m <= 1) {
      npc_no_effect(j);
      break;
    }
    if (f == 44)
      dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_DIVIDEHP, m);
    else
      dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_DIVIDEMP, m);
    write_companion(j, 0, buf.data(), 0);
    if (f == 44) {
      cd[j].hp = cd[j].hp * m / 100 + 1;
      if (cd[j].hp > cd[j].mhp)
        cd[j].hp = cd[j].mhp;
    } else {
      cd[j].mp = cd[j].mp * m / 100 + 1;
      if (cd[j].mp > cd[j].mmp)
        cd[j].mp = cd[j].mmp;
    }
    break;
  case 40:  /* HPダメージ */
    dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_HPDMG, mdp);
    write_companion(j, 0, buf.data(), 0);
    (void) hp_dmg(j, -mdp, 1);
    break;
  case 41:  /* ノックバック */
    dmprintf(j, j, buf.data(), buf.size(), DMM_KNOCK_BACK);
    write_companion(j, 0, buf.data(), 0);
    move_chr(j, cd[j].x, cd[j].y,
             cd[j].x + fsign(cd[j].x - sx), cd[j].y + fsign(cd[j].y - sy), 0);
    break;
  case 42:  /* 武器を叩き落す */
    k = -1;
    if (cd[j].type == CT_NORMAL && cd[j].weq != nullptr
        && (cd[j].weq->type & 0xf0) != WT_FIST
        && (cd[j].weq->type & 0xf00) != MT_NOBREAK) {
      for (k = 0; k < MAX_ITEMS; k++)
        if (&(cd[j].id[k]) == cd[j].weq)
          break;
      if (k >= MAX_ITEMS)
        k = -1;
    }
    if (k >= 0) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_FUMBLE, cd[j].weq->name);
      write_companion(j, 0, buf.data(), 0);
      save_id(j, k, 1);
      item_gc(j);
      set_fn(j);
    } else {
      npc_no_effect(j);
    }
    break;
  case 43:  /* 所持品劣化 */
    m = MAX_ITEMS;
    for (k = 0; k < MAX_ITEMS; k++) {
      if (cd[j].id[k].type != DT_NONE
          && (cd[j].id[k].type & 0xf00) != MT_NOBREAK) {
        if (m == MAX_ITEMS)
          m = k;
        else if (PER_DICE < 50)
          m = k;
        else
          break;
      }
    }
    if (m < MAX_ITEMS)
      k = speed_degrade(&(cd[j].id[m]));
    else
      k = 0;
    if (k == 0) {
      npc_no_effect(j);
    } else if (k == 1) {
      dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_DEGRADE, cd[j].id[m].name);
      write_companion(j, 0, buf.data(), 0);
      item_gc(j);
      set_fn(j);
    } else {
      dmprintf(j, j, buf.data(), buf.size(), DMM_NPCA_DEGRADE_BROKEN, cd[j].id[m].name);
      write_companion(j, 0, buf.data(), 0);
      item_gc(j);
      set_fn(j);
    }
    break;
  default:
    break;
  }
}

int   load_switch(int n, int sx, int sy, int f)
{
  ItemData id;
  /* wbuf は CPYSTR の宛先のため C 配列のまま */
  char   wbuf[256];
  int    x, y;
  int    xx, yy;
  int    prop, px, py;  /* 動作51/52/81/82 の伝播制御 (PHIDM NEO extension, from JET) */
  int    i, j, k, l, m, lpt, cn;
  int    cbuf[MAX_ENEMIES];
  ItemOData **ip, *ip2;
  int    done;
  char  *p;
  MagicData md;
  char exp; /* ADD スイッチデータ追加 2005/12/02 Savarl.F */

  done = 0; m = 0;
  prop = -1; px = sx; py = sy;
  for (cn = 0; cn < swtn; cn++) {
    if (swtd[cn] == nullptr)
      continue;
    lpt = 0;
/* delete for quick message search 20030501
    if (swtd[cn][lpt++].di != 'S')
      continue;
*/
    x = swtd[cn][lpt++].di;
    y = swtd[cn][lpt++].di;
    j = i = swtd[cn][lpt++].di;
    if (j == 6 || j == 8 || j == 11 || j == 16)
      j = 1;      /* 踏んだときチェック */
    if (j == 80)
      j = 1;      /* 職業・性別も踏んだときチェック (PHIDM NEO extension) */
    if (j == 92)
      j = 1;      /* ペットも踏んだときチェック (PHIDM NEO extension) */
    if (j == 9)
      j = 4;      /* 置いたときチェック */
    if (j == 12 || j == 20)
      j = 10;
    if (j == 13)  /* wildcard keyword added 20000228 */
      j = 3;
    if (j == 15)  /* turn check added 20001201 */
      j = (f==1? 1:15);
/* ↓スイッチデータ追加 2005/11/26 Savarl.F */
/*    if (f != j || ((x != sx || y != sy) && f != 10))	*/
    if (f == 12) {
      /* 動作82/52 からの起動では座標つきスイッチ (条件12) だけを対象にする
         (PHIDM NEO extension, from JET) */
      if (i != 12 || x != sx || y != sy)
        continue;
    }
    else if (f != j || ((x != sx || y != sy) && (f != 10 && f != 91 && (f < 21 || f > 25))))
/* ↑CHG */
      continue;     /* 起動条件が異なるか、座標が違う */
    switch (i) {
    case 0:
    case 2:
    case 17: /* add for hit switch 20020530 */
    case 22: /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
    case 23: /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
    case 24: /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
      break;
    case 91: /* 実時間 (PHIDM NEO extension, from MRDM) */
      j = swtd[cn][lpt++].di;   /* 月 */
      if (j != 99 && sttime->tm_mon != j - 1)
        continue;
      j = swtd[cn][lpt++].di;   /* 日 */
      if (j != 99 && sttime->tm_mday != j)
        continue;
      j = swtd[cn][lpt++].di;   /* 時 */
      if (j != 99 && sttime->tm_hour != j)
        continue;
      j = swtd[cn][lpt++].di;   /* 分 */
      if (j != 99 && sttime->tm_min != j)
        continue;
      break;
    case 80: /* 職業・性別 (PHIDM NEO extension) */
      j = swtd[cn][lpt++].di;   /* 00=性別 01=職業 */
      k = swtd[cn][lpt++].di;   /* 値 (0=未設定) */
      if (!jb_enabled())
        continue;               /* phi.jb の無い世界では無効 */
      if (j == 0) {
        if (jb_sex(n) != k)
          continue;
      } else if (j == 1) {
        if (jb_job(n) != k)
          continue;
      } else {
        continue;
      }
      break;
    case 92: /* ペット (PHIDM NEO extension) */
      j = swtd[cn][lpt++].di;   /* タイプ (0=任意) */
      k = swtd[cn][lpt++].di;   /* 状態 (0=任意 1行動可 2休止中) */
      m = pet_type(n);
      if (m == 0)
        continue;               /* ペット無しは不成立 */
      if (j != 0 && m != j)
        continue;
      if (k == 1 && !pet_active(n))
        continue;
      if (k == 2 && pet_active(n))
        continue;
      if (k != 0 && k != 1 && k != 2)
        continue;
      break;
    case 1:     /* あるレベルに達した時 */
    case 6:     /* 無言プレート */
    case 11:    /* あるレベルに達していない時 */
    case 16:    /* 無言プレート */
    case 25:    /* ADD スイッチデータ追加 2005/11/26 Savarl.F */
      m = (i > 10);
      j = swtd[cn][lpt++].di;
      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      if (i == 25) {
		exp = swtd[cn][lpt++].dc;
		k = swtd[cn][lpt++].di;
        if (exp == '>') {
		  m = 0; /* yori ue チェック */;
		  k++;   /* izyou deha nai */
		} else if (exp == '<') {
		  m = 1; /* yori sita チェック */;
		} else {
		  m = 0; /* yori ue チェック */;
		}
      } else
      /* ↑ADD */
      k = swtd[cn][lpt++].di;
      switch (j) {
      case 1:     /* レベル */
      case 2:
      case 3:
      case 4:
        if (cd[n].lv[j-1] < k)
          m = 1 - m;
        break;
      case 5:     /* 経験値 */
        if (cd[n].exp < k)
          m = 1 - m;
        break;
      case 6:     /* Karma */
        if (cd[n].align < k)
          m = 1 - m;
        break;
      case 7:     /* 武器のATP */
  for (j = 0; j < MAX_ITEMS; j++) {
    if (cd[n].id[j].type == DT_NONE)
      continue;
    if (cd[n].id[j].ATP >= k)
      break;
  }
  if (j >= MAX_ITEMS) /* 強い武器を持っていない */
    m = 1 - m;
  break;
      case 8:     /* 防具のDFP+MRP */
  for (j = 0; j < MAX_ITEMS; j++) {
    if ((cd[n].id[j].type & IT_MASK) != IT_ARMOR)
      continue;
    if ((cd[n].id[j].DFP + cd[n].id[j].MRP) >= k)
      break;
  }
  if (j >= MAX_ITEMS) /* 強い防具を持っていない */
    m = 1 - m;
  break;
      case 9:     /* 所持品総重量 */
  for (j = 0; j < MAX_ITEMS; j++) {
    if (cd[n].id[j].type == DT_NONE)
      continue;
    else
      k -= cd[n].id[j].wp;
  }
  if (k > 0)
    m = 1 - m;
  break;
  /* add from psp13j5.5 Silvis 20000607 */
      case 10:    /* 現在HP */
        if (cd[n].hp < k)
          m = 1 - m;
        break;
      case 11:    /* 最大HP */
  if (cd[n].mhp < k)
    m = 1 - m;
  break;
      case 12:    /* 現在MP */
        if (cd[n].mp < k)
          m = 1 - m;
        break;
      case 13:    /* 最大MP */
  if (cd[n].mmp < k)
    m = 1 - m;
  break;
      case 14:    /* 所持金額 add 20000906 */
        if (cd[n].gp < k)
          m = 1 - m;
        break;
      }

      /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
      if (i == 25) {  /* 現在の HP/MP チェック */
        if (m == 1)
          continue;

        j   = swtd[cn][lpt++].di;
		exp = swtd[cn][lpt++].dc;
        k   = swtd[cn][lpt++].di;
        if (exp == '>') {
		  m = 0; /* yori ue チェック */;
		  k++;   /* izyou deha nai */
		} else if (exp == '<') {
		  m = 1; /* yori sita チェック */;
		} else {
		  m = 0; /* yori ue チェック */;
		}

        switch (j) {
        case 0: /* 現在HP */
          if (cd[n].hp < k)
            m = 1 - m;
          break;
        case 1: /* 現在MP */
          if (cd[n].mp < k)
            m = 1 - m;
          break;
        }
      }
      /* ↑ADD */

      if (m == 1)
        continue;
      if (i % 10 == 1) {
        dmcheck_write(n, DMM_SWITCH_ACT);
      }
      break;
    case 3:     /* 特定のメッセージを話した時 */
      if (swtd[cn][lpt].dp != nullptr) {
  if (std::strcmp(swtd[cn][lpt].dp, cd[n].str))
    continue;
      }
      lpt++;
      break;
    case 13:     /* 特定のパターンに合致するメッセージを話した時 added 20000101 */
    case 30:     /* object on 20011203 */
    case 31:
      if (swtd[cn][lpt].dp != nullptr) {
  if (!wildcmp(swtd[cn][lpt].dp, cd[n].str))
    continue;
      }
      lpt++;
      break;
    case 4:     /* アイテムをその場においた時 */
    case 18:    /* 拾った時 */
      id.name = swtd[cn][lpt++].dp;
      id.type = swtd[cn][lpt++].di;
      id.wp   = swtd[cn][lpt++].di;
      id.fn   = swtd[cn][lpt++].di;
      id.op[0]= swtd[cn][lpt++].di;
      id.op[1]= swtd[cn][lpt++].di;
      id.op[2]= swtd[cn][lpt++].di;
      id.op[3]= swtd[cn][lpt++].di;
      id.extension = swtd[cn][lpt++].dp;
      if(!match_item(id, putbuf))
  continue;
      if (f == 18 || done) /* bug fix 20010221 */
        break;
      cd[0].x = x;
      cd[0].y = y;
      ip = nullptr;
      for (;;) {
  ip = finditem(x, y, ip);
  if (ip == nullptr)
    break;
  if (is_trap(*ip)) /* bug(?) fix */
    continue;
  if (!std::memcmp(&putbuf, &((*ip)->id), sizeof (ItemData)))
    break;
      }
      if (ip != nullptr) {
  ip2 = *ip;
  *ip = (*ip)->next;
  free(ip2);
  ip = nullptr;
    for(;;) { /* bug(?) fix */
      ip = finditem(x, y, ip);
      if (ip == nullptr || !is_trap(*ip))
        break;
    }
    if (ip == nullptr) {
      map[cd[n].y][cd[n].x].tip &= ~T_ITEM;
      delitempos(cd[n].x,cd[n].y);
    }
      }
      if (!trnf)
  f = 0;
      break;
    case 14:    /* 魔法を唱えた時 added 20000115 */
      md.spell  = swtd[cn][lpt++].dp;
      md.effect = swtd[cn][lpt++].di;
      md.fn     = swtd[cn][lpt++].di;
      md.extension = swtd[cn][lpt++].dp;
      if(!match_spell(md, cd[n].md[0]))
            continue;
      break;
    case 5:     /* お金をおいた時 */
      i = swtd[cn][lpt++].di;
      sscanf(cd[n].str, "%i", &j);
      if (j != i && i != 0)
  continue;
      cd[n].gp -= j;
      tax += j;
      dmprintf(n,0,wbuf, sizeof(wbuf),DMM_PUT_MONEY, j);
      write_companion(n, n, wbuf, 0);
      f = 0;
      break;
    case 7:     /* NPCが特定の言葉を話した時 */
      if (n < MAX_NTTYS)
  continue;
      if (std::strcmp(cd[n].str, swtd[cn][lpt++].dp))
  continue;
      break;
    case 8:     /* アイテム所持 */
      id.name = swtd[cn][lpt++].dp;
      id.type = swtd[cn][lpt++].di;
      id.wp   = swtd[cn][lpt++].di;
      id.fn   = swtd[cn][lpt++].di;
      id.op[0]= swtd[cn][lpt++].di;
      id.op[1]= swtd[cn][lpt++].di;
      id.op[2]= swtd[cn][lpt++].di;
      id.op[3]= swtd[cn][lpt++].di;
      id.extension = swtd[cn][lpt++].dp;
      match_n = 0; /* add for check match items number. */
      for(i = 0; i < MAX_ITEMS; i++)
  if(cd[n].id[i].type != DT_NONE && match_item(id, cd[n].id[i])) { /* Bug Fix by kzm */
    if (!match_n)
      std::memcpy(&putbuf,&(cd[n].id[i]),sizeof(ItemData)); /* add for appear item switch 20010817 */
    match_n++;
  }
      if (!match_n)
  continue;   /* ないので却下 */
      break;
    case 9:   /* 置かれたアイテムの総重量が規定値を越えた時 */
      i = swtd[cn][lpt++].di;
      ip = nullptr;
      match_n = 0;
      for (;;) {
  ip = finditem(x, y, ip);
  if (ip == nullptr)
    break;
  match_n += (*ip)->id.wp;
      }
      if (i > match_n)
  continue;   /* 重量不足なので却下 */
      break;
    case 10:      /* 汎用フラグが ON の時 */
    case 12:
      k = 0;
                if ((i == 10) || ( (x==sx) && (y==sy) ))
                {
      i = swtd[cn][lpt++].di; /* チェックすべきフラグの個数 */
      while (--i >= 0) {
  j = swtd[cn][lpt++].di;
  if (j < GFLAG_MAX) {
    if (generic_flag[j] == FLAG_OFF) {
      k = 0;
      break;
    }
  } else if (j < GFLAG_MAX + PFLAG_MAX) { /* add for personal flag 20000804 */
    if (!(cd[n].pflag & (1 << (j-GFLAG_MAX)))) {
      k = 0;
      break;
    }
  } else if (j < GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX) {
    if (!var_value(n,j-GFLAG_MAX-PFLAG_MAX)) {
      k = 0;
      break;
    }
  }
  for (l = 0; l < gfn; l++) {
    if (gfbuf[l] == j) { /* personal flag 変更時はバッファにもコピーされるので問題ない */
      k = 1;    /* 今回変更されたものを含んでいた */
      break;
    }
  }
      }
                }
      if (k == 0)
  continue;   /* OFF のフラグがあったか、関係なかった */
      break;
    case 15: /* turn check */
      j = swtd[cn][lpt++].di;
      if (j < 4) {
        if (cd[n].d != j)
            continue;
      } else {
        if (f==1)
            continue;
        if (cd[n].com[0] != "lrb"[j-4])
            continue;
      }
      break;
    case 20: /* check variable */
      l = 0;
      i = swtd[cn][lpt++].di;
      for (m=0;m<gfn;m++) {
        if (gfbuf[m] - GFLAG_MAX - PFLAG_MAX == i)
            break;
      }
      if (m==gfn)
        l = 1;
      i = var_value(n,i);
      j = swtd[cn][lpt++].di;
      if (swtd[cn][lpt++].di) {
        for (m=0;m<gfn;m++) {
          if (gfbuf[m] - GFLAG_MAX - PFLAG_MAX == j)
            break;
        }
        if (m==gfn)
          l |= 2;
        j = var_value(n,j);
      } else {
        l |= 2;
      }
      if (l == 3)
        continue;
      switch (swtd[cn][lpt++].dc) {
        case '=':
            k = (i == j);
            break;
        case '!':
            k = (i != j);
            break;
        case 'g':
            k = (i >= j);
            break;
        case '>':
            k = (i > j);
            break;
        case 'l':
            k = (i <= j);
            break;
        case '<':
            k = (i < j);
            break;
        case '&':
            k = (i & j);
            break;
        case '^':
            k = (i ^ j);
            break;
        case '_':
            k = ((i & j) == j);
            break;
        default:
            k = 0;
            break;
      }
      if (!k)
        continue;
      break;
    /* ↓ スイッチデータ追加 2005/11/26 Savarl.F */
    case 21: /* 指定NPC殺害時 */
      if (std::strcmp(cd[cd[n].target].name, swtd[cn][lpt++].dp)) {
        continue; /* 条件外のNPC殺害 */
      }
      break;
    /* ↑ADD */
    default:
      continue;
    }
    done = 1;
    xx = swtd[cn][lpt++].di; /* 動作位置 x */
    yy = swtd[cn][lpt++].di; /* 動作位置 y */
    i = swtd[cn][lpt++].di;  /* 動作結果   */
    switch (i) {
    case 0:
      break;
    case 1:
    case 23: /* change surface */
      wbuf[0] = (char) (swtd[cn][lpt].di >> 8);
      wbuf[1] = (char) (swtd[cn][lpt++].di & 0xff);
      wbuf[2] = '\0';
      if (i == 1) {
        map[yy][xx].tip = ((map[yy][xx].tip & (TCLEAR ^ T_ROOF)) | set_dd(wbuf));
        map[yy][xx].height = dd_height_of(wbuf);
      }
      else {
        map[yy][xx].surface = set_dd(wbuf);
        map[yy][xx].tip |= T_SURFACE;
      }
      snprintf(wbuf, sizeof(wbuf),"#remap\n");
      write_companion(n, 0, wbuf, 0);
      break;
    case 24:
      map[yy][xx].tip &= ~T_SURFACE;
      snprintf(wbuf, sizeof(wbuf),"#remap\n");
      write_companion(n, 0, wbuf, 0);
      break;
    case 11:   /* 広範囲地形変化 added by Qajin 20000102 change 20001130 */
      p = swtd[cn][lpt++].dp;
      j = swtd[cn][lpt++].di;
      for(; *p != '\0' && yy < MAX_Y-2 ; yy++)
        for(i = 0; *p != '\0' && i < j && xx+i < MAX_X-2; p += 2 , i++) {
          map[yy][xx+i].tip = ((map[yy][xx+i].tip & (TCLEAR ^ T_ROOF)) | set_dd(p));
          map[yy][xx+i].height = dd_height_of(p);
        }
      snprintf(wbuf, sizeof(wbuf),"#remap\n");
      write_companion(n, 0, wbuf, 0);
      break;
    case 3:
      dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp); /* change for dmprint 20000907 */
      write_companion(n, 0, wbuf, 0);
      break;
    case 90: /* add for recording log 20020112 */
      dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp);
      log_print(wbuf);
      break;
    case 60:    /* 刻印スイッチ (PHIDM NEO extension) */
      hall_engrave(n, swtd[cn][lpt++].dp);
      break;
    case 61:    /* 刻印表示スイッチ (PHIDM NEO extension) */
      hall_show(n, swtd[cn][lpt++].dp);
      break;
    case 62:    /* プログラム実行スイッチ (PHIDM NEO extension) */
      class_call(n, swtd[cn][lpt++].dp, sx, sy);
      break;
    case 63:    /* FX: マップ文字 (PHIDM NEO extension) */
      fx_switch_action(n, 63, swtd[cn][lpt++].dp);
      break;
    case 64:    /* FX: 暗転 (PHIDM NEO extension) */
      fx_switch_action(n, 64, swtd[cn][lpt++].dp);
      break;
    case 65:    /* FX: ワイプ (PHIDM NEO extension) */
      fx_switch_action(n, 65, swtd[cn][lpt++].dp);
      break;
    case 66:    /* FX: 画像表示 (PHIDM NEO extension) */
      fx_switch_action(n, 66, swtd[cn][lpt++].dp);
      break;
    case 67:    /* FX: 画面振動 (PHIDM NEO extension) */
      fx_switch_action(n, 67, swtd[cn][lpt++].dp);
      break;
    case 68:    /* FX: 放射発光 (PHIDM NEO extension) */
      fx_switch_action(n, 68, swtd[cn][lpt++].dp);
      break;
    case 16:         /* 特殊範囲メッセージ added by Qajin 20000102 */
      switch (swtd[cn][lpt++].di) {
        case 0:
          dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp); /* change for dmprint 20000907 */
          check_write(n,wbuf);
          break;
        case 1:
          dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp);
          write_companion(n, 0, wbuf, 1);
          break;
        case 2:
          l = swtd[cn][lpt++].di;
          dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp);
          for (i = 1; i < enmax; i++)
            if (cd[i].type && (int)(unsigned int) fld[map[cd[i].y][cd[i].x].area].area == l)
              check_write(i, wbuf);
          lpt++;
          break;
        case 3:
          l = swtd[cn][lpt++].di;
          dmprintf(n, 0, wbuf,sizeof(wbuf),swtd[cn][lpt++].dp);
          for (i = 1; i < enmax; i++)
            if (cd[i].type && cd[i].y >= yy && cd[i].y <= yy+l && cd[i].x >= xx && cd[i].x <= xx+l)
                  check_write(i, wbuf);
          lpt++;
          break;
      }
      break;
    case 44:  /* add for キャラ位置アイテム出現　by NEO 2006/01/08　*/
      xx = cd[n].x; /* キャラ位置 x */
      yy = cd[n].y; /* キャラ位置 y */
    case 4:
    case 14:
      cd[0].x = xx;
      cd[0].y = yy;
      p = swtd[cn][lpt++].dp;
      if (p == nullptr) { /* add for duplicate item 20010816 */
        switch (swtd[cn][lpt++].di) {
          case 1:
            std::memcpy(&(cd[0].id[0]),&putbuf,sizeof(ItemData));
            break;
          case 2:
            if (cd[n].weq == nullptr)
                continue;
            std::memcpy(&(cd[0].id[0]),cd[n].weq,sizeof(ItemData));
            break;
          case 3:
            if (cd[n].aeq == nullptr)
                continue;
            std::memcpy(&(cd[0].id[0]),cd[n].aeq,sizeof(ItemData));
            break;
          case 4:
            if (cd[n].oeq == nullptr)
                continue;
            std::memcpy(&(cd[0].id[0]),cd[n].oeq,sizeof(ItemData));
            break;
          default:
            break;
        }
        lpt += 7;
      } else {
        cd[0].id[0].name = p;
        cd[0].id[0].type = swtd[cn][lpt++].di;
        cd[0].id[0].wp = swtd[cn][lpt++].di;
        cd[0].id[0].fn = swtd[cn][lpt++].di;
        cd[0].id[0].op[0] = swtd[cn][lpt++].di;
        cd[0].id[0].op[1] = swtd[cn][lpt++].di;
        cd[0].id[0].op[2] = swtd[cn][lpt++].di;
        cd[0].id[0].op[3] = swtd[cn][lpt++].di;
        cd[0].id[0].extension = swtd[cn][lpt++].dp;
      }
      dmprintf(n,0,wbuf, sizeof(wbuf),DMM_APPEAR_SOMETHING, cd[0].id[0].name);
      save_id(0, 0, 1);
      if (i != 14)
  write_companion(n, 0, wbuf, 0);
      break;
    case 45:  /* add for キャラ位置お金出現　by NEO 2006/01/08　*/
      xx = cd[n].x; /* キャラ位置 x */
      yy = cd[n].y; /* キャラ位置 y */
    case 5:     /* お金が出現 */
      cd[0].gp = swtd[cn][lpt++].di;
      if (tax < 0)
  cd[0].gp = cd[0].gp * (10000 + tax) / 10000;
      if (cd[0].gp < 0)
  cd[0].gp = 0;
      cd[0].x = xx;
      cd[0].y = yy;
      dmprintf(n,0,wbuf, sizeof(wbuf),DMM_APPEAR_MONEY, cd[0].gp);
      write_companion(n, 0, wbuf, 0);
      tax -= cd[0].gp;
      save_gp(0);
      break;
    case 13:    /* 強制サーバ間転送 */
      xx = swtd[cn][lpt++].di;
      load_gate(n, xx);
      break;
    case 6:     /* 強制転送 */
    case 26:    /* 相対転送 */
      clear_chr(n);
      if (i == 6) {
        cd[n].x = xx;
        cd[n].y = yy;
      } else {
        cd[n].x += xx;
        cd[n].y += yy;
      }
      /* bug ? fix 20000802 */
      if (cd[n].x < 1 || cd[n].x >= MAX_X - 1)
        cd[n].x %= MAX_X;
      if (cd[n].y < 1 || cd[n].y >= MAX_Y - 1)
        cd[n].y %= MAX_Y;
      switch (swtd[cn][lpt++].di) {
      case 'n':
      case 'N':   /* 北 */
  cd[n].d = 0;
  break;
      case 'e':
      case 'E':   /* 東 */
  cd[n].d = 1;
  break;
      case 's':
      case 'S':   /* 南 */
  cd[n].d = 2;
  break;
      case 'w':
      case 'W':   /* 西 */
  cd[n].d = 3;
  break;
      case 'r':
      case 'R':   /* 90°右回転 */
  cd[n].d = (cd[n].d + 1) & 3;
  break;
      case 'l':
      case 'L':   /* 90°左回転 */
  cd[n].d = (cd[n].d - 1) & 3;
  break;
      case 'b':
      case 'B':   /* 180°回転 */
  cd[n].d = (cd[n].d + 2) & 3;
  break;
      }
      set_chr(n);
      print_map(n, 0);
      if (!trnf) {          /* :O T 指定時には 移動先のスイッチを動作させない */
        cd[n].bx = cd[n].x;
        cd[n].by = cd[n].y;
      }
      if (f != 10)    /* フラグが条件でなければ */
  cn = swtn;      /* この後のスイッチは無視 */
      break;
    case 7:     /* クリーチャー発生スイッチ */
      CPYSTR(wbuf, swtd[cn][lpt++].dp);
      i = std::strlen(wbuf);
      for (j = 0; j < max_enemy; j++) {
  if (ed_o[j].apc == 2 &&
      !std::strncmp(ed_o[j].name, wbuf, i) &&
      (ed_o[j].name[i] == '\0' || ed_o[j].name[i] == ' ')) {
    cbuf[j] = -1;
  } else {
    cbuf[j] = 0;
  }
      }
      for (j = MAX_NTTYS; j < enmax; j++) {
  if ((cd[j].et >= 0) && cbuf[cd[j].et] != 0) {
    if (cd[j].type == CT_NOEXIST) { /* 発生 */
      if (cd[j].ec == 0) {
        cd[j].wt = fsps(80, ed_o[cd[j].et].apt, 900000);
        cd[j].ec = 1;
      }
    }
  }
      }
      break;
    case 12:    /* #name再送スイッチ */
      if (n < MAX_NTTYS) {
        cd[n].cg = cd[n].ocg /*findstr("human")*/; /* added by Qajin 19991224 */
        if (!(cd[n].modef & (MD_M57 | MD_M121))) {
            snprintf(wbuf, sizeof(wbuf),"#name %s\n", cd[n].name);
            check_write(n, wbuf);
        }
      } else {                                    /* bug ? fix 20000102 */
        cd[n].cg = ed_o[cd[n].et].cg;
      }
      break;
    case 15:    /* キャラグラ変更スイッチ added by Qajin 19991224 from psp10 */
      cd[n].cg = swtd[cn][lpt++].dp;
      if (!(cd[n].modef & (MD_M57 | MD_M121))) {
          if (cd[n].cv >= 5090152) {           /* phiwin/Tphi 識別追加 added 20000101 */
            snprintf(wbuf,sizeof(wbuf),"#name %s\n",cd[n].cg);
            check_write(n,wbuf);
          } else if (cd[n].cv >= 5085910)      /* バージョンチェック追加 added by Qajin 19991228 */
            check_write(n,"#name __\n");
      }
      break;
    case 17:    /* 強制 Shrink/Gigant */
      cd[n].sg = swtd[cn][lpt++].di;
      break;
    case 8:     /* ダメージプレート */
      xx = swtd[cn][lpt++].di;
      yy = swtd[cn][lpt++].di;
      switch (xx) {
      case 0:
  cd[n].hp = cd[n].mhp;
  break;
      case 1:
  cd[n].mp = cd[n].mmp;
  break;
      case 2:
  cd[n].hp += fsps(20, yy, cd[n].mhp - cd[n].hp);
  break;
      case 3:
  cd[n].mp += fsps(20, yy, cd[n].mmp - cd[n].mp);
  break;
      case 4:
  cd[n].hp -= fsps(20, yy, cd[n].hp - 1);
  break;
      case 5:
  cd[n].mp -= fsps(20, yy, cd[n].mp - 1);
  break;
      case 6:
  cd[n].hp = yy;
  break;
      case 7:
  cd[n].mp = yy;
  break;
  /* max に対する % での増減 added by Qajin 20000101 */
      case 8:
  cd[n].hp += cd[n].mhp * yy / 10000;
  break;
      case 9:
  cd[n].mp += cd[n].mmp * yy / 10000;
  break;
      case 10:
  cd[n].hp -= cd[n].mhp * yy / 10000;
  break;
      case 11:
  cd[n].mp -= cd[n].mmp * yy / 10000;
  break;
      case 12:
  cd[n].hp  = cd[n].mhp * yy / 10000;
  break;
      case 13:
  cd[n].mp  = cd[n].mmp * yy / 10000;
  break;
      }
  if (cd[n].hp < 1)    cd[n].hp = 1;
  if (cd[n].hp > cd[n].mhp) cd[n].hp = cd[n].mhp;
  if (cd[n].mp < 1)    cd[n].mp = 1;
  if (cd[n].mp > cd[n].mmp) cd[n].mp = cd[n].mmp;
      break;
    case 9:     /* メッセージスイッチ */
      cd[n].st = ST_MSGW;
    case 19:    /* メッセージスイッチ2 */
      cd[n].anmf = swtd[cn][lpt++].di;
      cd[n].anmt = swtd[cn][lpt++].di - 1;
      if (cd[n].anmt <= 0)
  cd[n].anmt = 0;
      break;
    case 20:      /* 確率変化 */
      m = swtd[cn][lpt++].di; /* 確率 */
    case 10:      /* 汎用フラグを変化 */
    case 81:      /* 汎用フラグを変化 (スイッチを起動しない) (PHIDM NEO extension, from JET) */
    case 82:      /* 汎用フラグを変化 (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
      j = swtd[cn][lpt++].di; /* どう変化させるか */
      k = swtd[cn][lpt++].di; /* 変化させるフラグの個数 */
      {
        int gfn2_mark = gfn2;  /* 動作81 の変更は gfbuf2 へ残さない */
        while (--k >= 0) {
    l = swtd[cn][lpt++].di;
    if (i != 20 || PER_DICE < m)
      change_flag(n, l, j, 0);
        }
        if (i == 81 && gfn2 > gfn2_mark)
          gfn2 = gfn2_mark;
      }
      if (i == 10 || i == 20)
        prop = 0;
      else if (i == 81) {
        if (prop < 0)
          prop = 1;
      } else if (i == 82) {
        if (prop != 0) {
          prop = 2; px = xx; py = yy;
        }
      }
      break;
#if MAX_OBJECTS > 0
    case 21: /* オブジェクト移動 */
      p = swtd[cn][lpt++].dp;
      k = search_obj(0,p);
      l = swtd[cn][lpt++].di;
      move_obj(k,xx,yy,(l < 4 ? l : (cd[n].d + l) & 3));
      snprintf(wbuf, sizeof(wbuf),"#remap\n"); /* add 20001105 */
      write_companion(n, 0, wbuf, 0);
      break;
    case 22: /* オブジェクト撤去 */
      p = swtd[cn][lpt++].dp;
      k = search_obj(0,p);
      move_obj(k,-1,-1,-1);
      snprintf(wbuf, sizeof(wbuf),"#remap\n"); /* add 20001105 */
      write_companion(n, 0, wbuf, 0);
      break;
#endif
    case 18: /* コンディション変更 */
      i = swtd[cn][lpt++].di; /* コンディション種類 */
      if (i < 0)
        break;
      j = swtd[cn][lpt++].dc; /* + - = */
      xx = swtd[cn][lpt++].di; /* 値 */
      if (i < MAX_NCN) {
        yy = swtd[cn][lpt++].di; /* 時間倍率 */
        xx *= yy;
      }
      switch(j) {
        case '=':
        default:
          cd[n].cn[i] = xx;
          break;
        case '+':
          cd[n].cn[i] += xx;
          break;
        case '-':
          cd[n].cn[i] -= xx;
          break;
      }
      if (i < MAX_NCN) {
        cd[n].ncn[i] = yy;
      }
      break;
    case 30: /* call */
      cd[n].ix = xx;
      cd[n].iy = yy;
      break;
    case 31:
      kill_self_msg(n,DMM_KILL_SELF_TRAP);
      if (TCN_RAISEDEAD(n))
        raise_dead(n);
      else
        dead(n);
      break;
    case 25: /* add for switch show 20010621 */
      show_file(n,swtd[cn][lpt++].dp);
      break;
    case 50: /* calc expression */
    case 51: /* calc expression (スイッチを起動しない) (PHIDM NEO extension, from JET) */
    case 52: /* calc expression (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
      stpt = 0;
      log_send = n;
      if (i == 51) {
        int gfn2_mark = gfn2;  /* 動作51 の変更は gfbuf2 へ残さない */
        calc_expression(n,&lpt,swtd[cn]);
        if (gfn2 > gfn2_mark)
          gfn2 = gfn2_mark;
      } else {
        calc_expression(n,&lpt,swtd[cn]);
      }
      log_send = 0;
      if (i == 50)
        prop = 0;
      else if (i == 51) {
        if (prop < 0)
          prop = 1;
      } else if (i == 52) {
        if (prop != 0) {
          prop = 2; px = xx; py = yy;
        }
      }
      break;
    case 29: /* add for boot menu 20041212 */
      display_menu(n,swtd[cn][lpt++].di);
      break;
    case 34: /* NPC効果 (PHIDM NEO extension, from MRDM npcaction) */
      i = swtd[cn][lpt++].di;  /* 効果番号 */
      j = swtd[cn][lpt++].di;  /* 威力/パラメータ */
      npc_action_effect(n, sx, sy, i, j);
      prop = 0;  /* 汎用変数/フラグの変更は従来どおり伝播させる */
      break;
    case 80: /* 職業・性別を設定 (PHIDM NEO extension) */
      i = swtd[cn][lpt++].di;  /* 00=性別 01=職業 */
      j = swtd[cn][lpt++].di;  /* 値 (0=未設定に戻す) */
      if (i == 0)
        jb_set_sex(n, j);
      else if (i == 1)
        jb_set_job(n, j);
      break;
    case 92: /* ペット (PHIDM NEO extension) */
      i = swtd[cn][lpt++].di;  /* 動作 (0付与 1改名 2解放 3回復) */
      j = swtd[cn][lpt++].di;  /* タイプ (動作0の時のみ) */
      if (i == 0)
        pet_grant(n, j);
      else if (i == 1)
        pet_rename_start(n);
      else if (i == 2)
        pet_release_start(n);
      else if (i == 3)
        pet_restore(n);
      break;
   case 33:  /* コマンド実行 */
      input_analyze(n,swtd[cn][lpt++].dp);
      break;
    /* ↑ADD */
    }
  }
  if (gfn2 > 0) {   /* ON になったフラグがあった */
    for (i = 0; i < gfn2; i++) {/* gfbuf に gfbuf2 をコピー */
      gfbuf[i] = gfbuf2[i];
    }
    gfn = gfn2;
    gfn2 = 0;     /* gfbuf2 をクリア */
    if (prop == 1) {
      /* 動作81/51 のみが変更した場合は、どのスイッチも起動しない
         (PHIDM NEO extension, from JET) */
    } else if (prop == 2) {
      load_switch(n, px, py, 12);  /* 動作82/52: 座標つきスイッチだけ起動 */
    } else {
      load_switch(n, sx, sy, 10);
    }
  }
  return done;
}

/* 汎用/固有 フラグ操作 */
/* add 20000804 */
void   change_flag(int n, int f, int ctrl, int first)
{
  int p;
  int on;

  on = 0;
    if (f < GFLAG_MAX) {
      if (ctrl == FLAG_REV) {
        generic_flag[f] ^= 1;
      } else {
        generic_flag[f] = ctrl;
      }
      on = generic_flag[f];
    } else if (f < GFLAG_MAX + PFLAG_MAX) {
      p = 1 << (f - GFLAG_MAX);
      if (ctrl == FLAG_REV) {
        cd[n].pflag ^= p;
      } else if (ctrl == FLAG_ON) {
        cd[n].pflag |= p;
      } else {
        cd[n].pflag &= ~p;
      }
      on = (cd[n].pflag & p ? 1 : 0);
    } else if (f < GFLAG_MAX + PFLAG_MAX + GVAR_MAX) {
      if (ctrl == FLAG_REV) {
        generic_variable[f-GFLAG_MAX-PFLAG_MAX] = !generic_variable[f-GFLAG_MAX-PFLAG_MAX];
      } else {
        generic_variable[f-GFLAG_MAX-PFLAG_MAX] = ctrl;
      }
      on = generic_variable[f-GFLAG_MAX-PFLAG_MAX];
    } else if (f < GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX) {
      if (ctrl == FLAG_REV) {
        cd[n].pvar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX] = !(cd[n].pvar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX]);
      } else {
        cd[n].pvar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX] = ctrl;
      }
      on = cd[n].pvar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX];
    } else if (f < GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX) {
      if (ctrl == FLAG_REV) {
        cd[n].svar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX-SVAR_MAX] = !(cd[n].svar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX-SVAR_MAX]);
      } else {
        cd[n].svar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX-SVAR_MAX] = ctrl;
      }
      on = cd[n].svar[f-GFLAG_MAX-PFLAG_MAX-GVAR_MAX-SVAR_MAX];
    }

    if (on) {
      if (first)
        gfbuf[(gfn=1)-1] = f;
      else
        gfbuf2[gfn2++] = f;
    }
}

/* 読み込めたか返り値を返す様修正 change 20000621 */
int   load_sd(int n, int x, int y, int tp, int in)
{
  int    xx, yy;
  FILE  *fp;
  std::array<char, 256> buf;
  /* tstr は CPYSTR の宛先のため C 配列のまま */
  char   tstr[25];
  char  *p;
  std::array<char, 32> nm;
  int    i, j, k;
  /* add for display locked time 20000606 */
  std::array<char, 16> id;

#ifdef SDI_AREA
  snprintf(buf.data(),buf.size(),"phi_sdi/sd.%02x",fld[map[y][x].area].area);
  if ((fp = efopen(buf.data(), "rb")) == nullptr)
#else
  if ((fp = efopen("phi.sd", "rb")) == nullptr)
#endif
    return 0;
  k = 0;
  for (j = 0;k == 0;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      break;
    p = buf.data() + 2;
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    SKIP_SPACE(p);
    if (*(buf.data() + 1) == tp && x == xx && y == yy) {
      j++;
      if (j == in)
  switch (tp) {
  case 'G':
    sscanf(p, "%li", &tax);
    k = 1;
    break;
  case 'T':
  case 'B':
    /*** PSP7 by Aleph ***/
      *(buf.data() + 70) = '\n';
      *(buf.data() + 71) = 0;
    if (*yname != '\0')  /* 西暦との差 */
      year_format(buf.data(),buf.size()-1);
    check_write(n, buf.data() + 36);
    check_write(n, "----------------------------------------------\n");

     /*** PSP7 END ***/
    nm[0] = 'B';
    for (i = 0; i < 15; cd[0].uid[i++] = *(p++));
    id2f(0,nm.data()+10);
    nm[25] = 0;
    SKIP_SPACE(p);
    for (i = 1; i < 9; nm[i++] = *(p++));
    nm[9] = '.';
    for (i = 24; i >= 0 && fisspace(nm[i]); nm[i--] = 0);
    load_board(n, nm.data());
    k = 1;
    break;
  case 'L':
    /* change for display locked time 20000606 */
    SKIP_SPACE(p);
    for (i = 0; i < 16; id[i++] = *(p++)); /* bug fix 20000922 */
    id[15] = 0;
    for (i = 14; i >= 0 && fisspace(id[i]); id[i--] = 0);
    SKIP_SPACE(p);
    for (i = 0; i < 32; nm[i++] = *(p++));
    nm[31] = 0;
    for (i = 30; i >= 0 && fisspace(nm[i]); nm[i--] = 0);

    if (cd[n].auth != nullptr || !std::strcmp(id.data(),cd[n].uid)) {
      SKIP_SPACE(p);
      if (*p != '\0' && *p != '\n') {
        abstime = (time_t) hexadecimal_load(8, &p);
        sttime = (struct tm *) localtime(&abstime);
        strftime(tstr, 25, "%Y/%m/%d %H:%M", sttime);
      } else
        CPYSTR(tstr,"????");
      dmprintf(n,0,buf.data(), buf.size(),DMM_LOCKED_WITH_TIME, nm.data(), tstr);
    } else {
      dmprintf(n,0,buf.data(), buf.size(),DMM_LOCKED, nm.data());
    }
    check_write(n, buf.data());
    k = 1;
    break;
  case 'I':
    sscanf(p, "%li", &s_id);
    k = 1;
    break;
  }
    }
  }
  efclose(fp);
  return(k);
}

int    save_sd(int n, int x, int y, int tp, char *mes, int f)
/* 識別番号 n の PC が場所 (x,y) において tp で示される「もの」地形を  */
/* 加えたり(f==0)、削除したり(f 番目のもの)する変化を phi.sd に与える。*/
/* char *mes は看板に記述する文字列を指す。                            */
/* 返却値は -1(削除が受理されたとき)                                   */
/* または (x,y) に存在する tp で示される「もの」の数                   */
/* (ただし、MSGATE を DM 以外の PC が                                  */
/*  ロック(あるいはその解除)しようとした場合は 0)                      */
{
  char   own;
  int    xx, yy;
  FILE  *rfp, *wfp;
  /* rbuf は CPYSTR の宛先のため C 配列のまま */
  /* wbuf は CPYSTR の宛先のため C 配列のまま */
  char   rbuf[256], wbuf[256];
  char  *p;
  int    i, j;
  std::array<char, 16> fid;

  j = std::strlen(cd[n].uid); /* to refuse null id character 20000719 */
  /* まずバックアップ */
#ifdef SDI_AREA
  if (tp == 'G' || tp == 'I') {
    CPYSTR(rbuf,"phi.sd");
  } else {
    snprintf(rbuf,sizeof(rbuf),"phi_sdi/sd.%02x",fld[map[y][x].area].area);
  }
  rfp = efopen(rbuf, "rb");
#else
  if ((rfp = efopen("phi.sd", "rb")) == nullptr)
    return (1);
#endif
  if ((wfp = efopen("phi.sd.bak", "wb")) == nullptr) {
    if(rfp != nullptr)
      efclose(rfp);
    return (1);
  }
  if (rfp != nullptr)
  for (i = 0, own = 0;;) {         /* i は tp で示される「もの」の数 */
    if (efgets(rbuf, 255, rfp) == nullptr)
      break;
    if (*rbuf != ':')
      break;
    p = rbuf + 2;
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    if (xx < 1 || xx >= MAX_X - 1 || yy < 1 || yy >= MAX_Y - 1)
      continue;
    SKIP_SPACE(p);
    switch (*(rbuf + 1)) {
    case 'G':
    case 'I':
      continue;     /* G(TAX),I(new-player 用の番号) はバックアップしない */
    case 'L':
      switch (map[yy][xx].tip & TMASK) {
      case T_LBOX:
      case T_LGATE:
      case T_LSGATE:
      case T_LMSGATE:
  break;
      default:
  continue;   /* ロック情報は今もされているところのみバックアップ */
      }
      break;
    }
    if (x == xx && y == yy) {
      /* change to refuse null id character 20000719 */
      if (*(rbuf + 1) == 'T' && !std::strncmp(p, cd[n].uid, j) && CHKSP(p+j))
  own = 1;
      if (*(rbuf + 1) == tp)
  i++;
    }
    fputs(rbuf, wfp);
  }
  CPYSTR(wbuf, ".\n");
  fputs(wbuf, wfp);
  efclose(wfp);
  if (rfp != nullptr)
    efclose(rfp);
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"bakup phi.sd complete(save_sd)");
  log_print(debug_buf);
#endif

  /* phi.sd の改変 */
  if ((rfp = efopen("phi.sd.bak", "rb")) == nullptr)
    return (1);
#ifdef SDI_AREA
  if (tp == 'G' || tp == 'I') {
    CPYSTR(rbuf,"phi.sd");
  } else {
    snprintf(rbuf,sizeof(rbuf),"phi_sdi/sd.%02x",fld[map[y][x].area].area);
  }
  if ((wfp = efopen(rbuf, "wb")) == nullptr)
#else
  if ((wfp = efopen("phi.sd", "wb")) == nullptr)
#endif
  {
    efclose(rfp);
    return (1);
  }

  if (!f)     /* f==0 のとき、情報の付け足し */
    switch (tp) {
    case 'G':
      snprintf(wbuf, sizeof(wbuf),":G 001 001 %li\n", tax);
      fputs(wbuf, wfp);
      snprintf(wbuf, sizeof(wbuf),":I 001 001 %li\n", s_id);  /* ID 連番も書き込み */
      fputs(wbuf, wfp);
      break;
    case 'T':     /* 看板を建てる */
    case 'B':     /* 看板書き込み */
      id2f(n,wbuf);
      snprintf(rbuf, sizeof(rbuf),"B%s.%s", mes, wbuf);
      get_title(rbuf);
      map[y][x].tip |= T_MESSAGE;
      snprintf(wbuf, sizeof(wbuf),":%c %03i %03i %-15s %-8.8s %-15s (%-16.16s) %s\n"
        ,tp, x, y, cd[n].uid, mes, cd[n].name, asctime(sttime), rbuf);
      fputs(wbuf, wfp);
      break;
    case 'L':
      if (i)
        break;
      switch (map[y][x].tip & TMASK) {
      case T_BOX:
        map[y][x].tip = (map[y][x].tip & TCLEAR) | T_LBOX;
        break;
      case T_GATE:
        map[y][x].tip = (map[y][x].tip & TCLEAR) | T_LGATE;
        break;
      case T_SGATE:
        map[y][x].tip = (map[y][x].tip & TCLEAR) | T_LSGATE;
        break;
      case T_MSGATE:
        if (have_authority(n,MS_UNLOCK,x,y) != 0)  /* DM のみロック可 */
          map[y][x].tip = (map[y][x].tip & TCLEAR) | T_LMSGATE;
        break;
      }
      /* change for display locked time 20000606 */
      snprintf(wbuf, sizeof(wbuf),":L %03i %03i %-15s %-31s %08lx\n", x, y, cd[n].uid, cd[n].name, time(nullptr));
      fputs(wbuf, wfp);
      f = -1;
      break;
    }

  for (i = 0;;) {
    if (efgets(rbuf, 255, rfp) == nullptr)
      break;
    if (*rbuf != ':')
      break;
    p = rbuf + 2;
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    if (xx < 1 || xx >= MAX_X - 1 || yy < 1 || yy >= MAX_Y - 1)
      continue;
    SKIP_SPACE(p);
    if (*(rbuf + 1) == tp && x == xx && y == yy)
    {
      i++;      /* tp で表される「もの」の存在数 */
      if (f == i && (own || (!std::strncmp(p, cd[n].uid, j) && CHKSP(p+j)) || (have_authority(n,(tp == 'L' ? MS_UNLOCK : MS_DELETE),x,y) != 0)))
      {
        f = -1;
        i--;
        switch (tp)
        {
          case 'T':   /* 看板の削除 */
            map[y][x].tip &= ~T_MESSAGE;
          case 'B':   /* 看板のメッセージの削除 */
            CPYSTR(cd[0].uid,p);
            id2f(0,fid.data());
            snprintf(wbuf, sizeof(wbuf),"phi_docs/B%-8.8s.%-15.15s", p + 16, fid.data());
            unlink(wbuf);
            break;
          case 'L':   /* ロックの解除 */
            switch (map[y][x].tip & TMASK)
            {
              case T_LBOX:
                map[y][x].tip = (map[y][x].tip & TCLEAR) | T_BOX;
                break;
              case T_LGATE:
                map[y][x].tip = (map[y][x].tip & TCLEAR) | T_GATE;
                break;
              case T_LSGATE:
                map[y][x].tip = (map[y][x].tip & TCLEAR) | T_SGATE;
                break;
              case T_LMSGATE:
                map[y][x].tip = (map[y][x].tip & TCLEAR) | T_MSGATE;
                break;
            }
            break;
          }
        continue;
      }
    }
    fputs(rbuf, wfp);
  }
  CPYSTR(rbuf, ".\n");
  fputs(rbuf, wfp);
  efclose(wfp);
  efclose(rfp);
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"save_sd complete");
  log_print(debug_buf);
#endif
  if (tp == 'L' && (map[y][x].tip & TMASK) == T_MSGATE && !have_authority(n,MS_UNLOCK,x,y)) /* bug fix 20000701 */
    return (0);
  if (i<-1) i=0;
  return (f < 0 ? -1 : i);
}

int    set_title(int n, int x, int y, int f)
/* n のキャラが x, y の位置の f番目の書き込みを新たな看板タイトルにする */
/* 旧タイトルは単なる書き込みに変化する                                 */
/* 0(成功) -1(オーナーじゃない) -2(書き込みがない)                      */
{
  char   own;
  int    xx, yy;
  FILE  *rfp, *wfp;
  /* rbuf は CPYSTR の宛先のため C 配列のまま */
  char   rbuf[256];
  char  *p;
  int    i, j;

  j = std::strlen(cd[n].uid);
  /* まずバックアップ */
#ifdef SDI_AREA
  snprintf(rbuf,sizeof(rbuf),"phi_sdi/sd.%02x",fld[map[y][x].area].area);
  if ((rfp = efopen(rbuf, "rb")) == nullptr)
#else
  if ((rfp = efopen("phi.sd", "rb")) == nullptr)
#endif
    return (1);
  if ((wfp = efopen("phi.sd.bak", "wb")) == nullptr) {
    efclose(rfp);
    return (1);
  }
  for (i = 0, own = 0;;) {
    if (efgets(rbuf, 255, rfp) == nullptr)
      break;
    if (*rbuf != ':')
      break;
    p = rbuf + 2;
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    if (xx < 1 || xx >= MAX_X - 1 || yy < 1 || yy >= MAX_Y - 1)
      continue;
    SKIP_SPACE(p);
    switch (*(rbuf + 1)) {
    case 'G':
    case 'I':
      break;
    case 'L':
      switch (map[yy][xx].tip & TMASK) {
      case T_LBOX:
      case T_LGATE:
      case T_LSGATE:
      case T_LMSGATE:
  break;
      default:
  continue;   /* ロック情報は今もされているところのみバックアップ */
      }
      break;
    }
    if (x == xx && y == yy) {
      if (*(rbuf + 1) == 'T' && !std::strncmp(p, cd[n].uid, j) && CHKSP(p+j))
  own = 1;
      if (*(rbuf + 1) == 'B') /* bug fix for sd broken 20030915 */
        i++;
    }
    fputs(rbuf, wfp);
  }
  CPYSTR(rbuf, ".\n");
  fputs(rbuf, wfp);
  efclose(wfp);
  efclose(rfp);
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"bakup phi.sd complete(save_sd)");
  log_print(debug_buf);
#endif

  if (!own && !have_authority(n, MS_DELETE,x,y))   /* オーナーじゃない */
    return -1;

  if (i < f) /* bug fix for sd broken 20030915 */
    return -2;

  /* phi.sd の改変 */
  if ((rfp = efopen("phi.sd.bak", "rb")) == nullptr)
    return (1);
#ifdef SDI_AREA
  snprintf(rbuf,sizeof(rbuf),"phi_sdi/sd.%02x",fld[map[y][x].area].area);
  if ((wfp = efopen(rbuf, "wb")) == nullptr)
#else
  if ((wfp = efopen("phi.sd", "wb")) == nullptr)
#endif
  {
    efclose(rfp);
    return (1);
  }

  for (i = 0;;) {
    if (efgets(rbuf, 255, rfp) == nullptr)
      break;
    if (*rbuf != ':')
      break;
    p = rbuf + 2;
    xx = decimal_load(3, &p);
    yy = decimal_load(3, &p);
    if (x == xx && y == yy) {
      switch(*(rbuf+1)) {
        case 'B':
          i++;      /* 書き込みの存在数 */
          if (i == f)
            *(rbuf+1) = 'T';
          break;
        case 'T':
          *(rbuf+1) = 'B';
          break;
      }
    }
    fputs(rbuf, wfp);
  }
  CPYSTR(rbuf, ".\n");
  fputs(rbuf, wfp);
  efclose(wfp);
  efclose(rfp);
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"save_sd complete");
  log_print(debug_buf);
#endif
  return (0);
}

void   board_title(int n,char *title,int len)
{
  int    x, y;
  FILE  *fp;
  std::array<char, 256> rbuf; /* , wbuf[256] */
  char  *p;

#ifdef SDI_AREA
  snprintf(rbuf.data(),rbuf.size(),"phi_sdi/sd.%02x",fld[map[cd[n].y][cd[n].x].area].area);
  if ((fp = efopen(rbuf.data(), "rb")) == nullptr)
#else
  if ((fp = efopen("phi.sd", "rb")) == nullptr)
#endif
    return;
  for (;;) {
    if (efgets(rbuf.data(), 255, fp) == nullptr)
      break;
    if (*rbuf.data() != ':')
      break;
    if (*(rbuf.data() + 1) != 'T')
      continue;
    p = rbuf.data() + 2;
    SKIP_SPACE(p);
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (cd[n].x != x || cd[n].y != y)
      continue;
    snprintf(title, len,"#map B:%s", rbuf.data() + 36);
    break;
  }
  efclose(fp);
  return;
}

int    list_board(int n, int s)
{
  int    x, y;
  FILE  *fp;
  std::array<char, 256> rbuf, wbuf;
  char  *p;
  int    i;

#ifdef SDI_AREA
  snprintf(rbuf.data(),rbuf.size(),"phi_sdi/sd.%02x",fld[map[cd[n].y][cd[n].x].area].area);
  if ((fp = efopen(rbuf.data(), "rb")) == nullptr)
#else
  if ((fp = efopen("phi.sd", "rb")) == nullptr)
#endif
    return (0);
  for (i = 0;;) {
    if (efgets(rbuf.data(), 255, fp) == nullptr)
      break;
    if (*rbuf.data() != ':')
      break;
    if (*(rbuf.data() + 1) != 'B')
      continue;
    p = rbuf.data() + 2;
    SKIP_SPACE(p);
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (cd[n].x != x || cd[n].y != y)
      continue;
    i++;
    if (i < s)
      continue;
    if (i >= s + BOARD_FULL) {
    if (IS_CLIENT_RPHIWIN(n))
      snprintf(wbuf.data(), wbuf.size(),"[%2i] ... and more. \n", i);
    else
      snprintf(wbuf.data(), wbuf.size(),"9) [%2i] ... and more. \n", i);

      check_write(n, wbuf.data());
      cd[n].vf = i;
      break;
    }

    /*** PSP7 by Aleph ***/
    if (*yname != '\0') /* 西暦との差 */
      year_format(rbuf.data(),rbuf.size()-1);

    if (IS_CLIENT_RPHIWIN(n))
        snprintf(wbuf.data(),wbuf.size(),"[%2i] %s", i, rbuf.data() + 36);
    else
        snprintf(wbuf.data(),wbuf.size(),"%i) [%2i] %s", i-s+1, i, rbuf.data() + 36);
    /*** PSP7 END ***/

    check_write(n, wbuf.data());
  }
  efclose(fp);
  return (i);
}

int    search_board(int n, char *str)
{
  int    x, y;
  FILE  *fp;
  std::array<char, 256> rbuf, wbuf;
  char  *p;
  int    i;

#ifdef SDI_AREA
  snprintf(rbuf.data(),rbuf.size(),"phi_sdi/sd.%02x",fld[map[cd[n].y][cd[n].x].area].area);
  if ((fp = efopen(rbuf.data(), "rb")) == nullptr)
#else
  if ((fp = efopen("phi.sd", "rb")) == nullptr)
#endif
    return (0);
  check_write(n, "#more\n");
  for (i=0;;) {
    if (efgets(rbuf.data(), 255, fp) == nullptr)
      break;
    if (*rbuf.data() != ':')
      break;
    if (*(rbuf.data() + 1) != 'B' && *(rbuf.data() + 1) != 'T')
      continue;
    p = rbuf.data() + 2;
    SKIP_SPACE(p);
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (*yname != '\0')
       year_format(rbuf.data(),rbuf.size()-1);
    if (fstrstr(rbuf.data() + 36, str) == nullptr)
      continue;
    i++;
    if (cd[n].exp == 99999999) /* Add for マスターの場合は絶対座標を表示する　by NEO */
    	snprintf(wbuf.data(), wbuf.size()," [%3i %3i] %s", x, y, rbuf.data() + 36);
    else 
    	snprintf(wbuf.data(), wbuf.size()," [%+3iW,%+3iN] %s", cd[n].x - x, cd[n].y - y, rbuf.data() + 36);
    check_write(n, wbuf.data());
  }
  check_write(n, "#end-more\n");
  efclose(fp);
  return (i);
}


void   get_title(char *p)
{
  std::array<char, 256> buf;
  FILE  *fp;
  int    i;

  snprintf(buf.data(), buf.size(),"phi_docs/%s", p);
  if ((fp = efopen(buf.data(), "rb")) == nullptr)
    return;
  efgets(buf.data(), 255, fp);
  for (i = 0; i < 128 && buf[i] && buf[i] != '\n'; i++, p++)
    *p = buf[i];
  *p = 0;
  efclose(fp);
  return;
}

void   load_board(int n, char *fnm)
{
  std::array<char, 256> buf;
  FILE  *fp;

  snprintf(buf.data(), buf.size(),"phi_docs/%s", fnm);
  if ((fp = efopen(buf.data(), "rb")) == nullptr)
    return;
  check_write(n, "#more\n");
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    check_write(n, buf.data());
  }
  check_write(n, "#end-more\n");
  efclose(fp);
  return;
}

void year_format(char *p, int len)
{
  std::array<char, 256> buf;
  std::array<char, 256> tstr;
  char *bufp;

  bufp = p + 27;
  abstime = (time_t) hexadecimal_load(8, &bufp);
  sttime = (struct tm *) localtime(&abstime);
  strftime(tstr.data(), 25, "/%m/%d %H:%M:%S", sttime);
  snprintf(buf.data(), buf.size(),"(%s%d%s)%s", yname, (sttime->tm_year)+yslide+1900, tstr.data(), p+70);
  strlcpy(p+52,buf.data(), len-52);
}

void   append_file(int n, char *fnm, char *mes)
{
  FILE  *fp;
  std::array<char, 256> buf;
  std::array<char, 16> fid;

  id2f(n,fid.data());
  snprintf(buf.data(), buf.size(),"%s.%s", fnm, fid.data());
  if ((fp = efopen(buf.data(), "ab")) == nullptr)
    return;
  snprintf(buf.data(), buf.size(),"%s\n", mes);
  fputs(buf.data(), fp);
  efclose(fp);
  return;
}

/* add 20010215 */
/* vs write crack */
void   id2f(int n, char *buf)
{
    int i;
    char *p;

    p = cd[n].uid;
    for (i=0;i<16 && *p != '\0';i++) {
        if (*p != '/')
            *buf++ = *p;
        p++;
    }
    *buf = '\0';
}


int    save_id(int n, int in, int f)
{
  ItemOData **ip, *ip2;
  int    i, j;

  CPYSTR(cd[n].str, "Nothing");
  if (f) {      /* アイテムをその場におく */
    if (in < MAX_ITEMS && cd[n].id[in].type != DT_NONE
  && (cd[n].id[in].type & MT_MASK) != MT_NOBREAK) {
      /* add for storehouse 20020823 */
      if ((map[cd[n].y][cd[n].x].tip & TMASK) == T_STORE) {
        save_store(n,in,1,nullptr);
      } else {
        additem(cd[n].x, cd[n].y, &(cd[n].id[in]), 0);
        map[cd[n].y][cd[n].x].tip |= T_ITEM;
      }
      CPYSTR(cd[n].str, cd[n].id[in].name);
      if (n) /* bug fix 20001024 */
        std::memcpy(&putbuf, &(cd[n].id[in]), sizeof (ItemData));
      delete_item(&(cd[n].id[in]));
      return (0);
    }
    return -1;
  } else {      /* アイテムまたはお金を拾う */
    /* ban to get anything in locked box 20011130 */
    if ((map[cd[n].y][cd[n].x].tip & TMASK) == T_LBOX &&
         !have_authority(n,MS_UNLOCK,cd[n].x,cd[n].y))
        return (-1);
    /* add for storehouse 20020823 */
    if ((map[cd[n].y][cd[n].x].tip & TMASK) == T_STORE) {
        return (save_store(n,in,0,nullptr));
    } else {
    ip = nullptr;
    for (j = 0; j < in; j++) {
      ip = finditem(cd[n].x, cd[n].y, ip);
      if (ip == nullptr)
  return (-1);    /* そんなアイテムなし */
    }
    if (is_gp(*ip)) {   /* お金を拾う */
      cd[n].gp += jb_gp_gain(n, (*ip)->gp);  /* 商人ボーナス (PHIDM NEO extension) */
      if (cd[n].gp > 9999) {
  tax += cd[n].gp - 9999;
  cd[n].gp = 9999;
      }
    } else {      /* アイテムを拾う */
      for (i = 0; i < MAX_ITEMS && cd[n].id[i].type != DT_NONE; i++);
      if (i >= MAX_ITEMS)
  return (-1);    /* 持つ余裕なし */
      std::memcpy(&(cd[n].id[i]), &((*ip)->id), sizeof(ItemData));
      if (n) /* bug fix 20001024 */
        std::memcpy(&putbuf, &(cd[n].id[i]), sizeof (ItemData)); /* for take switch 20000803 */
    }
    item_name_on_floor(cd[n].str, *ip,sizeof(cd[n].str));
    ip2 = *ip;
    *ip = (*ip)->next;
    free(ip2);
    ip = nullptr;
    for(;;) {
      ip = finditem(cd[n].x, cd[n].y, ip);
      if (ip == nullptr || !is_trap(*ip))
        break;
    }
    if (ip == nullptr) {
      map[cd[n].y][cd[n].x].tip &= ~T_ITEM;
      delitempos(cd[n].x,cd[n].y);
    }
    }
  }
  return (0);
}

int save_store(int n, int in, int f, char *nm)
{
    FILE *rfp,*wfp;
    std::array<char, 256> buf;
    /* buf2 は CPYSTR の宛先のため C 配列のまま */
    char   buf2[256];
    std::array<char, 32> rname, wname;
    char *p;
    int i, j;

    if (n >= MAX_NTTYS || n < 1)
        return -1;

    j = 0;
    if (!f) {
      for (j = 0; j < MAX_ITEMS && cd[n].id[j].type != DT_NONE; j++);
        if (j >= MAX_ITEMS)
          return (-1);    /* 持つ余裕なし */
    }

    snprintf(rname.data(),rname.size(),"phi_store/%s.str",cd[n].uid);
    if ((rfp = efopen(rname.data(), "rb")) == nullptr && !f)
      return (-1);
    snprintf(wname.data(),wname.size(),"phi_store/%s.str.bak",cd[n].uid);
    if ((wfp = efopen(wname.data(), "wb")) == nullptr) {
      efclose(rfp);
      return (-1);
    }

  for(i = 0;rfp != nullptr;) { /* i はアイテム番号 */

      if (efgets(buf.data(), buf.size(), rfp) == nullptr)
  break;
      if (*buf.data() != ':' && *(buf.data()+1) != 'I')
  continue;
        i++;

      if (!f) {
        if (nm != nullptr) {
            p = buf.data()+3;
            loadstr(buf2,&p,DSPACE,sizeof(buf2));
        }
        if (i == in || (nm != nullptr && !std::strcmp(buf2,nm))) {
          load_item(&(cd[n].id[j]),buf.data()+2);
          std::memcpy(&putbuf, &(cd[n].id[j]), sizeof (ItemData));
          CPYSTR(cd[n].str, putbuf.name);
          nm = nullptr; in = -1;
          continue;
        }
      } else {
        if (in >= 0) {
            p = buf.data()+3;
            loadstr(buf2,&p,DSPACE,sizeof(buf2));
            if (std::strcmp(buf2,cd[n].id[in].name)>0) {
                CPYSTR(buf2,":I");
                save_item(&(cd[n].id[in]),buf2+2,sizeof(buf2)-2);
                fputs(buf2,wfp);
                in = -1;
            }
        }
      }
      fputs(buf.data(),wfp);
  }
  if (in >= 0) {
    /* for endless "get nothing" bug 20030705 */
    if (!f) {
      i = 0;
    } else {
      CPYSTR(buf2,":I");
      save_item(&(cd[n].id[in]),buf2+2,sizeof(buf2)-2);
      fputs(buf2,wfp);
    }
  }

  efclose(wfp);
  if (rfp != nullptr)
  efclose(rfp);
  rename(wname.data(),rname.data());

  return (!i ? -1 : 0);
}

void   save_gp(int n)
{
  ItemData id;

  if (cd[n].gp <= 0)
    return;
  id.name = findstr(cirunit);
  id.type = DT_GP;
  id.wp = 1;
  id.fn = FN_NONE;
  id.ATP = 1;
  id.ER = 0;
  id.AE = AE_NONE;
  id.SE = SE_NONE;
  id.active_time = 0;
  id.extension = nullptr; /* bug fix 20000204 */
  additem(cd[n].x, cd[n].y, &id, cd[n].gp);
  map[cd[n].y][cd[n].x].tip |= T_ITEM;
  return;
}

void   str_gc()
/* shash の garbage collection  */
/* 使用しなくなった文字列を解放 */
{
  int    i, j, x;
  StringData *p, **pp;
  ItemOData **ip;
  char  *dp;
  CrtData *q;
  ExportItemData *r;
  ItemPData *ipp;

  for (i = 0; i < HASHMAX; i++) { /* フラグをクリア */
    p = shash[i];
    while (p != nullptr) {
      p->c = 0;
      p = p->next;
    }
  }
  for (i = 1; i < enmax; i++) { /* クリーチャーの所持データ */
  /* PHI:00055 Bug Fix by EL */
    if (cd[i].type == CT_NOEXIST){
      if(i >= MAX_NTTYS) continue;
      if(cd[i].st == ST_CONNECTSERVER || cd[i].st == ST_CONNECTSERVER_ || cd[i].st == ST_SENDCHR){
  findstrdata(cd[i].name)->c = -1;
  continue;
      }
      if(cd[i].st != ST_RESERVE && cd[i].st != ST_TRANSFER &&
         cd[i].st != ST_TRYRESERVE && cd[i].st != ST_RECEIVECOMPLETE &&
         cd[i].st != ST_TRANSFAILE) continue;
    }
    if (i < MAX_NTTYS) {
      findstrdata(cd[i].name)->c = -1;
/****** 97/Jun/9 b-Yak-38 ADD ******/
      if (cd[i].cg != nullptr)
        findstrdata(cd[i].cg)->c = -1;
      if (cd[i].ocg != nullptr)
        findstrdata(cd[i].ocg)->c = -1;
/***********************************/
    } else if (cd[i].cg != ed_o[cd[i].et].cg) /* bug fix by Qajin 20000408 */
      findstrdata(cd[i].cg)->c = -1;
    /* add for suport formatA 20000928 */
    if (cd[i].formatA != nullptr)
      findstrdata(cd[i].formatA)->c = -1;
    for (j = 0; j < MAX_ITEMS; j++) {
      if (cd[i].id[j].type != DT_NONE) {
  findstrdata(cd[i].id[j].name)->c = -1;
        if (cd[i].id[j].extension != nullptr)
            findstrdata(cd[i].id[j].extension)->c = -1;
      }
      if (cd[i].md[j].spell != nullptr) {
  findstrdata(cd[i].md[j].spell)->c = -1;
        if (cd[i].md[j].extension != nullptr)
            findstrdata(cd[i].md[j].extension)->c = -1;
      }
      if (cd[i].td[j].string != nullptr)
  findstrdata(cd[i].td[j].string)->c = -1;
    }
  }

  for (i = 0; i < max_enemy; i++) { /* クリーチャーの元データ */
    if (ed_o[i].apc == 3) { /* 召喚アイテム名 */
      if (ed_o[i].apnm != nullptr)
  findstrdata(ed_o[i].apnm)->c = -1;
      if (ed_o[i].apdm != nullptr)
  findstrdata(ed_o[i].apdm)->c = -1;
    }
    for (j = 0;; j++) {
      if (ed_o[i].sd[j].flg == '\0')
  break;
      switch (ed_o[i].sd[j].flg) {
      case 'M':   /* 魔法 */
  findstrdata(ed_o[i].sd[j].data.md.spell)->c = -1;
  if(ed_o[i].sd[j].data.md.extension != nullptr)
      findstrdata(ed_o[i].sd[j].data.md.extension)->c = -1;
  continue;
      case 'I':   /* アイテム */
  findstrdata(ed_o[i].sd[j].data.id.name)->c = -1;
  if(ed_o[i].sd[j].data.id.extension != nullptr)
      findstrdata(ed_o[i].sd[j].data.id.extension)->c = -1;
  continue;
      case 'T':   /* 会話 */
  findstrdata(ed_o[i].sd[j].data.td.string)->c = -1;
  continue;
      case 'Z':   /* DUMMY */
  continue;
#ifdef DEBUG_STR_GC
      default:
  snprintf(debug_buf,sizeof(debug_buf),"str_gc:error:ed_o[%d].sd[%d].flg=(%d)",
        i, j, (int)ed_o[i].sd[j].flg);
  log_print(debug_buf);
#endif
      }
    }
  }

  /* 落ちているアイテム change 20020828 */
  for (i = 0; i < HASHMAX; i++)
    for(ipp = iphash[i]; ipp != nullptr; ipp = ipp->next) {
        dp = nullptr;
        for(ip = &(ipp->fid); ip != nullptr && *ip != nullptr; ip = finditem(ipp->x,ipp->y,ip)) {
            if (dp != (*ip)->id.name)
                findstrdata((dp = (*ip)->id.name))->c = -1;
            if ((*ip)->id.extension != nullptr)
                findstrdata((*ip)->id.extension)->c = -1;
        }
    }

    for (i = 0; i < gatemax; i++) { /* ゲートデータ */
      if(gatedata[i] == nullptr)
  continue;
      j = 0;
      switch (gatedata[i][j++].di) {
      case 'G':
      case 'g':
  j += 4;
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  break;
      case 'P':
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  break;
      case 'H':
  j += 3;
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  if ((dp = gatedata[i][j++].dp) != nullptr) {
    findstrdata(dp)->c = -1;
  }
  break;
      }
    }

    for (i = 0; i < swtn; i++) {/* SWITCH DATA */
      if (swtd[i] == nullptr)
  continue;
      j = 0;
/* delete for quick message search 20030501
      switch (swtd[i][j++].di) {
      case 'S':
*/
  j += 2;
  /* 条件 */
  switch (swtd[i][j++].di) {
  case 0:
  case 2:
  case 17: /* add for hit switch 20020530 */
  case 22:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
  case 23:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
  case 24:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
    break;
  case 1:   /* あるレベルに達した時 */
  case 6:   /* 無言プレート */
  case 11:    /* あるレベルに達していない時 */
  case 16:    /* 無言プレート */
  case 25:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
    j += 2;
    break;
  case 3:   /* 特定のメッセージを話した時 */
  case 7:   /* NPCが特定の言葉を話した時 */
  case 13:  /* パターンに合致するメッセージを話した時 added 20000101 */
  case 30:  /* object on 20011203 */
  case 31:
  case 21:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
    if ((dp = swtd[i][j++].dp) != nullptr) {
      findstrdata(dp)->c = -1;
    }
    break;
  case 4:   /* アイテムをその場においた時 */
  case 8:   /* アイテム所持 */
  case 18:  /* アイテムを拾った時 */
    findstrdata(swtd[i][j++].dp)->c = -1;
    j += 7;
    if(swtd[i][j].dp != nullptr)
       findstrdata(swtd[i][j].dp)->c = -1;
    j++;
    break;
  case 14:  /* 魔法を唱えた時 */
    findstrdata(swtd[i][j++].dp)->c = -1;
    j += 2;
    if(swtd[i][j].dp != nullptr)
       findstrdata(swtd[i][j].dp)->c = -1;
  case 5:   /* お金をおいた時 */
  case 45:   /* add for キャラ位置お金出現　by NEO 2005/01/08　*/
  case 9:   /* 総重量 bug fix by Qajin 19991217 */
  case 15:  /* turn check */
    j += 1;
    break;
  case 10:    /* フラグ */
  case 12:
    j += swtd[i][j].di; /* 不定長 */
    j += 1;
    break;
  case 20:  /* 変数チェック */
    j += 4;
    break;
  default:
    break;
  }
  j += 2;
  /* 動作 */
  switch (swtd[i][j++].di) {
  case 16:  /* 特殊範囲メッセージ added 20000102 */
     x = swtd[i][j++].di;
     if (x == 2 || x == 3)
         j++;
  case 3:   /* メッセージ */
  case 90:  /* ログ */
  case 11:  /* 広範囲地形変化 added 20000102 */
  case 7:   /* クリーチャー発生スイッチ */
  case 14:
  case 15:  /* added by Qajin 19991224 */
  case 21: /* オブジェクト移動 */
  case 22: /* オブジェクト非表示 */
  case 25: /* show 20010621 */
  case 33:  /* ADD スイッチデータ追加 2005/12/29 Savarl.F */
  case 60:  /* 刻印スイッチ (PHIDM NEO extension) */
  case 61:  /* 刻印表示スイッチ (PHIDM NEO extension) */
  case 62:  /* プログラム実行スイッチ (PHIDM NEO extension) */
  case 63:  /* FX スイッチ (PHIDM NEO extension) */
  case 64:  /* FX スイッチ (PHIDM NEO extension) */
  case 65:  /* FX スイッチ (PHIDM NEO extension) */
  case 66:  /* FX スイッチ (PHIDM NEO extension) */
  case 67:  /* FX スイッチ (PHIDM NEO extension) */
  case 68:  /* FX スイッチ (PHIDM NEO extension) */
    if ((dp = swtd[i][j++].dp) != nullptr) {
      findstrdata(dp)->c = -1;
    }
    break;
  case 4:   /* アイテム出現 */
  case 44:   /* add for キャラ位置アイテム出現　by NEO 2005/01/08　*/
    if ((dp = swtd[i][j++].dp) != nullptr) {
      findstrdata(dp)->c = -1;
    }
    j += 7;
    if(swtd[i][j].dp != nullptr)
       findstrdata(swtd[i][j].dp)->c = -1;
    break;
  case 20:    /* 確率フラグ */
    j += 1;
  case 10:    /* フラグ */
  case 81:    /* フラグ (スイッチを起動しない) (PHIDM NEO extension, from JET) */
  case 82:    /* フラグ (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
    j += 1;
    j += swtd[i][j].di; /* 不定長 */
    j += 1;
    break;
  case 50:    /* 演算 */
  case 51:    /* 演算 (スイッチを起動しない) (PHIDM NEO extension, from JET) */
  case 52:    /* 演算 (座標つきスイッチだけ起動) (PHIDM NEO extension, from JET) */
    break;
  default:
    break;
  }
/* delete for quick message search 20030501
  break;
      case 'A':
  j += 3;
  if (swtd[i][j++].di == 0) { * change for less memory 20000627 *
    if ((dp = swtd[i][j++].dp) != NULL) {
      findstrdata(dp)->c = -1;
    }
  }
  break;
      }
*/
    }

  /* add for quick message search 20030501 */
  for(i=0;i<msgn;i++) {
    if(msgd[i].type == 0 || msgd[i].type == 3) {
      if ((dp = msgd[i].data.mesg) != nullptr) {
        findstrdata(dp)->c = -1;
      }
    }
  }

  /* for ban   added by Qajin 19991020 */
  for(i=0;i<BANNEDIPMAX;i++)
    if (ip_ban[i].ip != nullptr)
        findstrdata(ip_ban[i].ip)->c = -1;

  /* for power spell added by Qajin 20000130 */
  for(i=0;i<MAX_POWER_SPELL;i++) {
    if (power_spell[i].pwmg.spell != nullptr) {
        findstrdata(power_spell[i].pwmg.spell)->c = -1;
        if (power_spell[i].pwmg.extension != nullptr)
            findstrdata(power_spell[i].pwmg.extension)->c = -1;
      }
  }

  for(q=first_create_data; q!=nullptr; q=q->next){
    if(q->product!=nullptr) {
      findstrdata(q->product->name)->c = -1;
      if(q->product->extension != nullptr)
        findstrdata(q->product->extension)->c = -1;
    }
    if(q->fail_product!=nullptr) {
      findstrdata(q->fail_product->name)->c = -1;
      if(q->fail_product->extension != nullptr)
        findstrdata(q->fail_product->extension)->c = -1;
    }
    findstrdata(q->staff1->name )->c = -1;
    if(q->staff1->extension != nullptr)
      findstrdata(q->staff1->extension)->c = -1;

    findstrdata(q->staff2->name )->c = -1;
    if(q->staff2->extension != nullptr)
      findstrdata(q->staff2->extension)->c = -1;
  }

  for(r=first_export_data; r!=nullptr; r=r->next)
    if(r->type == 'I' || r->type == 'i') {
      findstrdata(r->data.id->name)->c = -1;
      if(r->data.id->extension != nullptr)
          findstrdata(r->data.id->extension)->c = -1;
    } else if(r->type == 'M' || r->type == 'm') { /* add for ... 20001201 */
      findstrdata(r->data.md->spell)->c = -1;
      if(r->data.md->extension != nullptr)
          findstrdata(r->data.md->extension)->c = -1;
    }

#if MAX_OBJECTS > 0
  /* オブジェクト */
  for(i=0;i<MAX_OBJECTS;i++){
    if(od[i].gname != nullptr)
        findstrdata(od[i].gname)->c = -1;
    if(od[i].name != nullptr)
        findstrdata(od[i].name)->c = -1;
    if(od[i].id != nullptr)
        findstrdata(od[i].id)->c = -1;

  }
#endif

  /* マップパーツセット */
  for(i=0;i<flr_max;i++) {
    if(fld[i].mapset != nullptr)
        findstrdata(fld[i].mapset)->c = -1;
  }

  /* add for menu 20041212 */
  for(i=0;i<menun;i++) {
    if((dp = menud[i].sel) != nullptr) {
        findstrdata(dp)->c = -1;
    }
    if((dp = menud[i].caption) != nullptr) {
        findstrdata(dp)->c = -1;
    }
  }

  for (i = 0; i < HASHMAX; i++) { /* 領域解放 */
    pp = shash + i;
    while (*pp != nullptr) {
      if ((*pp)->c == 0) {
  p = *pp;
  *pp = (*pp)->next;
#ifdef DEBUG_STR_GC
snprintf(debug_buf,sizeof(debug_buf),"throw %s",p->string);
log_print(debug_buf);
#endif
  free(p->string);
  free(p);
      } else {
  pp = &((*pp)->next);
      }
    }
  }
#ifdef DEBUG
  snprintf(debug_buf,sizeof(debug_buf),"str_gc complete");
  log_print(debug_buf);
#endif
}

void   item_destruct()
{
  int    x, y;
  ItemOData **ip, *ip2,**ipb;
  int    i;
  int    f;

  for (x = 0; x < MAX_X; x++) {
    for (y = 0; y < MAX_Y; y++) {
      ip = nullptr;
      if (map[y][x].tip & (T_ITEM | T_TRAP)) {
  f = ~(T_ITEM | T_TRAP);
  for (;;) {
    ipb = ip; /* bug fix 20020601 */
    ip = finditem(x, y, ip);
    if (ip == nullptr)
      break;
    if (is_gp(*ip)) { /* お金 */
      if ((--((*ip)->gp)) == 0) {
        ip2 = (*ip);
        *ip = (*ip)->next;
        free(ip2);
        ip = ipb; /* bug fix 20020601 */
      } else
        f |= T_ITEM;
      tax++;
    } else {
    /* add for variable multi characters in one space 20000721 */
        for (i = 0; i < MAX_CHRS_INONE; i++)
            if (map[y][x].chr[i])
                break;
      if (i < MAX_CHRS_INONE ||
    ((map[y][x].tip & TMASK) == T_BOX) ||
    ((map[y][x].tip & TMASK) == T_LBOX)) {
        f |= (is_trap(*ip) ? T_TRAP : T_ITEM);
        continue;
    }
      if (degrade(&((*ip)->id))==2) {
        ip2 = (*ip);
        *ip = (*ip)->next;
        free(ip2);
        ip = ipb; /* bug fix 20020601 */
      } else {
        f |= (is_trap(*ip) ? T_TRAP : T_ITEM);
      }
    }
  }
  map[y][x].tip &= f;
  if (!(map[y][x].tip & (T_ITEM | T_TRAP)))
    delitempos(x, y);
      }
    }
  }
}

void   min_proc()
{
  ItemData id;
  int    x, y, dx, dy;

  remap_gate_and_switch();

  item_destruct();
  for (y = 0; y < MAX_Y; y++) {
    for (x = 0; x < MAX_X; x++) {
      switch (map[y][x].tip & TMASK) { /* 草が生える */
      case T_WATER:
  if (PER_DICE >= 50)
    break;
      case T_BWATER: /* add 20000711 */
      case T_TREE:
  if (PER_DICE >= 50)
    break;
      case T_PLANT:
  if (PER_DICE >= 50)
    break;
      case T_FLOWER:
  if (PER_DICE >= 2)
    break;
  dx = x;
  dy = y;
  switch (PER_DICE / 25) {
  case 0:
    dy--;
    break;
  case 1:
    dx++;
    break;
  case 2:
    dy++;
    break;
  case 3:
    dx--;
  }
  if ((dx > 0 && dx < MAX_X - 1) && (dy > 0 && dy < MAX_Y - 1) &&
      !(map[dy][dx].tip & T_ROOF) && !(map[dy][dx].tip & TMASK)) {
    map[dy][dx].tip |= T_PLANT;
  }
  break;
      }
      switch (map[y][x].tip & TMASK) {
      case T_FLOWER:
  if (PER_DICE < 10) {  /* 実がなる */
    map[y][x].tip &= TCLEAR;
    if (PER_DICE < 95)
      map[y][x].tip |= T_PLANT;
    if (PER_DICE < 5) {
      id.name = findstr("Seeds");
      id.type = (MT_WOOD | WT_FIST | IT_RESOURCE);
      id.UE = RS_PLANT;
      id.VALUE2 = 0;
      id.SE = SE_NONE;
      /* kzmが勝手に付け加えた */
/*  } else if (PER_DICE < 15){
      id.name = findstr("Uni");
      id.type = (MT_WOOD | WT_FIST | IT_CHARGE);
      id.VALUE = 1;
      id.VALUE2 = 10; */
    } else {
      id.name = findstr("Nuts");
      id.type = (MT_WOOD | WT_FIST | IT_FOOD);
      id.VALUE = 1;
      id.VALUE2 = 0;
      id.ENTROP = 0;
    }
    id.wp = id.ATP = 1;
    id.fn = FN_EARTH;
    id.extension = nullptr;
    additem(x, y, &id, 0);
    map[y][x].tip |= T_ITEM;
  }
  break;
      case T_PLANT:   /* 草が枯れる／花が咲く */
  if (PER_DICE < 1) {
    map[y][x].tip &= TCLEAR;
    if (PER_DICE < 95)
      map[y][x].tip |= T_FLOWER;
  }
  break;
      }
    }
  }
  return;
}

void    clear_gate_and_switch_on_map(){
  int x, y;
  for(y=1; y<=MAX_Y-2; y++)
    for(x=1; x<=MAX_X-2; x++){
      if((map[y][x].tip & TMASK) == T_TGATE)
  map[y][x].tip = (map[y][x].tip & TCLEAR) | T_EWALL ;
  map[y][x].tip &= ~T_SWITCH;
    }
}

void    remap_gate_and_switch()
{
  int  x, y, cn;

  for(cn=0;cn<gatemax;cn++) {
    if (gatedata[cn] == nullptr)
        continue;
    if (gatedata[cn][0].di == 'G') {
      x = gatedata[cn][1].di;
      y = gatedata[cn][2].di;
      if (x < 1 || x >= MAX_X - 1 || y < 1 || y >= MAX_Y - 1)
  continue;
      map[y][x].tip = ((map[y][x].tip & TCLEAR) | T_TGATE);
    }
  }

  for(cn=0;cn<swtn;cn++) {
    if (swtd[cn] == nullptr)
        continue;
/*
    if (swtd[cn][0].di == 'S') {
*/
      x = swtd[cn][0].di;
      y = swtd[cn][1].di;
      if (x < 1 || x >= MAX_X - 1 || y < 1 || y >= MAX_Y - 1)
  continue;
      map[y][x].tip |= T_SWITCH;
/*
    }
*/
  }
}

int    list_item(int n, int s)
/* 識別子 n のキャラクタがいる場所で、                                */
/* 1) アイテムが存在しない場合、マップ上からアイテムの存在表示を消し、*/
/*    返却値として 0 を返す。(安全性のためか？)                       */
/* 2) (nm == NULL の場合、                                            */
/*    tp で指定されたアイテムタイプのアイテムの表示を) 条件削除 20020823 */
/*    (tp == 0 の場合は全てのアイテムに対して)                        */
/*    s 番目から s + ITEM_FULL -1 番目まで行う。                      */
/*    s + ITEM_FULL 番目以上のアイテムが存在する時は、                */
/*    cd[n].vf に s + ITEM_FULL が入る。返却値は 1。(返値変更 20020823) */
{
  ItemOData **ip;
  int    i;
  std::array<char, 32> itemname;
  std::array<char, 256> buf;

  if((map[cd[n].y][cd[n].x].tip & TMASK) == T_STORE) {
    return (list_store(n,s));
  }

  if((ip = finditem(cd[n].x, cd[n].y, nullptr))==nullptr){
    map[cd[n].y][cd[n].x].tip &= ~T_ITEM;
    delitempos(cd[n].x,cd[n].y);
    return 0;
  }

  /* add for loading item in circle 20020807 */
  load_id_in_circle(cd[n].x,cd[n].y);

  for(i = 1; /* i はアイテム番号 */
      ip != nullptr;
      ip = finditem(cd[n].x, cd[n].y, ip),i++){

      if(i < s) continue;
      if(i >= s + ITEM_FULL){
        if (IS_CLIENT_RPHIWIN(n))
          snprintf(buf.data(), buf.size(),"[%2i] ... and more. \n", i);
        else
          snprintf(buf.data(), buf.size(),"9) [%2i] ... and more. \n", i);
  check_write(n, buf.data());
  cd[n].vf = i;
  break;
      }
      item_name_on_floor(itemname.data(), *ip,itemname.size());
      if (IS_CLIENT_RPHIWIN(n))
        snprintf(buf.data(), buf.size(),"[%2i] %s \n", i, itemname.data());
      else
        snprintf(buf.data(), buf.size(),"%1i) [%2i] %s \n", i-s+1, i, itemname.data());
      check_write(n, buf.data());

  }
  return 1;
}

/* 倉庫内アイテムリストアップ */
/* add 20020823 */
int list_store(int n, int s)
{
    FILE *fp;
    std::array<char, 256> rbuf, wbuf;
    int i;

    if (n >= MAX_NTTYS)
        return 0;

    snprintf(rbuf.data(),rbuf.size(),"phi_store/%s.str",cd[n].uid);
    if ((fp = efopen(rbuf.data(), "rb")) == nullptr)
      return (0);

  for(i = 0;;) { /* i はアイテム番号 */

      if (efgets(rbuf.data(), rbuf.size(), fp) == nullptr)
  break;
      if (*rbuf.data() != ':' || *(rbuf.data()+1) != 'I')
  continue;
        i++;

      if(i < s) continue;
      if(i >= s + ITEM_FULL){
        if (IS_CLIENT_RPHIWIN(n))
          snprintf(wbuf.data(), wbuf.size(),"[%2i] ... and more. \n", i);
        else
          snprintf(wbuf.data(), wbuf.size(),"9) [%2i] ... and more. \n", i);
  check_write(n, wbuf.data());
  cd[n].vf = i;
  break;
      }

      rbuf[3+32] = '\0';
      if (IS_CLIENT_RPHIWIN(n))
        snprintf(wbuf.data(), wbuf.size(),"[%2i] %s \n", i, rbuf.data()+3);
      else
        snprintf(wbuf.data(), wbuf.size(),"%1i) [%2i] %s \n", i-s+1, i, rbuf.data()+3);
      check_write(n, wbuf.data());

  }
  efclose(fp);
  return (i ? 1 : 0);

}

int    item_number_on_floor(int n, char *nm, int tp)
/* 識別子 n のキャラクタがいる場所で、                                */
/* 1) アイテムが存在しない場合、マップ上からアイテムの存在表示を消し、*/
/*    返却値として 0 を返す。(安全性のためか？)                       */
/* 3) nm != NULL の場合、                                             */
/*    nm とアイテム名が一致するか、tp とアイテムタイプが一致する      */
/*    最初のアイテムに対して、アイテム名を nm にコピーし、            */
/*    そのアイテムが何番目のアイテムかを返却値として返す。            */
/*    tp == 0 の時は、一番上のアイテムが選択される。                  */
/*    一致するアイテムが無かった場合は、返却値として 0 が返される。   */
/*    *nm = '\0' とすれば tp だけで、tp = -1 とすれば nm でだけで     */
/*    検索されることに注意。                                          */
{
  ItemOData **ip;
  int    i;
  std::array<char, 32> itemname;

  if((map[cd[n].y][cd[n].x].tip & TMASK) == T_STORE) {
    return 0;
  }

  if((ip = finditem(cd[n].x, cd[n].y, nullptr))==nullptr){
    map[cd[n].y][cd[n].x].tip &= ~T_ITEM;
    delitempos(cd[n].x,cd[n].y);
    return 0;
  }

  /* add for loading item in circle 20020807 */
  load_id_in_circle(cd[n].x,cd[n].y);

  for(i = 1; /* i はアイテム番号 */
      ip != nullptr;
      ip = finditem(cd[n].x, cd[n].y, ip),i++){

      item_name_on_floor(itemname.data(), *ip,itemname.size());
      if (tp == 0 || (nm != nullptr && !std::strcmp(nm, itemname.data())) || tp == ((*ip)->id.type & IT_MASK)) {
  strlcpy(nm, itemname.data(),1024);/* 注！ main()のrbufのサイズに依存 */
  return (i);
      }
    }

  return 0;
}

void   item_name_on_floor(char *itemname, ItemOData *itemodata, int len)
/* "get" するために必要な名前を ItemOData 型の変数から得る */
{
  if(is_gp(itemodata))
    /* "get アイテム名" で拾うことを考えると、空白は入れない方が良い？ */
    snprintf(itemname, len-1,"%d%s", itemodata->gp, cirunit);
  else
    strlcpy(itemname, itemodata->id.name, len);
}

ItemOData  *investigate_item_on_floor(int n, int itemtype)
/* 拾う前に拾うものをチェック。 */
/* itemtype で示されたアイテムタイプに始めてマッチするものを返す。*/
/* itemtype == 0 の場合は、一番上のアイテム。*/
/* 存在しない場合は、NULL を返す。*/
{
  ItemOData **ip;

  if((ip = finditem(cd[n].x, cd[n].y, nullptr))==nullptr){
    map[cd[n].y][cd[n].x].tip &= ~T_ITEM;
    delitempos(cd[n].x,cd[n].y);
    return nullptr;
  }   /* とりあえずセーフティーに… */

  if(itemtype==0) return (*ip);

  for(;ip!=nullptr; ip = finditem(cd[n].x,cd[n].y,ip))
    if(itemtype == ((*ip)->id.type & IT_MASK))
      return (*ip);
  return nullptr;
}

int    load_item(ItemData *id, char *p)
/* 文字列から item data を読み取る。*/
/* 返却値 は 0(失敗),1(成功),2(成功、読み取りデータはワイルドカードを含む) */
{
  int    i, j, f;
  std::array<char, 32> name;
  std::array<char, 256> buf;
  std::array<char, 256> buf2;

  id->extension = nullptr;
  SKIP_SPACE(p);    /* 名前読み込み */
  f = 1;
  for (i = 0; i < 31 && (!fisspace(*p) || !fisspace(*(p + 1))); i++) {
    if (*p == '\0')
      return (0);
    name[i] = *(p++);
  }
  for (; i <= 31; name[i++] = '\0');
  if (name[0] == '\0')
    return (0);
  name[0] = ftoupper(name[0]);
  id->name = findstr(name.data());
  id->type = 0;
  SKIP_SPACE(p);
  if (*p == '-') {
    id->type |= 0x80000;
    f = 2;
  } else if (*p >= '0' && *p <= '9') {
    id->type |= ((chartoint(*p) - '0') << 12);
  } else if (*p >= 'a' && *p <= 'z') {
    id->type |= (((chartoint(*p) - 'a') + 10) << 12);
  } else {
    return (0);
  }
  p++;
  if (*p == '-') {
    id->type |= 0x40000;
    f = 2;
  } else if (*p >= '0' && *p <= '9') {
    id->type |= ((chartoint(*p) - '0') << 8);
  } else if (*p >= 'a' && *p <= 'z') {
    id->type |= (((chartoint(*p) - 'a') + 10) << 8);
  } else {
    return (0);
  }
  p++;
  if (*p == '-') {
    id->type |= 0x20000;
    f = 2;
  } else if (*p >= '0' && *p <= '9') {
    id->type |= ((chartoint(*p) - '0') << 4);
  } else if (*p >= 'a' && *p <= 'z') {
    id->type |= (((chartoint(*p) - 'a') + 10) << 4);
  } else {
    return (0);
  }
  p++;
  if (*p == '-') {
    id->type |= 0x10000;
    f = 2;
  } else if (*p >= '0' && *p <= '9') {
    id->type |= (chartoint(*p) - '0');
  } else if (*p >= 'a' && *p <= 'z') {
    id->type |= (chartoint(*p) - 'a' + 10);
  } else {
    return (0);
  }
  p++;
  SKIP_SPACE(p);
  if (p[0] == '-' && p[1] == '-') {
    id->wp = 0x100;
    f = 2;
    p += 2;
  } else {
    if ((id->wp = decimal_load(2, &p)) < 0)
      return (0);
    if (*p=='^'){
      id->wp |= 0x100;
      f = 2;
      p++;
    } else if (*p=='v'){
      id->wp |= 0x200;
      f = 2;
      p++;
    }
  }
  SKIP_SPACE(p);
  if (*p == '-') {
    id->fn = -1;
    f = 2;
    p++;
  } else {
    if ((id->fn = fn_load(*(p++))) < 0)
      return (0);
  }
  for (i = 0; i < 4; i++) {
    SKIP_SPACE(p);
    if (p[0] == '-' && p[1] == '-') {
      id->op[i] = 0x100;
      f = 2;
      p += 2;
    } else {
      if ((id->op[i] = decimal_load(2, &p)) < 0)
  return (0);
      if (*p=='^'){
  id->op[i] |= 0x100;
  f = 2;
  p++;
      } else if (*p=='v'){
  id->op[i] |= 0x200;
  f = 2;
  p++;
      }
    }
  }
  SKIP_SPACE(p);
  if(*p=='-') {
    id->active_time = ULONG_MAX;
    p++;
  }
  else id->active_time = 0;

  /* added for extension 20000116 */
  SKIP_SPACE(p);
  if (*p != '\0' && *p != '\n') {
    loadstr(buf.data(),&p,'\n',256);
    /* add for object resource 20011129 bug fix 20020511 */
    if ((id->type & IT_MASK) != IT_RESOURCE) {
        set_property(buf.data(),buf2.data(),buf2.size(),DEII_RSCANY,nullptr);
        p = buf2.data();
    } else
        p = buf.data();
    id->extension = findstr(p);
  } else
    id->extension = nullptr;

  if ((id->type & IT_MASK) == IT_CHARGE) {
    i = id->UE & 0x0ff;
    j = id->UE & 0xf00;
    if ((i == 38) || (i == 39))
      i -= 24;
    else if ((i >= 33) && (i <= 37))
      i += 28;
    else if ((i >= 14) && (i <= 32))
      i += 2;
    else if ((i >= 61) && (i <= 65))
      i -= 26;
    id->UE = i | j;
  }

  return (f);
}

void   save_item(ItemData * id, char *p, int len)
/* ワイルドカードを含まないものに限る */
{
  int    rsn;

  rsn = id->UE;

  if ((id->type & IT_MASK) == IT_CHARGE) {
    if ((rsn == 14) || (rsn == 15))
      rsn += 24;
    else if ((rsn >= 16) && (rsn <= 34))
      rsn -= 2;
    else if ((rsn >= 35) && (rsn <= 39))
      rsn += 26;
    else if ((rsn >= 61) && (rsn <= 65))
      rsn -= 28;
  }
  if (id->extension == nullptr)
    snprintf(p, len-1," %-31s %04x %02i %c %02i %02i %02i %02i\n"
    ,id->name, id->type, id->wp, kFnc[id->fn]
    ,id->op[0], id->op[1], rsn, id->op[3]);
  else
    snprintf(p, len-1," %-31s %04x %02i %c %02i %02i %02i %02i %s\n"
    ,id->name, id->type, id->wp, kFnc[id->fn]
    ,id->op[0], id->op[1], rsn, id->op[3], id->extension);

  return;
}

int    match_item(ItemData id1, ItemData id2)
{
  int    j;

  if (id1.name == nullptr || id2.name == nullptr) return 0;
  if (!wildcmp(id1.name, id2.name))
    return 0;
  j = 0;
  if (!(id1.type & 0x80000))
    j |= 0xf000;
  if (!(id1.type & 0x40000))
    j |= 0x0f00;
  if (!(id1.type & 0x20000))
    j |= 0x00f0;
  if (!(id1.type & 0x10000))
    j |= 0x000f;
  if ((id1.type ^ id2.type) & j)
    return 0;
  if(id1.wp & 0x100){
    if(!((id1.wp & 0xff) <= id2.wp))
      return 0;
  }else if(id1.wp & 0x200){
    if(!((id1.wp & 0xff) >= id2.wp))
      return 0;
  }else if(id1.wp != id2.wp)
    return 0;
  if (id1.fn != -1 && id1.fn != id2.fn)
    return 0;
  for (j = 0; j < 4; j++)
    if(id1.op[j] & 0x100){
      if(!((id1.op[j] & 0xff) <= id2.op[j]))
  return 0;
    }else if(id1.op[j] & 0x200){
      if(!((id1.op[j] & 0xff) >= id2.op[j]))
  return 0;
    }else if(id1.op[j] != id2.op[j])
      return 0;
  /* add for extend data check 20010614 */
  if (!match_property(id1.extension,id2.extension))
      return 0;
  return 1;
}

int   load_spell(MagicData * m, char *p)
/* ワイルドカード使用可能に変更 changed by Qajin 20000115 */
{
  int    i, rsn, f;
  std::array<char, 32> name;
  std::array<char, 256> buf;

  m->extension = nullptr;
  f = 1;
  SKIP_SPACE(p);
  for (i = 0; i < 31 && (!fisspace(*p) || !fisspace(*(p + 1))); i++) {
/*    if (*p == '*' || *p == '?')
      f = 2; */
    if (*p == '\0')
      return (0);
    name[i] = *(p++);
  }
  for (; i <= 31; name[i++] = 0);
  m->spell = findstr(name.data());
  SKIP_SPACE(p);
  if(*p == '-' && *(p+1) == '-') {
    rsn = -1;
    f = 2;
    p += 2;
  } else {
    rsn = decimal_load(2, &p);
    if (rsn <= 0)
      return (0);
  }
  SKIP_SPACE(p);
  if(*p == '-') {
    m->fn = -1;
    f = 2;
  } else
    m->fn = fn_load(*p);
  p++;

  if ((rsn == 38) || (rsn == 39))
    rsn -= 24;
  else if ((rsn >= 33) && (rsn <= 37))
    rsn += 28;
  else if ((rsn >= 14) && (rsn <= 32))
    rsn += 2;
  else if ((rsn >= 61) && (rsn <= 65))
    rsn -= 26;

  m->effect = rsn;

  SKIP_SPACE(p);
  if(*p=='*') {
    m->is_inherence = 1;
    p++;
  }
  else m->is_inherence = 0;

  /* added for extension 20000116 */
  SKIP_SPACE(p);
  if (*p != '\0' && *p != '\n') {
    loadstr(buf.data(),&p,'\n',255);
    m->extension = findstr(buf.data());
  }

  return(f);
}

void   save_spell(MagicData * m, char *p, int len)
{
  int    rsn;

  rsn = m->effect;

  if ((rsn == 14) || (rsn == 15))
    rsn += 24;
  else if ((rsn >= 16) && (rsn <= 34))
    rsn -= 2;
  else if ((rsn >= 35) && (rsn <= 39))
    rsn += 26;
  else if ((rsn >= 61) && (rsn <= 65))
    rsn -= 28;

  if(m->is_inherence == 1)
    snprintf(p, len-1, " %-31s %02i %c *", m->spell, rsn, kFnc[m->fn]);
  else
    snprintf(p, len-1, " %-31s %02i %c", m->spell, rsn, kFnc[m->fn]);

  /* add for extension 20000116 */
  if(m->extension != nullptr) {
    strlcat(p," ",len);
    strlcat(p,m->extension,len);
  }

  strlcat(p,"\n",len);

  return;
}

int match_spell(MagicData mg1, MagicData mg2)
{
  if (mg1.spell == nullptr || mg2.spell == nullptr) return 0;
  if (!wildcmp(mg1.spell, mg2.spell))
    return 0;
  if (mg1.effect != -1 && mg1.effect != mg2.effect)
      return 0;
  if (mg1.fn != -1 && mg1.fn != mg2.fn)
    return 0;
  /* add for extend data check 20010614 */
  if (!match_property(mg1.extension,mg2.extension))
      return 0;
  return 1;
}

void   load_talk(TalkData * t, char *p)
{
  int    i;
  std::array<char, 64> talk;

  SKIP_SPACE(p);
  t->type = decimal_load(2, &p);
  SKIP_SPACE(p);
  for (i = 0; i < 63 && *p != '\n'; talk[i++] = *(p++));
  talk[i] = '\0';
  t->string = findstr(talk.data());
  return;
}

void   save_talk(TalkData * t, char *p, int len)
{
  snprintf(p, len-1," %02i %s\n", t->type, t->string);
  return;
}

void   load_app(AppData * t, char *p)
{
  int    i;
  std::array<char, 64> apname;

  SKIP_SPACE(p);
  t->type = decimal_load(2, &p);
  SKIP_SPACE(p);
  for (i = 0; i < 63 && *p != '\n'; apname[i++] = *(p++));
  apname[i] = '\0';
  t->string = findstr(apname.data());
  return;
}

void   bulletin_board(int n, char *fnm)
{
  FILE  *fp;
  std::array<char, 256> buf;

  check_write(n, "\n");
  if ((fp = efopen(fnm, "rb")) == nullptr)
    return;
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    check_write(n, buf.data());
  }
  efclose(fp);
  check_write(n, "\n");
  return;
}

int    decimal_load_arbitrarily(char **p)
{
  int   n;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  for(n = 0;
      n <= 10 && '0'<=*(*p+n) && *(*p+n)<='9';
      n++);
  if(n == 11) {
    if (lcnt > 0) {
        snprintf(buf.data(), buf.size(),"%s at line %d: too long parameter.(decimal_load_arbitrarily)",
    pfname, lcnt);
        log_print(buf.data());
  n = 10; /* for long */
    } else {
#ifdef DEBUG
      log_print("Warning: Something wrong while execution.\n");
#endif
      return(-1);
    }
  }
  if(n == 0) {
    if (lcnt > 0) {
      snprintf(buf.data(), buf.size(),"%s at line %d: no parameter.(decimal_load_arbitrarily)",
                pfname, lcnt);
        log_print(buf.data());
  return(-1);
    } else {
#ifdef DEBUG
      log_print("Warning: Something wrong while execution.\n");
#endif
      return(-1);
    }
  }
  return(decimal_load(n, p));
}

int    decimal_load(int n, char **p)
{
  int    i, j;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  for (i = 1; n > 1; i *= 10, n--);
  for (j = 0; i >= 1; i /= 10, (*p)++) {
    if ((**p) >= '0' && (**p) <= '9') {
      j += ((**p) - '0') * i;
    } else {
      if (lcnt > 0) {
  snprintf(buf.data(), buf.size(),"%s at line %d: Invalid parameter \'%c\' appeared.(decimal load)\n", pfname, lcnt, **p);
  log_print(buf.data());
      } else {
/*        log_print("Warning: Something wrong while execution.\n");*/
  return (-1);
      }
    }
  }
  return (j);
}

int    decimal_load_p(int n, char **p)
{
  int    i, j;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  for (i = 1; n > 1; i *= 10, n--);
  for (j = 0; i >= 1; i /= 10, (*p)++) {
    if ((**p) >= '0' && (**p) <= '9') {
      j += ((**p) - '0') * i;
    } else {
	if ( (**p) =! '-' ) { /* Wildcard error 対策 added by NEO 20060707 */
	      if (lcnt > 0) {
	  	snprintf(buf.data(), buf.size(),"%s at line %d: Invalid parameter \'%c\' appeared.(decimal load)\n", pfname, lcnt, **p);
	  	log_print(buf.data());
	      } else {
/*        	log_print("Warning: Something wrong while execution.\n");*/
	  	return (-1);
		}
	}
    }
  }
  return (j);
}

int    hexadecimal_load_arbitrarily(char **p)
{
  int   n;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  for(n = 0;
      n <= 8 && (('0'<=*(*p+n) && *(*p+n)<='9') || ('a'<=*(*p+n) && *(*p+n)<='f'));
      n++);
  if(n == 9) {
    if (lcnt > 0) {
        snprintf(buf.data(), buf.size(),"%s at line %d: too long parameter.(hexadecimal_load_arbitrarily)",
    pfname, lcnt);
        log_print(buf.data());
  n = 8;  /* でも、とりあえうず、８桁まで読み込もうか */
    } else {
#ifdef DEBUG
      log_print("Warning: Something wrong while execution.\n");
#endif
      return(-1);
    }
  }
  if(n == 0) {
    if (lcnt > 0) {
      snprintf(buf.data(), buf.size(),"%s at line %d: no parameter.(hexadecimal_load_arbitrarily)",
                pfname, lcnt);
        log_print(buf.data());
  return(-1);
    } else {
#ifdef DEBUG
      log_print("Warning: Something wrong while execution.\n");
#endif
      return(-1);
    }
  }
  return(hexadecimal_load(n, p));
}

int    hexadecimal_load(int n, char **p)
{
  int    i, j;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  for (i = 1; n > 1; i *= 0x10, n--);
  for (j = 0; i >= 1; i /= 0x10, (*p)++) {
    if ((**p) >= '0' && (**p) <= '9')
      j += ((**p) - '0') * i;
    else if ((**p) >= 'a' && (**p) <= 'f')
      j += ((**p) - 'a' + 0xa) * i;
        else if ((**p) >= 'A' && (**p) <= 'F')
            j += ((**p) - 'A' + 0xa) * i;
    else {
      if (lcnt > 0) {
  snprintf(buf.data(), buf.size(),"%s at line %d: Invalid parameter \'%c\' appeared.(hexadecimal load)\n", pfname, lcnt, **p);
  log_print(buf.data());
      } else {
/*        log_print("Warning: Something wrong while execution.\n");*/
  return (-1);
      }
    }
  }
  return (j);
}

/* add for personal flag 20000804 */
int    flagno_load(char **p)
{
  int j;
  std::array<char, 256> buf;

  SKIP_SPACE(*p);
  j = 0;
  switch (**p) {
   case 'z':
   case 'Z':
   case 'y':
   case 'Y':
   case 'x':
   case 'X':
    if ((j = varno_load(p))<0)
        return (-1);
    return (j + GFLAG_MAX + PFLAG_MAX);
     break;
   case 's': /* for saved flag 20010622 */
   case 'S':
    j = SFLAG_MAX;
   case 'p':
   case 'P':
    j += GFLAG_MAX;
    (*p)++;
    if ((**p) >= '0' && (**p) <= '9')
      j += ((**p) - '0');
    else if ((**p) >= 'a' && (**p) <= 'f')
      j += ((**p) - 'a' + 0xa);
    else {
      if (lcnt > 0) {
        snprintf(buf.data(), buf.size(),"%s at line %d: Invalid parameter \'%c\' appeared.(flagno load)\n", pfname, lcnt, **p);
        log_print(buf.data());
      } else         /* bug fix 20001126 */
        return (-1);
    }
    (*p)++; /* bug fix 20001126 */
    return (j);
  }
  return (hexadecimal_load_arbitrarily(p));
}

int    varno_load(char **p)
{
  int j;

  SKIP_SPACE(*p);
  j = 0;
  switch (**p) {
   case 'z':
   case 'Z':
    j = PVAR_MAX;
   case 'y':
   case 'Y':
    j += GVAR_MAX;
   case 'x':
   case 'X':
    (*p)++;
    return (j+hexadecimal_load_arbitrarily(p));
  }
  return (-1);
}

int var_value(int n,int i)
{
    if (i < GVAR_MAX)
        return (generic_variable[i]);
    else if (i < GVAR_MAX + PVAR_MAX)
        return (cd[n].pvar[i-GVAR_MAX]);
    else if (i < GVAR_MAX + PVAR_MAX + SVAR_MAX)
        return (cd[n].svar[i-GVAR_MAX-PVAR_MAX]);
    return (0);
}

/* add for Lua dm.set_var (PHIDM NEO extension): var_value と同じ
   アドレッシングでスイッチ変数へ値を書き込む */
void var_set_value(int n,int i,int v)
{
    if (i < 0)
        return;
    if (i < GVAR_MAX)
        generic_variable[i] = v;
    else if (i < GVAR_MAX + PVAR_MAX)
        cd[n].pvar[i-GVAR_MAX] = v;
    else if (i < GVAR_MAX + PVAR_MAX + SVAR_MAX)
        cd[n].svar[i-GVAR_MAX-PVAR_MAX] = v;
}

void   loadstr(char *buf, char **p, int f, int le)
/* 文字列 *p から 最大 le-1 の長さの文字列を文字 f が出現するまで */
/* *buf に入れ、その次の文字まで p を進める。*/
{
  int    i;

  i = 0;
  while (**p != '\0' && **p != '\n') {
    if (i == le - 1) {    /* 文字数オーバ */
      (*p)++;
      break;
    }
    if (f == DSPACE) {    /* デリミタ */
      if (fisspace(**p) && fisspace(*(1 + *p)))
  break;
    } else if (f > 0xff) {
      if (((chartoint(**p) << 8) | chartoint(*(1 + *p))) == f) {
  (*p) += 2;
  break;
      }
    } else {
      if (chartoint(**p) == f) {
  (*p)++;
  break;
      }
    }
    *(buf++) = *((*p)++);
    i++;
  }
  *buf = '\0';
}

int    wildcmp(char *p1, char *p2)
/* ワイルドカードを考慮した文字列の比較 */
/* p1 の文字列中 に ? と * が使える。*/
/* ? は一文字、* は複数文字のワイルドカード */
/* 返却値は一致すると -1, しないと 0 */
/* 20020524 拡張
   # = 数字(0～9)と一致
   \ = エスケープ(次の文字のパターンとしての意味を無くす)
   [...] = 中の文字のどれかに一致
           1文字目を ^ にすると中にない文字と一致
           - で 文字範囲を指定可
           ],-を指定する時は1文字目に(],-の順) */
{

  while (*p1 != '\0') {
    switch (*p1) {
      case '?':
        if (*p2 == '\0')
          return (0);
        p1++;
        p2++;
        break;
      case '*':
        p1++;
        if (*p1 == '\0')
          return (-1);
        while (*p2 != '\0') {
          if (wildcmp(p1, p2))
            return (-1);
          p2++;
        }
        return (0);
      case '#':
        if (*p2 < '0' || *p2 > '9')
          return (0);
        p1++;
        p2++;
        break;
      case '[':
        p1++;
        if (*p1 == '^') {
          p1++;
          if (*p1 == ']') {
            if (*p2 == ']')
              return (0);
            p1++;
          }
          if (*p1 == '-') {
            if (*p2 == '-')
              return (0);
            p1++;
          }
          do {
            if (*p1 == '\0')
              return (0);   /* 都合によりエラー出力はなし */
            if (*p1 == '-') {
              if (*(p1-1) < *p2 && *p2 <= *(p1+1))
                return (0);
              p1++;
            } else if (*p1 == *p2)
                return (0);
          } while (*(++p1) != ']');
          p1++;
          p2++;
          break;
        }
        switch (*p1) {
          case ']':
            if (*(p1++) == *p2)
              break;
          case '-':
            if (*(p1++) == *p2)
              break;
          default:
            while (1) {
              if (*p1 == ']' || *p1 == '\0')
                return (0);
              if (*p1 == '-' && *(p1+1) != ']') {
                if (*(p1-1) < *p2 && *p2 <= *(p1+1))
                  break;
              } else if (*p1 == *p2)
                  break;
              p1++;
            }
            break;
        }
        while (*p1 != '\0' && *(p1++) != ']');
        p2++;
        break;
      case '\\':
        p1++;
      default:
        if (*(p1++) != *(p2++))
          return (0);
        /* for sjis second code 20030704 */
        if (((unsigned char)*(p1-1) >= 0x81 && (unsigned char)*(p1-1) <= 0x9f) ||
             ((unsigned char)*(p1-1) >= 0xe0 && (unsigned char)*(p1-1) <= 0xfc)) {
            if (*(p1++) != *(p2++))
              return (0);
        }
    }
  }
  if (*p2 == '\0')
    return (-1);
  return (0);
}

int    fn_load(char p)
{
  switch (p) {
  case 's':
    return (FN_SPIRIT);
    break;
  case 'w':
    return (FN_WATER);
    break;
  case 'f':
    return (FN_FIRE);
    break;
  case 'e':
    return (FN_EARTH);
    break;
  case 'l':
    return (FN_LIGHT);
    break;
  case 'a':
    return (FN_AIR);
    break;
  case 'v':
    return (PER_DICE < 50 ? frand(6) + 1 : FN_NONE);
    break;
  /* add new fn 20000324 */
  case 'm':
    return (FN_MIRROR);
    break;
  case 'p':
    return (FN_PRISM);
    break;
  case 'r':
    return (FN_RAINBOW);
    break;
  default:
    return (FN_NONE);
    break;
  }
  return (FN_NONE);
}

void load_crt()
{
  FILE *fp;
  std::array<char, 256> buf;
  char   *q;
  CrtData **p;
  ItemData product,fail_product,staff1,staff2;
  int is_exist_product, is_exist_fail_product;
  int level;

  if ((fp = efopen("phi.crt", "rb")) == nullptr) {
    fprintf(stderr, "Warning: Can not find create item file. [phi.crt] \n");
    first_create_data = nullptr;
    return;
  }
  p=&first_create_data;
  while(1){
    is_exist_product = is_exist_fail_product = 0;
    if(efgets(buf.data(), 255, fp)==nullptr) break;
    if(buf[0] !=':' || (buf[1] !='P' && buf[1]!='L' && buf[1]!='F')) continue;
    level = -1; /* change for lv 0 safe create 20020529 */
    if(buf[1]=='L'){
      q=&(buf[2]);
      level=decimal_load(4,&q);
      if(efgets(buf.data(), 255, fp)==nullptr) break;
      if(buf[0] !=':' || (buf[1] !='P' && buf[1]!='F')) continue;
    }
    if(buf[1]=='P'){
      if(load_item(&product, &(buf[2]))!=1){
  snprintf(buf.data(),buf.size(),"phi.crt:line %d:Something wrong.",lcnt);
  log_print(buf.data());
  continue;
      }
      is_exist_product = 1;
      if(efgets(buf.data(), 255, fp)==nullptr) break;
      if(buf[0] !=':' || (buf[1] !='F' && buf[1] !='S')) continue;
    }
    if(buf[1]=='F'){
      if(load_item(&fail_product, &(buf[2]))!=1){
        snprintf(buf.data(),buf.size(),"phi.crt:line %d:Something wrong.",lcnt);
        log_print(buf.data());
        continue;
      }
      is_exist_fail_product = 1;
      if(efgets(buf.data(), 255, fp)==nullptr) break;
      if(buf[0] !=':' || buf[1] !='S') continue;
    }

    if(load_item(&staff1,  &(buf[2]))==0){
      snprintf(buf.data(),buf.size(),"phi.crt:line %d:Something wrong.",lcnt);
      log_print(buf.data());
      continue;
    }
    if(efgets(buf.data(), 255, fp)==nullptr) break;
    if(buf[0] !=':' || buf[1] !='S') continue;
    if(load_item(&staff2,  &(buf[2]))==0){
      snprintf(buf.data(),buf.size(),"phi.crt:line %d:Something wrong.",lcnt);
      log_print(buf.data());
      continue;
    }
    if((*p=(CrtData *)malloc(sizeof (CrtData)))==nullptr
       || ((*p)->staff1= (ItemData *)malloc(sizeof (ItemData)))==nullptr
       || ((*p)->staff2= (ItemData *)malloc(sizeof (ItemData)))==nullptr){
      log_print("Insufficient Memory\n");
      exit(0);
    }
    if(is_exist_product &&
       ((*p)->product=(ItemData *)malloc(sizeof (ItemData)))==nullptr){
      log_print("Insufficient Memory\n");
      exit(0);
    }
    if(is_exist_fail_product &&
       ((*p)->fail_product=(ItemData *)malloc(sizeof (ItemData)))==nullptr){
      log_print("Insufficient Memory\n");
      exit(0);
    }
    (*p)->level=level;
    if(is_exist_product)
      std::memcpy((*p)->product,&product,sizeof (ItemData));
    else (*p)->product = nullptr;
    if(is_exist_fail_product)
      std::memcpy((*p)->fail_product,&fail_product,sizeof (ItemData));
    else (*p)->fail_product = nullptr;
    std::memcpy((*p)->staff1, &staff1, sizeof (ItemData));
    std::memcpy((*p)->staff2, &staff2, sizeof (ItemData));
#ifdef DEBUG
    log_print("create set:");
    snprintf(debug_buf,sizeof(debug_buf),"level=%d",(*p)->level);
    log_print(debug_buf);
    if((*p)->product!=nullptr){
      snprintf(debug_buf,sizeof(debug_buf),"product=%s",(*p)->product->name);
      log_print(debug_buf);
    }
    if((*p)->fail_product!=nullptr){
      snprintf(debug_buf,sizeof(debug_buf),"fail_product=%s",(*p)->fail_product->name);
      log_print(debug_buf);
    }
    snprintf(debug_buf,sizeof(debug_buf),"staff1=%s",(*p)->staff1->name);
    log_print(debug_buf);
    snprintf(debug_buf,sizeof(debug_buf),"staff2=%s",(*p)->staff2->name);
    log_print(debug_buf);
#endif
    p=&((*p)->next);
  }
  *p = nullptr;
  efclose(fp);
}

void clear_crt()
{
  CrtData *p,*q;

  for(p=first_create_data; p!=nullptr;){
    if(p->product!=nullptr)
      free(p->product);
    if(p->fail_product!=nullptr)
      free(p->fail_product);
    free(p->staff1);
    free(p->staff2);
    q=p->next;
    free(p);
    p=q;
  }
}

int    get_server_port()
{
  FILE *fp;
  std::array<char, 257> buf;
  char *p;
  int i;

  sport = SERV_TCP_PORT;
  if((fp = efopen("phi.td", "rb")) == nullptr) {
    fprintf(stderr, " Can not find transfer data file. [phi.td] ");
    exit(1);
  }
  while (1) {
    if (efgets(buf.data(), 255, fp) == nullptr) break;
    if (*buf.data() != ':') continue;
    if(*(buf.data() + 1)!='O') continue;
    p = buf.data() + 2;
    for (;;) {
      SKIP_SPACE(p);
      if (*p == '\0' || *p == '\n') break;
      if(*(p++)!='O') continue;
      SKIP_SPACE(p);
      i = 0;
      while (*p >= '0' && *p <= '9') {
  i *= 10;
  i += (int) (*p++ - '0');
      }
      if (i > 0) sport = i;
    }
  }
  efclose(fp);
  return(sport);
}

int    server_version_check(int n, int sv, char *errmsg, int len)
{
  int i;
  int sv_spell_jewel;

  sv_spell_jewel = std::atoi(VERS_SPELL_JEWEL);
  for(i=0; i<MAX_ITEMS; i++)
    if((cd[n].id[i].type & IT_MASK) == IT_SPELL_JEWEL && sv < sv_spell_jewel){
      dmprintf(n,0,errmsg, len,DMM_TRANS_NO_SPLJEWEL, cd[n].id[i].name);
      /* ST_TRANSFER の奴には言っても聞こえない */
      return 0;
    }
  return 1;
}

void    load_ex()
/* change for import & ... 20001128 */
{
  FILE *fp;
  std::array<char, 256> buf;
  char   *q;
  ExportItemData **p;
  ItemData item;
  ExGateData gate;
  MagicData spell;

  if ((fp = efopen("phi.ex", "rb")) == nullptr) {
    fprintf(stderr, "Warning: Can not find export item file. [phi.ex] \n");
    first_export_data = nullptr;
    return;
  }

  p=&first_export_data;
  while(1){
    if(efgets(buf.data(), 255, fp)==nullptr) break;
    if(buf[0]=='.'){
      if((*p=(ExportItemData *)malloc(sizeof (ExportItemData)))==nullptr){
  log_print("Insufficient Memory\n");
  exit(0);
      }
      (*p)->type = '.';
      (*p)->data.gd = nullptr;
      p=&((*p)->next);
      continue;
    }
    if(buf[0]!=':') continue;
    q=&(buf[2]);
    switch (buf[1]) {
     case 'G':
     case 'g':
      SKIP_SPACE(q);
      if(!std::strncmp(q, "---", 3)){
  gate.x=-1;
  q+=3;
      } else gate.x=decimal_load(3, &q);
      SKIP_SPACE(q);
      if(!std::strncmp(q, "---", 3)){
  gate.y=-1;
  q+=3;
      } else gate.y=decimal_load(3, &q);
      SKIP_SPACE(q);
      loadstr(gate.name, &q, DSPACE, 32);
      SKIP_SPACE(q);
      loadstr(gate.ip, &q, ' ', 16);

      if((*p=(ExportItemData *)malloc(sizeof (ExportItemData)))==nullptr
   || ((*p)->data.gd=(ExGateData *)malloc(sizeof (ExGateData)))==nullptr){
  log_print("Insufficient Memory\n");
  exit(0);
      }
      (*p)->type = buf[1];
      std::memcpy((*p)->data.gd, &gate, sizeof(ExGateData));
      p=&((*p)->next);
      continue;

      break;
     case 'I':
     case 'i':
      if(load_item(&item, q)==0){
  snprintf(buf.data(), buf.size(),"phi.ex:line %d:Something wrong.",lcnt);
  log_print(buf.data());
  continue;
      }

      if((*p=(ExportItemData *)malloc(sizeof (ExportItemData)))==nullptr
   || ((*p)->data.id=(ItemData *)malloc(sizeof (ItemData)))==nullptr){
  log_print("Insufficient Memory\n");
  exit(0);
      }
      (*p)->type = buf[1];
      std::memcpy((*p)->data.id, &item, sizeof(ItemData));
      p=&((*p)->next);
      continue;
      break;
     case 'M':
     case 'm':
      if(load_spell(&spell, q)==0){
  snprintf(buf.data(), buf.size(),"phi.ex:line %d:Something wrong.",lcnt);
  log_print(buf.data());
  continue;
      }

      if((*p=(ExportItemData *)malloc(sizeof (ExportItemData)))==nullptr
   || ((*p)->data.md=(MagicData *)malloc(sizeof (MagicData)))==nullptr){
  log_print("Insufficient Memory\n");
  exit(0);
      }
      (*p)->type = buf[1];
      std::memcpy((*p)->data.md, &spell, sizeof(MagicData));
      p=&((*p)->next);
      continue;
    }
  }
  *p = nullptr;
  efclose(fp);
}

void   free_ex()
{
  ExportItemData *p, *q;

  for(p=first_export_data; p!=nullptr;){
    if(p->type=='G' || p->type=='g')
      free(p->data.gd);
    else if(p->type=='I' || p->type=='i')
      free(p->data.id);
    else if(p->type=='M' || p->type=='m')
      free(p->data.md);
    q=p->next;
    free(p);
    p=q;
  }
}

int    export_item_check(int n, int x, int y, char *name, char *ip, char ex)
/* change for import & etc... 20001128 */
{
  ExportItemData *p;
  int i;
  std::array<char, 100> buf;
  int iok[MAX_ITEMS]; /* add for permissive items 20001204 */
  int mok[MAX_MAGICS];

  if(cd[n].exp == 99999999)
    return 1;
  for(p=first_export_data; p!=nullptr; p=p->next){
    if(p->type==ex
       && (p->data.gd->x==-1 || p->data.gd->x == x)
       && (p->data.gd->y==-1 || p->data.gd->y == y)
       && wildcmp(p->data.gd->name, name)
       && wildcmp(p->data.gd->ip, ip)){
      for(i=0;i<MAX_ITEMS; i++){
        iok[i] = 0;
      }
      for(i=0;i<MAX_MAGICS; i++){
        mok[i] = 0;
      }
      for(;p!=nullptr && p->type!='.'; p=p->next){
  if(p->type=='i')
   for(i=0; i<MAX_ITEMS; i++){
    if(cd[n].id[i].type == DT_NONE) continue;
    if(match_item(*(p->data.id), cd[n].id[i]))
      iok[i] = 1;
   }
  else if(p->type=='I')
   for(i=0; i<MAX_ITEMS; i++){
    if(cd[n].id[i].type == DT_NONE) continue;
    if(match_item(*(p->data.id), cd[n].id[i]) && !iok[i]){
      dmprintf(n,0,buf.data(),buf.size(),DMM_TRANS_NO_BADITEM, cd[n].id[i].name);
      check_write(n, buf.data());
      return 0;
    }
      }
  else if(p->type=='m')
   for(i=0; i<MAX_MAGICS; i++){
    if(cd[n].md[i].effect == EF_NONE) continue;
    if(match_spell(*(p->data.md), cd[n].md[i]))
      mok[i] = 1;
   }
  else if(p->type=='M')
   for(i=0; i<MAX_MAGICS; i++){
    if(cd[n].md[i].effect == EF_NONE) continue;
    if(match_spell(*(p->data.md), cd[n].md[i]) && !mok[i]){
      dmprintf(n,0,buf.data(),buf.size(),DMM_TRANS_NO_BADMAGIC, cd[n].md[i].spell);
      check_write(n, buf.data());
      return 0;
    }
  }
      }
      if(p==nullptr) break;
    }
  }
  return 1;
}

#if MAX_OBJECTS > 0

/* オブジェクトデータ作成 */
/* 成功 = ObjNo, 失敗 = -1 */
/*             空き無 = -2 */
/* add 20000105 */
int str_to_obj(char **p)
{
  std::array<char, 32> name;
  int n;

    SKIP_SPACE(*p);
    loadstr(name.data(),p,DSPACE,16);

    if (name[0] == '\0')
        return (-1);

    for(n = 0; n < MAX_OBJECTS; n++)
      if (od[n].gname == nullptr)
        break;

    if (n == MAX_OBJECTS)
      return (-2);

    SKIP_SPACE(*p);

    od[n].dir = 0x00;
      /* 向き */
      switch (**p) {
        case 'N':
        case 'n':
          od[n].dir = 0;
           (*p)++;
           break;
        case 'E':
        case 'e':
          od[n].dir = 1;
           (*p)++;
           break;
        case 'S':
        case 's':
          od[n].dir = 2;
           (*p)++;
           break;
        case 'W':
        case 'w':
          od[n].dir = 3;
           (*p)++;
           break;
        case 'F':
        case 'f':
          od[n].dir = OD_CONSTDIR | 2;
           (*p)++;
           break;
        case 'L':
        case 'l':
          od[n].dir = OD_CONSTDIR | 3;
           (*p)++;
           break;
        case 'B':
        case 'b':
          od[n].dir = OD_CONSTDIR | 0;
           (*p)++;
           break;
        case 'R':
        case 'r':
          od[n].dir = OD_CONSTDIR | 1;
           (*p)++;
           break;
        default:
           break;
      }

      od[n].gname = findstr(name.data());

      SKIP_SPACE(*p);
      /* 視線トレース */
      if (**p == 'T' || **p == 't') {
          od[n].dir |= OD_TRACE;
           (*p)++;
      }

      /* デブキャラ使用 */
      if (**p == 'D' || **p == 'd') {
          od[n].dir |= OD_DEBU;
           (*p)++;
      }

      SKIP_SPACE(*p);
      switch (**p) {
      /* 看板として表示 */
        case 'K':
        case 'k':
            od[n].dir |= OD_BOARD;
            (*p)++;
            break;
      /* 押せるオブジェ */
        case 'P':
        case 'p':
            od[n].dir |= OD_PUSH;
            (*p)++;
            break;
        default:
            break;
      }

      SKIP_SPACE(*p);
      /* 表示オブジェクト名 */
      if (**p == '\"') {
        (*p)++;
        if (**p != '\"') {
             loadstr(name.data(),p,'\"',32);
             od[n].name = findstr(name.data());
        } else {
            (*p)++;
            od[n].name = nullptr;
        }
      } else {
         od[n].name = nullptr;
      }

      SKIP_SPACE(*p);
      /* ID */
      if (**p != '\0' && **p != '\n') {
         loadstr(name.data(),p,'\n',16);
         od[n].id = findstr(name.data());
      } else {
         od[n].id = nullptr;
      }

      return (n);
}

void clear_obj()
{
  int i;

  std::memset(dobj,'\0',sizeof(dobj));
  std::memset(od,'\0',sizeof(od));
  for(i=0;i<MAX_OBJECTS;i++)
    od[i].x = od[i].y = -1;
}

/* ファイルからのオブジェクト読み込み */
void load_obj()
{
  FILE  *fp;
  int x,y,n;
  std::array<char, 256> buf;
  char *p;

  clear_obj();

  if ((fp = efopen("phi.od", "rb")) == nullptr) {
    fprintf(stderr, "Warning: Can not find object data file. [phi.od] \n");
    return;
  }

  /* オブジェクトインスタンス読み込み */
  for(;;) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (buf[0] != ':' || buf[1] != 'O')
      continue;

    p = buf.data() + 2;
    SKIP_SPACE(p);

    if (*p == '-') {
        x = -1;
        SKIP_TO_SPACE(p);
        SKIP_SPACE(p);
        y = -1;
        SKIP_TO_SPACE(p);
        SKIP_SPACE(p);
    } else {
        x = decimal_load(3,&p);
        y = decimal_load(3,&p);
        if(x < 0 || x > MAX_X - 1 || y < 0 || y > MAX_Y - 1)
            continue;
    }

    n = str_to_obj(&p);
    if (n >= 0) {
        od[n].x = x;
        od[n].y = y;
        if (x >= 0) {
            dobj[y][x] = n+1;
        }
    }

  }
  init_magic_objects();
  efclose(fp);

}

/* エラーを返すよう変更 20000126 */
int save_obj()
{
  FILE  *fp;
  int i;
  /* buf は CATSTR の宛先のため C 配列のまま */
  char   buf[256];
  std::array<char, 256> buf2;

  if ((fp = efopen("phi.od", "wb")) == nullptr)
    return(-1);

  for (i = 0; i < MAX_OBJECTS; i++) {
    if (od[i].gname != nullptr) {
      if (od[i].x < 0) {
          snprintf(buf,sizeof(buf),":O --- --- %-15s  %c",od[i].gname,
                      "NESWBRFL"[od[i].dir & (OD_DIRECTION | OD_CONSTDIR)]);
      } else {
          snprintf(buf,sizeof(buf),":O %03i %03i %-15s  %c",od[i].x,od[i].y,od[i].gname,
                      "NESWBRFL"[od[i].dir & (OD_DIRECTION | OD_CONSTDIR)]);
      }
      if (od[i].dir & OD_TRACE)
          CATSTR(buf,"T");
      if (od[i].dir & OD_DEBU)
          CATSTR(buf,"D");
      if (od[i].dir & OD_BOARD)
          CATSTR(buf,"K");
      if (od[i].dir & OD_PUSH)
          CATSTR(buf,"P");
      if (od[i].name != nullptr) {
          snprintf(buf2.data(),buf2.size()," \"%-*.31s\"", 0, od[i].name);
          CATSTR(buf,buf2.data());
      } else {
          CATSTR(buf," \"\"");
      }
      if (od[i].id != nullptr) {
          snprintf(buf2.data(),buf2.size()," %-*.15s", 0, od[i].id);
          CATSTR(buf,buf2.data());
      }
      CATSTR(buf,"\n");
      fputs(buf,fp);
    }
  }

  efclose(fp);

  return (0);
}

/* 指定IDのオブジェを探す */
int search_obj(int s,char *p)
{
    int i;

    for(i=s;i<MAX_OBJECTS;i++)
        if (od[i].id != nullptr && !std::strncmp(od[i].id,p,32))
            break;
    return (i<MAX_OBJECTS ? i : -1);
}

/* オブジェを移動させる   */
/* (-1,-1)は ID有は非表示 */
/*           ID無は消す   */
int move_obj(int n,int x,int y,int d)
{
    int i;

    if (n < 0)
        return (-1);
    if (od[n].x >= 0 && od[n].y >= 0) {
        dobj[od[n].y][od[n].x] = 0;
    }
    od[n].x = x;
    od[n].y = y;
    if (d >= 0 && !(od[n].dir & OD_CONSTDIR)) {
        od[n].dir &= ~OD_DIRECTION;
        od[n].dir |= (d & OD_DIRECTION);
    }
    if (x >= 0 && y >= 0) {
        i = dobj[y][x]-1;
        dobj[y][x] = n+1;
        return (i);
    } else if (od[n].id == nullptr) {
        od[n].gname = nullptr;
        od[n].name = nullptr;
    }
    return(-1);
}

void init_magic_objects()
{
    std::array<char, 256> buf;
    int n,i;

/* delete 20020715
    if (!is_see_magic_effects)
        return;
*/

    /* line */
    for(i = 0;i < 3; i++) {
        snprintf(buf.data(),buf.size(),"!MLine%1i",i);
        if ((MLine[i] = n = search_obj(0,buf.data())) >= 0) {
            od[n].x = -1;
            od[n].y = -1;
            od[n].dir |= OD_EFFECT;
        }
    }

    /* hit */
    if ((MHit = n = search_obj(0,"!MHit")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }

    /* explode */
    for(i = 0;i < 9; i++) {
        snprintf(buf.data(),buf.size(),"!MExplode%1i",i);
        if ((MExplode[i / 3][i % 3] = n = search_obj(0,buf.data())) >= 0) {
            od[n].x = -1;
            od[n].y = -1;
            od[n].dir |= OD_EFFECT;
        }
    }

    if ((MHeal = n = search_obj(0,"!MHeal")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }
    if ((MCoat = n = search_obj(0,"!MCoat")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }
    if ((MProtect = n = search_obj(0,"!MProtect")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }
    if ((MIllusion = n = search_obj(0,"!MIllusion")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }
    if ((MDown = n = search_obj(0,"!MDown")) >= 0) {
        od[n].x = -1;
        od[n].y = -1;
        od[n].dir |= OD_EFFECT;
    }
}

//----------------------------------------------------------------------------
// 防御側の鎧・アクセの欠け added by NEO 20120212 from DM by Cress
//----------------------------------------------------------------------------
void DegradeArmorAndAccesory( int as, int ds )
{
	std::array<char, 256> buf;

	if ( cd[ds].aeq != nullptr )
	{
		skillup( ds, SKL_DETECT_IT, IT_ARMOR, 0, 50, 1 );
		if ( PER_DICE < ( magic_anti_degrade ? 10 * ( 100 - OCN_PROTECT( ds ) ) / 100 : 10 ) )  /* プロテクトで欠け率減少 */
		{
			if ( degrade( cd[ds].aeq ) == 2 )
			{ /* 鎧破壊 */
				dmprintf( ds, as, buf.data(), buf.size(), DMM_BROKEN_ARMOR );
				write_companion( ds, as, buf.data(), 0 );
				cd[ds].aeq = nullptr;
				cd[ds].asep = 0;
				if ( ds >= MAX_NTTYS )  /* NPC ならば */
					select_armor( ds );  /* 次の鎧を選択する */
				item_gc( ds );
				set_fn( ds );
			}
		}
	}
	if ( cd[ds].oeq != nullptr )
	{
		skillup(ds,SKL_DETECT_IT,IT_ACCESSORY,0,50,1);
		if ( PER_DICE < ( magic_anti_degrade ? 10 * ( 100 - OCN_SHELL( ds ) ) / 100 : 10 ) )  /* シェルで欠け率減少 */
		{
			if ( degrade( cd[ds].oeq ) == 2 )
			{ /* アクセ破壊 */
				dmprintf( ds, as, buf.data(), buf.size(), DMM_BROKEN_ACCESORY );
				write_companion( ds, as, buf.data(), 0 );
				cd[ds].oeq = nullptr;
				cd[ds].at = AT_NONE;
				cd[ds].osep = 0;
				if ( ds >= MAX_NTTYS )  /* NPC ならば */
				{
					select_accessory( ds );  /* 次のアクセを選択する */
					select_armor( ds );  /* 次の鎧を選択する */
				}
				item_gc( ds );
				set_fn( ds );
			}
		}
	}
}

#endif
