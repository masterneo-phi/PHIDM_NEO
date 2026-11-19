/* for Phi m81     */

/* #m81 形式の地図 (9x9)。phi_m57.cpp の 7x7 版を 9x9 に拡張した
 * もので、行の書式 (#m81 M / #m81 O / #m81 .) とセルの 2 バイト
 * 形式は m57 と同じ約束にする。差分判定・送信の流れも m57 と同じ。
 *
 * 視界の形: 北固定 (solid) では自キャラが中心 (4,4)。回転 (turn)
 * では前方に広く、自キャラは後ろから 3 行目 (左右は中心) に置く
 * (m57 の turn が前方 4・後方 2 なのと同じ考え方で、前方 5・
 * 後方 3)。
 *
 * m57 にある FF_M57 土地向けの端の視界制限は、9x9 では適用しない
 * (9x9 は新しい土地向けのモードという位置付け)。
 */

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <utility>

#include  "phi_dm.h"
#include  "phi_data.h"
#include  "phi_calc.h"
#include  "phi_obj.h"
#include  "phi_str.h"
#include  "phi_m57.h"
#include  "phi_m81.h"

extern int    px[];
extern int    py[];

extern int reflesh_last_map[MAX_NTTYS];

#if MAX_OBJECTS > 0
extern int reflesh_objects;
#endif

#define UNSEE9(y,x)	{memset(&(view9[(y)][(x)]),'\0',sizeof(MapData)); view9[(y)][(x)].tip = T_UNKNOWN;}

/* 視界の大きさ。set_view9() が 81 セルを近い順に埋める。 */
constexpr int kViewSize = 9;
constexpr int kViewCells = kViewSize * kViewSize;
constexpr int kCenter = 4;	/* 視界内の自キャラ列 (0 起点) */

static std::array<std::array<MapData, kViewSize>, kViewSize> view9; /* 9x9 map用 全キャラ共有 */
static int sp9;		/* 視界内自キャラ行 turn(3)/solid(4) */
static int cmr9;	/* マップ方角 turn(キャラ方角)/solid(北) */

static std::array<char *, 4> mapset9{};	/* マップセット(4組) */
static std::array<int, 4> mapuse9{};

namespace {

/* 自分に近い側から可視・不可視を決定していくための走査順序。
 * is_visible9() が対象より中心に近いセルを参照するので、中心から
 * のマンハッタン距離の昇順でなければならない (m57 の tsx/ssx と
 * 同じ前提を、表を生成することで満たす)。 */
struct ScanOrder {
    std::array<int, kViewCells> x{};
    std::array<int, kViewCells> y{};
};

ScanOrder make_scan(int dx_min, int dx_max, int dy_min, int dy_max)
{
    std::array<std::pair<int, int>, kViewCells> cells{};
    int k = 0;
    for (int dy = dy_min; dy <= dy_max; dy++)
        for (int dx = dx_min; dx <= dx_max; dx++)
            cells[k++] = {dx, dy};
    std::sort(cells.begin(), cells.end(),
              [](const std::pair<int, int> &a, const std::pair<int, int> &b) {
                  const int da = std::abs(a.first) + std::abs(a.second);
                  const int db = std::abs(b.first) + std::abs(b.second);
                  if (da != db)
                      return da < db;
                  const int ma = std::max(std::abs(a.first), std::abs(a.second));
                  const int mb = std::max(std::abs(b.first), std::abs(b.second));
                  if (ma != mb)
                      return ma < mb;
                  if (a.second != b.second)
                      return a.second < b.second;
                  return a.first < b.first;
              });
    ScanOrder s;
    for (int i = 0; i < kViewCells; i++) {
        s.x[i] = cells[i].first;
        s.y[i] = cells[i].second;
    }
    return s;
}

/* for solid: 自キャラ中心、全方向 ±4 */
const ScanOrder &scan_solid()
{
    static const ScanOrder s = make_scan(-4, 4, -4, 4);
    return s;
}

/* for turn: 前方 +5・後方 -3、左右 ±4 */
const ScanOrder &scan_turn()
{
    static const ScanOrder s = make_scan(-4, 4, -3, 5);
    return s;
}

} // namespace

/* (dx, dy) のセルが視界に入るかを返す。phi_m57.cpp の is_visible()
 * と同じ規則 (壁 T_MIST の向こうは見えない)。 */
static int is_visible9(int dx, int dy)
{
    const int d = std::abs(dx) + std::abs(dy);

    if (d < 2)
        return 1;		/* すぐ隣 */
    if (dx == 0) {			/* 前後方向 */
        if (view9[dy + sp9 - fsign(dy)][dx + kCenter].tip & T_MIST)
            return 0;
        return 1;
    }
    if (dy == 0) {			/* 横方向 */
        if (view9[dy + sp9][dx + kCenter - fsign(dx)].tip & T_MIST)
            return 0;
        return 1;
    }
    /* 斜め */
    if (d > 2 && view9[dy + sp9 - fsign(dy)][dx + kCenter - fsign(dx)].tip & T_MIST)
        return 0;
    /* 斜め続きの壁の向こうは見えない。ただし d == 3 は例外。 */
    if (d != 3 && /* <- 例外... */
        view9[dy + sp9][dx + kCenter - fsign(dx)].tip & T_MIST &&
        view9[dy + sp9 - fsign(dy)][dx + kCenter].tip & T_MIST)
        return 0;
    return 1;
}

void   set_view9(int n)
{
  cmr9 = (cd[n].modef & MD_M57SOLID ? 0 : cd[n].d);
  const int blind = TCN_BLIND(n);
  const ScanOrder *scan;
  if ((cd[n].modef & MD_M57SOLID) || chr_pos_psp_compati) {
      scan = &scan_solid();
      sp9 = 4;
  } else {
      scan = &scan_turn();
      sp9 = 3;
  }
  mapset9.fill(nullptr);
  mapuse9.fill(0);
  for (int i = 0; i < kViewCells; i++) {
      const int ox = scan->x[i];
      const int oy = scan->y[i];

      const int xx = cd[n].x + px[cmr9] * oy - py[cmr9] * ox;
      const int yy = cd[n].y + py[cmr9] * oy + px[cmr9] * ox;

      if (xx < 0 || xx >= MAX_X || yy < 0 || yy >= MAX_Y) {
        UNSEE9(oy + sp9, ox + kCenter);
        continue;
      }
      if (is_visible9(ox, oy)) {
        std::memcpy(&(view9[oy + sp9][ox + kCenter]), &(map[yy][xx]), sizeof(MapData));

  /* 2x2 の大きな敵は基準マスのセルだけに残す (他の体のマスからは
     消す)。地図送信で同じ敵が4回出るのを防ぐため。 */
  for (int ci = 0; ci < MAX_CHRS_INONE; ci++) {
    int cj = view9[oy + sp9][ox + kCenter].chr[ci] - 1;
    if (cj >= 0 && chr_big(cj) && (cd[cj].x != xx || cd[cj].y != yy))
      view9[oy + sp9][ox + kCenter].chr[ci] = 0;
  }
        view9[oy + sp9][ox + kCenter].tip &= ~T_ITEM7;
        view9[oy + sp9][ox + kCenter].tip |= flooritemtype(xx, yy) << 8;
        if (blind > 0 && (map[yy][xx].tip & TMASK) == T_MIST) {
            view9[oy + sp9][ox + kCenter].tip = (map[yy][xx].tip & TCLEAR) | T_SPACE;
        } else if (blind < 0 && i) {	/* 盲目でも立ち位置は見える */
            view9[oy + sp9][ox + kCenter].tip = (map[yy][xx].tip & TCLEAR & ~T_SURFACE) | T_MIST;
        }
        /* マップセットは最大 4 組まで (m57 と同じ)。 */
        int j;
        for (j = 0; j < 4; j++) {
            if (mapuse9[j] == 0) {
                mapuse9[j] = 1;
                mapset9[j] = fld[view9[oy + sp9][ox + kCenter].area].mapset;
                view9[oy + sp9][ox + kCenter].area = j;
                break;
            }
            if (mapset9[j] == fld[view9[oy + sp9][ox + kCenter].area].mapset) {
                view9[oy + sp9][ox + kCenter].area = j;
                break;
            }
        }
        if (j == 4) {
            view9[oy + sp9][ox + kCenter].area = 0;
        }
      } else {
        UNSEE9(oy + sp9, ox + kCenter);
      }
    }
}

#define MAX_PRINT_NPC 30

/* #m81 地図を送る。print_m57() と同じ差分判定・送信順序。 */
void   print_m81(int n, int f)
{
  static constexpr std::array<char, 4> dc = {'N', 'E', 'S', 'W'};
  static constexpr std::array<char, 4> rdc = {'B', 'R', 'F', 'L'};
  std::array<char, 256> buf{};
  /* 前回送った視界とキャラ一覧。接続ごとに保持する。 */
  static std::array<std::array<std::array<MapData, 9>, 9>, MAX_NTTYS> last_view9{};
  /* last_npc は CPYSTR (strlcpy + sizeof) を使うため C 配列のまま。 */
  static char last_npc[MAX_NTTYS][MAX_PRINT_NPC][256];
  static std::array<int, MAX_NTTYS> last_npc_num{};
  static std::array<std::array<char *, 3>, MAX_NTTYS> last_mapset{};
  int need_check;
  int k;
#if MAX_OBJECTS > 0
  static std::array<int, MAX_NTTYS> obj_start_num{};
#endif

  if (n >= MAX_NTTYS || cd[n].fd == -1)
    return;

  need_check = 0;

#if MAX_OBJECTS > 0
  if (!reflesh_objects) {
#endif

  if (reflesh_last_map[n]) {
    need_check = 1;
    reflesh_last_map[n] = 0;
    /* bug fix 20010803 */
    for (int i = 0; i < 3; i++)
        last_mapset[n][i] = nullptr;
  }

  set_view9(n);
  if (std::memcmp(&(last_view9[n][0][0]), &(view9[0][0]), sizeof(MapData) * 9 * 9)) {
    need_check = 1;
    std::memcpy(&(last_view9[n][0][0]), &(view9[0][0]), sizeof(MapData) * 9 * 9);
  }

  k = 0;

#if MAX_OBJECTS > 0
  /* 背景オブジェ。 */
  for (int y = 8 - sp9; y >= -sp9 && k < MAX_PRINT_NPC; y--)
    for (int x = -4; x <= 4 && k < MAX_PRINT_NPC; x++) {
      if (view9[y + sp9][x + 4].tip == T_UNKNOWN)
         continue;

      const int xx = cd[n].x + px[cmr9] * y - py[cmr9] * x;
      const int yy = cd[n].y + py[cmr9] * y + px[cmr9] * x;

      const int obj = dobj[yy][xx] - 1;
      if (obj < 0)
         continue;

      if (od[obj].gname == nullptr)
         continue;

      if (od[obj].dir & OD_EFFECT)
         continue;

      snprintf(buf.data(), buf.size(),
        "#m81 O B%04x:%01i %01i %c %-31s 00 %-15s %c %02x \n",
        obj, x + 4, 8 - sp9 - y,
        (od[obj].dir & OD_TRACE) ?
            rdc[(((std::abs(y) > std::abs(x)) ? (y >= 0 ? 0 : 2) : (x > 0 ? 1 : 3)) + od[obj].dir) & OD_DIRECTION] : /* 観測者との位置関係による向き */
        ((od[obj].dir & OD_CONSTDIR) ?
            rdc[od[obj].dir & OD_DIRECTION] :                          /* 固定向き */
            rdc[(4 + od[obj].dir - cmr9) & OD_DIRECTION]),                 /* ノーマル */
        (od[obj].name != nullptr) ? od[obj].name : "",
         od[obj].gname,
       (od[obj].dir & OD_DEBU) ? '*' : '#',
         ((od[obj].dir & OD_BOARD) >> 4) + OT_STATUE);

      if (need_check
         || last_npc_num[n] <= k
         || std::strcmp(last_npc[n][k] + 6, buf.data() + 6)) {
        need_check = 1;
        CPYSTR(last_npc[n][k], buf.data());
      }
      k++;
    }
#endif

  /* キャラ。行頭 6 バイト (O 行の識別子と番号) は比較から除く。 */
  for (int y = 0; y < 9 && k < MAX_PRINT_NPC; y++)
    for (int x = 0; x < 9 && k < MAX_PRINT_NPC; x++) {
      int l = 0;
      for (int i = 0; i < MAX_CHRS_INONE && k < MAX_PRINT_NPC; i++) {
        const int j = view9[8 - y][x].chr[i] - 1;
        if (j < 0)
          continue;
        if (((cd[j].name[0] != ' ' && !TCN_VANISH(j))
                 || cd[n].at == AT_VISIBLE)) {
          snprintf(buf.data(), buf.size(),
            "#m81 O C%04x:%01i %01i %c %-31s %02x %-15s %c %02x \n",
            j, x, y,
            rdc[(4 + cd[j].d - cmr9) & 3], cd[j].name,
            (!(cd[j].st) && CN_GASIFY(j) >= 60 ? ST_GASIFY : cd[j].st),
            CHRGRA(j),
             chr_marker(j),
            /* for change character graphic added by Qajin 19991224 */
            (j < MAX_NTTYS ? ((cd[j].cg != nullptr || cd[j].ocg != nullptr) ? 0x40 : 0) : cd[j].type));

          if (need_check
             || last_npc_num[n] <= k
             || std::strcmp(last_npc[n][k] + 6, buf.data() + 6)) {
            need_check = 1;
            CPYSTR(last_npc[n][k], buf.data());
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
    set_view9(n);
  }
#endif

  if (!need_check && ((f & MD_CHAT) || !cd[n].eye))
    return;

#if MAX_OBJECTS > 0
  /* エフェクトオブジェ。 */
  for (int y = 8 - sp9; y >= -sp9 && k < MAX_PRINT_NPC; y--)
    for (int x = -4; x <= 4 && k < MAX_PRINT_NPC; x++) {
      if (view9[y + sp9][x + 4].tip == T_UNKNOWN)
         continue;

      const int xx = cd[n].x + px[cmr9] * y - py[cmr9] * x;
      const int yy = cd[n].y + py[cmr9] * y + px[cmr9] * x;

      const int obj = dobj[yy][xx] - 1;
      if (obj < 0)
         continue;

      if (od[obj].gname == nullptr)
         continue;

      if (!(od[obj].dir & OD_EFFECT))
         continue;

      snprintf(buf.data(), buf.size(),
        "#m81 O F%04x:%01i %01i %c %-31s 00 %-15s %c %02x \n",
        obj, x + 4, 8 - sp9 - y,
        (od[obj].dir & OD_TRACE) ?
            rdc[(((std::abs(y) > std::abs(x)) ? (y >= 0 ? 0 : 2) : (x > 0 ? 1 : 3)) + od[obj].dir) & OD_DIRECTION] : /* 観測者との位置関係による向き */
        ((od[obj].dir & OD_CONSTDIR) ?
            rdc[od[obj].dir & OD_DIRECTION] :                          /* 固定向き */
            rdc[(4 + od[obj].dir - cmr9) & OD_DIRECTION]),                 /* ノーマル */
        (od[obj].name != nullptr) ? od[obj].name : "",
         od[obj].gname,
       (od[obj].dir & OD_DEBU) ? '*' : '#',
         OT_MAGIC);

      if (need_check
         || last_npc_num[n] <= k
         || std::strcmp(last_npc[n][k] + 6, buf.data() + 6)) {
        need_check = 1;
        CPYSTR(last_npc[n][k], buf.data());
      }
      k++;
    }
#endif


  if (need_check || last_npc_num[n] != k) {
    need_check = 1;
    last_npc_num[n] = k;
  }

  /* 以上で判定終了 */

  cd[n].m_wait_c = 0;
  if (!need_check)
    return;

  /* 2 組目以降のマップセットが変わっていたら全組送り直す。
   * 1 組目は送らない。 */
  int i;
  for (i = 1; i < 4 && mapuse9[i] != 0; i++)
    if (last_mapset[n][i - 1] != mapset9[i])
        break;

  if (i < 4 && mapuse9[i] != 0) {
    for (i = 1; i < 4 && mapuse9[i] != 0; i++) {
      if (mapset9[i] == nullptr) {
        snprintf(buf.data(), buf.size(), "#mapset-define %1i default\n", i);
      } else {
        snprintf(buf.data(), buf.size(), "#mapset-define %1i %s\n", i, mapset9[i]);
      }
      check_write(n, buf.data());
      last_mapset[n][i - 1] = mapset9[i];
    }
  }

  /* 行頭 "#m81 M %c %08lx:" は 18 バイト。セルは 2 バイトずつ、
   * 行ごとに 18 バイトで 162 バイト。末尾に " \n" を付ける。 */
  snprintf(buf.data(), buf.size(), "#m81 M %c %08lx:", dc[cd[n].d], std::time(nullptr) + tslide * 60 * 60);
  for (int y = 0; y < 9; y++)
    for (int x = 0; x < 9; x++) {
      dd_m57(buf.data() + 18 + y * 18 + x * 2, &(view9[8 - y][x]), 0x8);
    }
  buf[180] = ' ';
  buf[181] = '\n';
  buf[182] = 0;
  check_write(n, buf.data());

  for (k = 0; k < last_npc_num[n]; k++)
    check_write(n, last_npc[n][k]);

  snprintf(buf.data(), buf.size(), "#m81 .\n");
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
