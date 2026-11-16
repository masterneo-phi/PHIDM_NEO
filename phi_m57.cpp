/* for Phi m57     */
/* written by Qajin */

/* #m57 形式の地図 (7x7) と Eagle eye の広域地図 (#ex-eagleeye) の生成。
 *
 * set_view7() が視界 7x7 を組み立て、print_m57() が前回との差分を
 * 判定して #m57 M / #m57 O 行を送る。dd_m57() が 1 セルを
 * 2 バイト (地形文字 + 属性バイト) に変換する。eagleeye_m57() は
 * Eagle eye の広域地図 (#ex-eagleeye M 行) を送る。
 *
 * 視界の組み立て順序 (近いセルから) と 2 バイト形式はクライアント
 * との約束なので、書き換えではバイト単位で保存している。
 */

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include  "phi_dm.h"
#include  "phi_data.h"
#include  "phi_calc.h"
#include  "phi_obj.h"
#include  "phi_str.h"
#include  "phi_m57.h"

/* Single definitions of the globals declared extern in phi_m57.h.
   (C++ stage 0: the C build relied on -fcommon merging the
   tentative definitions from the header.) */
int chr_pos_psp_compati;
int turn_permission;
int mapsize_only55;


extern int    px[];
extern int    py[];

extern int reflesh_last_map[MAX_NTTYS];

#if MAX_OBJECTS > 0
extern int reflesh_objects;
#endif

#ifdef DEBUG
char debug_buf[256];
#endif

/* phi_m57.h の UNSEE7 と同一。見えないセルを空にして
 * T_UNKNOWN を立てる。 */
#define UNSEE7(y,x)	{memset(&(view7[(y)][(x)]),'\0',sizeof(MapData)); view7[(y)][(x)].tip = T_UNKNOWN;}

/* 視界の大きさ。set_view7() が 49 セルを近い順に埋める。 */
constexpr int kViewSize = 7;
constexpr int kViewCells = kViewSize * kViewSize;

static std::array<std::array<MapData, kViewSize>, kViewSize> view7; /* 7x7 map用 全キャラ共有 */
static int sp;		/* 視界内自キャラ位置 turn(2)/solid(3) */
static int cmr;		/* マップ方角 turn(キャラ方角)/solid(北) */

static MapData eagle; /* 15x15 eagle eye用 */

static std::array<char *, 4> mapset{};	/* マップセット(4組) */
static std::array<int, 4> mapuse{};

/* (dx, dy) のセルが視界に入るかを返す。壁 (T_MIST) の向こうは
 * 見えない。set_view7() が近いセルから順に埋めていくため、ここで
 * 参照する隣接セルは常に対象より先に決まっている (この順序が前提)。 */
int is_visible(int dx,int dy)
{
	const int d = std::abs(dx) + std::abs(dy);

	if (d < 2)
		return 1;		/* すぐ隣 */
	if (dx == 0) {			/* 前後方向 */
		if (view7[dy+sp-fsign(dy)][dx+3].tip & T_MIST)
			return 0;
		return 1;
	}
	if (dy == 0) {			/* 横方向 */
		if (view7[dy+sp][dx+3-fsign(dx)].tip & T_MIST)
			return 0;
		return 1;
	}
	/* 斜め */
	/* d > 2 の条件がないと、自分の立ち位置が壁だと斜めが見えなくなる */
	if (d > 2 && view7[dy+sp-fsign(dy)][dx+3-fsign(dx)].tip & T_MIST)
		return 0;
	/* 斜め続きの壁の向こうは見えない。ただし d == 3 は例外。 */
	if (d != 3 && /* <- 例外... */
		view7[dy+sp][dx+3-fsign(dx)].tip & T_MIST &&
		view7[dy+sp-fsign(dy)][dx+3].tip & T_MIST)
		return 0;
	return 1;
}

/* 自分に近い側から可視・不可視を決定していくための走査順序。
 * (x, y) は自キャラを原点とした視界内の相対位置。先頭は必ず
 * (0, 0) = 自分の立ち位置。 */
/* for turn */
static constexpr std::array<int, kViewCells> tsx = {
    0, 0, 1, 0,-1,-1, 1, 1,-1, 0, 2, 0,-2,-1, 1, 2, 2, 1,-1,-2,-2, 0, 3,-3,-2, 2, 2,-2,-1, 1, 3, 3,-3,-3,-2, 2, 3, 3,-3,-3, 0, 3,-3,-1, 1,-2, 2,-3, 3};
static constexpr std::array<int, kViewCells> tsy = {
    0, 1, 0,-1, 0, 1, 1,-1,-1, 2, 0,-2, 0, 2, 2, 1,-1,-2,-2,-1, 1, 3, 0, 0, 2, 2,-2,-2, 3, 3, 1,-1,-1, 1, 3, 3, 2,-2,-2, 2, 4, 3, 3, 4, 4, 4, 4, 4, 4};

/* for solid */
static constexpr std::array<int, kViewCells> ssx = {
    0, 0, 1, 0,-1,-1, 1, 1,-1, 0, 2, 0,-2,-1, 1, 2, 2, 1,-1,-2,-2, 0, 3, 0,-3,-2, 2, 2,-2,-1, 1, 3, 3, 1,-1,-3,-3,-2, 2, 3, 3, 2,-2,-3,-3, 3, 3,-3,-3};
static constexpr std::array<int, kViewCells> ssy = {
    0, 1, 0,-1, 0, 1, 1,-1,-1, 2, 0,-2, 0, 2, 2, 1,-1,-2,-2,-1, 1, 3, 0,-3, 0, 2, 2,-2,-2, 3, 3, 1,-1,-3,-3,-1, 1, 3, 3, 2,-2,-3,-3,-2, 2, 3,-3,-3, 3};

void   set_view7(int n)
{
  cmr = (cd[n].modef & MD_M57SOLID ? 0 : cd[n].d);
  const int blind = TCN_BLIND(n);
  const std::array<int, kViewCells> *vx;
  const std::array<int, kViewCells> *vy;
  if ((cd[n].modef & MD_M57SOLID) || chr_pos_psp_compati) {
	  vx = &ssx; vy = &ssy;
	  sp = 3;
  } else {
	  vx = &tsx; vy = &tsy;
	  sp = 2;
  }
  mapset.fill(nullptr);
  mapuse.fill(0);
  for (int i = 0; i < kViewCells; i++) {
      const int ox = (*vx)[i];
      const int oy = (*vy)[i];

      const int xx = cd[n].x + px[cmr] * oy - py[cmr] * ox;
      const int yy = cd[n].y + py[cmr] * oy + px[cmr] * ox;
/*
 *   y^--+          +------>xx
 *    |  |          |  +-+      d= 0    1    2    3
 *    |  |    ==>   |  +-+         up right down left
 *    +-->x       yy|
 *   (view)         v  (map)
 *
 */

      if (xx < 0 || xx >= MAX_X || yy < 0 || yy >= MAX_Y) {
		UNSEE7(oy+sp,ox+3);
		continue;
      }
      if (is_visible(ox,oy)) {
		std::memcpy(&(view7[oy+sp][ox+3]),&(map[yy][xx]),sizeof(MapData));
		view7[oy+sp][ox+3].tip &= ~T_ITEM7;
		view7[oy+sp][ox+3].tip |= flooritemtype(xx,yy) << 8;
		if (blind > 0 &&  (map[yy][xx].tip & TMASK) == T_MIST) {
			view7[oy + sp][ox + 3].tip = (map[yy][xx].tip & TCLEAR) | T_SPACE;
		} else if (blind < 0 && i) {	/* 盲目でも立ち位置は見える */
			view7[oy + sp][ox + 3].tip = (map[yy][xx].tip & TCLEAR & ~T_SURFACE) | T_MIST;
		}
		/* マップセットは最大 4 組まで。同じポインタ (同じ定義) の
		 * セルは同じ番号にする。4 組を超えたセルは 0 番にする。 */
		int j;
		for (j = 0; j < 4; j++) {
			if (mapuse[j]==0) {
				mapuse[j] = 1;
				mapset[j] = fld[view7[oy+sp][ox+3].area].mapset;
				view7[oy+sp][ox+3].area=j;
				break;
			}
			if (mapset[j] == fld[view7[oy+sp][ox+3].area].mapset) {
				view7[oy+sp][ox+3].area=j;
				break;
			}
		}
		if (j==4) {
			view7[oy+sp][ox+3].area=0;
		}
	  } else {
		UNSEE7(oy+sp,ox+3);
	  }
    }

  /* 下位互換視界。FF_M57 の土地では、向きに応じて視界の端の
   * 列・行を見えなくする。 */
  if (fld[map[cd[n].y][cd[n].x].area].flag & FF_M57) {
    if (cd[n].modef & MD_M57SOLID) {
      if(cd[n].d & 1) {
        for(int i=0;i<7;i++) {
          UNSEE7(0,i);
          UNSEE7(6,i);
        }
        if(cd[n].d & 2) {
          for(int i=1;i<6;i++) {
            UNSEE7(i,5);
            UNSEE7(i,6);
          }
        } else {
          for(int i=1;i<6;i++) {
            UNSEE7(i,0);
            UNSEE7(i,1);
          }
        }
      } else {
        for(int i=0;i<7;i++) {
          UNSEE7(i,0);
          UNSEE7(i,6);
        }
        if(cd[n].d & 2) {
          for(int i=1;i<6;i++) {
            UNSEE7(5,i);
            UNSEE7(6,i);
          }
        } else {
          for(int i=1;i<6;i++) {
            UNSEE7(0,i);
            UNSEE7(1,i);
          }
        }
      }
    } else {
      for(int i=0;i<7;i++) {
        UNSEE7(i,0);
        UNSEE7(i,6);
      }
      for(int i=1;i<6;i++) {
        UNSEE7(0,i);
      }
      if (!chr_pos_psp_compati) {
        for(int i=1;i<6;i++) {
          UNSEE7(6,i);
        }
      } else {
        for(int i=1;i<6;i++) {
          UNSEE7(1,i);
        }
      }
    }
  }
}

/* m57形式に埋め込むアイテムタイプ。罠でない最初の床アイテムを
 * 探し、種類を 1～7 の番号で返す。罠は表示しない。 */
int flooritemtype(int x,int y)
{
	ItemOData **flooritem;

	flooritem = finditem(x,y,nullptr);
	while(flooritem != nullptr && *flooritem != nullptr) {
		if (!is_trap(*flooritem))
			break;
		flooritem = finditem(x,y,flooritem);
	}

	if (flooritem == nullptr || *flooritem == nullptr)
		return 0;
	if (is_gp(*flooritem))
		return 1;
	switch ((*flooritem)->id.type & IT_MASK) {
		case IT_FOREIGN:
			return 1;
		case IT_WEAPON:
			return 2;
		case IT_ARMOR:
			return 3;
		case IT_ACCESSORY:
			return 4;
		case IT_JEWEL:
		case IT_SPELL_JEWEL:
			return 5;
		case IT_FOOD:
			return 6;
		case IT_CHARGE:
		case IT_NOUSE:
		case IT_RESOURCE:
		case IT_EVENTLY:
		case IT_SCROLL:
			return 7;
	}
	return 0;
}

/* m57形式でマップチップを出力する。p の指す位置に 2 バイト書く。
 * p 自体は値渡しのため進まない (呼び出し側が 2 進める)。
 * flag: 0x8 = エリア番号を入れる、0x4 = キャラの存在を 'X' で
 * 示す (eagle eye 用)。
 * 2 バイト目の形式は 1iiimraa。1 は基数の 0x80、iii は
 * アイテムタイプ、m はメッセージ、r は屋根、aa はエリア番号。 */
void   dd_m57(char *p, MapData *d, int f)
{
  *p = ' ';
  /* 地形(1byte目) */
  switch (((d->tip & T_SURFACE) ? d->surface : d->tip & TMASK)) {
    case T_PPLATE:
      *p = 'o';
      break;
    case T_PLANT:
      *p = ':';
      break;
    case T_FLOWER:
      *p = '+';
      break;
    case T_WATER:
    case T_BWATER:
      *p = '_';
      break;
    case T_BOX:
      *p = 'x';
      break;
    case T_MIST:
      *p = '/';
      break;
    case T_TGATE:
      *p = '>';
      break;
    case T_GATE:
      *p = '[';
      break;
    case T_JAIL:
      *p = 'I';
      break;
    case T_WINDOW:
      *p = '|';
      break;
    case T_GLASS:
      *p = '=';
      break;
    case T_LBOX:
      *p = '%';
      break;
    case T_LGATE:
      *p = '{';
      break;
    case T_MWALL:
    case T_MSGATE:
    case T_LMSGATE:
      *p = 'H';
      break;
    case T_SGATE:
    case T_LSGATE:
    case T_WALL:
      *p = '#';
      break;
    case T_ROCK:
      *p = '@';
      break;
    case T_TREE:
      *p = 'T';
      break;
    case T_UNKNOWN:
      *p = '?';
      break;
    default:
      break;
  }
  /* バイナリ(2byte目) */
  *(++p) = static_cast<char>(0x80);
  if (f & 0x8)
    *p |= (d->area & 3);
  if (d->tip & T_ROOF) {
    *p |= 0x04;
  }
  if (d->tip & T_ITEM7) {
    *p |= (d->tip & T_ITEM7) >> 4;
  }
  if (d->tip & T_MESSAGE)
    *p |= 0x08;

  /* キャラの存在 for eagle eye。2 バイト目全体を 'X' で置き換える。
   * 接続キャラは常に、敵・NPC は名前が空白で始まらず、消えて
   * いない場合に印を付ける。 */
  if (f & 0x4) {
    for (int j = 0; j < MAX_CHRS_INONE; j++) {
      const int i = d->chr[j] - 1;
      if (i >= 0 && (i < MAX_NTTYS || *cd[i].name != ' ') && !TCN_VANISH(i)) {
        *p = 'X';
        break;
      }
    }
  }
}

#define MAX_PRINT_NPC 30

/* #m57 地図を送る。前回送った視界・キャラ一覧と比較し、変化が
 * あるときだけ送る。差分判定は、視界 7x7 全体の比較と、キャラ
 * 一覧の行ごとの比較 (先頭 6 バイトを除く) の 2 段で行う。
 *
 * reflesh_objects が立っているとき (魔法エフェクトの再描画) は
 * 差分判定を飛ばし、前回の途中 (obj_start_num) からエフェクト
 * オブジェだけを走査する。 */
void   print_m57(int n, int f)
{
  static constexpr std::array<char, 4> dc = {'N', 'E', 'S', 'W'};
  static constexpr std::array<char, 4> rdc = {'B', 'R', 'F', 'L'};
  std::array<char, 256> buf{};
  /* 前回送った視界とキャラ一覧。接続ごとに保持する。 */
  static std::array<std::array<std::array<MapData, 7>, 7>, MAX_NTTYS> last_view7{};
  /* last_npc は CPYSTR (strlcpy + sizeof) を使うため C 配列のまま。
   * std::array にして .data() を渡すと sizeof がポインタの
   * 大きさになってしまい、行が途中で切れる。 */
  static char last_npc[MAX_NTTYS][MAX_PRINT_NPC][256];
  static std::array<int, MAX_NTTYS> last_npc_num{};
  static std::array<std::array<char *, 3>, MAX_NTTYS> last_mapset{};
  int need_check;
  int k;
#if MAX_OBJECTS > 0
  static std::array<int, MAX_NTTYS> obj_start_num{};
#endif

  if (n>= MAX_NTTYS || cd[n].fd == -1)
    return;

  need_check = 0;

#if MAX_OBJECTS > 0
  if(!reflesh_objects) {
#endif

  if(reflesh_last_map[n]){
    need_check = 1;
    reflesh_last_map[n] = 0;
    /* bug fix 20010803 */
    for(int i=0;i<3;i++)
    	last_mapset[n][i] = nullptr;
  }

  set_view7(n);
  if(std::memcmp(&(last_view7[n][0][0]), &(view7[0][0]),sizeof(MapData)*7*7)){
    need_check = 1;
    std::memcpy(&(last_view7[n][0][0]), &(view7[0][0]),sizeof(MapData)*7*7);
  }

  k = 0;

#if MAX_OBJECTS > 0
  /* 背景オブジェ。エフェクトオブジェの走査とは、効果フィルタの
   * 向き・行頭の文字 (B/F)・末尾の種類 (OT_STATUE/OT_MAGIC) の
   * 3 点だけが違う。 */
  for (int y = 6-sp; y >= -sp && k<MAX_PRINT_NPC; y--)
    for (int x = -3; x <= 3 && k<MAX_PRINT_NPC; x++) {
      if (view7[y + sp][x + 3].tip == T_UNKNOWN)
         continue;

      const int xx = cd[n].x + px[cmr] * y - py[cmr] * x;
      const int yy = cd[n].y + py[cmr] * y + px[cmr] * x;

      const int obj = dobj[yy][xx]-1;
      if (obj < 0)
         continue;

      if (od[obj].gname == nullptr)
         continue;

      if (od[obj].dir & OD_EFFECT)
         continue;

      std::snprintf(buf.data(), buf.size(),
        "#m57 O B%04x:%01i %01i %c %-31s 00 %-15s %c %02x \n",
        obj, x+3, 6-sp-y,
        (od[obj].dir & OD_TRACE) ?
            rdc[(((std::abs(y)>std::abs(x))?(y>=0?0:2):(x>0?1:3)) + od[obj].dir) & OD_DIRECTION] : /* 観測者との位置関係による向き */
        ((od[obj].dir & OD_CONSTDIR) ?
            rdc[od[obj].dir & OD_DIRECTION] :                          /* 固定向き */
            rdc[(4 + od[obj].dir - cmr) & OD_DIRECTION]),                 /* ノーマル */
        (od[obj].name != nullptr) ? od[obj].name : "",
         od[obj].gname,
       (od[obj].dir & OD_DEBU) ? '*' : '#',
         ((od[obj].dir & OD_BOARD) >> 4) + OT_STATUE);

      if(need_check
         || last_npc_num[n]<=k
         || std::strcmp(last_npc[n][k]+6,buf.data()+6)){
        need_check = 1;
        CPYSTR(last_npc[n][k],buf.data());
      }
      k++;
    }
#endif

  /* キャラ。行頭 6 バイト (O 行の識別子と番号) は比較から除く。 */
  for (int y=0; y<7 && k<MAX_PRINT_NPC; y++)
    for (int x=0; x<7 && k<MAX_PRINT_NPC; x++) {
      int l = 0;
      for (int i = 0; i < MAX_CHRS_INONE && k < MAX_PRINT_NPC; i++) {
        const int j = view7[6-y][x].chr[i] - 1;
        if (j < 0)
          continue;
        if (((cd[j].name[0] != ' ' && !TCN_VANISH(j))
                 || cd[n].at == AT_VISIBLE)) {
          std::snprintf(buf.data(), buf.size(),
            "#m57 O C%04x:%01i %01i %c %-31s %02x %-15s %c %02x \n",
            j, x, y,
            rdc[(4 + cd[j].d - cmr) & 3] , cd[j].name ,
            (!(cd[j].st) && CN_GASIFY(j) >= 60 ? ST_GASIFY : cd[j].st),
            CHRGRA(j),
             ((!(cd[j].sg & 3) && gigant_check(j))||(cd[j].sg & 1)) ? '*' : '#',
            /* for change character graphic added by Qajin 19991224 */
            (j < MAX_NTTYS ? ((cd[j].cg != nullptr || cd[j].ocg != nullptr) ? 0x40: 0) : cd[j].type));

          if(need_check
             || last_npc_num[n]<=k
             || std::strcmp(last_npc[n][k]+6,buf.data()+6)){
            need_check = 1;
            CPYSTR(last_npc[n][k],buf.data());
          }
          k++;
          l++;
        }
      }
    }

#if MAX_OBJECTS > 0
  obj_start_num[n] = k;
  } else {
    k = obj_start_num[n];
    set_view7(n);
  }
#endif

  if(!need_check && ((f & MD_CHAT) || !cd[n].eye))
    return;

#if MAX_OBJECTS > 0
  /* エフェクトオブジェ。背景オブジェの走査とは、効果フィルタの
   * 向き・行頭の文字 (B/F)・末尾の種類 (OT_STATUE/OT_MAGIC) の
   * 3 点だけが違う。 */
  for (int y = 6-sp; y >= -sp && k<MAX_PRINT_NPC; y--)
    for (int x = -3; x <= 3 && k<MAX_PRINT_NPC; x++) {
      if (view7[y + sp][x + 3].tip == T_UNKNOWN)
         continue;

      const int xx = cd[n].x + px[cmr] * y - py[cmr] * x;
      const int yy = cd[n].y + py[cmr] * y + px[cmr] * x;

      const int obj = dobj[yy][xx]-1;
      if (obj < 0)
         continue;

      if (od[obj].gname == nullptr)
         continue;

      if (!(od[obj].dir & OD_EFFECT))
         continue;

      std::snprintf(buf.data(), buf.size(),
        "#m57 O F%04x:%01i %01i %c %-31s 00 %-15s %c %02x \n",
        obj, x+3, 6-sp-y,
        (od[obj].dir & OD_TRACE) ?
            rdc[(((std::abs(y)>std::abs(x))?(y>=0?0:2):(x>0?1:3)) + od[obj].dir) & OD_DIRECTION] : /* 観測者との位置関係による向き */
        ((od[obj].dir & OD_CONSTDIR) ?
            rdc[od[obj].dir & OD_DIRECTION] :                          /* 固定向き */
            rdc[(4 + od[obj].dir - cmr) & OD_DIRECTION]),                 /* ノーマル */
        (od[obj].name != nullptr) ? od[obj].name : "",
         od[obj].gname,
       (od[obj].dir & OD_DEBU) ? '*' : '#',OT_MAGIC);

      if(need_check
         || last_npc_num[n]<=k
         || std::strcmp(last_npc[n][k]+6,buf.data()+6)){
        need_check = 1;
        CPYSTR(last_npc[n][k],buf.data());
      }
      k++;
    }
#endif


  if(need_check || last_npc_num[n]!=k){
    need_check = 1;
    last_npc_num[n]=k;
  }

  /* 以上で判定終了 */

  cd[n].m_wait_c = 0;
  if(!need_check)
    return;

#ifdef DEBUG_M57
  std::snprintf(debug_buf, sizeof(debug_buf),"print m57(%d)(print_m57)",n);
  log_print(debug_buf);
#endif

  /* 2 組目以降のマップセットが変わっていたら全組送り直す。
   * 1 組目は送らない。 */
  int i;
  for(i=1;i<4 && mapuse[i] != 0;i++)
    if (last_mapset[n][i-1] != mapset[i])
    	break;

  if (i<4 && mapuse[i] != 0) {
    for(i=1;i<4 && mapuse[i] != 0;i++) {
	  if (mapset[i] == nullptr) {
        std::snprintf(buf.data(), buf.size(), "#mapset-define %1i default\n",i);
	  } else {
        std::snprintf(buf.data(), buf.size(), "#mapset-define %1i %s\n",i,mapset[i]);
      }
      check_write(n,buf.data());
      last_mapset[n][i-1] = mapset[i];
    }
  }

  /* 行頭 "#m57 M %c %08lx:" は 18 バイト。セルは 2 バイトずつ、
   * 行ごとに 14 バイトで 98 バイト。末尾に " \n" を付ける。 */
  std::snprintf(buf.data(), buf.size(), "#m57 M %c %08lx:",dc[cd[n].d],std::time(nullptr)+tslide*60*60);
  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 7; x++) {
      dd_m57(buf.data() + 18 + y * 14 + x * 2, &(view7[6 - y][x]), 0x8);
    }
  buf[116] = ' ';
  buf[117] = '\n';
  buf[118] = 0;
  check_write(n, buf.data());

  for(k = 0; k<last_npc_num[n]; k++)
    check_write(n, last_npc[n][k]);

  std::snprintf(buf.data(), buf.size(), "#m57 .\n");
  check_write(n, buf.data());

  if (map[cd[n].y][cd[n].x].tip & T_MESSAGE) {
    if (f & MD_BOARD) {
      dmcheck_write(n, DMM_LIST_BOARD);
      load_sd(n, cd[n].x, cd[n].y, 'T', 1);
      list_board(n, 1);
    }
  }
  if (map[cd[n].y][cd[n].x].tip & (T_ITEM | T_TRAP)) {
    if (f & MD_FLOORITEM) {
      dmcheck_write(n, DMM_LIST_ITEM);
      list_item(n, 1);
    }
  }
}

/* Eagle eye の広域地図 (#ex-eagleeye M 行) を送る。 */
void eagleeye_m57(int n)
{
  const int fl = fld[map[cd[n].y][cd[n].x].area].floor;
  /* m81 (9x9) クライアントでは Eagle eye は 20x20 にする
   * (9x9 の方が 7x7 の 15x15 より広く見えるのが仕様、2026-11-17)。
   * それ以外は従来通り (FF_M57 の土地では 11x11、それ以外は 15x15)。
   * 20 は偶数なので範囲は -10..+9 (中心は1マス北西寄り) で、
   * ヘッダの size は マス数-1 = 19。 */
  const bool m81 = (cd[n].modef & MD_M81) != 0;
  const int size = (m81 ? 19
                    : ((fld[map[cd[n].y][cd[n].x].area].flag & FF_M57) ? 10 : 14));
  const int lo = m81 ? -10 : -size / 2;
  const int hi = m81 ? 9 : size / 2;
#ifndef NO_SKILL
  const int j = cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_EAGLE];
#endif
  std::array<char, 256> buf{};
  char *p;

  /* check_write() は引数を書き換えない (中身を別バッファへ
   * 写してから送る) ため、文字列リテラルを渡してよい。 */
  check_write(n, (char *)"#ex-eagleeye start\n");
  /* 取り敢えず 座標通知は無し */

  for (int y = lo; y <= hi; y++) {
    /* 行頭 "#ex-eagleeye M %2.2d %2.2d " は 21 バイト。 */
    std::snprintf(buf.data(),buf.size(),"#ex-eagleeye M %2.2d %2.2d ",size,y-lo);
    p = buf.data()+21;
    for (int x = lo; x <= hi; x++) {

#ifndef NO_SKILL
      /* bug fix for No Skill 20041212 */
      /* add for common magic skill 20020522 */
      /* 注意: NO_SKILL 未定義のとき、下の else がこの if に結び付く。
       * #endif の位置を変えると結び付きが変わる。 */
      if (fisqrt((x * x + y * y)*100) > j / 10 + 40) {
        *p++ = '?';
        *p++ = static_cast<char>(0x80);
        continue;
      } else
#endif
             if (cd[n].x + x < 0 || cd[n].x + x >= MAX_X
          || cd[n].y + y < 0 || cd[n].y + y >= MAX_Y) {
        *p++ = '?';
        *p++ = static_cast<char>(0x80);
        continue;
      } else if (fld[map[cd[n].y + y][cd[n].x + x].area].floor != fl) {
        *p++ = '?';
        *p++ = static_cast<char>(0x80);
        continue;
      }
	  /* add for item type on eagle 20021015 */
	  std::memcpy(&eagle,&(map[cd[n].y + y][cd[n].x + x]),sizeof(MapData));
	  eagle.tip &= ~T_ITEM7;
	  eagle.tip |= flooritemtype(cd[n].x + x,cd[n].y + y) << 8;
      dd_m57(p, &eagle, 0x4);
      p += 2;
    }
    *p++ = ' '; /* 文字化け対策 */
    *p++ = '\n';
    *p++ = '\0';
    check_write(n, buf.data());
  }

  check_write(n, (char *)"#ex-eagleeye end\n");
}
