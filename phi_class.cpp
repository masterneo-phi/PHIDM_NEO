/* phi_class.cpp - PHIDM NEO extension: phi.cls (Lua) switch programs.
 *
 * See phi_class.h. The Lua state is deliberately small: only the base,
 * table, string and math libraries are opened (no io / os / package),
 * the file-loading base functions are removed, and every call runs
 * under an instruction-count hook so a runaway program is aborted and
 * logged instead of hanging the server. Errors never crash the DM:
 * they are caught with lua_pcall and written to phi.log.
 *
 * Script API (global table "dm"):
 *   dm.msg(text)            message to the player who fired the switch
 *   dm.teleport(x, y)       move that player (chain-teleport semantics)
 *   dm.get_cell(x, y)       -> "HH" style terrain pair of a cell
 *   dm.set_cell(x, y, pair) rewrite a cell (extension_dd semantics:
 *                           switch / gate flags on the cell survive)
 *   dm.rand(n)              1..n random
 *   dm.rand_range(a, b)     a..b random
 *   dm.give("item def")     an item appears at the player's feet
 *   dm.has_item("pattern")  -> bool, possession check (match_item)
 *   dm.player()             -> table {name,x,y,dir,hp,mhp,mp,mmp,gp,
 *                                     lv_f,lv_w,lv_m,lv_c}
 *   dm.spawn("enemy name")  arm switch-spawn (apc=2) slots, -> count
 *   dm.log(text)            write to phi.log
 *   dm.chain(num)           fire an :A chain for the player (same
 *                           anmf/anmt assignment as actions 9/19)
 *   dm.get_var("z3")        read a switch variable -> integer or nil
 *                           (names are the expression engine's xnn /
 *                           ynn / znn, resolved by varno_load)
 *   dm.set_var("z3", v)     write a switch variable -> bool
 *   dm.get_str(key)         read the player's string store -> string
 *                           or nil (persisted in phi.qst, see below)
 *   dm.set_str(key, value)  write the player's string store -> bool
 *   dm.del_str(key)         delete a string store entry -> bool
 *   dm.pet_type()           -> "fairy"/"cat"/"dog"/"bird" or nil
 *   dm.pet_name()           -> the pet's name or nil
 *   dm.pet_active()         -> bool: the pet can act (pool left)
 *   dm.pet_grant("dog")     grant a pet (starts the naming prompt)
 *                           -> bool; false while a pet is owned
 *   dm.pet_release()        release the pet at once -> bool
 *   dm.pet_restore()        refill both pet pools -> bool
 *   dm.vars                 plain table: world-shared variables,
 *                           kept in memory only (reset by Load class)
 *   dm.fx.text(pos, size, dur, text) / dm.fx.textoff(pos) /
 *   dm.fx.dark(level, dur, mode) / dm.fx.wipe(pattern, dir) /
 *   dm.fx.img(where, name, dur) / dm.fx.shake(amp, dur) /
 *   dm.fx.glow(dur)         DM FX lines; each returns whether the
 *                           player declared FX support (MD_FX)
 * The program function receives one argument: a context table
 *   {player=<index>, name=<name>, x=<x>, y=<y>, sx=<switch x>,
 *    sy=<switch y>}
 *
 * If phi.cls defines a global function named on_second, it is called
 * once per second for every connected player with the same context
 * table (sx/sy carry the player's own position). Worlds that do not
 * define it see no change.
 */
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "phi_dm.h"
#include "phi_str.h" /* dmprintf */
#include "phi_class.h"
#include "lua/lua.hpp"

/* provided by phi_data.cpp */
int load_item(ItemData *id, char *p);
int save_id(int n, int in, int f);
int match_item(ItemData id1, ItemData id2);
int fsps(int ps, int v, int mx);
unsigned short set_dd(char *p);
int varno_load(char **p);
int var_value(int n, int i);
void var_set_value(int n, int i, int v);
FILE *efopen(char *fnm, char *mod);
void efclose(FILE *fp);
/* DM FX senders (PHIDM NEO extension) */
int fx_send_payload(int n, const char *payload);
int fx_send_img(int n, const char *where, const char *name, long dur);
/* provided by phi_data.cpp / phi_calc.cpp / phi_enemy.cpp (pet system) */
int ecn(int, int);
int load_switch(int, int, int, int);
int hp_dmg(int, int, int);
int mr_rate(int);
void kill_other(int, int, int);
void raise_dead(int);
void angry_check(int, int);
extern CHROData *ed_o;
extern int enmax;

static lua_State *cls_L = nullptr;
static int cls_ready = 0;
static int cls_has_tick = 0;  /* phi.cls defines on_second */
static int cls_cur = -1;      /* player index while a program runs */
static long cls_budget = 0;   /* instruction budget, in hook ticks */

/* ---------------- character string store (phi.qst) ----------------
 * Per-character named strings, kept in a dedicated file so the
 * phi.cd format does not change. The whole store is held in memory
 * (loaded by qstr_load() at server startup) and written back by
 * qstr_save() on the same occasions as save_sav(): character logout
 * and gate transfer. The file format mirrors phi.sav: a ":<uid>"
 * header line, one ":S <key> <value>" line per entry, and a "."
 * terminator line per character.
 * Keys are 1-32 bytes and may not contain spaces or '=' and may
 * not start with '.' or ':' (a ':' inside a key is fine, so
 * namespaced keys like "flag:met_elder" work); values are up to
 * 255 bytes and may contain spaces. Both are raw byte strings
 * (world data is CP932) and can
 * never contain CR/LF: set_str cuts the value at the first CR/LF.
 * A missing or unreadable phi.qst is not fatal: the store simply
 * starts empty and the fact is logged. */

#define QSTR_KEY_MAX 32
#define QSTR_VAL_MAX 255
#define QSTR_KEYS_MAX 128 /* entries per character */

static const char *qstr_file = "phi.qst";
static std::map<std::string, std::map<std::string, std::string>> qstr_store;
static int qstr_loaded = 0;

static int qstr_key_ok(const char *key)
{
  std::size_t len = std::strlen(key);
  if (len < 1 || len > QSTR_KEY_MAX)
    return 0;
  if (key[0] == '.' || key[0] == ':')
    return 0;
  for (std::size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)key[i];
    if (c <= ' ' || c == '=')
      return 0;
  }
  return 1;
}

void qstr_load(void)
{
  qstr_store.clear();
  qstr_loaded = 1;
  FILE *fp = efopen((char *)qstr_file, (char *)"rb");
  if (!fp) {
    log_print((char *)"qstr     : phi.qst not found"
                      " (string store starts empty)");
    return;
  }
  char buf[QSTR_KEY_MAX + QSTR_VAL_MAX + 64];
  std::string uid;
  int chars = 0, entries = 0;
  while (std::fgets(buf, sizeof(buf), fp)) {
    std::size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
      buf[--len] = '\0';
    if (buf[0] == ':' && buf[1] == 'S' && buf[2] == ' ' &&
        !uid.empty()) {
      /* ":S <key> <value>" entry of the current record */
      char *p = buf + 3;
      char *sp = std::strchr(p, ' ');
      if (!sp)
        continue;
      *sp = '\0';
      if (!qstr_key_ok(p))
        continue;
      std::string value(sp + 1);
      if (value.size() > QSTR_VAL_MAX)
        value.resize(QSTR_VAL_MAX);
      auto &rec = qstr_store[uid];
      if (rec.count(p) || (int)rec.size() < QSTR_KEYS_MAX) {
        rec[p] = value;
        entries++;
      }
    } else if (buf[0] == ':') {
      /* record header ":<uid>" (trailing spaces are not part of it) */
      uid = buf + 1;
      while (!uid.empty() && uid.back() == ' ')
        uid.pop_back();
      if (!uid.empty())
        chars++;
    } else if (buf[0] == '.') {
      uid.clear();
    }
    /* anything else is ignored, like the other data file loaders */
  }
  efclose(fp);
  {
    char lbuf[128];
    snprintf(lbuf, sizeof(lbuf),
             "qstr     : phi.qst loaded (%i characters, %i entries)",
             chars, entries);
    log_print(lbuf);
  }
}

int qstr_save(int n)
{
  if (n < 0 || n >= MAX_NTTYS)
    return 1;
  if (!qstr_loaded || cd[n].uid[0] == '\0')
    return 1;
  FILE *wfp = efopen((char *)"phi.qst.bak", (char *)"wb");
  if (!wfp) {
    log_print((char *)"qstr     : phi.qst save failed (open)");
    return 0;
  }
  for (auto &rec : qstr_store) {
    if (rec.second.empty())
      continue;
    std::fprintf(wfp, ":%s\n", rec.first.c_str());
    for (auto &kv : rec.second)
      std::fprintf(wfp, ":S %s %s\n", kv.first.c_str(),
                   kv.second.c_str());
    std::fprintf(wfp, ".\n");
  }
  efclose(wfp);
  if (std::rename("phi.qst.bak", qstr_file) != 0) {
    log_print((char *)"qstr     : phi.qst save failed (rename)");
    return 0;
  }
  return 1;
}

/* One hook tick is 10000 instructions; 100 ticks = 1,000,000 per call. */
static void cls_hook(lua_State *L, lua_Debug *)
{
  if (--cls_budget <= 0)
    luaL_error(L, "program execution limit exceeded");
}

static void cls_log_error(const char *what)
{
  char buf[512];
  const char *msg = cls_L ? lua_tostring(cls_L, -1) : nullptr;
  snprintf(buf, sizeof(buf), "class    : %s: %s", what,
                msg ? msg : "(no message)");
  log_print(buf);
}

/* ---------------- dm.* API ---------------- */

static int l_msg(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  if (cls_cur >= 0) {
    std::string text(s);
    if (text.empty() || text.back() != '\n')
      text += '\n';
    std::vector<char> buf(text.size() + 64);
    dmprintf(cls_cur, 0, buf.data(), (int)buf.size(), (char *)"%s",
             text.c_str());
    check_write(cls_cur, buf.data());
  }
  return 0;
}

static int l_teleport(lua_State *L)
{
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  if (cls_cur < 0)
    return 0;
  if (x < 1 || x >= MAX_X - 1 || y < 1 || y >= MAX_Y - 1)
    return 0;
  /* same sequence as a chain teleport (load_mesg type 1) */
  clear_chr(cls_cur);
  cd[cls_cur].x = x;
  cd[cls_cur].y = y;
  set_chr(cls_cur);
  print_map(cls_cur, 0);
  if (!trnf) {
    cd[cls_cur].bx = cd[cls_cur].x;
    cd[cls_cur].by = cd[cls_cur].y;
  }
  return 0;
}

static int l_get_cell(lua_State *L)
{
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y)
    return 0;
  /* mirror save_dd's pair encoding */
  char pair[3];
  char *p = pair;
  *p = ' ';
  *(p + 1) = ' ';
  switch (map[y][x].tip & TMASK) {
  default:
    break;
  case T_ANTIMAGIC: *p = '*'; *(p + 1) = '*'; break;
  case T_PPLATE:    *p = 'o'; *(p + 1) = ' '; break;
  case T_PLANT:     *p = ':'; *(p + 1) = ':'; break;
  case T_FLOWER:    *p = '+'; *(p + 1) = ':'; break;
  case T_WATER:     *p = '_'; *(p + 1) = '_'; break;
  case T_BWATER:    *p = '^'; *(p + 1) = '^'; break;
  case T_BOX:
  case T_LBOX:      *p = 'x'; *(p + 1) = 'x'; break;
  case T_MIST:      *p = '/'; *(p + 1) = '/'; break;
  case T_EWALL:
  case T_TGATE:     *p = '.'; *(p + 1) = '.'; break;
  case T_GATE:
  case T_LGATE:     *p = '['; *(p + 1) = ']'; break;
  case T_SGATE:
  case T_LSGATE:    *p = '$'; *(p + 1) = '$'; break;
  case T_CWALL:     *p = '!'; *(p + 1) = '!'; break;
  case T_AWALL:     *p = 'a'; *(p + 1) = ' '; break;
  case T_JAIL:      *p = 'I'; *(p + 1) = 'I'; break;
  case T_WINDOW:    *p = '|'; *(p + 1) = '|'; break;
  case T_TREE:      *p = 'T'; *(p + 1) = 'T'; break;
  case T_GLASS:     *p = '='; *(p + 1) = '='; break;
  case T_MWALL:     *p = 'H'; *(p + 1) = 'H'; break;
  case T_MSGATE:
  case T_LMSGATE:   *p = 'Z'; *(p + 1) = 'Z'; break;
  case T_WALL:      *p = '#'; *(p + 1) = '#'; break;
  case T_ROCK:      *p = '@'; *(p + 1) = '@'; break;
  case T_UNKNOWN:   *p = '?'; *(p + 1) = '?'; break;
  case T_STORE:     *p = 's'; *(p + 1) = 's'; break;
  }
  if (map[y][x].tip & T_ROOF)
    *(p + 1) = '~';
  pair[2] = '\0';
  lua_pushstring(L, pair);
  return 1;
}

static int l_set_cell(lua_State *L)
{
  int x = (int)luaL_checkinteger(L, 1);
  int y = (int)luaL_checkinteger(L, 2);
  const char *pair = luaL_checkstring(L, 3);
  if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y)
    return 0;
  if (std::strlen(pair) < 2)
    return 0;
  char buf[4];
  buf[0] = pair[0];
  buf[1] = pair[1];
  buf[2] = '\0';
  /* same formula as extension_dd: dynamic flags (T_SWITCH etc.) and
     the surface survive; terrain and roof come from the pair */
  map[y][x].tip = (map[y][x].tip & (TCLEAR ^ (T_ROOF | T_SURFACE)))
                  | set_dd(buf);
  return 0;
}

static int l_rand(lua_State *L)
{
  int n = (int)luaL_checkinteger(L, 1);
  if (n < 1)
    n = 1;
  lua_pushinteger(L, 1 + frand(n));
  return 1;
}

static int l_rand_range(lua_State *L)
{
  int a = (int)luaL_checkinteger(L, 1);
  int b = (int)luaL_checkinteger(L, 2);
  if (b < a)
    b = a;
  lua_pushinteger(L, a + frand(b - a + 1));
  return 1;
}

/* Parse an item definition string with load_item. The engine's
   convention is that the item name is terminated by TWO consecutive
   spaces before the type field; script authors easily write one.
   When the strict parse fails, rescue the common case: find the
   last 4-character type-like token and widen the gap before it. */
static int parse_item_def(const char *def, ItemData *out)
{
  char buf[256];
  snprintf(buf, sizeof(buf), "%s", def);
  if (load_item(out, buf))
    return 1;
  std::string s(def);
  std::size_t pos = std::string::npos;
  for (std::size_t i = 0; i + 5 <= s.size(); i++) {
    if (s[i] != ' ')
      continue;
    bool istype = true;
    for (int k = 1; k <= 4; k++) {
      char ch = s[i + k];
      if (!(ch == '-' || (ch >= '0' && ch <= '9') ||
            (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')))
        istype = false;
    }
    if (istype && (i + 5 == s.size() || s[i + 5] == ' '))
      pos = i;
  }
  if (pos == std::string::npos)
    return 0;
  s.insert(pos, 1, ' ');
  snprintf(buf, sizeof(buf), "%s", s.c_str());
  return load_item(out, buf) != 0;
}

static int l_give(lua_State *L)
{
  const char *def = luaL_checkstring(L, 1);
  if (cls_cur < 0)
    return 0;
  /* cd[0] is the engine's scratch slot, as in switch action 14 */
  cd[0].x = cd[cls_cur].x;
  cd[0].y = cd[cls_cur].y;
  if (!parse_item_def(def, &cd[0].id[0]))
    return 0;
  std::vector<char> wbuf(512);
  dmprintf(cls_cur, 0, wbuf.data(), (int)wbuf.size(),
           DMM_APPEAR_SOMETHING, cd[0].id[0].name);
  check_write(cls_cur, wbuf.data());
  save_id(0, 0, 1);
  return 0;
}

static int l_has_item(lua_State *L)
{
  const char *pattern = luaL_checkstring(L, 1);
  if (cls_cur < 0) {
    lua_pushboolean(L, 0);
    return 1;
  }
  ItemData pat;
  if (!parse_item_def(pattern, &pat)) {
    lua_pushboolean(L, 0);
    return 1;
  }
  int found = 0;
  for (int i = 0; i < MAX_ITEMS; i++) {
    if (cd[cls_cur].id[i].type != DT_NONE &&
        match_item(pat, cd[cls_cur].id[i])) {
      found = 1;
      break;
    }
  }
  lua_pushboolean(L, found);
  return 1;
}

static int l_player(lua_State *L)
{
  if (cls_cur < 0)
    return 0;
  CHRData &c = cd[cls_cur];
  lua_newtable(L);
  lua_pushstring(L, c.name);
  lua_setfield(L, -2, "name");
  lua_pushinteger(L, c.x);
  lua_setfield(L, -2, "x");
  lua_pushinteger(L, c.y);
  lua_setfield(L, -2, "y");
  lua_pushinteger(L, c.d);
  lua_setfield(L, -2, "dir");
  lua_pushinteger(L, c.hp);
  lua_setfield(L, -2, "hp");
  lua_pushinteger(L, c.mhp);
  lua_setfield(L, -2, "mhp");
  lua_pushinteger(L, c.mp);
  lua_setfield(L, -2, "mp");
  lua_pushinteger(L, c.mmp);
  lua_setfield(L, -2, "mmp");
  lua_pushinteger(L, (lua_Integer)c.gp);
  lua_setfield(L, -2, "gp");
  lua_pushinteger(L, c.lv[LV_FIGHTER]);
  lua_setfield(L, -2, "lv_f");
  lua_pushinteger(L, c.lv[LV_WIZARD]);
  lua_setfield(L, -2, "lv_w");
  lua_pushinteger(L, c.lv[LV_MERCHANT]);
  lua_setfield(L, -2, "lv_m");
  lua_pushinteger(L, c.lv[LV_CREATER]);
  lua_setfield(L, -2, "lv_c");
  return 1;
}

static int l_spawn(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  std::size_t len = std::strlen(name);
  std::vector<char> mark(max_enemy > 0 ? max_enemy : 1, 0);
  for (int j = 0; j < max_enemy; j++) {
    if (ed_o[j].apc == 2 &&
        !std::strncmp(ed_o[j].name, name, len) &&
        (ed_o[j].name[len] == '\0' || ed_o[j].name[len] == ' '))
      mark[j] = 1;
  }
  int count = 0;
  for (int j = MAX_NTTYS; j < enmax; j++) {
    if (cd[j].et >= 0 && cd[j].et < max_enemy && mark[cd[j].et] &&
        cd[j].type == CT_NOEXIST && cd[j].ec == 0) {
      cd[j].wt = fsps(80, ed_o[cd[j].et].apt, 900000);
      cd[j].ec = 1;
      count++;
    }
  }
  lua_pushinteger(L, count);
  return 1;
}

static int l_log(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  char buf[512];
  snprintf(buf, sizeof(buf), "class    : %s", s);
  log_print(buf);
  return 0;
}

static int l_chain(lua_State *L)
{
  int num = (int)luaL_checkinteger(L, 1);
  if (cls_cur >= 0) {
    /* the same assignment switch actions 9/19 make, with no wait */
    cd[cls_cur].anmf = num;
    cd[cls_cur].anmt = 0;
  }
  return 0;
}

/* Resolve a switch variable name ("xnn" / "ynn" / "znn", hex index)
 * through varno_load(), the same parser the switch engine uses,
 * with the per-series bounds the expression parser applies (a name
 * whose index spills into the next series is rejected). Returns the
 * unified variable index for var_value / var_set_value, or -1. */
static int cls_var_index(const char *name)
{
  if (!name)
    return -1;
  char c = name[0];
  if (c >= 'A' && c <= 'Z')
    c = (char)(c + 'a' - 'A');
  if (c != 'x' && c != 'y' && c != 'z')
    return -1;
  if (name[1] == '\0')
    return -1;
  for (const char *p = name + 1; *p; p++) {
    char h = *p;
    if (!((h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') ||
          (h >= 'A' && h <= 'F')))
      return -1;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%s", name);
  char *p = buf;
  int idx = varno_load(&p);
  if (idx < 0)
    return -1;
  if (c == 'x' && idx >= GVAR_MAX)
    return -1;
  if (c == 'y' && (idx < GVAR_MAX || idx >= GVAR_MAX + PVAR_MAX))
    return -1;
  if (c == 'z' &&
      (idx < GVAR_MAX + PVAR_MAX || idx >= GVAR_MAX + PVAR_MAX + SVAR_MAX))
    return -1;
  return idx;
}

static int l_get_var(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  int idx = cls_var_index(name);
  if (idx < 0 || cls_cur < 0) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushinteger(L, var_value(cls_cur, idx));
  return 1;
}

static int l_set_var(lua_State *L)
{
  const char *name = luaL_checkstring(L, 1);
  lua_Integer v = luaL_checkinteger(L, 2);
  int idx = cls_var_index(name);
  int ok = 0;
  if (idx >= 0 && cls_cur >= 0) {
    var_set_value(cls_cur, idx, (int)v);
    ok = 1;
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_get_str(lua_State *L)
{
  const char *key = luaL_checkstring(L, 1);
  if (cls_cur < 0 || cd[cls_cur].uid[0] == '\0') {
    lua_pushnil(L);
    return 1;
  }
  auto uit = qstr_store.find(cd[cls_cur].uid);
  if (uit == qstr_store.end()) {
    lua_pushnil(L);
    return 1;
  }
  auto kit = uit->second.find(key);
  if (kit == uit->second.end()) {
    lua_pushnil(L);
    return 1;
  }
  lua_pushstring(L, kit->second.c_str());
  return 1;
}

static int l_set_str(lua_State *L)
{
  const char *key = luaL_checkstring(L, 1);
  const char *val = luaL_checkstring(L, 2);
  int ok = 0;
  if (cls_cur >= 0 && cd[cls_cur].uid[0] != '\0' && qstr_key_ok(key)) {
    auto &rec = qstr_store[cd[cls_cur].uid];
    if (rec.count(key) || (int)rec.size() < QSTR_KEYS_MAX) {
      std::string v(val);
      std::size_t cut = v.find_first_of("\r\n");
      if (cut != std::string::npos)
        v.resize(cut);
      if (v.size() > QSTR_VAL_MAX)
        v.resize(QSTR_VAL_MAX);
      rec[key] = v;
      ok = 1;
    }
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_del_str(lua_State *L)
{
  const char *key = luaL_checkstring(L, 1);
  int ok = 0;
  if (cls_cur >= 0 && cd[cls_cur].uid[0] != '\0') {
    auto uit = qstr_store.find(cd[cls_cur].uid);
    if (uit != qstr_store.end())
      ok = uit->second.erase(key) ? 1 : 0;
  }
  lua_pushboolean(L, ok);
  return 1;
}

/* ---------------- dm.pet_* API (pet system, PHIDM NEO extension) ----
 * The pet core lives at the bottom of this file; these glue functions
 * only use its public entry points (phi_class.h). All of them act on
 * the player who fired the switch / whose on_second is running. */

static const char *pet_lua_typename(int t)
{
  switch (t) {
  case 1: return "fairy";
  case 2: return "cat";
  case 3: return "dog";
  case 4: return "bird";
  }
  return nullptr;
}

static int l_pet_type(lua_State *L)
{
  const char *s = (cls_cur >= 0) ? pet_lua_typename(pet_type(cls_cur))
                                 : nullptr;
  if (s)
    lua_pushstring(L, s);
  else
    lua_pushnil(L);
  return 1;
}

static int l_pet_name(lua_State *L)
{
  const char *s = (cls_cur >= 0) ? pet_name_of(cls_cur) : "";
  if (s && *s)
    lua_pushstring(L, s);
  else
    lua_pushnil(L);
  return 1;
}

static int l_pet_active(lua_State *L)
{
  lua_pushboolean(L, (cls_cur >= 0 && pet_active(cls_cur)) ? 1 : 0);
  return 1;
}

static int l_pet_grant(lua_State *L)
{
  const char *s = luaL_checkstring(L, 1);
  int t = 0;
  for (int i = 1; i <= 4; i++)
    if (std::strcmp(s, pet_lua_typename(i)) == 0)
      t = i;
  lua_pushboolean(L, (cls_cur >= 0 && t > 0
                      && pet_grant(cls_cur, t)) ? 1 : 0);
  return 1;
}

static int l_pet_release(lua_State *L)
{
  lua_pushboolean(L, (cls_cur >= 0 && pet_release_now(cls_cur)) ? 1 : 0);
  return 1;
}

static int l_pet_restore(lua_State *L)
{
  lua_pushboolean(L, (cls_cur >= 0 && pet_restore(cls_cur)) ? 1 : 0);
  return 1;
}

static int l_print(lua_State *L)
{
  std::string out("class    : print: ");
  int n = lua_gettop(L);
  for (int i = 1; i <= n; i++) {
    const char *s = lua_tostring(L, i);
    if (i > 1)
      out += "\t";
    out += s ? s : "(nil)";
  }
  std::vector<char> buf(out.size() + 1);
  std::memcpy(buf.data(), out.c_str(), out.size() + 1);
  log_print(buf.data());
  return 0;
}

/* ---------------- dm.fx.* API (DM FX, PHIDM NEO extension) ----------------
 * Each function sends one #fx line to the player who fired the switch
 * through the shared sender in phi_data.cpp, and returns whether that
 * player declared FX support (MD_FX). */

static int l_fx_text(lua_State *L)
{
  const char *pos = luaL_checkstring(L, 1);
  long size = (long) luaL_checkinteger(L, 2);
  long dur = (long) luaL_checkinteger(L, 3);
  const char *text = luaL_checkstring(L, 4);
  char buf[1024];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "text %s %ld %ld %s", pos, size, dur, text);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_textoff(lua_State *L)
{
  const char *pos = luaL_checkstring(L, 1);
  char buf[256];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "textoff %s", pos);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_dark(lua_State *L)
{
  long level = (long) luaL_checkinteger(L, 1);
  long dur = (long) luaL_checkinteger(L, 2);
  long mode = (long) luaL_checkinteger(L, 3);
  char buf[256];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "dark %ld %ld %ld", level, dur, mode);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_wipe(lua_State *L)
{
  const char *pattern = luaL_checkstring(L, 1);
  const char *dir = luaL_checkstring(L, 2);
  char buf[256];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "wipe %s %s", pattern, dir);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_img(lua_State *L)
{
  const char *where = luaL_checkstring(L, 1);
  const char *name = luaL_checkstring(L, 2);
  long dur = (long) luaL_checkinteger(L, 3);
  int ok = 0;
  if (cls_cur >= 0)
    ok = fx_send_img(cls_cur, where, name, dur);
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_shake(lua_State *L)
{
  long amp = (long) luaL_checkinteger(L, 1);
  long dur = (long) luaL_checkinteger(L, 2);
  char buf[256];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "shake %ld %ld", amp, dur);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static int l_fx_glow(lua_State *L)
{
  long dur = (long) luaL_checkinteger(L, 1);
  char buf[256];
  int ok = 0;
  if (cls_cur >= 0) {
    snprintf(buf, sizeof(buf), "glow %ld", dur);
    ok = fx_send_payload(cls_cur, buf);
  }
  lua_pushboolean(L, ok);
  return 1;
}

static const luaL_Reg fxlib[] = {
  {"text", l_fx_text},
  {"textoff", l_fx_textoff},
  {"dark", l_fx_dark},
  {"wipe", l_fx_wipe},
  {"img", l_fx_img},
  {"shake", l_fx_shake},
  {"glow", l_fx_glow},
  {nullptr, nullptr}
};

static const luaL_Reg dmlib[] = {
  {"msg", l_msg},
  {"teleport", l_teleport},
  {"get_cell", l_get_cell},
  {"set_cell", l_set_cell},
  {"rand", l_rand},
  {"rand_range", l_rand_range},
  {"give", l_give},
  {"has_item", l_has_item},
  {"player", l_player},
  {"spawn", l_spawn},
  {"log", l_log},
  {"chain", l_chain},
  {"get_var", l_get_var},
  {"set_var", l_set_var},
  {"get_str", l_get_str},
  {"set_str", l_set_str},
  {"del_str", l_del_str},
  {"pet_type", l_pet_type},
  {"pet_name", l_pet_name},
  {"pet_active", l_pet_active},
  {"pet_grant", l_pet_grant},
  {"pet_release", l_pet_release},
  {"pet_restore", l_pet_restore},
  {nullptr, nullptr}
};

/* ---------------- state management ---------------- */

void class_load(void)
{
  if (cls_L) {
    lua_close(cls_L);
    cls_L = nullptr;
  }
  cls_ready = 0;
  cls_has_tick = 0;
  cls_L = luaL_newstate();
  if (!cls_L) {
    log_print((char *)"class    : lua state creation failed");
    return;
  }
  static const luaL_Reg libs[] = {
    {"_G", luaopen_base},
    {"table", luaopen_table},
    {"string", luaopen_string},
    {"math", luaopen_math},
    {nullptr, nullptr}
  };
  for (const luaL_Reg *lib = libs; lib->func; lib++) {
    luaL_requiref(cls_L, lib->name, lib->func, 1);
    lua_pop(cls_L, 1);
  }
  /* the sandbox gets the DM API only: no file loading from scripts */
  lua_pushnil(cls_L);
  lua_setglobal(cls_L, "dofile");
  lua_pushnil(cls_L);
  lua_setglobal(cls_L, "loadfile");
  lua_pushnil(cls_L);
  lua_setglobal(cls_L, "load");
  lua_pushcfunction(cls_L, l_print);
  lua_setglobal(cls_L, "print");

  luaL_newlib(cls_L, dmlib);
  luaL_newlib(cls_L, fxlib);
  lua_setfield(cls_L, -2, "fx");
  lua_newtable(cls_L);
  lua_setfield(cls_L, -2, "vars");
  lua_setglobal(cls_L, "dm");

  /* The class file is phi.cls (renamed from phi.class in v1.8: the
   * .class extension is hard to open on macOS). Phi.cls and the old
   * phi.class are accepted as fallbacks. */
  static const char *cls_files[] = {"phi.cls", "Phi.cls", "phi.class"};
  const char *cls_name = nullptr;
  int rc = LUA_ERRFILE;
  for (int i = 0; i < 3; i++) {
    rc = luaL_loadfile(cls_L, cls_files[i]);
    if (rc != LUA_ERRFILE) {
      cls_name = cls_files[i];
      break;
    }
    lua_pop(cls_L, 1);
  }
  if (rc == LUA_ERRFILE) {
    log_print((char *)"class    : phi.cls not found"
                      " (class switches are no-ops)");
    return;
  }
  if (rc != LUA_OK) {
    cls_log_error("phi.cls load failed");
    lua_pop(cls_L, 1);
    return;
  }
  cls_budget = 100;
  lua_sethook(cls_L, cls_hook, LUA_MASKCOUNT, 10000);
  rc = lua_pcall(cls_L, 0, 0, 0);
  lua_sethook(cls_L, nullptr, 0, 0);
  if (rc != LUA_OK) {
    cls_log_error("phi.cls top-level error");
    lua_pop(cls_L, 1);
    return;
  }
  cls_ready = 1;
  lua_getglobal(cls_L, "on_second");
  cls_has_tick = lua_isfunction(cls_L, -1) ? 1 : 0;
  lua_pop(cls_L, 1);
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "class    : %s loaded", cls_name);
    log_print(buf);
  }
}

/* Run one phi.cls global function for player n with a fresh context
 * table. Missing programs are logged only when log_missing is set
 * (the per-second tick stays silent about a missing on_second; the
 * cls_has_tick flag normally keeps it from being called at all). */
static void cls_run(int n, const char *prog, int sx, int sy,
                    int log_missing)
{
  lua_getglobal(cls_L, prog);
  if (!lua_isfunction(cls_L, -1)) {
    if (log_missing) {
      char buf[256];
      snprintf(buf, sizeof(buf),
                    "class    : program not found: %s", prog);
      log_print(buf);
    }
    lua_pop(cls_L, 1);
    return;
  }
  lua_newtable(cls_L);
  lua_pushinteger(cls_L, n);
  lua_setfield(cls_L, -2, "player");
  lua_pushstring(cls_L, cd[n].name);
  lua_setfield(cls_L, -2, "name");
  lua_pushinteger(cls_L, cd[n].x);
  lua_setfield(cls_L, -2, "x");
  lua_pushinteger(cls_L, cd[n].y);
  lua_setfield(cls_L, -2, "y");
  lua_pushinteger(cls_L, sx);
  lua_setfield(cls_L, -2, "sx");
  lua_pushinteger(cls_L, sy);
  lua_setfield(cls_L, -2, "sy");

  cls_cur = n;
  cls_budget = 100;
  lua_sethook(cls_L, cls_hook, LUA_MASKCOUNT, 10000);
  int rc = lua_pcall(cls_L, 1, 0, 0);
  lua_sethook(cls_L, nullptr, 0, 0);
  cls_cur = -1;
  if (rc != LUA_OK) {
    char buf[256];
    snprintf(buf, sizeof(buf), "program %s failed", prog);
    cls_log_error(buf);
    lua_pop(cls_L, 1);
  }
}

void class_call(int n, const char *prog, int sx, int sy)
{
  if (n < 0 || n >= MAX_NTTYS)
    return; /* programs run for players only (same guard as hall) */
  if (!prog)
    return;
  if (!cls_L)
    class_load();
  if (!cls_L || !cls_ready)
    return;
  cls_run(n, prog, sx, sy, 1);
}

void class_tick(int n)
{
  if (n < 0 || n >= MAX_NTTYS)
    return; /* players only, same guard as class_call */
  if (!cls_L || !cls_ready || !cls_has_tick)
    return;
  cls_run(n, "on_second", cd[n].x, cd[n].y, 0);
}

/* ---------------- job / gender system (phi.jb) ----------------
 * PHIDM NEO extension: a light, title-like job (shokugyou) and
 * gender attribute for player characters, deliberately much smaller
 * than JET's phi.ec rule engine. The system is active only in
 * worlds that ship a phi.jb definition file; jb_load() is called
 * once at startup and, without the file, every entry point below
 * is inert (no buffs, condition/action 80 never fire, %job/%sex
 * expand to nothing), so worlds without phi.jb behave exactly as
 * before.
 *
 * Storage: two ChrEx properties on the character itself, "job:<n>"
 * and "sex:<n>". They ride the ordinary phi.cd ":?" extension
 * lines, so the phi.cd format does not change, old character data
 * loads unchanged (no property = unset = no effect), and the
 * values follow the character across gates. The buffs apply only
 * while the world the character is currently in has phi.jb.
 *
 * phi.jb format (one directive per line; unknown lines ignored):
 *   :N <job 1-9> <name>        rename a job
 *   :R <job 1-9> <r0> [<r1>]   override effect rates (see below)
 *   :S <sex 1-2> <name>        rename a gender
 * Effect slots per job (a rate is in the effect's own unit):
 *   1 warrior   [0] physical damage +%        base 10
 *   2 priest    [0] healing +%                base 20
 *   3 fighter   [0] critical chance +points   base 5
 *   4 wizard    [0] magic damage +%           base 10
 *   5 merchant  [0] GP income +%              base 20
 *   6 thief     [0] attack weight -%          base 10
 *   7 sage      [0] healing +% base 10, [1] magic damage +% base 5
 *   8 beastkin  [0] df_rate +points           base 5
 *   9 demon     [0] mr_rate +points           base 5
 * A world may set each rate freely from 0 up to 150% of the base
 * rate; the engine clamps anything above the cap down to it (and
 * negative values to 0). Effects without an override use the base
 * rate. Gender has no numeric effect at all: it exists for display
 * (%sex), switch condition 80 tests and world storytelling.
 * Setting is switch action 80, testing is switch condition 80;
 * both take "pp vv" (pp 00 = gender, 01 = job; vv 0 = unset). */

#define JB_MAX_JOB 9

static const char *jb_base_name[JB_MAX_JOB + 1] = {
  "", "戦士", "僧侶", "武闘家", "魔法使い",
  "商人", "盗賊", "賢者", "獣人", "魔族"
};
static const int jb_base_rate[JB_MAX_JOB + 1][2] = {
  {0, 0}, {10, 0}, {20, 0}, {5, 0}, {10, 0},
  {20, 0}, {10, 0}, {10, 5}, {5, 0}, {5, 0}
};
static const int jb_slots[JB_MAX_JOB + 1] = {
  0, 1, 1, 1, 1, 1, 1, 2, 1, 1
};
static const char *jb_base_sexname[3] = {"", "男", "女"};

static int jb_on = 0;
static char jb_name[JB_MAX_JOB + 1][64];
static int jb_rate_tab[JB_MAX_JOB + 1][2];
static char jb_sexname[3][64];

int jb_enabled(void)
{
  return jb_on;
}

int jb_rate(int job, int slot)
{
  if (!jb_on || job < 1 || job > JB_MAX_JOB || slot < 0 || slot > 1)
    return 0;
  return jb_rate_tab[job][slot];
}

/* --- ChrEx property access (the phi.cd ":?" extension lines) --- */

static int jb_chrex_get(int n, const char *key)
{
  if (n < 0 || n >= MAX_NTTYS + MAX_ENEMIES)
    return 0;
  std::size_t klen = std::strlen(key);
  for (CHRExData *e = cd[n].extension; e != nullptr; e = e->next) {
    if (e->ext_data != nullptr
        && std::strncmp(e->ext_data, key, klen) == 0
        && e->ext_data[klen] == ':')
      return std::atoi(e->ext_data + klen + 1);
  }
  return 0;
}

static void jb_chrex_set(int n, const char *key, int v)
{
  if (n < 0 || n >= MAX_NTTYS + MAX_ENEMIES)
    return;
  std::size_t klen = std::strlen(key);
  CHRExData **pp = &cd[n].extension;
  while (*pp != nullptr) {
    if ((*pp)->ext_data != nullptr
        && std::strncmp((*pp)->ext_data, key, klen) == 0
        && (*pp)->ext_data[klen] == ':') {
      if (v == 0) {  /* unset: drop the property line entirely */
        CHRExData *dead = *pp;
        *pp = dead->next;
        std::free(dead->ext_data);
        std::free(dead);
      } else {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%s:%i", key, v);
        char *dup = strdup(buf);
        if (dup != nullptr) {
          std::free((*pp)->ext_data);
          (*pp)->ext_data = dup;
        }
      }
      return;
    }
    pp = &(*pp)->next;
  }
  if (v == 0)
    return;
  /* append a new entry, same allocation style as load_chrex() */
  CHRExData *e = (CHRExData *)std::malloc(sizeof(CHRExData));
  if (e == nullptr)
    return;
  char buf[40];
  std::snprintf(buf, sizeof(buf), "%s:%i", key, v);
  e->ext_data = strdup(buf);
  if (e->ext_data == nullptr) {
    std::free(e);
    return;
  }
  e->next = nullptr;
  *pp = e;
}

int jb_job(int n)
{
  int v = jb_chrex_get(n, "job");
  return (v >= 1 && v <= JB_MAX_JOB) ? v : 0;
}

int jb_sex(int n)
{
  int v = jb_chrex_get(n, "sex");
  return (v >= 1 && v <= 2) ? v : 0;
}

void jb_set_job(int n, int v)
{
  if (!jb_on)
    return;
  if (v < 0 || v > JB_MAX_JOB)
    v = 0;
  jb_chrex_set(n, "job", v);
  jb_send_self(n);
  jb_send_chr(n);
}

void jb_set_sex(int n, int v)
{
  if (!jb_on)
    return;
  if (v < 0 || v > 2)
    v = 0;
  jb_chrex_set(n, "sex", v);
  jb_send_self(n);
  jb_send_chr(n);
}

const char *jb_job_name(int job)
{
  if (!jb_on || job < 1 || job > JB_MAX_JOB)
    return "";
  return jb_name[job];
}

const char *jb_sex_name(int sex)
{
  if (!jb_on || sex < 1 || sex > 2)
    return "";
  return jb_sexname[sex];
}

/* Healing bonus rate of character n (priest slot 0, sage slot 0). */
int jb_heal_rate(int n)
{
  int j = jb_job(n);
  if (j == 2 || j == 7)
    return jb_rate(j, 0);
  return 0;
}

/* Magic damage bonus rate of character n (wizard slot 0, sage 1). */
int jb_matk_rate(int n)
{
  int j = jb_job(n);
  if (j == 4)
    return jb_rate(4, 0);
  if (j == 7)
    return jb_rate(7, 1);
  return 0;
}

/* A GP income amount with the merchant bonus applied. Applied at
 * the moments GP flows in as earnings: ground money pickup, jewel
 * exchange and robbery. Switch-spawned GP is deliberately not
 * routed through here at its spawn point (it is paid from tax by
 * the world), but ground money is indistinguishable at pickup
 * time, so spawned money picked up off the ground is bonused too. */
int jb_gp_gain(int n, int gp)
{
  if (gp > 0 && jb_job(n) == 5)
    return gp * (100 + jb_rate(5, 0)) / 100;
  return gp;
}

void jb_load(void)
{
  jb_on = 0;
  for (int j = 0; j <= JB_MAX_JOB; j++) {
    std::snprintf(jb_name[j], sizeof(jb_name[j]), "%s", jb_base_name[j]);
    jb_rate_tab[j][0] = jb_base_rate[j][0];
    jb_rate_tab[j][1] = jb_base_rate[j][1];
  }
  for (int s = 0; s < 3; s++)
    std::snprintf(jb_sexname[s], sizeof(jb_sexname[s]), "%s",
                  jb_base_sexname[s]);
  FILE *fp = efopen((char *)"phi.jb", (char *)"rb");
  if (!fp) {
    log_print((char *)"job      : phi.jb not found"
                      " (job/gender system disabled)");
    return;
  }
  jb_on = 1;
  char buf[256];
  int names = 0, rates = 0, clamped = 0;
  while (std::fgets(buf, sizeof(buf), fp)) {
    std::size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
      buf[--len] = '\0';
    if (buf[0] != ':')
      continue;
    char kind = buf[1];
    if (kind != 'N' && kind != 'R' && kind != 'S')
      continue;
    char *p = buf + 2;
    while (*p == ' ')
      p++;
    char *end = nullptr;
    long no = std::strtol(p, &end, 10);
    if (end == p)
      continue;
    p = end;
    if (kind == 'R') {
      if (no < 1 || no > JB_MAX_JOB)
        continue;
      for (int slot = 0; slot < jb_slots[no]; slot++) {
        while (*p == ' ')
          p++;
        if (*p == '\0')
          break;
        long v = std::strtol(p, &end, 10);
        if (end == p)
          break;
        p = end;
        int cap = jb_base_rate[no][slot] * 3 / 2;
        if (v > cap) {
          v = cap;
          clamped++;
        }
        if (v < 0)
          v = 0;
        jb_rate_tab[no][slot] = (int)v;
        rates++;
      }
    } else {
      /* name directive: the rest of the line is the name */
      while (*p == ' ')
        p++;
      char *e = p + std::strlen(p);
      while (e > p && e[-1] == ' ')
        *--e = '\0';
      if (*p == '\0')
        continue;
      if (kind == 'N' && no >= 1 && no <= JB_MAX_JOB) {
        std::snprintf(jb_name[no], sizeof(jb_name[no]), "%s", p);
        names++;
      } else if (kind == 'S' && no >= 1 && no <= 2) {
        std::snprintf(jb_sexname[no], sizeof(jb_sexname[no]), "%s", p);
        names++;
      }
    }
  }
  efclose(fp);
  {
    char lbuf[160];
    std::snprintf(lbuf, sizeof(lbuf),
                  "job      : phi.jb loaded"
                  " (%i names, %i rates, %i clamped to cap)",
                  names, rates, clamped);
    log_print(lbuf);
  }
}

/* ---------------- pet system (phi.pet / phi.pt) ----------------
 * PHIDM NEO extension: a permanent companion for player characters.
 * The pet has no map entity: the server keeps only its state (type,
 * name, two action pools) and applies its support effects, while
 * the following picture is drawn by the client (NeoPHI, #ex-pet).
 * Behaviour is a fixed per-type pattern reacting to the owner:
 * healers watch the owner's HP (per-second tick plus an emergency
 * reaction inside the hp_dmg() hook), attackers add a follow-up
 * blow inside attack_to() after the owner's hit lands. Pets never
 * die; acting drains the MP / stamina pools, and an empty pool
 * means the pet rests until the pool refills.
 *
 * Persistence mirrors phi.qst: a uid-keyed file "phi.pet" with
 * ":<uid>" / ":P <item> <value>" / "." records, loaded at startup
 * and written back on logout and gate transfer. phi.cd is not
 * touched, and pets do not follow the character across gates
 * (each world keeps its own phi.pet). Tuning lives in "phi.pt"
 * (":P <type> <key> <value>" lines); without the file the design
 * defaults apply -- the system is not gated on phi.pt because a
 * pet can only come into being through switch action 92 or the
 * dm.pet_* Lua API, both explicit world-side acts.
 *
 * Engine caps (design decision): proc rates <= 50%, healing
 * <= 25% of the owner's max HP, and for physical follow-ups
 * rate x per-hit share <= 2500 (an expected share of the owner's
 * damage of at most 25%). The physical follow-up is a share of
 * the owner's own final damage of that hit, so the Flv multiplier
 * and the df reduction are already inside the number it scales;
 * the magic follow-up applies the Wlv multiplier and the target's
 * mr_rate to its design-formula unit, like mg_attack_to() does. */

#define PET_FAIRY 1
#define PET_CAT   2
#define PET_DOG   3
#define PET_BIRD  4
#define PET_NAME_MAX 20 /* bytes, CP932 */

struct PetCfg {
  int heal_pct;   /* heal amount, % of owner max HP (0 = no heal) */
  int heal_thr;   /* tick heal triggers below this HP % */
  int heal_cost;  /* MP per tick heal */
  int emg_thr;    /* emergency heal threshold HP % (0 = none) */
  int emg_cost;   /* MP per emergency heal */
  int emg_cd;     /* emergency heal cooldown, seconds */
  int phys_rate;  /* physical follow-up proc % */
  int phys_pct;   /* % of the owner's final hit damage per proc */
  int phys_cost;  /* stamina per proc (once per attack command) */
  int mag_rate;   /* magic follow-up proc % */
  int mag_pct;    /* % of the magic unit amount */
  int mag_cost;   /* MP per proc (once per attack command) */
  int sup_rate;   /* bird support: proc % per interval */
  int sup_pct;    /* % of the magic unit amount */
  int sup_cost;   /* MP per support cast */
  int sup_int;    /* support interval, seconds */
};

static PetCfg pet_cfg[5];
static int pet_mp_max = 100;
static int pet_st_max = 100;
static int pet_regen_out = 3; /* per second, out of combat */
static int pet_regen_in = 1;  /* per second, in combat */

static const char *pet_type_id[5] = {"", "fairy", "cat", "dog", "bird"};
static const char *pet_type_disp[5] = {"", "妖精", "猫", "犬", "鳥"};
static const char *pet_def_name[5] = {"", "ようせい", "ねこ", "いぬ", "とり"};

struct PetRec {
  int type = 0;
  std::string name;
  int mp = 0;
  int st = 0;
};

static const char *pet_file = "phi.pet";
static std::map<std::string, PetRec> pet_store;
static int pet_loaded = 0;

/* Per-slot runtime state for online characters. The persistent
 * part (type/name/mp/st) is mirrored into pet_store by
 * pet_sync_rt(); the rest is session-only by design (combat timer,
 * last target, cooldowns, per-command budget, message edges). */
struct PetRT {
  int loaded = 0;
  std::string uid;
  int type = 0;
  std::string name;
  int mp = 0;
  int st = 0;
  long last_combat = 0;  /* last attack / battle damage, epoch sec */
  long last_support = 0; /* last bird support cast */
  long emg_ready = 0;    /* emergency heal ready at this time */
  int last_target = -1;  /* owner's most recent attack target */
  int spent_phys = 0;    /* pool already charged for this command */
  int spent_mag = 0;
  int tired_mp = 0;      /* "ran dry" already announced */
  int tired_st = 0;
  int pending = 0;       /* naming in progress: 1 grant, 2 rename */
};

static PetRT pet_rt[MAX_NTTYS];

static int pet_name_valid(const char *s)
{
  std::size_t len = std::strlen(s);
  if (len < 1 || len > PET_NAME_MAX)
    return 0;
  for (std::size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c <= ' ' || c == 0x7f || c == '%')
      return 0;
  }
  return 1;
}

static void pet_sync_rt(PetRT *rt)
{
  if (rt->type > 0) {
    PetRec &rec = pet_store[rt->uid];
    rec.type = rt->type;
    rec.name = rt->name;
    rec.mp = rt->mp;
    rec.st = rt->st;
  } else {
    pet_store.erase(rt->uid);
  }
}

/* The runtime record of the character in slot n, loaded from the
 * store on first use. Returns nullptr for non-players; the record
 * itself may still be empty (type == 0 = no pet). */
static PetRT *pet_rt_get(int n)
{
  if (n < 1 || n >= MAX_NTTYS)
    return nullptr;
  if (cd[n].type != CT_NORMAL || cd[n].uid[0] == '\0')
    return nullptr;
  PetRT &rt = pet_rt[n];
  if (rt.loaded && rt.uid == cd[n].uid)
    return &rt;
  rt = PetRT();
  rt.loaded = 1;
  rt.uid = cd[n].uid;
  rt.last_target = -1;
  auto it = pet_store.find(rt.uid);
  if (it != pet_store.end()) {
    rt.type = it->second.type;
    rt.name = it->second.name;
    rt.mp = it->second.mp;
    rt.st = it->second.st;
    if (rt.mp < 0)
      rt.mp = 0;
    if (rt.mp > pet_mp_max)
      rt.mp = pet_mp_max;
    if (rt.st < 0)
      rt.st = 0;
    if (rt.st > pet_st_max)
      rt.st = pet_st_max;
    if (!pet_name_valid(rt.name.c_str()))
      rt.name = pet_def_name[rt.type];
  }
  return &rt;
}

static void pet_tell(int n, const char *fmt, const char *arg)
{
  std::array<char, 512> buf;
  dmprintf(n, 0, buf.data(), (int)buf.size(), (char *)fmt, (char *)arg);
  check_write(n, buf.data());
}

/* A pool was charged; announce the moment it runs dry. */
static void pet_pool_spent(int n, PetRT *rt, int is_mp)
{
  if (is_mp) {
    if (rt->mp <= 0 && !rt->tired_mp) {
      rt->tired_mp = 1;
      pet_tell(n, DMM_PET_TIRED_MP, rt->name.c_str());
      pet_send_self(n);
    }
  } else {
    if (rt->st <= 0 && !rt->tired_st) {
      rt->tired_st = 1;
      pet_tell(n, DMM_PET_TIRED_ST, rt->name.c_str());
      pet_send_self(n);
    }
  }
}

/* Notify the owner (once per pool) that the pet wanted to act but
 * its pool is below the action cost. The tired flag is cleared when
 * the pool refills completely in pet_tick. */
static void pet_note_low(int n, PetRT *rt, int is_mp)
{
  if (is_mp) {
    if (!rt->tired_mp) {
      rt->tired_mp = 1;
      pet_tell(n, DMM_PET_TIRED_MP, rt->name.c_str());
      pet_send_self(n);
    }
  } else {
    if (!rt->tired_st) {
      rt->tired_st = 1;
      pet_tell(n, DMM_PET_TIRED_ST, rt->name.c_str());
      pet_send_self(n);
    }
  }
}

int pet_type(int n)
{
  PetRT *rt = pet_rt_get(n);
  return rt ? rt->type : 0;
}

const char *pet_name_of(int n)
{
  PetRT *rt = pet_rt_get(n);
  return (rt && rt->type > 0) ? rt->name.c_str() : "";
}

/* "Can act" for condition 92: judged on the type's main pool
 * (dog = stamina, cat/bird = MP, fairy = either). */
int pet_active(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt || rt->type == 0)
    return 0;
  const PetCfg &cfg = pet_cfg[rt->type];
  switch (rt->type) {
  case PET_DOG:   /* main pool: stamina vs physical cost */
    return rt->st >= cfg.phys_cost;
  case PET_CAT:   /* main pool: MP vs heal cost */
    return rt->mp >= cfg.heal_cost;
  case PET_BIRD:  /* main pool: MP vs magic cost */
    return rt->mp >= cfg.mag_cost;
  default:        /* fairy: either pool can pay for an action */
    return rt->mp >= cfg.mag_cost || rt->st >= cfg.phys_cost;
  }
}

/* ---------------- #ex-pet frames ---------------- */

static void pet_chr_frame(int n, char *buf, std::size_t sz)
{
  PetRT *rt = pet_rt_get(n);
  if (rt && rt->type > 0)
    std::snprintf(buf, sz, "#ex-pet chr \"%s\" %s %s %i\n",
                  cd[n].name, pet_type_id[rt->type],
                  rt->name.c_str(), pet_active(n));
  else
    std::snprintf(buf, sz, "#ex-pet chr \"%s\" none - 0\n",
                  cd[n].name);
}

static int pet_near(int a, int b)
{
  if (map[cd[a].y][cd[a].x].area != map[cd[b].y][cd[b].x].area)
    return 0;
  int dx = cd[a].x - cd[b].x;
  int dy = cd[a].y - cd[b].y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  return dx <= 15 && dy <= 15;
}

void pet_send_self(int n)
{
  if (n < 1 || n >= MAX_NTTYS || !(cd[n].modef & MD_PET))
    return;
  PetRT *rt = pet_rt_get(n);
  char buf[256];
  if (rt && rt->type > 0)
    std::snprintf(buf, sizeof(buf), "#ex-pet self %s %s %i %i %i %i %i\n",
                  pet_type_id[rt->type], rt->name.c_str(),
                  rt->mp, rt->st, pet_active(n),
                  pet_mp_max, pet_st_max);
  else
    std::snprintf(buf, sizeof(buf), "#ex-pet self none - 0 0 0 %i %i\n",
                  pet_mp_max, pet_st_max);
  check_write(n, buf);
}

void pet_send_chr(int n)
{
  if (n < 1 || n >= MAX_NTTYS || cd[n].type != CT_NORMAL)
    return;
  char buf[256];
  pet_chr_frame(n, buf, sizeof(buf));
  for (int m = 1; m < MAX_NTTYS; m++) {
    if (m == n || cd[m].type != CT_NORMAL)
      continue;
    if (!(cd[m].modef & MD_PET))
      continue;
    if (pet_near(n, m))
      check_write(m, buf);
  }
}

void pet_send_chr_all(int n)
{
  if (n < 1 || n >= MAX_NTTYS || !(cd[n].modef & MD_PET))
    return;
  for (int o = 1; o < MAX_NTTYS; o++) {
    if (o == n || cd[o].type != CT_NORMAL)
      continue;
    if (pet_type(o) == 0)
      continue;
    if (!pet_near(n, o))
      continue;
    char buf[256];
    pet_chr_frame(o, buf, sizeof(buf));
    check_write(n, buf);
  }
}

/* ---------------- #ex-job frames ----------------
 * Job / gender display for MD_JOB clients, modeled on #ex-pet.
 * Names are quoted (phi.jb names are world-defined text). In a
 * world without phi.jb, jb_job_name()/jb_sex_name() return "",
 * so the frames carry empty names and clients show nothing. */

static void jb_chr_frame(int n, char *buf, std::size_t sz)
{
  std::snprintf(buf, sz, "#ex-job chr \"%s\" \"%s\" \"%s\"\n",
                cd[n].name, jb_job_name(jb_job(n)),
                jb_sex_name(jb_sex(n)));
}

void jb_send_self(int n)
{
  if (n < 1 || n >= MAX_NTTYS || !(cd[n].modef & MD_JOB))
    return;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "#ex-job self \"%s\" \"%s\"\n",
                jb_job_name(jb_job(n)), jb_sex_name(jb_sex(n)));
  check_write(n, buf);
}

void jb_send_chr(int n)
{
  if (n < 1 || n >= MAX_NTTYS || cd[n].type != CT_NORMAL)
    return;
  char buf[256];
  jb_chr_frame(n, buf, sizeof(buf));
  for (int m = 1; m < MAX_NTTYS; m++) {
    if (m == n || cd[m].type != CT_NORMAL)
      continue;
    if (!(cd[m].modef & MD_JOB))
      continue;
    if (pet_near(n, m))
      check_write(m, buf);
  }
}

void jb_send_chr_all(int n)
{
  if (n < 1 || n >= MAX_NTTYS || !(cd[n].modef & MD_JOB))
    return;
  for (int o = 1; o < MAX_NTTYS; o++) {
    if (o == n || cd[o].type != CT_NORMAL)
      continue;
    if (!pet_near(n, o))
      continue;
    char buf[256];
    jb_chr_frame(o, buf, sizeof(buf));
    check_write(n, buf);
  }
}

/* ---------------- phi.pt tuning ---------------- */

static void pet_cfg_defaults(void)
{
  for (int t = 0; t <= 4; t++)
    pet_cfg[t] = PetCfg();
  /* fairy: all-rounder, everything at 70% of the specialist */
  pet_cfg[PET_FAIRY].heal_pct = 5;
  pet_cfg[PET_FAIRY].heal_thr = 60;
  pet_cfg[PET_FAIRY].heal_cost = 15;
  pet_cfg[PET_FAIRY].phys_rate = 20;
  pet_cfg[PET_FAIRY].phys_pct = 35;
  pet_cfg[PET_FAIRY].phys_cost = 10;
  pet_cfg[PET_FAIRY].mag_rate = 20;
  pet_cfg[PET_FAIRY].mag_pct = 70;
  pet_cfg[PET_FAIRY].mag_cost = 12;
  /* cat: healer, plus a weak magic follow-up at half the bird */
  pet_cfg[PET_CAT].heal_pct = 10;
  pet_cfg[PET_CAT].heal_thr = 50;
  pet_cfg[PET_CAT].heal_cost = 20;
  pet_cfg[PET_CAT].emg_thr = 25;
  pet_cfg[PET_CAT].emg_cost = 30;
  pet_cfg[PET_CAT].emg_cd = 10;
  pet_cfg[PET_CAT].mag_rate = 15;
  pet_cfg[PET_CAT].mag_pct = 50;
  pet_cfg[PET_CAT].mag_cost = 12;
  /* dog: physical follow-up, 35% x 50% = 17.5% expected share */
  pet_cfg[PET_DOG].phys_rate = 35;
  pet_cfg[PET_DOG].phys_pct = 50;
  pet_cfg[PET_DOG].phys_cost = 10;
  /* bird: magic follow-up plus an interval support cast */
  pet_cfg[PET_BIRD].mag_rate = 35;
  pet_cfg[PET_BIRD].mag_pct = 100;
  pet_cfg[PET_BIRD].mag_cost = 12;
  pet_cfg[PET_BIRD].sup_rate = 50;
  pet_cfg[PET_BIRD].sup_pct = 50;
  pet_cfg[PET_BIRD].sup_cost = 15;
  pet_cfg[PET_BIRD].sup_int = 5;
  pet_mp_max = 100;
  pet_st_max = 100;
  pet_regen_out = 3;
  pet_regen_in = 1;
}

static int pet_clamp(int v, int lo, int hi, int *clamped)
{
  if (v < lo) {
    (*clamped)++;
    return lo;
  }
  if (v > hi) {
    (*clamped)++;
    return hi;
  }
  return v;
}

/* Engine caps, applied to defaults and phi.pt values alike. */
static void pet_cfg_caps(int *clamped)
{
  pet_mp_max = pet_clamp(pet_mp_max, 1, 100000, clamped);
  pet_st_max = pet_clamp(pet_st_max, 1, 100000, clamped);
  pet_regen_out = pet_clamp(pet_regen_out, 0, 1000, clamped);
  pet_regen_in = pet_clamp(pet_regen_in, 0, 1000, clamped);
  for (int t = 1; t <= 4; t++) {
    PetCfg &c = pet_cfg[t];
    c.heal_pct = pet_clamp(c.heal_pct, 0, 25, clamped);
    c.heal_thr = pet_clamp(c.heal_thr, 0, 99, clamped);
    c.heal_cost = pet_clamp(c.heal_cost, 0, 1000, clamped);
    c.emg_thr = pet_clamp(c.emg_thr, 0, 99, clamped);
    c.emg_cost = pet_clamp(c.emg_cost, 0, 1000, clamped);
    c.emg_cd = pet_clamp(c.emg_cd, 0, 3600, clamped);
    c.phys_rate = pet_clamp(c.phys_rate, 0, 50, clamped);
    c.phys_pct = pet_clamp(c.phys_pct, 0, 100, clamped);
    if (c.phys_rate > 0 && c.phys_rate * c.phys_pct > 2500) {
      c.phys_pct = 2500 / c.phys_rate; /* expected share <= 25% */
      (*clamped)++;
    }
    c.phys_cost = pet_clamp(c.phys_cost, 0, 1000, clamped);
    c.mag_rate = pet_clamp(c.mag_rate, 0, 50, clamped);
    c.mag_pct = pet_clamp(c.mag_pct, 0, 100, clamped);
    c.mag_cost = pet_clamp(c.mag_cost, 0, 1000, clamped);
    c.sup_rate = pet_clamp(c.sup_rate, 0, 50, clamped);
    c.sup_pct = pet_clamp(c.sup_pct, 0, 100, clamped);
    c.sup_cost = pet_clamp(c.sup_cost, 0, 1000, clamped);
    if (c.sup_rate > 0)
      c.sup_int = pet_clamp(c.sup_int, 1, 3600, clamped);
  }
}

static int *pet_cfg_field(PetCfg &c, const char *key)
{
  struct KeyMap { const char *name; int PetCfg::*field; };
  static const KeyMap keys[] = {
    {"heal_pct", &PetCfg::heal_pct}, {"heal_thr", &PetCfg::heal_thr},
    {"heal_cost", &PetCfg::heal_cost}, {"emg_thr", &PetCfg::emg_thr},
    {"emg_cost", &PetCfg::emg_cost}, {"emg_cd", &PetCfg::emg_cd},
    {"phys_rate", &PetCfg::phys_rate}, {"phys_pct", &PetCfg::phys_pct},
    {"phys_cost", &PetCfg::phys_cost}, {"mag_rate", &PetCfg::mag_rate},
    {"mag_pct", &PetCfg::mag_pct}, {"mag_cost", &PetCfg::mag_cost},
    {"sup_rate", &PetCfg::sup_rate}, {"sup_pct", &PetCfg::sup_pct},
    {"sup_cost", &PetCfg::sup_cost}, {"sup_int", &PetCfg::sup_int},
  };
  for (const KeyMap &k : keys)
    if (std::strcmp(key, k.name) == 0)
      return &(c.*(k.field));
  return nullptr;
}

static int pet_type_token(const char *s)
{
  if (std::strcmp(s, "0") == 0 || std::strcmp(s, "all") == 0)
    return 0;
  for (int t = 1; t <= 4; t++)
    if (std::strcmp(s, pet_type_id[t]) == 0)
      return t;
  if (s[0] >= '1' && s[0] <= '4' && s[1] == '\0')
    return s[0] - '0';
  return -1;
}

static void pet_pt_load(int *clamped)
{
  FILE *fp = efopen((char *)"phi.pt", (char *)"rb");
  if (!fp) {
    log_print((char *)"pet      : phi.pt not found"
                      " (design defaults apply)");
    return;
  }
  char buf[256];
  int values = 0;
  while (std::fgets(buf, sizeof(buf), fp)) {
    std::size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
      buf[--len] = '\0';
    if (buf[0] != ':' || buf[1] != 'P')
      continue;
    char type_s[32], key[32];
    long val;
    if (std::sscanf(buf + 2, "%31s %31s %ld", type_s, key, &val) != 3)
      continue;
    int t = pet_type_token(type_s);
    if (t < 0)
      continue;
    if (t == 0) {
      if (std::strcmp(key, "mp_max") == 0) {
        pet_mp_max = (int)val;
        values++;
      } else if (std::strcmp(key, "st_max") == 0) {
        pet_st_max = (int)val;
        values++;
      } else if (std::strcmp(key, "regen_out") == 0) {
        pet_regen_out = (int)val;
        values++;
      } else if (std::strcmp(key, "regen_in") == 0) {
        pet_regen_in = (int)val;
        values++;
      }
      continue;
    }
    int *field = pet_cfg_field(pet_cfg[t], key);
    if (field) {
      *field = (int)val;
      values++;
    }
  }
  efclose(fp);
  {
    char lbuf[128];
    std::snprintf(lbuf, sizeof(lbuf),
                  "pet      : phi.pt loaded (%i values)", values);
    log_print(lbuf);
  }
  (void)clamped;
}

/* ---------------- phi.pet records ---------------- */

void pet_load(void)
{
  pet_store.clear();
  pet_loaded = 1;
  pet_cfg_defaults();
  int clamped = 0;
  pet_pt_load(&clamped);
  pet_cfg_caps(&clamped);
  if (clamped > 0) {
    char lbuf[128];
    std::snprintf(lbuf, sizeof(lbuf),
                  "pet      : %i tuning values clamped to the caps",
                  clamped);
    log_print(lbuf);
  }
  FILE *fp = efopen((char *)pet_file, (char *)"rb");
  if (!fp) {
    log_print((char *)"pet      : phi.pet not found"
                      " (pet store starts empty)");
    return;
  }
  char buf[256];
  std::string uid;
  PetRec *rec = nullptr;
  int chars = 0;
  while (std::fgets(buf, sizeof(buf), fp)) {
    std::size_t len = std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
      buf[--len] = '\0';
    if (buf[0] == ':' && buf[1] == 'P' && buf[2] == ' ' && rec) {
      char item[32], value[128];
      if (std::sscanf(buf + 3, "%31s %127s", item, value) == 2) {
        if (std::strcmp(item, "type") == 0) {
          int t = pet_type_token(value);
          rec->type = (t >= 1) ? t : 0;
        } else if (std::strcmp(item, "name") == 0) {
          rec->name = value;
        } else if (std::strcmp(item, "mp") == 0) {
          rec->mp = std::atoi(value);
        } else if (std::strcmp(item, "st") == 0) {
          rec->st = std::atoi(value);
        }
      }
    } else if (buf[0] == ':') {
      uid = buf + 1;
      while (!uid.empty() && uid.back() == ' ')
        uid.pop_back();
      rec = nullptr;
      if (!uid.empty()) {
        rec = &pet_store[uid];
        chars++;
      }
    } else if (buf[0] == '.') {
      rec = nullptr;
      uid.clear();
    }
  }
  efclose(fp);
  /* drop records that failed to parse into a valid pet */
  for (auto it = pet_store.begin(); it != pet_store.end();) {
    if (it->second.type < 1 || it->second.type > 4) {
      it = pet_store.erase(it);
    } else {
      if (!pet_name_valid(it->second.name.c_str()))
        it->second.name = pet_def_name[it->second.type];
      it++;
    }
  }
  {
    char lbuf[128];
    std::snprintf(lbuf, sizeof(lbuf),
                  "pet      : phi.pet loaded (%i characters)", chars);
    log_print(lbuf);
  }
}

int pet_save(int n)
{
  if (n < 0 || n >= MAX_NTTYS)
    return 1;
  if (!pet_loaded || cd[n].uid[0] == '\0')
    return 1;
  PetRT &rt = pet_rt[n];
  if (rt.loaded && rt.uid == cd[n].uid)
    pet_sync_rt(&rt);
  /* A world that has never held a pet record stays untouched:
   * do not create an empty phi.pet there. (Once the file exists it
   * keeps being rewritten, so erasures persist.) */
  int any = 0;
  for (auto &rec : pet_store)
    if (rec.second.type >= 1)
      any = 1;
  if (!any) {
    FILE *probe = std::fopen(pet_file, "rb");
    if (!probe)
      return 1;
    std::fclose(probe);
  }
  FILE *wfp = efopen((char *)"phi.pet.bak", (char *)"wb");
  if (!wfp) {
    log_print((char *)"pet      : phi.pet save failed (open)");
    return 0;
  }
  for (auto &rec : pet_store) {
    if (rec.second.type < 1)
      continue;
    std::fprintf(wfp, ":%s\n", rec.first.c_str());
    std::fprintf(wfp, ":P type %s\n", pet_type_id[rec.second.type]);
    std::fprintf(wfp, ":P name %s\n", rec.second.name.c_str());
    std::fprintf(wfp, ":P mp %i\n", rec.second.mp);
    std::fprintf(wfp, ":P st %i\n", rec.second.st);
    std::fprintf(wfp, ".\n");
  }
  efclose(wfp);
  if (std::rename("phi.pet.bak", pet_file) != 0) {
    log_print((char *)"pet      : phi.pet save failed (rename)");
    return 0;
  }
  return 1;
}

/* ---------------- grant / rename / release / restore ---------------- */

int pet_grant(int n, int type)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt || type < 1 || type > 4)
    return 0;
  if (rt->type != 0) {
    pet_tell(n, DMM_PET_ALREADY, rt->name.c_str());
    return 0;
  }
  rt->type = type;
  rt->name = pet_def_name[type];
  rt->mp = pet_mp_max;
  rt->st = pet_st_max;
  rt->last_combat = 0;
  rt->last_support = 0;
  rt->emg_ready = 0;
  rt->last_target = -1;
  rt->spent_phys = rt->spent_mag = 0;
  rt->tired_mp = rt->tired_st = 0;
  pet_sync_rt(rt);
  pet_tell(n, DMM_PET_JOIN, pet_type_disp[type]);
  rt->pending = 1;
  cd[n].st = ST_PETNAME;
  pet_tell(n, DMM_PET_NAME_PROMPT, pet_def_name[type]);
  pet_send_self(n);
  pet_send_chr(n);
  return 1;
}

int pet_rename_start(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt)
    return 0;
  if (rt->type == 0) {
    pet_tell(n, DMM_PET_NONE, "");
    return 0;
  }
  rt->pending = 2;
  cd[n].st = ST_PETNAME;
  pet_tell(n, DMM_PET_RENAME_PROMPT, rt->name.c_str());
  return 1;
}

int pet_release_start(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt)
    return 0;
  if (rt->type == 0) {
    pet_tell(n, DMM_PET_NONE, "");
    return 0;
  }
  cd[n].st = ST_PETREL;
  pet_tell(n, DMM_PET_REL_PROMPT, rt->name.c_str());
  return 1;
}

int pet_release_now(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt || rt->type == 0)
    return 0;
  std::string name = rt->name;
  rt->type = 0;
  rt->name.clear();
  rt->pending = 0;
  rt->last_target = -1;
  pet_sync_rt(rt);
  pet_tell(n, DMM_PET_RELEASED, name.c_str());
  pet_send_self(n);
  pet_send_chr(n);
  return 1;
}

int pet_restore(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt)
    return 0;
  if (rt->type == 0) {
    pet_tell(n, DMM_PET_NONE, "");
    return 0;
  }
  rt->mp = pet_mp_max;
  rt->st = pet_st_max;
  rt->tired_mp = rt->tired_st = 0;
  pet_tell(n, DMM_PET_RESTORED, rt->name.c_str());
  pet_send_self(n);
  return 1;
}

void pet_name_input(int n, char *s)
{
  PetRT *rt = pet_rt_get(n);
  cd[n].st = ST_COMMAND;
  if (!rt || rt->type == 0 || rt->pending == 0) {
    if (rt)
      rt->pending = 0;
    return;
  }
  int mode = rt->pending;
  rt->pending = 0;
  /* the whole input line is the candidate name */
  char cand[64];
  std::snprintf(cand, sizeof(cand), "%s", s ? s : "");
  std::size_t len = std::strlen(cand);
  while (len > 0 && (cand[len - 1] == ' ' || cand[len - 1] == '\t'))
    cand[--len] = '\0';
  char *p = cand;
  while (*p == ' ' || *p == '\t')
    p++;
  if (pet_name_valid(p)) {
    rt->name = p;
    pet_tell(n, DMM_PET_NAMED, rt->name.c_str());
  } else if (mode == 2) {
    /* rename cancelled/invalid: keep the current name */
    pet_tell(n, DMM_PET_NAME_DEFAULT, rt->name.c_str());
  } else {
    rt->name = pet_def_name[rt->type];
    pet_tell(n, DMM_PET_NAME_DEFAULT, rt->name.c_str());
  }
  pet_sync_rt(rt);
  pet_send_self(n);
  pet_send_chr(n);
}

void pet_release_input(int n, char *s)
{
  PetRT *rt = pet_rt_get(n);
  cd[n].st = ST_COMMAND;
  if (!rt || rt->type == 0)
    return;
  char cand[64];
  std::snprintf(cand, sizeof(cand), "%s", s ? s : "");
  std::size_t len = std::strlen(cand);
  while (len > 0 && (cand[len - 1] == ' ' || cand[len - 1] == '\t'))
    cand[--len] = '\0';
  char *p = cand;
  while (*p == ' ' || *p == '\t')
    p++;
  if (std::strcmp(p, "はい") == 0 || std::strcmp(p, "yes") == 0) {
    pet_release_now(n);
  } else {
    pet_tell(n, DMM_PET_REL_CANCEL, "");
  }
}

/* ---------------- combat effects ---------------- */

/* Kill credit for a pet-inflicted kill: the owner is the killer
 * (experience, kill messages, condition 21/22), exactly like the
 * tail of attack_to(). The pet itself is never the killer. */
static void pet_kill_credit(int as, int ds, int pet_type_)
{
  if (pet_type_ == PET_DOG) {
    std::array<char, 512> buf;
    PetRT *rt = pet_rt_get(as);
    dmprintf(as, ds, buf.data(), (int)buf.size(), DMM_PET_BARK,
             rt ? rt->name.c_str() : "");
    write_companion(as, ds, buf.data(), 0);
  }
  kill_other(as, ds, LV_FIGHTER);
  if (as < MAX_NTTYS && ds < MAX_NTTYS) {
    if (as != ds)
      load_switch(as, cd[as].x, cd[as].y, 22);  /* PC kill */
  } else if (as < MAX_NTTYS) {
    int target = cd[as].target;
    cd[as].target = ds;
    load_switch(as, cd[as].x, cd[as].y, 21);    /* NPC kill */
    cd[as].target = target;
  }
}

static int pet_magic_blocked(int ds)
{
  if (CN_GASIFY(ds) >= 60 || (ds < MAX_NTTYS && CN_ALAG(ds)))
    return 1;
  if (fld[map[cd[ds].y][cd[ds].x].area].flag & FF_MAGICEFFECT)
    return 1;
  return 0;
}

/* One pet magic hit on ds for pct % of the magic unit amount.
 * Returns the hp_dmg() result (0 alive, 1 killed, 2 raise). */
static int pet_magic_hit(int as, int ds, int pct)
{
  PetRT *rt = pet_rt_get(as);
  if (!rt || rt->type == 0)
    return 0;
  int unit = TCN_SPELL(as) * 25 / 100 + 8;
  int amt = unit * pct / 100;
  amt = amt * (80 + frand(41)) / 100;             /* x0.8 - x1.2 */
  amt = amt * (10000 + cd[as].lv[LV_WIZARD]) / 5000; /* Wlv factor */
  amt = amt * (100 - mr_rate(ds)) / 100;
  if (amt <= 0)
    return 0;
  angry_check(as, ds);
  std::array<char, 512> buf;
  dmprintf(as, ds, buf.data(), (int)buf.size(), DMM_PET_MAGIC,
           rt->name.c_str(), amt);
  write_companion(as, ds, buf.data(), 0);
  int r = hp_dmg(ds, -amt, 1);
  if (r == 1)
    pet_kill_credit(as, ds, rt->type);
  else if (r == 2)
    raise_dead(ds);
  return r;
}

void pet_attack_begin(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt)
    return;
  rt->spent_phys = 0;
  rt->spent_mag = 0;
}

void pet_assist(int as, int ds, int owner_dmg, int friend_attack)
{
  PetRT *rt = pet_rt_get(as);
  if (!rt || rt->type == 0)
    return;
  if (ds < 0 || ds >= enmax || ds == as)
    return;
  if (cd[ds].type == 0 || cd[ds].hp <= 0)
    return;
  rt->last_combat = time(nullptr);
  rt->last_target = ds;
  const PetCfg &cfg = pet_cfg[rt->type];

  /* physical follow-up: a share of the owner's final hit damage
   * (friend_attack's 1/3 is undone first: the pet is not a party
   * member and its share is defined on the undivided damage) */
  if (cfg.phys_rate > 0 && owner_dmg > 0
      && PER_DICE < cfg.phys_rate) {
    if (rt->st < cfg.phys_cost) {
      pet_note_low(as, rt, 0);
    } else {
    int base = friend_attack ? owner_dmg * 3 : owner_dmg;
    int amt = base * cfg.phys_pct / 100;
    if (amt > 0) {
      if (!rt->spent_phys) {
        rt->st -= cfg.phys_cost;
        rt->spent_phys = 1;
      }
      std::array<char, 512> buf;
      dmprintf(as, ds, buf.data(), (int)buf.size(), DMM_PET_BITE,
               rt->name.c_str(), amt);
      write_companion(as, ds, buf.data(), 0);
      int r = hp_dmg(ds, -amt, 1);
      if (r == 1)
        pet_kill_credit(as, ds, rt->type);
      else if (r == 2)
        raise_dead(ds);
      pet_pool_spent(as, rt, 0);
      if (cd[ds].type == 0 || cd[ds].hp <= 0)
        return; /* target is gone; no magic follow-up */
    }
    }
  }

  /* magic follow-up */
  if (cfg.mag_rate > 0 && PER_DICE < cfg.mag_rate
      && !pet_magic_blocked(ds)) {
    if (rt->mp < cfg.mag_cost) {
      pet_note_low(as, rt, 1);
    } else {
      if (!rt->spent_mag) {
        rt->mp -= cfg.mag_cost;
        rt->spent_mag = 1;
      }
      pet_magic_hit(as, ds, cfg.mag_pct);
      pet_pool_spent(as, rt, 1);
    }
  }
}

/* hp_dmg() hook: battle damage (f == 1) on a player updates the
 * pet's combat timer and may trigger the cat's emergency heal.
 * Poison and other f == 0 damage deliberately do not trigger the
 * emergency heal (the per-second tick heal still covers them). */
void pet_on_damaged(int n, int p, int f)
{
  if (n < 1 || n >= MAX_NTTYS || p >= 0 || f != 1)
    return;
  PetRT *rt = pet_rt_get(n);
  if (!rt || rt->type == 0)
    return;
  rt->last_combat = time(nullptr);
  const PetCfg &cfg = pet_cfg[rt->type];
  if (cfg.emg_thr <= 0 || cfg.heal_pct <= 0)
    return;
  if (cd[n].hp <= 0 || cd[n].mhp <= 0)
    return;
  if (cd[n].hp * 100 >= cd[n].mhp * cfg.emg_thr)
    return;
  long now = time(nullptr);
  if (now < rt->emg_ready)
    return;
  if (rt->mp < cfg.emg_cost)
    return;
  rt->mp -= cfg.emg_cost;
  rt->emg_ready = now + cfg.emg_cd;
  int amt = cd[n].mhp * cfg.heal_pct / 100;
  if (amt < 1)
    amt = 1;
  std::array<char, 512> buf;
  dmprintf(n, 0, buf.data(), (int)buf.size(), DMM_PET_HEAL_EMG,
           rt->name.c_str(), amt);
  write_companion(n, 0, buf.data(), 0);
  hp_dmg(n, amt, 0);
  pet_pool_spent(n, rt, 1);
}

/* ---------------- per-second tick ---------------- */

static int pet_target_ok(int n, int t)
{
  if (t < 0 || t >= enmax || t == n)
    return 0;
  if (cd[t].type == 0 || cd[t].hp <= 0)
    return 0;
  if (map[cd[t].y][cd[t].x].area != map[cd[n].y][cd[n].x].area)
    return 0;
  int dx = cd[t].x - cd[n].x;
  int dy = cd[t].y - cd[n].y;
  if (dx < 0)
    dx = -dx;
  if (dy < 0)
    dy = -dy;
  return dx <= 12 && dy <= 12;
}

void pet_tick(int n)
{
  PetRT *rt = pet_rt_get(n);
  if (!rt || rt->type == 0)
    return;
  const PetCfg &cfg = pet_cfg[rt->type];
  long now = time(nullptr);
  int in_combat = (now - rt->last_combat < 10);

  /* pool regeneration */
  int regen = in_combat ? pet_regen_in : pet_regen_out;
  if (rt->mp < pet_mp_max)
    rt->mp = std::min(pet_mp_max, rt->mp + regen);
  if (rt->st < pet_st_max)
    rt->st = std::min(pet_st_max, rt->st + regen);
  int recovered = 0;
  if (rt->tired_mp && rt->mp >= pet_mp_max) {
    rt->tired_mp = 0;
    recovered = 1;
  }
  if (rt->tired_st && rt->st >= pet_st_max) {
    rt->tired_st = 0;
    recovered = 1;
  }
  if (recovered) {
    pet_tell(n, DMM_PET_RECOVER, rt->name.c_str());
    pet_send_self(n);
  }

  /* healer pets: HP watch */
  if (cfg.heal_pct > 0 && cd[n].hp > 0 && cd[n].hp < cd[n].mhp
      && cd[n].hp * 100 < cd[n].mhp * cfg.heal_thr) {
    if (rt->mp < cfg.heal_cost) {
      pet_note_low(n, rt, 1);
    } else {
      int amt = cd[n].mhp * cfg.heal_pct / 100;
      if (amt < 1)
        amt = 1;
      rt->mp -= cfg.heal_cost;
      std::array<char, 512> buf;
      dmprintf(n, 0, buf.data(), (int)buf.size(), DMM_PET_HEAL,
               rt->name.c_str(), amt);
      write_companion(n, 0, buf.data(), 0);
      hp_dmg(n, amt, 0);
      pet_pool_spent(n, rt, 1);
    }
  }

  /* bird support cast at the owner's most recent target */
  if (cfg.sup_rate > 0 && in_combat
      && now - rt->last_support >= cfg.sup_int) {
    int t = rt->last_target;
    if (t >= 0 && (cd[t].type == 0 || cd[t].hp <= 0))
      rt->last_target = -1;
    else if (pet_target_ok(n, t) && !pet_magic_blocked(t)
             && rt->mp < cfg.sup_cost) {
      pet_note_low(n, rt, 1);
    }
    else if (pet_target_ok(n, t) && !pet_magic_blocked(t)) {
      if (PER_DICE < cfg.sup_rate) {
        rt->last_support = now;
        rt->mp -= cfg.sup_cost;
        pet_magic_hit(n, t, cfg.sup_pct);
        pet_pool_spent(n, rt, 1);
      } else {
        rt->last_support = now; /* rolled and missed */
      }
    }
    /* no valid target: retry next second */
  }
}
