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
 *   dm.vars                 plain table: world-shared variables,
 *                           kept in memory only (reset by Load class)
 * The program function receives one argument: a context table
 *   {player=<index>, name=<name>, x=<x>, y=<y>, sx=<switch x>,
 *    sy=<switch y>}
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
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
extern CHROData *ed_o;
extern int enmax;

static lua_State *cls_L = nullptr;
static int cls_ready = 0;
static int cls_cur = -1;      /* player index while a program runs */
static long cls_budget = 0;   /* instruction budget, in hook ticks */

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
  {
    char buf[64];
    snprintf(buf, sizeof(buf), "class    : %s loaded", cls_name);
    log_print(buf);
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
  lua_getglobal(cls_L, prog);
  if (!lua_isfunction(cls_L, -1)) {
    char buf[256];
    snprintf(buf, sizeof(buf),
                  "class    : program not found: %s", prog);
    log_print(buf);
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
