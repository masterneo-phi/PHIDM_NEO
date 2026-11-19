
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38  */

#include  <cstdio>
#include  <cstdlib>
#include  <cstring>
#include  <csignal>
#include  <cmath>
#include  <ctime>
#include  <climits>
#include  <array>

#include  "phi_dm.h"
#include  "phi_attack.h"
#include  "phi_calc.h"
#include  "phi_data.h"
#include  "phi_magic.h"
#include  "phi_enemy.h"
#include  "phi_use.h"
#include  "phi_str.h"
#include  "phi_m57.h"
#include  "tcp.h"

#include  "ext_skills.h"

#ifdef DEBUG
char   debug_buf[256];
#endif

#include "phi_name.h"

extern int px[];
extern int py[];
extern int enmax;
extern CHROData *ed_o;

namespace {
/* shutdown 用の隠し呪文。原文の文字列のまま */
constexpr char kCthulhu[] = "Ph'nglui mglw'nafh Cthulhu R'lyeh wgah'nagl fhtagn";
} // namespace

int MLine[3];
int MHit;
int MExplode[3][3];
int MHeal;
int MCoat;
int MProtect;
int MIllusion;
int MDown;

#if MAX_OBJECTS > 0
extern int reflesh_objects;
#endif

extern int log_send;

int mgstrlen(char *str)
{
  int    nest;
  int    len;

  for (len = 0, nest = 0; *str; str++) {
    if (*str == '/' && *(str + 1) == '*')
      nest++;
    else if (nest && *str == '/' && *(str - 1) == '*')
      nest--;
    else if (!nest)
      len++;
  }
  return (len);
}

int pow_mg_multiplier(MagicData md)
{
  int    i;

  for (i = 0; i < MAX_POWER_SPELL && power_spell[i].pwmg.spell != nullptr; i++)
    if (match_spell(power_spell[i].pwmg,md)) {
      return (power_spell[i].multiplier);
    }
  if (adding_power_spell_flag == 0)
    return (1);
  else
    return (0);
}

void pow_mg_msg(int n, MagicData md)
{
  int    m;

  m = pow_mg_multiplier(md);
  if (adding_power_spell_flag == 0) {
    if (m == 1)
      return;
  } else {
    if (m == 0)
      return;
  }
  if (m >= 1)
    dmcheck_write(n, DMM_HARMONY_PLUS);
  else if (m == 0)
    dmcheck_write(n, DMM_HARMONY_ZERO);
  else if (m <= -1)
    dmcheck_write(n, DMM_HARMONY_MINUS);
}

static void master_search(int n)
/* phi.sd の最新 100 件 (看板・ロック) を表示する。phi.sd は新しい記録が
   先頭に書かれるので、先頭から数える。 */
{
  FILE *fp;
  std::array<char, 256> buf;
  std::array<char, 256> out;
  std::array<char, 32> nm;
  std::array<char, 25> tstr;
  char *p;
  int x, y, cnt, i;
  time_t abstime;
  struct tm *sttime;

  if ((fp = efopen("phi.sd", "rb")) == nullptr) {
    dmcheck_write(n, DMM_MSEARCH_NONE);
    return;
  }
  cnt = 0;
  while (cnt < 100) {
    if (efgets(buf.data(), 255, fp) == nullptr)
      break;
    if (*buf.data() != ':')
      break;
    if (buf[1] != 'B' && buf[1] != 'T' && buf[1] != 'L')
      continue;
    p = buf.data() + 2;
    x = decimal_load(3, &p);
    y = decimal_load(3, &p);
    if (buf[1] == 'L') {
      if (std::strlen(buf.data()) < 67)
        continue;
      std::strncpy(nm.data(), buf.data() + 27, 31);
      nm[31] = 0;
      for (i = 30; i >= 0 && fisspace(nm[i]); nm[i--] = 0)
        ;
      abstime = (time_t) std::strtol(buf.data() + 59, nullptr, 16);
      sttime = std::localtime(&abstime);
      std::strftime(tstr.data(), tstr.size(), "%Y/%m/%d %H:%M", sttime);
      dmprintf(n, 0, out.data(), out.size(), DMM_MSEARCH_L, x, y, nm.data(), tstr.data());
    } else {
      if (std::strlen(buf.data()) <= 36)
        continue;
      p = buf.data() + 36;
      for (i = 0; p[i]; i++)
        if (p[i] == '\n' || p[i] == '\r')
          p[i] = 0;
      dmprintf(n, 0, out.data(), out.size(), DMM_MSEARCH_B, x, y, p);
    }
    check_write(n, out.data());
    cnt++;
  }
  efclose(fp);
  dmprintf(n, 0, out.data(), out.size(), DMM_MSEARCH_END, cnt);
  check_write(n, out.data());
  return;
}

void spell_analyze(int n, char *spell)
{
  /* buf は CPYSTR/CATSTR (sizeof(d) を使うマクロ) で使うため C 配列のまま */
  char   buf[256];
  int    er;
  int    cbuf[MAX_ENEMIES];
  int    ebuf[MAX_ENEMIES];
  int    ebuf2[MAX_ENEMIES];
  int    mdp;
  int    x, y;
  int    i, j, k, l, m, o;
  FILE  *fp;
  ItemData id;
  MapData *pmap;

  if (CN_REFLEX(n) & 0xffff) {
    dmcheck_write(n, DMM_MAGIC_NO_REFREX);
    return;
  }
  mdp = mgstrlen(spell) + 4;
  if (cd[n].weq != nullptr) {
  	  if ((cd[n].weq->type & IT_MASK) == IT_WEAPON && cd[n].weq->ER > 0)
  	    mdp = mdp * (100 - cd[n].weq->ER) / 100;

  	  if (sys_magic_no_degrade != 1) { /* システム魔法で武器が欠けないようにする added by NEO 20120211 */
  	  /* bug fix by NEO 20120826 ERが正常に機能しなくなるバグフィックス */

  	  	if (PER_DICE < (magic_anti_degrade ? 10 * (100 - OCN_ILLUSION(n)) / 100 : 10)) {
  	    	if (degrade(cd[n].weq)==2) {
  			dmprintf(n, 0, buf, sizeof(buf), DMM_BROKEN_WEAPON);
  			write_companion(n, 0, buf, 0);
  			cd[n].weq = nullptr;
  			cd[n].wsep = 0;
  			item_gc(n);
  			set_fn(n);
  		    }
  		}
  	  }
  }
  if (cd[n].mmp > 0 && cd[n].mhp > 0) {
    if (mdp >= cd[n].mp) {
      dmcheck_write(n, DMM_MORE_MP);
      cd[n].mp = 1;
      return;
    }
    (void) mp_dmg(n, -mdp, 0); /* 死なないことは保証されている */
    dmprintf(n, 0, buf, sizeof(buf),DMM_LEFT_MP);
    check_write(n, buf);
  }
  if (*spell == 0)
    return;
  /* add for switchable spell 20041216 */
  if (check_and_process_cmd(n,'M',spell))
    return;
  pmap = &(map[cd[n].y+py[cd[n].d]][cd[n].x+px[cd[n].d]]);
  if (!std::strcmp(spell, "Return")) {
      if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_RETURN) {
  dmcheck_write(n, DMM_RETURN_NO);
  return;
      }
    /* add for common magic skill 20020522 */
    skillup(n,SKL_MAGIC_SP,SKL_MGSP_RETURN,99,50,1);
#ifndef NO_SKILL
    if ((j=cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_RETURN])/100==0 && cd[n].exp < 99999999 && PER_DICE > j) {
  dmcheck_write(n, DMM_RETURN_MISS);
  return;
    }
#endif
    clear_chr(n);
    cd[n].x = cd[n].ix;
    cd[n].y = cd[n].iy;
#ifdef NO_SKILL
    while (PER_DICE < 50 && cd[n].exp < 99999999)
#else
    while (PER_DICE < 95 - 45 * std::log10(j/100+1.0) && cd[n].exp < 99999999)
#endif
    {
      i = frand(8);
      delete_item(&(cd[n].id[i]));
      item_gc(n);
      set_fn(n);
      skillup(n,SKL_MAGIC_SP,SKL_MGSP_RETURN,99,50,1);
    }
    dmcheck_write(n, DMM_RETURN);
    set_chr(n);
    print_map(n, cd[n].modef & (MD_BOARD | MD_FLOORITEM));
    if (!trnf) {
      cd[n].bx = cd[n].x;
      cd[n].by = cd[n].y;
    }
    return;
  } else if (!std::strcmp(spell, "Call")) {
    if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_CALL) {
      cd[n].ix = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].x;
      cd[n].iy = fldt[fld[map[cd[n].y][cd[n].x].area].teleport].y;
      dmcheck_write(n, DMM_CALL_ANYWHERE);
    } else {
      cd[n].ix = cd[n].x;
      cd[n].iy = cd[n].y;
      dmcheck_write(n, DMM_CALL_HERE);
    }
    return;
  } else if (!std::strcmp(spell, "Wizard eye")) {
      wizard_eye_f(n, 0);
    return;
  } else if (!std::strcmp(spell, "Party eye")) { /* add for party eye 20020826 */
      wizard_eye_f(n, 1);
    return;
  } else if (!std::strcmp(spell, "Wizard light")) {
    wizard_light(n);
    return;
  } else if (!std::strcmp(spell, "List")) {
    user_lists(n);
    return;
  } else if (!std::strcmp(spell, "Eagle eye")) {
      if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_EAGLEEYE) {
  dmcheck_write(n, DMM_EAGLEEYE_NO);
  return;
      }
    /* add for common magic skill 20020522 */
    skillup(n,SKL_MAGIC_SP,SKL_MGSP_EAGLE,99,50,1);
#ifndef NO_SKILL
    if ((j=cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_EAGLE])/100==0 && cd[n].exp < 99999999 && PER_DICE > j) {
  dmcheck_write(n, DMM_EAGLEEYE_MISS);
  return;
    }
#endif
    dmcheck_write(n, DMM_EAGLEEYE);
      if (cd[n].modef & MD_FEAGLE) {
        eagleeye_m57(n);
        return;
      }
      i = fld[map[cd[n].y][cd[n].x].area].floor;
#ifndef NO_SKILL
      if(cd[n].exp == 99999999)
        j = 9999;
#endif
    for (y = -5; y <= 5; y++) {
      CPYSTR(buf, " [                      ] \n");
      for (x = -5; x <= 5; x++) {
#ifndef NO_SKILL
  /* add for common magic skill 20020522 */
  if (fisqrt((x * x + y * y) * 100) > j / 10 + 40) {
    buf[2 + (x + 5) * 2] = '?';
    buf[3 + (x + 5) * 2] = '?';
    continue;
  } else
#endif
  if (cd[n].x + x < 0 || cd[n].x + x >= MAX_X
      || cd[n].y + y < 0 || cd[n].y + y >= MAX_Y) {
    buf[2 + (x + 5) * 2] = '?';
    buf[3 + (x + 5) * 2] = '?';
    continue;
  } else if (fld[map[cd[n].y + y][cd[n].x + x].area].floor != i) {
    buf[2 + (x + 5) * 2] = '?';
    buf[3 + (x + 5) * 2] = '?';
    continue;
  }
  dd_char(buf + 2 + (x + 5) * 2, map[cd[n].y + y][cd[n].x + x], 0xf);
      }
      check_write(n, buf);
    }
    return;
  } else if (!std::strcmp(spell, "Wizard lock")) {
      if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_LOCK) {
  dmcheck_write(n, DMM_LOCK_NO);
  return;
      }
    /* change for obj over terrion 20000104 */
    x = cd[n].x + px[cd[n].d];
    y = cd[n].y + py[cd[n].d];
    if ((map[y][x].tip & TMASK) != T_BOX
  && (map[y][x].tip & TMASK) != T_GATE
  && (map[y][x].tip & TMASK) != T_SGATE
  && (map[y][x].tip & TMASK) != T_MSGATE) {
      dmcheck_write(n, DMM_LOCK_NO);
      return;
    }
    if (save_sd(n, x, y, 'L', nullptr, 0)<0)
      dmcheck_write(n, DMM_LOCK);
    else
      dmcheck_write(n, DMM_LOCK_FAIL);
    return;
  } else if (!std::strcmp(spell, "Unlock")) {
    /* change for obj over terrion 20000104 */
    x = cd[n].x + px[cd[n].d];
    y = cd[n].y + py[cd[n].d];
    if ((map[y][x].tip & TMASK) != T_LBOX
  && (map[y][x].tip & TMASK) != T_LGATE
  && (map[y][x].tip & TMASK) != T_LSGATE
  && (map[y][x].tip & TMASK) != T_LMSGATE) {
      dmcheck_write(n, DMM_UNLOCK_NO);
      return;
    }
    if (save_sd(n, x, y, 'L', nullptr, 1)<0)
      dmcheck_write(n, DMM_UNLOCK);
    else
      dmcheck_write(n, DMM_UNLOCK_FAIL);
    return;
  } else if (!std::strcmp(spell, "Search")) {
    dmcheck_write(n, DMM_SEARCH_INPUT);
    cd[n].st = ST_SEARCH;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Create")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type != DT_NONE)
  break;
    if (i >= MAX_ITEMS) {
      dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
      return;
    }
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
  continue;
      j++;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].id[i].name);
      check_write(n, buf);
    }
    j++;
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_CREATE_SELECT);
    cd[n].st = ST_CREATE;
    CN_IDLE(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Charge spell")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if ((cd[n].id[i].type & IT_MASK) == IT_JEWEL)
        break;
    if (i >= MAX_ITEMS)
    {
      dmcheck_write(n, DMM_NO_ITEM_JEWEL);
      return;
    }
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if ((cd[n].id[i].type & IT_MASK) != IT_JEWEL)
        continue;
      j++;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].id[i].name);
      check_write(n, buf);
    }
    j++;
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_CHARGESPL_SELECTJWL);
    cd[n].st = ST_CHARGE_SPELL;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Detect")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type != DT_NONE)
  break;
    if (i >= MAX_ITEMS) {
      dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
      return;
    }
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
  continue;
      j++;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].id[i].name);
      check_write(n, buf);
    }
    j++;
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_DETECT_SELECT);
    cd[n].st = ST_DETECT;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Analyze")) {
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_MAGICS; i++) {
      if (cd[n].md[i].spell == nullptr)
  break;
      j++;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].md[i].spell);
      check_write(n, buf);
    }
    j++;
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_ANALYZE_SELECT);
    cd[n].st = ST_ANALYZE;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Forget")) {
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_MAGICS; i++) {
      if (cd[n].md[i].spell == nullptr)
  break;
      j++;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].md[i].spell);
      check_write(n, buf);
    }
    j++;
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_FORGET_SELECT);
    cd[n].st = ST_FORGET;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Identify")) {
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
  continue;
      dmprintf(n, j, buf, sizeof(buf),DMM_IDENTIFY);
      write_companion(n, 0, buf, 0);
      ident_to(n, j);
    }
    return;
  } else if (!std::strcmp(spell, "Destroy")) {
    for (i = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type != DT_NONE)
  break;
    }
    if (i == MAX_ITEMS) {
      dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
      return;
    }
    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
  continue;
      std::snprintf(buf, sizeof(buf), "[%2i] %s \n", ++j, cd[n].id[i].name);
      check_write(n, buf);
    }
    std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", ++j);
    check_write(n, buf);
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_DESTROY_SELECT);
    cd[n].st = ST_DESTROY;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Win talk")) {
    for(i=0; i<MAX_TALKS; i++)
      if(cd[n].td[i].type == TT_KILL){
  cd[n].td[i].type = TT_NONE;
  cd[n].td[i].string = nullptr;
  dmcheck_write(n, DMM_WINTALK_DELETE);
  return;
      }
    for(i=0; i<MAX_TALKS; i++)
      if(cd[n].td[i].type == TT_NONE) break;
    if(i>=MAX_TALKS){
      dmcheck_write(n, DMM_WINTALK_NO_ADD);
      return;
    }
    dmcheck_write(n, DMM_WINTALK_INPUT);
    cd[n].st = ST_WINTALK;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strcmp(spell, "Lose talk")) { /* add 20001212 */
    for(i=0; i<MAX_TALKS; i++)
      if(cd[n].td[i].type == TT_DYING){
  cd[n].td[i].type = TT_NONE;
  cd[n].td[i].string = nullptr;
  dmcheck_write(n, DMM_LOSETALK_DELETE);
  return;
      }
    for(i=0; i<MAX_TALKS; i++)
      if(cd[n].td[i].type == TT_NONE) break;
    if(i>=MAX_TALKS){
      dmcheck_write(n, DMM_LOSETALK_NO_ADD);
      return;
    }
    dmcheck_write(n, DMM_LOSETALK_INPUT);
    cd[n].st = ST_LOSETALK;
    CN_GASIFY(n) = 0x10000;
    check_write(n, "#s-edit\n");
    return;
  } else if (!std::strncmp(spell, "Appear", 6)) {
	SKIP_TO_SPACE(spell);
	SKIP_SPACE(spell);
    if (*spell == '\0') {
      dmcheck_write(n, DMM_APPEAR_BAD);
      return;
    }
    *spell = ftoupper(*spell);
    i = (int) std::strlen(spell);
    k = l = m = 0;
    for (j = 0; j < max_enemy; j++) {
      if (ed_o[j].apc == 3) {
  if ((!std::strncmp(spell, ed_o[j].name, i) &&
       (ed_o[j].name[i] == '\0' || ed_o[j].name[i] == ' ')) &&
      ((ed_o[j].apcode == 0) || /* 召喚コード */
       ((ed_o[j].apcode != 0) &&
        (((cd[n].oeq != nullptr) && (cd[n].oeq->SE == SE_APPEAR) &&
    (cd[n].oeq->name == ed_o[j].apnm)) ||
         ((cd[n].aeq != nullptr) && (cd[n].aeq->SE == SE_APPEAR) &&
    (cd[n].aeq->name == ed_o[j].apnm)) ||
         ((cd[n].weq != nullptr) && (cd[n].weq->SE == SE_APPEAR) &&
    (cd[n].weq->name == ed_o[j].apnm)))))) {
    cbuf[j] = -1;
    l += ed_o[j].aplvl; /* レベル合計 */
    m += ed_o[j].nm;  /* 数合計 */
    k++;
  } else {
    cbuf[j] = 0;
  }
      } else {
    cbuf[j] = 0;
      }
    }
    if (k == 0) {
      dmcheck_write(n, DMM_APPEAR_NO);
      return;
    }
    k = (l-cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_APPEAR])/100;
    if (k <= 0)
      k = 1;
    if (k > 10)
      k = 9;
    skillup(n,SKL_MAGIC_SP,SKL_MGSP_APPEAR,(l < 1000 ? 10 : l/100),50,k);
    if (l > 0) {    /* バースト */
#ifdef NO_SKILL
      k = fisqrt((7 * cd[n].lv[LV_WIZARD] + 7 * cd[n].lv[LV_CREATER]
      + cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_MERCHANT])
     * 2500 / (16 * l));
#else
      /* change for sammon skill 20020522 */
      k = fisqrt(cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_APPEAR] * 2500 / l);
#endif
      if (n >= MAX_NTTYS)
  k = 100;
      if ((cd[n].exp != 99999999) && (PER_DICE > k)) {
  i = mdp * mdp / 2;
  if(is_see_burst){
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST);
    write_companion(n, 0, buf, 0);
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SPHERE,
      i);
    write_companion(n, 0, buf, 0);
  } else {
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SELF, i);
    check_write(n, buf);
  }
  if(hp_dmg(n, -i, 2)){ /* 死亡 */
    kill_self_msg(n,DMM_KILL_SELF_BURST);
    return;
  }
  i = frand(MAX_LEVEL) + LV_FIGHTER;
  for (j = LV_FIGHTER; j < MAX_LEVEL + LV_FIGHTER; j++) {
    if (j == i) {
      cd[n].lv[j] = (cd[n].lv[j] * (100 - penalty_rate)) / 100;  /* changed by NEO 2011/11/13 */
    } else {
      cd[n].lv[j] = cd[n].lv[j] * 99 / 100;  /* changed by NEO 2011/11/13 */
  	}
  }
  return;
  }
      m = fsps(80, m * k / 100, m); /* 発生数 */
      if (m < 1)
  m = 1;
    }
    for (j = 0; j < max_enemy; j++) { /* 発生位置 */
      if (cbuf[j]) {
  ed_o[j].ix = cd[n].x;
  ed_o[j].iy = cd[n].y;
      }
    }
    o = k = 0;
    for (j = MAX_NTTYS; j < enmax; j++) {
      if ((cd[j].et >= 0) && cbuf[cd[j].et]) {
  if (cd[j].type == CT_NOEXIST) {
    ebuf[k++] = j;  /* 存在しない */
  } else {
    ebuf2[o++] = j; /* 存在している */
  }
      }
    }
    for (i = 0; i < k; i++) { /* 存在している NPC をソート */
      j = frand(k);
      l = ebuf[j];
      ebuf[j] = ebuf[i];
      ebuf[i] = l;
    }
    for (i = 0; i < o; i++) { /* 存在していない NPC をソート */
      j = frand(o);
      l = ebuf2[j];
      ebuf2[j] = ebuf2[i];
      ebuf2[i] = l;
    }
    for (i = 0; i < o; i++) { /* 発生順配列生成 */
      ebuf[k++] = ebuf2[i];
    }
    if (m > k)
      m = k;
    for (i = 0; i < m; i++) {
      j = ebuf[i];
      if ((cd[j].et >= 0) && cbuf[cd[j].et]) {
  if (cd[j].type != CT_NOEXIST) { /* 消滅 */
    for (i = 0; i < MAX_ITEMS; i++) {
      delete_item(&(cd[j].id[i]));
    }
    tax += cd[j].gp;
    cd[j].type = CT_NOEXIST;
    cd[j].gp = 0;
    clear_chr(j);
  }
  cd[j].wt = fsps(80, ed_o[cd[j].et].apt, 900000);
  cd[j].ec = 1;   /* 発生 */
  cd[j].vf2 = n;  /* add for controlable sammon 20000614 */
      }
    }
    dmcheck_write(n, DMM_APPEAR);
    return;
  } else if (!std::strncmp(spell, "Disappear", 9)) {
	SKIP_TO_SPACE(spell);
	SKIP_SPACE(spell);
	*spell = ftoupper(*spell);
    i = std::strlen(spell);
    k = l = 0;
    for (j = 0; j < max_enemy; j++) {
      if (ed_o[j].apc == 3) {
  if (((ed_o[j].apcode == 0) && (i != 0) &&
       (!std::strncmp(spell, ed_o[j].name, i) &&
        (ed_o[j].name[i] == '\0' || ed_o[j].name[i] == ' '))) ||
      ((ed_o[j].apcode != 0) &&
       (((cd[n].oeq != nullptr) && (cd[n].oeq->SE == SE_APPEAR) &&
         (cd[n].oeq->name == ed_o[j].apnm)) ||
        ((cd[n].aeq != nullptr) && (cd[n].aeq->SE == SE_APPEAR) &&
         (cd[n].aeq->name == ed_o[j].apnm)) ||
        ((cd[n].weq != nullptr) && (cd[n].weq->SE == SE_APPEAR) &&
         (cd[n].weq->name == ed_o[j].apnm))) &&
       ((i == 0) || ((!std::strncmp(spell, ed_o[j].name, i) &&
      (ed_o[j].name[i] == '\0' || ed_o[j].name[i] == ' ')))))) {
    cbuf[j] = -1;
    l += ed_o[j].aplvl;
    k++;
  } else {
    cbuf[j] = 0;
  }
      } else {
  cbuf[j] = 0;
      }
    }
    if (k == 0) {
      dmcheck_write(n, DMM_DISAPPEAR_NO);
      return;
    }
    k = (l-cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_DISAPR])/100;
    if (k <= 0)
      k = 1;
    if (k > 10)
      k = 9;
    skillup(n,SKL_MAGIC_SP,SKL_MGSP_DISAPR,(l < 1000 ? 10 : l/100),50,k);
    if (l > 0) {
#ifdef NO_SKILL
      k = (7 * cd[n].lv[LV_WIZARD] + 7 * cd[n].lv[LV_CREATER]
     + cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_MERCHANT])
  * 2500 / (16 * l);
#else
      k = cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_DISAPR] * 2500 / l;
#endif
      if ((cd[n].exp != 99999999) && (PER_DICE > fisqrt(k))) {
  i = mdp * mdp / 2;
  if(is_see_burst){
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST);
    write_companion(n, 0, buf, 0);
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SPHERE,
      i);
    write_companion(n, 0, buf, 0);
  } else {
    dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SELF, i);
    check_write(n, buf);
  }
  if(hp_dmg(n, -i, 2)){
    kill_self_msg(n, DMM_KILL_SELF_BURST);
    return;
  }
  i = frand(MAX_LEVEL) + LV_FIGHTER;
  for (j = LV_FIGHTER; j < MAX_LEVEL + LV_FIGHTER; j++) {
    if (j == i) {
      cd[n].lv[j] = (cd[n].lv[j] * (100 - penalty_rate)) / 100;  /* changed by NEO 2011/11/13 */
    } else {
      cd[n].lv[j] = cd[n].lv[j] * 99 / 100;  /* changed by NEO 2011/11/13 */
    }
  }
  return;
      }
    }
    i = 0;
    for (j = MAX_NTTYS; j < enmax; j++) {
      if ((cd[j].et >= 0) && (cbuf[cd[j].et] != 0)) {
        /* add for controlable sammon 20000614 */
        if((i = cd[j].vf2) >= 0 && i != n && l) {
          k = (7 * cd[n].lv[LV_WIZARD] + 7 * cd[n].lv[LV_CREATER]
             + cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_MERCHANT]) * 2500 / (16 * l);
          o = (7 * cd[i].lv[LV_WIZARD] + 7 * cd[i].lv[LV_CREATER]
             + cd[i].lv[LV_FIGHTER] + cd[i].lv[LV_MERCHANT]) * 2500 / (16 * l);
          if(lv_dice(k) < lv_dice(o)) {
            i = mdp * mdp / 2;
            if(is_see_burst){
              dmprintf(n, 0, buf, sizeof(buf),DMM_BURST);
              write_companion(n, 0, buf, 0);
              dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SPHERE,
                i);
              write_companion(n, 0, buf, 0);
            } else {
              dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SELF, i);
              check_write(n, buf);
            }
            if(hp_dmg(n, -i, 2)){
              kill_self_msg(n, DMM_KILL_SELF_BURST);
            }
            return;
          }
        }
  cd[j].ec = 0;
  if (cd[j].type != CT_NOEXIST) {
    for (i = 0; i < MAX_ITEMS; i++) { /* 消滅 */
      delete_item(&(cd[j].id[i]));
    }
    tax += cd[j].gp;
    cd[j].type = CT_NOEXIST;
    cd[j].gp = 0;
    clear_chr(j);
    cd[j].vf2 = -1;
  }
  i++;
      }
    }
    if (i == 0) {
      dmcheck_write(n, DMM_DISAPPEAR_NO);
    } else {
      dmcheck_write(n, DMM_DISAPPEAR);
    }
    return;
  }
  if (cd[n].auth != nullptr) {
	i=1; /* for double message 20011203 */
    if (!std::strcmp(spell, "Evil wind from Hyades") && (i=have_authority(n,MS_SHUTDOWN,0,0))) {
      for (i = 1; i < MAX_NTTYS; i++) {
  if (cd[i].type) {
    std::snprintf(buf, sizeof(buf), "shut     : %s ", cd[i].name);
    log_print(buf);
    dmcheck_write(i, DMM_SHUT_CLIENTS);
    dmcheck_write(i, DMM_SAVEDATA);
    check_write(i, "#close\n");
    /* cut by el cd[i].x=cd[i].ix; cd[i].y=cd[i].iy; cut by el */
    nfflush(i, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
    close_connect(i);
  }
      }
      return;
    } else if(!std::strncmp(spell, "Evil wind to ", 13) && (i=have_authority(n,MS_KICK,0,0))) {
      if((i=std::atoi(spell+13))<MAX_NTTYS && cd[i].type){
        std::snprintf(buf, sizeof(buf), "kick     : %s ", cd[i].name);
        log_print(buf);
        dmcheck_write(i, DMM_KICK);
        dmcheck_write(i, DMM_SAVEDATA);
        check_write(i, "#close\n");
        nfflush(i, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
        close_connect(i);
        dmprintf(i, 0, buf, sizeof(buf),DMM_KICK_BC);
        write_companion(n, 0, buf, 1);
      }
      return;
    } else if(!std::strncmp(spell, "Be Pygmalion ", 13) && (i=have_authority(n,MS_KICK,0,0))) {
      if((i=std::atoi(spell+13))<MAX_NTTYS && cd[i].type){
        dmprintf(i, 0, buf, sizeof(buf),DMM_MARIONET);
        CN_PALSY(i) = 0x10000;
        CN_SILENCE(i) = 0x10000;
        CN_BLIND(i) = -0x10000;
        CN_CONFUSE(i) = 0x10000;
        write_companion(n, i, buf, 0);
        std::snprintf(buf, sizeof(buf), "freeze   : %s ", cd[i].name);
        log_print(buf);
      }
      return;
    } else if(!std::strncmp(spell, "Basi Rurla ", 11) && (i=have_authority(n,MS_KICK | MS_TRANS,0,0))) {
      if((i=std::atoi(spell+11))<enmax && cd[i].type){
		spell += 11;
		SKIP_SPACE(spell);
		SKIP_TO_SPACE(spell);
		clear_chr(i);
		sscanf(spell,"%i %i",&(cd[i].x),&(cd[i].y));
        set_chr(i);
        dmprintf(i, 0, buf, sizeof(buf),DMM_TRANS_OTHER);
        write_companion(n, i, buf, 0);
      }
      return;
/*  Draw（マスター専用）マスターの元へプレイヤーを呼び寄せる　20050305 by NEO */
    } else if(!std::strncmp(spell, "Draw ", 5) && (i=have_authority(n,MS_KICK | MS_TRANS,0,0))) {
      if((i=std::atoi(spell+5))<enmax && cd[i].type){
		spell += 5;
		SKIP_SPACE(spell);
		SKIP_TO_SPACE(spell);
		clear_chr(i);
		cd[i].x = cd[n].x;
		cd[i].y = cd[n].y;
        set_chr(i);
        dmprintf(i, 0, buf, sizeof(buf),DMM_DRAW, cd[n].name , cd[i].name );
        write_companion(n, i, buf, 0);
      }
      return;
/*  Jump（マスター専用）プレイヤーの元へジャンプ　20050305 by NEO */
	} else if(!std::strncmp(spell, "Jump ", 5) && (i=have_authority(n,MS_KICK | MS_TRANS,0,0))) {
      if((i=std::atoi(spell+5))<enmax && cd[i].type){
		spell += 5;
		SKIP_SPACE(spell);
		SKIP_TO_SPACE(spell);
		clear_chr(n);
		cd[n].x = cd[i].x;
		cd[n].y = cd[i].y;
        set_chr(n);
        dmprintf(i, 0, buf, sizeof(buf),DMM_JUMP, cd[n].name , cd[i].name );
        write_companion(n, i, buf, 0);
      }
      return;
/*  IP&IDリスト（マスター専用）20050305 by NEO */
    } else if (!std::strcmp(spell, "Master list") && (i=have_authority(n,MS_KICK | MS_BAN,0,0))) {
      master_lists(n);
      return;
/*  PK禁止設定魔法（マスター専用）20050316 by NEO */
    } else if (!std::strcmp(spell, "Deny PK") && (i=have_authority(n,MS_SHUTDOWN,0,0))) {
      deny_pk = 1;
      dmprintf(n, 0, buf, sizeof(buf),DMM_SET_DENY_PK);
      write_companion(n, 0, buf, 1);
      return;
/*  PK禁止設定解除魔法（マスター専用）20050316 by NEO */
    } else if (!std::strcmp(spell, "Permit PK") && (i=have_authority(n,MS_SHUTDOWN,0,0))) {
      deny_pk = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_SET_PERMIT_PK);
      write_companion(n, 0, buf, 1);
      return;
    } else if(!std::strncmp(spell, "Brizzard from Cokyutos to ", 26) && (i=have_authority(n,MS_KICK | MS_BAN,0,0))) {
/* character banned magic added by Qajin 19991020 */
      for(j=0;j < BANNEDIPMAX && !(ip_ban[j].ip == nullptr || ip_ban[j].time < time(nullptr));j++);
      if (j == BANNEDIPMAX) {
        dmcheck_write(n, DMM_BAN_FULL);
      } else if((i=std::atoi(spell+26))<MAX_NTTYS && cd[i].type){
        if (i == n) {
          dmcheck_write(n, DMM_BAN_SELF_ALART);
        } else {
          char *p;
          p = spell + 26;
          SKIP_SPACE(p);
          SKIP_TO_SPACE(p);
          SKIP_SPACE(p);
          if (*p == '-') {
			id_ban_add(cd[i].uid);
		  }
          std::snprintf(buf, sizeof(buf), "banned   : %s(%s:%s) ", cd[i].name,ip_domain_check(i),ip_address_check(i));
          log_print(buf);
          dmcheck_write(i, DMM_KICK);
          dmcheck_write(i, DMM_SAVEDATA);
          check_write(i, "#close\n");
          nfflush(i, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
          close_connect(i);
          ip_ban[j].ip = findstr(ip_address_check(i));
          ip_ban[j].time = time(nullptr) + 60 * (std::atoi(p) == 0 ? 60 : std::atoi(p));  /* 取り敢えず１時間 */
          dmprintf(i, 0, buf, sizeof(buf),DMM_BAN_BC);
          write_companion(n, 0, buf, 1);
        }
      }
      return;
    } else if (!std::strcmp(spell, "Close box of Pandora") && (i=have_authority(n,MS_BAN,0,0))) {
/* address banned magic added by Qajin 19991021 */
      for(i=0;i<BANNEDIPMAX;i++)
        if (ip_ban[i].ip == nullptr || ip_ban[i].time < time(nullptr))
           break;
      if (i == BANNEDIPMAX) {
        dmcheck_write(n, DMM_BAN_FULL);
      } else {
      dmcheck_write(n, DMM_BAN_INPUT);
      cd[n].st = ST_BAN;
      check_write(n, "#s-edit\n");
      }
      return;
    } else if (!std::strcmp(spell, "Open box of Pandora") && (i=have_authority(n,MS_BAN,0,0))) {
      dmcheck_write(n, DMM_BAN_LIFT_INPUT);
      cd[n].st = ST_BAN_;
      check_write(n, "#s-edit\n");
      return;
/* end */
    } else if (!std::strcmp(spell, "Flag") && (i=have_authority(n,MS_QUEST,0,0))) {
      check_write(n, "#more\n");
      for (i = 0; i < GFLAG_MAX; i++) {
  if (generic_flag[i] == FLAG_ON) {
    std::snprintf(buf, sizeof(buf), "%x\n", i);
    check_write(n, buf);
  }
      }
      check_write(n, "#end-more\n");
      return;
    } else if (!std::strcmp(spell, "Variable") && (i=have_authority(n,MS_QUEST,0,0))) {
    /* add for variables 20010803 */
      check_write(n, "#more\n");
      for (i = 0; i < GVAR_MAX; i++) {
  if (generic_variable[i]) {
    std::snprintf(buf, sizeof(buf), "x%.2x=%4i\n",i, generic_variable[i]);
    check_write(n, buf);
  }
      }
      check_write(n, "#end-more\n");
      return;
    } else if (!std::strcmp(spell, "Eye of the Yog=Sothoth") && (i=have_authority(n,MS_LOAD,0,0))) {
      fp = fopen("phied.log", "w");
      check_write(n, "#more\n");
      for (i = 1, j = 0; i < enmax; i++)
  if (cd[i].type) {
    j++;
    std::snprintf(buf, sizeof(buf), " [%+3iW,%+3iN] %-31s \n"
      ,cd[n].x - cd[i].x, cd[n].y - cd[i].y, cd[i].name);
    fprintf(fp, buf);
    check_write(n, buf);
  }
      std::snprintf(buf, sizeof(buf), " total %3i \n", j);
      fprintf(fp, buf);
      check_write(n, buf);
      check_write(n, "#end-more\n");
      fclose(fp);
      return;
    } else if (!std::strcmp(spell, "Book of the Yog=Sothoth") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_PHIED_OUT);
      check_write(n, buf);
      fp = fopen("phied.log","w");
      for (i = 1, j = 0; i < enmax; i++){
  if (cd[i].type) {
    j++;
      fprintf(fp, "[%03iW,%03iN] (%s)\n", cd[i].x, cd[i].y,
#ifndef AREA_SUB
        fld[map[cd[i].y][cd[i].x].area].name);
#else
        fld[map[cd[i].y][cd[i].x].area].name[map[cd[i].y][cd[i].x].area_sub]);
#endif
    fprintf(fp, "Name: %s (%s)\n",
      cd[i].name,(i<MAX_NTTYS ? "PC" : "NPC"));

    fprintf(fp, "Status:\n");
    fprintf(fp, "  Hp : %4li / %4li   Mp : %4li / %4li \n", cd[i].hp, cd[i].mhp, cd[i].mp, cd[i].mmp);
    if(i<MAX_NTTYS)
      fprintf(fp, "  Exp: %8i      Gp : %4i\n", cd[i].exp, cd[i].gp);
    else
      fprintf(fp, "  Gp : %4i\n", cd[i].gp);
    fprintf(fp, "  Fighter  %4i    Wizard   %4i \n", cd[i].lv[LV_FIGHTER], cd[i].lv[LV_WIZARD]);
    if(i<MAX_NTTYS)
      fprintf(fp, "  Merchant %4i    Creater  %4i \n", cd[i].lv[LV_MERCHANT], cd[i].lv[LV_CREATER]);
    fprintf(fp, "  ATP   : %3i \n", (cd[i].weq != nullptr ? cd[i].weq->ATP : 0));
    fprintf(fp, "  ATT Fn: %s \n", fn_name[cd[i].afn]);
    if(cd[i].weq != nullptr && ((cd[i].weq->type & IT_MASK) == IT_WEAPON))
      fprintf(fp, "  Effect: %s \n", ae_name[cd[i].weq->AE]);
    fprintf(fp, "  DFP   : %3i \n", (cd[i].aeq != nullptr ? cd[i].aeq->DFP : 0));
    fprintf(fp, "  MRP   : %3i \n", (cd[i].aeq != nullptr ? cd[i].aeq->MRP : 0));
    /* IT_ARMOR でないものは、鎧として着ることができない */
    fprintf(fp, "  DEF Fn: %s \n", fn_name[cd[i].dfn]);
    fprintf(fp, "  Karma : %2i \n", cd[i].align);
    fprintf(fp, "  Speed : %2i \n", cd[i].sp);
    if (cd[i].type < CT_MAGICAL)
      fprintf(fp, "  Status: %s \n", at_name[cd[i].at]);
    else if ((cd[i].at < AT_VISIBLE) || (cd[i].at == AT_IMMORTAL))
      fprintf(fp, "  Status: Immortal \n"); /* 結局これ */
    else
      fprintf(fp, "  Status: Immortal %s \n", at_name[cd[i].at]);
    fprintf(fp, "  Special: ");
    k = 0;
    if (cd[i].weq != nullptr)
      if (((cd[i].weq->type) & IT_MASK) != IT_FOOD)
        if (cd[i].weq->SE != SE_NONE) {
    fprintf(fp, "%s ",se_name[cd[i].weq->SE]);
    k++;
        }
    if (cd[i].aeq != nullptr)
      if (cd[i].aeq->SE != SE_NONE) {
        fprintf(fp, "%s ",se_name[cd[i].aeq->SE]);
        k++;
      }
    if (cd[i].oeq != nullptr)
      if (cd[i].oeq->SE != SE_NONE) {
        fprintf(fp, "%s ",se_name[cd[i].oeq->SE]);
        k++;
      }
    if (cd[i].innate & IA_STONEHEAD) {
      fprintf(fp, "Stonehead ");
      k++;
    }
    if (cd[i].innate & IA_UMBRELLA) {
      fprintf(fp, "Umbrella ");
      k++;
    }
    if (cd[i].innate & IA_FLYING) {
      fprintf(fp, "Flying ");
      k++;
    }
    if (cd[i].innate & IA_RICK) {
      fprintf(fp, "Rick ");
      k++;
    }
    if (cd[i].innate & IA_DRAGON) {
      fprintf(fp, "Dragon ");
      k++;
    }
    if (cd[i].innate & IA_GOD) {
      fprintf(fp, "Godhead ");
      k++;
    }
    if (cd[i].innate & IA_ELEMENT) {
      fprintf(fp, "Element ");
      k++;
    }
    if (cd[i].innate & IA_DEMON) {
      fprintf(fp, "Demon ");
      k++;
    }
    if (k == 0) {
      fprintf(fp, "None ");
    }
    fprintf(fp, "\n");

    fprintf(fp,"Items:\n");
    for(k=0; k<MAX_ITEMS; k++)
      if(cd[i].id[k].type != DT_NONE){
        save_item(&(cd[i].id[k]), buf, sizeof(buf));
        buf[std::strlen(buf)-1]='\0'; /* \n を消す */
        if(cd[i].weq == &(cd[i].id[k]))
    CATSTR(buf, " [Wpn]");
        else if(cd[i].aeq == &(cd[i].id[k]))
    CATSTR(buf, " [Arm]");
        else if(cd[i].oeq == &(cd[i].id[k]))
    CATSTR(buf, " [Acr]");
        else if(cd[i].id[k].active_time ==ULONG_MAX)
    CATSTR(buf, " -    ");
        else
    CATSTR(buf, "      ");
        if(is_need_charge_time
     && (cd[i].id[k].type & IT_MASK) == IT_CHARGE
     && cd[i].id[k].active_time > time(nullptr))
    CATSTR(buf, " (charging)");
        CATSTR(buf,"\n");
        fprintf(fp," %s",buf);
      }
    fprintf(fp,"Spells:\n");
    for(k=0; k<MAX_MAGICS; k++)
      if(cd[i].md[k].effect != EF_NONE){
        save_spell(&(cd[i].md[k]), buf, sizeof(buf));
        fprintf(fp," %s",buf);
      }
    fprintf(fp,"Talks:\n");
    for(k=0; k<MAX_TALKS; k++)
      if(cd[i].td[k].type != TT_NONE){
        save_talk(&(cd[i].td[k]), buf, sizeof(buf));
        fprintf(fp," %s",buf);
    }

    fprintf(fp, "Environment:\n");
    fprintf(fp, "  afn: %i dfn: %i sfn: %i \n", cd[i].afn, cd[i].dfn, cd[i].sfn);
    fprintf(fp, "  wd: %04x IA: %04x UD: %04x AT: %i \n", cd[i].wd, cd[i].innate, cd[i].undetect, cd[i].at);
    fprintf(fp, "  wSE: %i aSE: %i oSE: %i \n", cd[i].wsep, cd[i].asep, cd[i].osep);
    fprintf(fp, "  regene:  %4i will:     %4i \n", TCN_REGENE(i), TCN_WILL(i));
    fprintf(fp, "  coat:    %4i spell:    %4i \n", TCN_COAT(i), TCN_SPELL(i));
    fprintf(fp, "  protect: %4i shell:    %4i \n", TCN_PROTECT(i), TCN_SHELL(i));
    fprintf(fp, "  haste:   %4i illusion: %4i \n", TCN_HASTE(i), TCN_ILLUSION(i));
    fprintf(fp, "  poison:  %4i palsy:    %4i \n", TCN_POISON(i), TCN_PALSY(i));
    fprintf(fp, "  panic:   %4i confuse:  %4i \n", TCN_PANIC(i), TCN_CONFUSE(i));
    fprintf(fp, "  berserk: %4i silence:  %4i \n", TCN_BERSERK(i), TCN_SILENCE(i));
    fprintf(fp, "  karma:   %4i vanish:   %4i \n", TCN_KARMA(i), TCN_VANISH(i));
    fprintf(fp, "  blind:   %4i mirror:   %4i \n", TCN_BLIND(i), TCN_MIRROR(i));
    fprintf(fp, "  paswall: %4i gigant:   %4i \n", TCN_PASSWALL(i), TCN_GIGANT(i));
    fprintf(fp, "  nomove:  %4i raise:    %4i \n", TCN_NOMOVE(i), TCN_RAISEDEAD(i));

    /* add for personal flags 20010627 */
    fprintf(fp,"Flags:\n");
    fprintf(fp,"  P:");
    for(k=0; k<PFLAG_MAX; k++) {
	  if (k==SFLAG_MAX)
        fprintf(fp," S:");
      fprintf(fp,(cd[i].pflag & (1 << k)) ? "1" : "0");
	}
    fprintf(fp, "\n");

    /* add for personal variables 20010803 */
    fprintf(fp,"Variables:\n");
    for(k=0; k<PVAR_MAX; k++) {
      fprintf(fp,"  y%x=%4i",k,cd[i].pvar[k]);
      if (k % 4 == 3)
        fprintf(fp,"\n");
	}
    fprintf(fp, "\n");
    for(k=0; k<SVAR_MAX; k++) {
      fprintf(fp,"  z%x=%4i",k,cd[i].svar[k]);
      if (k % 4 == 3)
        fprintf(fp,"\n");
	}
    fprintf(fp, "\n");

    fprintf(fp, "\n");
  }
      }
      fprintf(fp, "---\n");
      fprintf(fp, "total %3i \n", j);
      fclose(fp);
      dmprintf(n, 0, buf, sizeof(buf),DMM_PHIED_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Dumapic") && (i=have_authority(n,MS_TRANS,cd[n].x,cd[n].y))) {
      std::snprintf(buf, sizeof(buf), "  Position [ %3d, %3d]\n", cd[n].x, cd[n].y);
      check_write(n, buf);
      return;
    } else if (!std::strncmp(spell, "Resource", 8) && (i=have_authority(n,MS_RESOURCE,X_FORE(n),Y_FORE(n)))) {  /* リソース魔法 */
      x = cd[n].x + px[cd[n].d];
      y = cd[n].y + py[cd[n].d];
      if(y <= 0 || y >= MAX_Y - 1 || x <= 0 || x >= MAX_X - 1){
  dmcheck_write(n, DMM_RESOURCE_FAIL);
  return;
      }
      SKIP_TO_SPACE(spell);
      SKIP_SPACE(spell);
      buf[3] = '\0';
      switch (*spell) {
      case '\0':
  buf[0] = ' ';
  buf[1] = ' ';
  break;
      case '~':
  buf[0] = ' ';
  buf[1] = '~';
  spell++;
  break;
      case 'o':
      case '*':
      case ':':
      case '+':
      case '_':
      case '^': /* add 20000627 */
      case 'x':
      case '/':
      case '.':
      case '[':
      case '$':
      case '!':
      case 'a': /* add 2000711 */
      case 'I':
      case '|':
      case 'T':
      case '=':
      case 'H':
      case '#':
      case 'Z':
      case '@':
      case 's': /* add 20020823 */
  buf[0] = *(spell++);
  buf[1] = *(spell++);
  break;
      default:
  dmcheck_write(n, DMM_RESOURCE_FAIL);
  return;
      }
      if (
    (map[y][x].tip & TMASK) == T_LBOX ||
    (map[y][x].tip & TMASK) == T_LGATE ||
    (map[y][x].tip & TMASK) == T_LMSGATE ||
    (map[y][x].tip & TMASK) == T_LSGATE
  ) {     /* ロックのチェック */
  save_sd(n, x, y, 'L', nullptr, 1);
      }
      map[y][x].tip = ((map[y][x].tip & (TCLEAR ^ T_ROOF)) | set_dd(buf));
      dmcheck_write(n, DMM_RESOURCE);
      load_id_in_circle(x,y);
      if (*spell != '\0') {
		SKIP_SPACE(spell);
        map[y][x].surface = (unsigned char)(set_dd(spell));
        map[y][x].tip |= T_SURFACE;
        dmcheck_write(n, DMM_RESOURCE_SURFACE);
	  } else {
        map[y][x].tip &= ~T_SURFACE;
	  }
      return;

#if MAX_OBJECTS > 0

  /* オブジェ魔法 add 20000105 change 20010302 */
    } else if (!std::strncmp(spell, "Createobj ", 10) && (i=have_authority(n,MS_RESOURCE,X_FORE(n),Y_FORE(n)))) {
      SKIP_TO_SPACE(spell);
      SKIP_SPACE(spell);
      i = str_to_obj(&spell);
      if (i == -1) {
  dmcheck_write(n, DMM_CREOBJ_BAD);
  return;
      } else if (i == -2) {
  dmcheck_write(n, DMM_CREOBJ_NO);
  return;
	  }

	  move_obj(i,cd[n].x+px[cd[n].d],cd[n].y+py[cd[n].d],-1);
      dmcheck_write(n, DMM_CREOBJ);
      return;

    } else if (!std::strncmp(spell, "Eraseobj", 8) && (i=have_authority(n,MS_RESOURCE,X_FORE(n),Y_FORE(n)))) {
      x = cd[n].x + px[cd[n].d];
      y = cd[n].y + py[cd[n].d];
	  move_obj(dobj[y][x]-1,-1,-1,-1);
      dmcheck_write(n, DMM_ERAOBJ);
      return;

#endif

    } else if (!std::strncmp(spell, "Wish", 4) && (i=have_authority(n,MS_ITEM,0,0))) {  /* アイテム生成魔法 */
      for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
    break;
      }
      if (i == MAX_ITEMS) {
  dmcheck_write(n, DMM_WISH_NO_TOO_ITEM);
  return;
      }
      if (load_item(&id, spell + 5) == 1) {
		id.active_time = 0; /* bug(?) fix 20030707 */
  std::memcpy(&(cd[n].id[i]), &id, sizeof (ItemData));
  dmcheck_write(n, DMM_WISH);
  return;
      } else {
  dmcheck_write(n, DMM_WISH_BAD);
  return;
      }
    } else if (!std::strncmp(spell, "Dup", 3) && (i=have_authority(n,MS_ITEM,0,0))) {
      for (i = 0; spell[i] != ' ' && spell[i] != '\0'; i++);
      if (spell[i++] == ' ') {  /* アイテム番号を指定した場合 */
  j = spell[i++] - '0';
  k = 1;
  if (spell[i++] == ' ') {/* アイテム数 */
    k = std::atoi(spell + i);
    if (k < 1) {
      dmcheck_write(n, DMM_NO_NUMBER);
    }
  }
  l = 0;      /* アイテムを探す */
  for (i = 0; i < MAX_ITEMS; i++) {
    if (cd[n].id[i].type != DT_NONE) {
      if (++l == j)
        break;
    }
  }
  if (i == MAX_ITEMS) {
    dmcheck_write(n, DMM_DUP_BAD);
    return;
  }
  for (j = 0; j < MAX_ITEMS; j++) { /* 複製 */
    if (cd[n].id[j].type == DT_NONE) {
      std::memcpy(&(cd[n].id[j]), &(cd[n].id[i]),
       sizeof (ItemData));
      if ((--k) == 0)
        break;
    }
  }
  while (k-- > 0) {
    additem(cd[n].x, cd[n].y, &(cd[n].id[i]), 0);
    map[cd[n].y][cd[n].x].tip |= T_ITEM;
  }
  dmcheck_write(n, DMM_DUP);
  return;
      }
      for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
    break;
      }
      if (i == MAX_ITEMS) {
  dmcheck_write(n, DMM_DUP_NO_TOO_ITEM);
  return;
      }
      check_write(n, "#list\n");
      for (i = 0, j = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
    continue;
  std::snprintf(buf, sizeof(buf), "[%2i] %s \n", ++j, cd[n].id[i].name);
  check_write(n, buf);
      }
      std::snprintf(buf, sizeof(buf), "[%2i] CANCEL \n", ++j);
      check_write(n, buf);
      check_write(n, "#end-list\n");
      dmcheck_write(n, DMM_DUP_SELECT);
      cd[n].st = ST_DUPLICATE;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Malor") && (i=have_authority(n,MS_TRANS,0,0))) {
      dmcheck_write(n, DMM_MALOR_INPUT);
      cd[n].st = ST_MALOR;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if(!std::strcmp(spell, "Metamorphosis") && (i=have_authority(n,MS_QUEST,0,0))) {
      dmcheck_write(n, DMM_METAMORPHOSYS_INPUT);
      cd[n].st = ST_METAMORPHOSIS;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Restoration") && (i=have_authority(n,MS_QUEST,0,0))) {
      cd[n].hp = cd[n].mhp;
      cd[n].mp = cd[n].mmp;
      for (i = 0; i <= NORMAL_CN; i++)
  if (cd[n].cn[i] < 0)
    cd[n].cn[i] = 0;
      dmcheck_write(n, DMM_RESTOR);
      return;
    } else if (!std::strcmp(spell, "Cache") && (i=have_authority(n,MS_ITEM,0,0))) {
      if (cd[n].gp == 0) {
  cd[n].gp = 9999;
  /* tax -= 9999 * 100 / cirate;  TAX は現地レート */
  /* でないと、1 (現地レート)の増減が表現不可 */
  tax -= 9999;
  dmcheck_write(n, DMM_CACHE_HAND);
      } else {
  /* tax += cd[n].gp * 100 / cirate; */
  tax += cd[n].gp;
  cd[n].gp = 0;
  dmcheck_write(n, DMM_CACHE_TAX);
      }
      std::snprintf(buf, sizeof(buf), "tax      : %li ", tax);
      log_print(buf);
      return;
    } else if (!std::strcmp(spell, "Evil wind from Malaika") && (i=have_authority(n,MS_SHUTDOWN,0,0))) {
      for (i = 1; i < MAX_NTTYS; i++) {
  if (cd[i].type) {
    std::snprintf(buf, sizeof(buf), "shut     : %s ", cd[i].name);
    log_print(buf);
    dmcheck_write(i, DMM_SHUT_SERVER);
    dmcheck_write(i, DMM_SAVEDATA);
    check_write(i, "#close\n");
    nfflush(i, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
    close_connect(i);
  }
      }
      signal_trap(SIGTERM);
      return;
    } else if (!std::strcmp(spell, "Pass wall") && (i=have_authority(n,MS_TRANS,0,0))) {
      mdp = mgstrlen(spell) + 4; /* bug(?) fix 20010726 */
      CN_PASSWALL(n) = mdp;
      dmprintf(n, 0, buf, sizeof(buf),DMM_PASSWALL, mdp);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Aegis of Athena") && (i=have_authority(n,MS_TRANS,0,0))) {
      CN_AEGIS(n) = mgstrlen((char *)"Pass wall") + 4; /* 持続時間は Pass wall と同一 */
      dmprintf(n, 0, buf, sizeof(buf),DMM_AEGIS, CN_AEGIS(n));
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Remote lock") && (i=have_authority(n,MS_UNLOCK,0,0))) {
      dmcheck_write(n, DMM_REMOTE_INPUT);
      cd[n].st = ST_REMOTE_LOCK;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Remote unlock") && (i=have_authority(n,MS_UNLOCK,0,0))) {
      dmcheck_write(n, DMM_REMOTE_INPUT);
      cd[n].st = ST_REMOTE_UNLOCK;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Remote boards") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmcheck_write(n, DMM_REMOTE_INPUT);
      cd[n].st = ST_REMOTE_BOARDS;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Remote erase") && (i=have_authority(n,MS_DELETE,0,0))) {
      dmcheck_write(n, DMM_REMOTE_INPUT);
      cd[n].st = ST_REMOTE_ERASE;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Thunder of Zeus") && (i=have_authority(n,MS_KICK,0,0))) {
      dmcheck_write(n, DMM_THUNDER_INPUT);
      cd[n].st = ST_THUNDER;
      CN_GASIFY(n) = 0x10000;
      check_write(n, "#s-edit\n");
      return;
    } else if (!std::strcmp(spell, "Master search") && (i=have_authority(n,MS_LOAD,0,0))) {
      master_search(n);
      return;
    } else if (!std::strcmp(spell, "Save id") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf), DMM_SAVEID);
      check_write(n, buf);
      if (savef_id()) {
  dmprintf(n, 0, buf, sizeof(buf),DMM_SAVEID_NO);
      } else {
  dmprintf(n, 0, buf, sizeof(buf),DMM_SAVEID_COMP);
      }
      check_write(n, buf);
      return;

#if MAX_OBJECTS > 0
    } else if (!std::strcmp(spell, "Save obj") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_SAVEOD);
      check_write(n, buf);
      if (save_obj()) {
  dmprintf(n, 0, buf, sizeof(buf),DMM_SAVEOD_NO);
      } else {
  dmprintf(n, 0, buf, sizeof(buf),DMM_SAVEOD_COMP);
      }
      check_write(n, buf);
      return;
#endif

    } else if (!std::strcmp(spell, "Load td") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADTD);
      check_write(n, buf);
      log_send = n;
      put_gaten(n, 1);    /* close gate */
      free_gatedata();
      free_swtd();
      load_td_gate();
      load_td_swt();
      load_td_ban(); /* added by Qajin 19991021 */
      put_gaten(n, 0);    /* open gate */
      clear_gate_and_switch_on_map();
      remap_gate_and_switch();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADTD_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load gate") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADGATE);
      check_write(n, buf);
      log_send = n;
      put_gaten(n, 1);    /* close gate */
      free_gatedata();
      load_td_gate();
      put_gaten(n, 0);
      clear_gate_and_switch_on_map();
      remap_gate_and_switch();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADGATE_COMP);
      check_write(n, buf);
      return;
    /* add 20010206 */
    } else if (!std::strcmp(spell, "Close gate") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_CLOSEGATE);
      check_write(n, buf);
      log_send = n;
      put_gaten(n, 1);    /* close gate */
      free_outgatedata();
      clear_gate_and_switch_on_map();
      remap_gate_and_switch();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_CLOSEGATE_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load swt") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADSWT);
      check_write(n, buf);
      log_send = n;
      free_swtd();
      load_td_swt();
      clear_gate_and_switch_on_map();
      remap_gate_and_switch();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADSWT_COMP);
      check_write(n, buf);
      return;
/* added by Qajin 19991021 */
    } else if (!std::strcmp(spell, "Load ban") && (i=have_authority(n,MS_LOAD | MS_BAN,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADBAN);
      check_write(n, buf);
      log_send = n;
      load_td_ban();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADBAN_COMP);
      check_write(n, buf);
      return;
/* add for switchable command 20041216 */
    } else if (!std::strcmp(spell, "Load cmd") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADCMD);
      check_write(n, buf);
      log_send = n;
      load_cmd();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADCMD_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load fd") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADFD);
      check_write(n, buf);
      log_send = n;
      free_fd();
      load_fd();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADFD_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load crt") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADCRT);
      check_write(n, buf);
      log_send = n;
      clear_crt();
      load_crt();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADCRT_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load ed") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADED);
      check_write(n, buf);
      log_send = n;
      free_ed_o();
      load_ed_o();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADED_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, "Load ddx") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADDDX);
      check_write(n, buf);
      log_send = n;
      extension_dd(n);
      clear_gate_and_switch_on_map();
      remap_gate_and_switch();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADDDX_COMP);
      check_write(n, buf);
      return;
    } else if(!std::strcmp(spell, "Load ex") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADEX);
      check_write(n, buf);
      log_send = n;
      free_ex();
      load_ex();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADEX_COMP);
      check_write(n, buf);
      return;

#if MAX_OBJECTS > 0
  /* added 20000105 */
    } else if(!std::strcmp(spell, "Load obj") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADOD);
      check_write(n, buf);
      log_send = n;
      load_obj();
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADOD_COMP);
      check_write(n, buf);
      return;
  /* added 20000728 */
#endif

    } else if(!std::strcmp(spell, "Load sd") && (i=have_authority(n,MS_LOAD,0,0))) {
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADSD);
      check_write(n, buf);
      log_send = n;
      remap_lock_and_board(1);
      str_gc();
      log_send = 0;
      dmprintf(n, 0, buf, sizeof(buf),DMM_LOADSD_COMP);
      check_write(n, buf);
      return;
    } else if (!std::strcmp(spell, kCthulhu) && (i=have_authority(n,MS_SHUTDOWN,0,0))) {
      signal_trap(0xff);
      return;
    }
    /* for double message 20011203 */
    if (!i)
    	return;
  }
  if (map[cd[n].y][cd[n].x].tip & T_ANTIMAGIC) {
    dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
    return;
  }
    if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGIC) {
      dmcheck_write(n, DMM_CAST_BLOCKED);
      return;
    }

  if (cd[n].weq != nullptr) { /* bug fix by NEO 20121024  */

  /* システム魔法以外では武器が欠けるようにする added by NEO 20120211 */
  	if (sys_magic_no_degrade == 1) {
  		  /* bug fix by NEO 20120826 ERが正常に機能しなくなるバグフィックス */

  		  if (PER_DICE < (magic_anti_degrade ? 10 * (100 - OCN_ILLUSION(n)) / 100 : 10)) {
  		  	if (degrade(cd[n].weq)==2) {
  		  	dmprintf(n, 0, buf, sizeof(buf), DMM_BROKEN_WEAPON);
  		  	write_companion(n, 0, buf, 0);
  		  	cd[n].weq = nullptr;
  		  	cd[n].wsep = 0;
  		  	item_gc(n);
  		  	set_fn(n);
  		  	}
  		  }
  	}

  }

  er = cd[n].lv[LV_WIZARD] / 100;
  if (!cd[n].md[0].is_inherence
      && PER_DICE < mdp && PER_DICE >= er) {
    i = mdp * mdp / 2;
    if(is_see_burst){
      dmprintf(n, 0, buf, sizeof(buf),DMM_BURST);
      write_companion(n, 0, buf, 0);
      dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SPHERE,
        i);
      write_companion(n, 0, buf, 0);
    } else {
      dmprintf(n, 0, buf, sizeof(buf),DMM_BURST_DAMAGE_SELF, i);
      check_write(n, buf);
    }
    if(hp_dmg(n, -i, 2))
      kill_self_msg(n, DMM_KILL_SELF_BURST);
    return;
  }
  for (i = 0; i < MAX_MAGICS; i++) {
    if (cd[n].md[i].spell == nullptr)
      continue;
    if (!std::strcmp(spell, cd[n].md[i].spell)) {
      effect_analyze(n, &(cd[n].md[i]), 0);
      copy_spell(n, &(cd[n].md[i]));
      exchange_magic(n, &(cd[n].md[i]));
      if (map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]].tip & T_SWITCH)
        load_switch(n, cd[n].x + px[cd[n].d], cd[n].y + py[cd[n].d], 14);
      return;
    }
  }
  dmcheck_write(n, DMM_CAST_BAD);
  return;
}

void   user_lists(int n)
{
  std::array<char, 256> buf;
  int    i, j, k, l;
  std::array<char, 32> sp;
  int    tlv;

  l = 0;/*  add for 接続者数表示　20060107 by NEO */

  /* トータルレベル表示 20050302 by NEO */
  std::snprintf(buf.data(), buf.size(), "No.  Name                            Total Lv\n");
  check_write(n, buf.data());
  
  for (i = 1; i < MAX_NTTYS; i++) {
    if (cd[i].type) {
      /* add for comment distartion 20000927 */
      k = mgstrlen(cd[i].name);
      l += 1; /*  add for 接続者数表示　20060107 by NEO */
      nstr(sp.data(),' ',32-k);
        if (cd[i].exp == 99999999)
          std::snprintf(buf.data(), buf.size(), "%4d %s/*.*/%s /*color=+mp*/Master/*.*/  %c %c %c\n",
          i,cd[i].name,sp.data(),
          (cd[i].eye ? ' ' : '*'),
          (cd[i].modef & MD_CHAT ? 'c' : ' '),
          (cd[i].party ? (cd[n].party==cd[i].party ? 'P' : 'p') : ' '));
	    else {
          tlv = (cd[i].lv[LV_FIGHTER] + cd[i].lv[LV_WIZARD] + cd[i].lv[LV_MERCHANT] + cd[i].lv[LV_CREATER] + cd[i].exp / 10) / 2000;
	        if (tlv > 999)
	        tlv = 999;
          std::snprintf(buf.data(), buf.size(), "%4d %s/*.*/%s Lv.%3i  %c %c %c\n",
          i,cd[i].name,sp.data(),tlv,
          (cd[i].eye ? ' ' : '*'),
          (cd[i].modef & MD_CHAT ? 'c' : ' '),
          (cd[i].party ? (cd[n].party==cd[i].party ? 'P' : 'p') : ' '));
        }    
      check_write(n, buf.data());
    }
  }
  std::snprintf(buf.data(), buf.size(),DMM_CHECK_ACCESS, l ); /* add for 接続者数表示　20060107 by NEO */
  check_write(n, buf.data());
  return;
}

  /* ID&IP表示 20050305 by NEO */
void   master_lists(int n)
{
  std::array<char, 256> buf;
  int    i, j, k;
  std::array<char, 32> sp;

  std::snprintf(buf.data(), buf.size(), "No.  Name                            IP address       User ID\n");
  check_write(n, buf.data());
  
  for (i = 1; i < MAX_NTTYS; i++) {
    if (cd[i].type) {
      k = mgstrlen(cd[i].name);
      nstr(sp.data(),' ',32-k);
      std::snprintf(buf.data(), buf.size(), "%4d %s/*.*/%s %-15s  %s %c %c %c\n",
      i,cd[i].name,sp.data(),ip_address_check(i),cd[i].uid,
      (cd[i].eye ? ' ' : '*'),
      (cd[i].modef & MD_CHAT ? 'c' : ' '),
      (cd[i].party ? (cd[n].party==cd[i].party ? 'P' : 'p') : ' '));    
      check_write(n, buf.data());
    }
  }
  return;
}
void wizard_light(int n)
{
  std::array<char, 256> buf;
  std::array<char, 32> sp;
  int    i, j;

  /* Master動作変更 20000924 */
  if (cd[n].exp != 99999999) {
    user_lists(n);
    return;
  }

  std::snprintf(buf.data(), buf.size(), " Name                            Address                         Idling  Lag\n");
  check_write(n, buf.data());
/*  check_write(n, "----------------------------------------------------------------------------\n");*/
  j = mgstrlen(cd[n].name);
  nstr(sp.data(),' ',32-j);
  if ((j = CN_LAG(n)) < 0)
    std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %-31s %04i\'%02i ---\n",
      cd[n].name,sp.data(), cd[n].adr, CN_IDLE(n) / 60, CN_IDLE(n) % 60);
  else
    std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %-31s %04i\'%02i %01i.%01i\n",
      cd[n].name,sp.data(), cd[n].adr, CN_IDLE(n) / 60, CN_IDLE(n) % 60, j / 10, j % 10);
  check_write(n, buf.data());
  for (i = 1; i < MAX_NTTYS; i++) {
    if (i == n)
      continue;
    if (cd[i].type) {
      j = mgstrlen(cd[i].name);
      nstr(sp.data(),' ',32-j);
      if ((j = CN_LAG(i)) < 0)
  std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %-31s %04i\'%02i ---\n", cd[i].name,sp.data(),
    cd[i].adr, CN_IDLE(i) / 60, CN_IDLE(i) % 60);
      else
  std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %-31s %04i\'%02i %01i.%01i\n", cd[i].name,sp.data(),
    cd[i].adr, CN_IDLE(i) / 60, CN_IDLE(i) % 60, j / 10, j % 10);
      check_write(n, buf.data());
    }
  }
  return;
}

/* change for party eye 20020826 */
void wizard_eye_f(int n, int f)
{
  std::array<char, 256> buf;
  std::array<char, 32> nbuf;
  std::array<char, 19> sp1;
  std::array<char, 32> sp2;
  std::array<char, 13> psbuf;
  int    i, j, k, l;
  int    ar1, ar2;
#ifdef AREA_SUB
  int    ars;
#endif

  std::snprintf(buf.data(), buf.size(), " Name                Area                              Position  Idling  Lag\n");
  check_write(n, buf.data());
  ar1 = (int) (unsigned int) map[cd[n].y][cd[n].x].area;
#ifdef AREA_SUB
  ars = (int) (unsigned int) map[cd[n].y][cd[n].x].area_sub;
#endif
  /* add for comment distartion 20000927 */
  k = cmtstrcpy(nbuf.data(), nbuf.size(),cd[n].name,20);
  nstr(sp1.data(),' ',20-k);
#ifndef AREA_SUB
  l = mgstrlen(fld[ar1].name);
#else
  l = mgstrlen(fld[ar1].name[ars]);
#endif
  nstr(sp2.data(),' ',32-l);
  if ((j = CN_LAG(n)) < 0)
    std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %s/*.*/%s [ +0W, +0N] %04i\'%02i --- %c %c %c\n",
#ifndef AREA_SUB
      nbuf.data(),sp1.data(),fld[ar1].name,sp2.data(),
#else
      nbuf.data(),sp1.data(),fld[ar1].name[ars],sp2.data(),
#endif
      CN_IDLE(n) / 60, CN_IDLE(n) % 60,
      (cd[n].eye ? ' ' : '*'),(cd[n].modef & MD_CHAT ? 'c' : ' '),
      (cd[n].party ? 'P' : ' '));
  else
    std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %s/*.*/%s [ +0W, +0N] %04i\'%02i %01i.%01i %c %c %c\n",
#ifndef AREA_SUB
      nbuf.data(),sp1.data(),fld[ar1].name,sp2.data(),
#else
      nbuf.data(),sp1.data(),fld[ar1].name[ars],sp2.data(),
#endif
      CN_IDLE(n) / 60, CN_IDLE(n) % 60, j / 10, j % 10,
      (cd[n].eye ? ' ' : '*'),(cd[n].modef & MD_CHAT ? 'c' : ' '),
      (cd[n].party ? 'P' : ' '));
  check_write(n, buf.data());
  for (i = 1; i < MAX_NTTYS; i++) {
    if (i == n)
      continue;
    /* add for party eye 20020826 */
    if (f && (!cd[n].party || cd[n].party != cd[i].party))
      continue;
    if (cd[i].type) {
      ar2 = (int) (unsigned int) map[cd[i].y][cd[i].x].area;
#ifdef AREA_SUB
      ars = (int) (unsigned int) map[cd[n].y][cd[n].x].area_sub;
#endif
      /* add for comment distartion 20000927 */
      k = cmtstrcpy(nbuf.data(), nbuf.size(),cd[i].name,20);
      nstr(sp1.data(),' ',20-k);
#ifndef AREA_SUB
      l = mgstrlen(fld[ar2].name);
#else
      l = mgstrlen(fld[ar2].name[ars]);
#endif
      nstr(sp2.data(),' ',32-l);
      if (fld[ar1].floor == fld[ar2].floor) {
  std::snprintf(psbuf.data(), psbuf.size(),"[%+3iW,%+3iN] ",
    cd[n].x - cd[i].x, cd[n].y - cd[i].y);
      } else {
        nstr(psbuf.data(),' ',13);
      }
      if ((j = CN_LAG(i)) < 0)
  std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %s/*.*/%s %s%04i\'%02i --- %c %c %c\n",
#ifndef AREA_SUB
    nbuf.data(),sp1.data(),fld[ar2].name,sp2.data(),psbuf.data(),
#else
    nbuf.data(),sp1.data(),fld[ar2].name[ars],sp2.data(),psbuf.data(),
#endif
    CN_IDLE(i) / 60, CN_IDLE(i) % 60,
    (cd[i].eye ? ' ' : '*'),(cd[i].modef & MD_CHAT ? 'c' : ' '),
    (cd[i].party ? (cd[n].party==cd[i].party ? 'P' : 'p') : ' '));
      else
  std::snprintf(buf.data(), buf.size(), " %s/*.*/%s %s/*.*/%s %s%04i\'%02i %01i.%01i %c %c %c\n",
#ifndef AREA_SUB
    nbuf.data(),sp1.data(),fld[ar2].name,sp2.data(),psbuf.data(),
#else
    nbuf.data(),sp1.data(),fld[ar2].name[ars],sp2.data(),psbuf.data(),
#endif
    CN_IDLE(i) / 60, CN_IDLE(i) % 60, j / 10, j % 10,
    (cd[i].eye ? ' ' : '*'),(cd[i].modef & MD_CHAT ? 'c' : ' '),
    (cd[i].party ? (cd[n].party==cd[i].party ? 'P' : 'p') : ' '));
      check_write(n, buf.data());
    }
  }
  return;
}

namespace {
/* create_item が使う素材係数テーブル(読み取り専用) */
constexpr std::array<double, 16> kMtAtp =
{
  0.50, 0.50, 0.75, 1.00, 1.00, 1.25, 1.50, 2.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00
};

constexpr std::array<std::array<double, 16>, 2> kMtDfp =
{{
  {1.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 1.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00, 0.00},
  {0.25, 0.50, 0.75, 1.00, 1.00, 1.25, 1.50, 1.75, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00, 1.00}
}};

constexpr std::array<std::array<int, 16>, 2> kMtMrp =
{{
  {0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 10, 25, 50, 0, 75, 0, 0, 0, 0, 0, 0, 0}
}};

constexpr std::array<int, 16> kMtEr =
{
  0, 0, 0, 0, 10, 25, 50, 0, 00, 0, 0, 0, 0, 0, 0, 0
};
} // namespace

void create_item(int n, int in1)
{
  int    in2;
  std::array<char, 256> buf;
  /* name は CPYSTR (sizeof(d) を使うマクロ) で使うため C 配列のまま */
  char   name[32];
  int    i, sef;

  sef = 0;      /* FALSE値だとcreate後のアイテムのS.E.をNONEにする */

  for (i = 0; i < MAX_ITEMS; i++)
    if (&(cd[n].id[i]) == cd[n].weq)
      break;
  if (i >= MAX_ITEMS) {
    dmcheck_write(n, DMM_CREATE_NO);
    return;
  }
  in2 = i;
  i = ((cd[n].id[in1].wp * ((cd[n].id[in1].type & MT_MASK) / MT_PLACE + 1)
  + cd[n].id[in2].wp * ((cd[n].id[in2].type & MT_MASK) / MT_PLACE + 1))
       * (10000 - cd[n].lv[LV_MERCHANT]) / 10000);
  if (nciflag) {
    i = i * cirate / 100 + 1;
  } else {
    i = i + 1;
  }
  if (tax < 0)
    i = i * (10000 - tax) / 10000;
  if (cd[n].gp < i) {
    dmcheck_write(n, DMM_CREATE_NO_TAX);
    return;
  }
  cd[n].gp -= i;
  tax += i;
  if (cd[n].id[in2].wp > cd[n].id[in1].wp) {
    i = in1;
    in1 = in2;
    in2 = i;
  }
  i=1;
#ifdef NO_SKILL
  if (PER_DICE > (cd[n].lv[LV_CREATER] + 600) / 20)
#else
  if (PER_DICE > (cd[n].lv[LV_CREATER] + 600) / 20 ||
      PER_DICE > 20+39*std::log10(skill_crt_check(n,&(cd[n].id[in1])) * 2 +
                             skill_crt_check(n,&(cd[n].id[in2]))/300+1.0) )
#endif
  {
    i=0;
  }
  skillup_create(n, &(cd[n].id[in1]), 0);
  skillup_create(n, &(cd[n].id[in2]), 0);  
  if (!create_special_item(n, in1, in2, i)) {
    if (!i) {  /* bug? fix 20000325 */
      cd[n].id[in1].wp /= 2;
      cd[n].id[in2].wp /= 2;
    }
    if ((cd[n].id[in1].type & IT_MASK) == IT_RESOURCE)
      cd[n].id[in1].wp = 0;
    if ((cd[n].id[in2].type & IT_MASK) == IT_RESOURCE)
      cd[n].id[in2].wp = 0;
    if ((cd[n].id[in1].type & IT_MASK) == IT_SPELL_JEWEL){
      cd[n].id[in1].type = (cd[n].id[in1].type ^ IT_SPELL_JEWEL) ^ IT_JEWEL;
      cd[n].id[in1].VALUE = 0;
      cd[n].id[in1].VALUE2 = 0;
    }
    if ((cd[n].id[in2].type & IT_MASK) == IT_SPELL_JEWEL){
      cd[n].id[in2].type = (cd[n].id[in2].type ^ IT_SPELL_JEWEL) ^ IT_JEWEL;
      cd[n].id[in2].VALUE = 0;
      cd[n].id[in2].VALUE2 = 0;
    }
    switch (cd[n].id[in1].type & IT_MASK) {
    case IT_ARMOR:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
      case IT_WEAPON:
      case IT_CHARGE:
      case IT_ACCESSORY:
  if ((cd[n].id[in1].type & MT_MASK) == (cd[n].id[in2].type & MT_MASK))
    cd[n].id[in1].wp += cd[n].id[in2].wp;
  else {
    cd[n].id[in1].wp /= 2;
    cd[n].id[in2].wp /= 2;
  }
  if (cd[n].id[in1].wp < 25) {
    cd[n].id[in1].DFP = (cd[n].id[in1].wp + 1);
    cd[n].id[in1].DFP *= kMtDfp[0][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    cd[n].id[in1].MRP = kMtMrp[0][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    std::snprintf(name, sizeof(name),"%s cloth"
      ,mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  } else if (cd[n].id[in1].wp < 50) {
    cd[n].id[in1].DFP = (cd[n].id[in1].wp + 1) / 2;
    cd[n].id[in1].DFP *= kMtDfp[1][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    cd[n].id[in1].MRP = kMtMrp[1][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    std::snprintf(name,sizeof(name), "%s plate"
      ,mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  } else {
    if (cd[n].id[in1].wp > 99)
      cd[n].id[in1].wp = 99;
    cd[n].id[in1].DFP = (cd[n].id[in1].wp - 49) / 2 + 25;
    cd[n].id[in1].DFP *= kMtDfp[1][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    cd[n].id[in1].MRP = kMtMrp[1][(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
    std::snprintf(name, sizeof(name), "%s armor"
      ,mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  }
  break;
      case IT_FOOD:
  std::snprintf(name, sizeof(name), "%s armor"
    ,mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  break;
      case IT_RESOURCE:
  if(!is_limit_mrp
     || PER_DICE < 50 + cd[n].lv[LV_CREATER]/100 - cd[n].id[in1].MRP
     || PER_DICE < 5){
    cd[n].id[in1].MRP = 99 - (99 - cd[n].id[in1].MRP) * 95 / 100;
    i = std::strlen(cd[n].id[in1].name);
    if (cd[n].id[in1].name[i - 2] == ' '
        && cd[n].id[in1].name[i - 1] == '+')
      CPYSTR(name, cd[n].id[in1].name);
    else if (i < 30)
      std::snprintf(name, sizeof(name), "%s +", cd[n].id[in1].name);
    else
      CPYSTR(name, "Enhance armor");
  } else
    CPYSTR(name, cd[n].id[in1].name);
  break;
      case IT_JEWEL:
  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }
  std::snprintf(name, sizeof(name), "%s armor", (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL) ?
        fn_name[cd[n].id[in1].fn] : mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  break;
      }
      break;
    case IT_ACCESSORY:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
      case IT_WEAPON:
      case IT_CHARGE:
  if (!std::strcmp(wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE],
        wt_aname[(cd[n].id[in2].type & WT_MASK) / WT_PLACE])) {
    cd[n].id[in1].wp += cd[n].id[in2].wp / 2;
  }
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_FOOD:
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_RESOURCE:
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK & ~WD_MASK) | IT_CHARGE);
  if (cd[n].id[in1].fn == FN_NONE)
    cd[n].id[in1].fn = frand(6) + 1;
  cd[n].id[in1].CHARGE = 10;
  cd[n].id[in1].UE = frand(0x24) + 1; /* bug(?)fix & add 20010126 1231 */
  std::snprintf(name, sizeof(name), "%s %s", ((cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL)
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_JEWEL:
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }

  if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_ACCESSORY:
  if (cd[n].id[in1].SE == cd[n].id[in2].SE) {
    sef = 1;
    cd[n].id[in1].ATP++;
    cd[n].id[in1].wp++;
  } else if (cd[n].id[in1].AT == cd[n].id[in2].AT) {
    if (fn(cd[n].id[in1].fn,cd[n].id[in2].fn,100) != 25 && (cd[n].id[in1].fn != FN_NONE || cd[n].id[in2].fn!= FN_NONE))
      cd[n].id[in1].SE =
        cd[n].id[in2].SE = SE_NONE;
    if (cd[n].id[in1].SE == SE_NONE) {
      if (cd[n].id[in2].SE == SE_NONE) {
        cd[n].id[in1].SE = frand(21) + 1;
        switch (cd[n].id[in1].SE) {
        case SE_ATPUP:  /* +1で効果反転 */
        case SE_DEFUP:
        case SE_MRPUP:
        case SE_SPELLUP:
        case SE_HASTE:
        case SE_CLAIR:
    cd[n].id[in1].SE++;
    break;
        case SE_ILLUSION:/* しかたないので */
    cd[n].id[in1].SE = SE_CONTRAST;
    break;
        case SE_REGENE:
    cd[n].id[in1].SE = SE_POISON;
    break;
        case SE_WILL:
    cd[n].id[in1].SE = SE_PANIC;
    break;
        }
      } else
        cd[n].id[in1].SE = cd[n].id[in2].SE;
    }
    sef = 1;
  } else if (cd[n].id[in1].AT > cd[n].id[in2].AT) {
    cd[n].id[in1].AT = cd[n].id[in2].AT;
  }
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  if (cd[n].id[in1].SE != SE_NONE)
    std::snprintf(name, sizeof(name), "%s %s", se_name[cd[n].id[in1].SE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      }
      break;
    default:
    case IT_WEAPON:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
      case IT_WEAPON:
      case IT_CHARGE:
      case IT_ACCESSORY:
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK) | IT_WEAPON);
  if ((cd[n].id[in1].type & MT_MASK) == (cd[n].id[in2].type & MT_MASK))
    cd[n].id[in1].wp += cd[n].id[in2].wp;
  else {
    cd[n].id[in1].wp /= 2;
    cd[n].id[in2].wp /= 2;
  }
  cd[n].id[in1].ER = kMtEr[(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
  cd[n].id[in1].AE = ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL
       && (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL ? frand(13)
          : AE_NONE);
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].AE == AE_NONE
           ? mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]
        : ae_name[cd[n].id[in1].AE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_FOOD:
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].AE == AE_NONE
           ? mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]
        : ae_name[cd[n].id[in1].AE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_RESOURCE:
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK & ~WD_MASK) | IT_CHARGE);
  if (cd[n].id[in1].fn == FN_NONE)
    cd[n].id[in1].fn = frand(6) + 1;
  cd[n].id[in1].CHARGE = 10;
  cd[n].id[in1].UE = frand(0x24) + 1; /* bug(?)fix & add 20010126 1231 */
  if (frand(5) == 1)
    cd[n].id[in1].SE = SE_HITTOUSE;
  else
    cd[n].id[in1].SE = SE_HITISUSE;
  sef = 1;
  std::snprintf(name, sizeof(name), "%s %s", ((cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL)
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_JEWEL:
  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }
  std::snprintf(name, sizeof(name), "%s %s",  (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL) ?
        fn_name[cd[n].id[in1].fn] : mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      }
      break;
    case IT_FOOD:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
  i = in1;
  in1 = in2;
  in2 = i;
  std::snprintf(name, sizeof(name), "%s armor"
    ,mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  break;
      case IT_WEAPON:
  i = in1;
  in1 = in2;
  in2 = i;
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].AE == AE_NONE
           ? mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]
        : ae_name[cd[n].id[in1].AE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_JEWEL:
  i = in1;
  in1 = in2;
  in2 = i;
  CPYSTR(name, "Jewel seed");
  break;
      case IT_CHARGE:
  i = in1;
  in1 = in2;
  in2 = i;
  if ((cd[n].id[in1].SE == SE_HITISUSE) || (cd[n].id[in1].SE == SE_HITTOUSE))
    sef = 1;
  if (cd[n].id[in1].fn == FN_NONE) {
    cd[n].id[in1].fn = frand(6) + 1;
  }
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_FOOD:
  cd[n].id[in1].type = (MT_FOOD | WT_FIST | IT_FOOD);
  cd[n].id[in1].wp += cd[n].id[in2].wp;
  cd[n].id[in1].fn = FN_NONE;
  cd[n].id[in1].VALUE = cd[n].id[in2].VALUE;
  if (cd[n].id[in1].wp > cd[n].id[in1].ENTROP)
    if (cd[n].id[in2].wp > cd[n].id[in2].ENTROP) {
      if (cd[n].id[in1].ENTROP > cd[n].id[in2].ENTROP)
        cd[n].id[in1].ENTROP = cd[n].id[in2].ENTROP;
    } else
      cd[n].id[in1].ENTROP = cd[n].id[in1].wp - 10;
  else if (cd[n].id[in2].wp > cd[n].id[in2].ENTROP)
    cd[n].id[in1].ENTROP--;
  else if (cd[n].id[in1].ENTROP < cd[n].id[in2].ENTROP)
    cd[n].id[in1].ENTROP = cd[n].id[in2].ENTROP;

  if (cd[n].id[in1].VALUE2 == 0)
    if (std::strcmp(cd[n].id[in1].name, "Food"))
      cd[n].id[in1].VALUE2 = std::abs(cd[n].id[in1].name[0] - 'N') % 30;
  if (cd[n].id[in2].VALUE2 == 0)
    if (std::strcmp(cd[n].id[in2].name, "Food"))
      cd[n].id[in2].VALUE2 = std::abs(cd[n].id[in2].name[0] - 'N') % 30;
  cd[n].id[in1].VALUE2 = ((cd[n].id[in1].VALUE2) * 2 + cd[n].id[in2].VALUE2) / 3 + 1;
  if (cd[n].id[in1].VALUE2 * 80 > cd[n].lv[LV_CREATER]) {
    cd[n].id[in1].VALUE2 = 0;
    CPYSTR(name, "Food");
  } else {
    if (cd[n].id[in1].VALUE2 > 99) cd[n].id[in1].VALUE2 = 99;  /* 19990915 fixed by Aleph */
    if (cd[n].id[in1].VALUE2 < 1 ) cd[n].id[in1].VALUE2 = 1;   /*                         */
    CPYSTR(name, foods[cd[n].id[in1].VALUE2]);
  }
  break;
      case IT_RESOURCE:
  cd[n].id[in1].type = (MT_MAGICAL | WT_FIST | IT_RESOURCE);
  cd[n].id[in1].wp = 1;
  cd[n].id[in1].fn = FN_NONE;
  cd[n].id[in1].VALUE = frand(11);  /* PHI:00055 Bug Fix by EL */
  CPYSTR(name, rs_effect[cd[n].id[in1].VALUE]);
  break;
      case IT_ACCESSORY:
  i = in1;
  in1 = in2;
  in2 = i;
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      }
      break;
    case IT_RESOURCE:
      cd[n].id[in1].type = (MT_MAGICAL | WT_FIST | IT_RESOURCE);
      cd[n].id[in1].wp = 1;
      cd[n].id[in1].fn = FN_NONE;
      cd[n].id[in1].VALUE = frand(11);
      CPYSTR(name, rs_effect[cd[n].id[in1].VALUE]);
      break;
    case IT_JEWEL:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
  i = in1;
  in1 = in2;
  in2 = i;
  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }
  std::snprintf(name, sizeof(name), "%s armor",  (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL) ?
        fn_name[cd[n].id[in1].fn] : mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
  break;
      case IT_WEAPON:
      case IT_ACCESSORY:
  i = in1;
  in1 = in2;
  in2 = i;
  if ((cd[n].id[in1].type & IT_MASK) == IT_WEAPON) {
    cd[n].id[in1].AE = AE_NONE;
  }
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK) | IT_ACCESSORY);
  if (fn(cd[n].id[in2].fn, cd[n].id[in1].fn, 100) == 200)
    cd[n].id[in1].AT = AT_NONE;

  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }

  if ((cd[n].id[in1].AT != AT_NONE) && (cd[n].id[in1].AT != AT_ABSORB))
    std::snprintf(name, sizeof(name), "%s %s", at_name[cd[n].id[in1].AT], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else if (cd[n].id[in1].fn != FN_NONE && cd[n].id[in1].fn <= FN_NORMAL)
    if (cd[n].id[in1].AT == AT_ABSORB)
      std::snprintf(name, sizeof(name), "%s absorber", fn_name[cd[n].id[in1].fn]);
    else
      std::snprintf(name, sizeof(name), "%s %s", fn_name[cd[n].id[in1].fn], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  else
    std::snprintf(name, sizeof(name), "%s %s", mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE], wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_CHARGE:
  i = in1;
  in1 = in2;
  in2 = i;
  if ((cd[n].id[in1].SE == SE_HITISUSE) || (cd[n].id[in1].SE == SE_HITTOUSE))
    sef = 1;
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }
  break;
      case IT_JEWEL:
  cd[n].id[in1].type = (MT_SILVER | WT_FIST | IT_JEWEL);
  cd[n].id[in1].wp += cd[n].id[in2].wp;
  cd[n].id[in1].VALUE += cd[n].id[in2].VALUE;
  CPYSTR(name, "Jewel seed");
  break;
      case IT_FOOD:
  CPYSTR(name, "Jewel seed");
  break;
      case IT_RESOURCE:
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK & ~WD_MASK) | IT_CHARGE);
  cd[n].id[in1].CHARGE = 10;
  cd[n].id[in1].UE = frand(0x24) + 1; /* bug(?)fix & add 20010126 1231 */
  if (cd[n].id[in1].fn == FN_NONE)
    cd[n].id[in1].fn = frand(6) + 1;
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE]), wt_aname[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      }
      break;
    case IT_CHARGE:
      switch (cd[n].id[in2].type & IT_MASK) {
      default:
      case IT_ARMOR:
      case IT_WEAPON:
      case IT_CHARGE:
      case IT_ACCESSORY:
  cd[n].id[in1].type = ((cd[n].id[in1].type & ~IT_MASK & ~WD_MASK) | IT_WEAPON);
  if ((cd[n].id[in1].type & MT_MASK) == (cd[n].id[in2].type & MT_MASK))
    cd[n].id[in1].wp += cd[n].id[in2].wp;
  cd[n].id[in1].ER = kMtEr[(cd[n].id[in1].type & MT_MASK) / MT_PLACE];
  cd[n].id[in1].AE = ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL
       && (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL ? frand(13)
          : AE_NONE);
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].AE == AE_NONE
           ? mt_name[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]
        : ae_name[cd[n].id[in1].AE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_FOOD:
  if ((cd[n].id[in1].SE == SE_HITISUSE) || (cd[n].id[in1].SE == SE_HITTOUSE))
    sef = 1;
  if (cd[n].id[in1].fn == FN_NONE)
    cd[n].id[in1].fn = frand(6) + 1;
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      case IT_RESOURCE:
  cd[n].id[in1].CHARGE += 10;
  if (cd[n].id[in1].CHARGE > 99)
    cd[n].id[in1].CHARGE = 99;
  if ((cd[n].id[in1].SE == SE_HITISUSE) || (cd[n].id[in1].SE == SE_HITTOUSE))
    sef = 1;
  if (cd[n].id[in1].fn == FN_NONE)
    cd[n].id[in1].fn = frand(6) + 1;
  CPYSTR(name, cd[n].id[in1].name);
  /*  snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]); */
  break;
      case IT_JEWEL:
  if ((cd[n].id[in1].SE == SE_HITISUSE) || (cd[n].id[in1].SE == SE_HITTOUSE))
    sef = 1;
  /* add for new fn 20000327*/
  switch (cd[n].id[in2].fn) {
    case FN_RAINBOW:
      if (cd[n].id[in1].fn == FN_RAINBOW ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_RAINBOW;
      else
        cd[n].id[in1].fn = frand(7);
      break;
    case FN_PRISM:
      if (cd[n].id[in1].fn == FN_PRISM ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_PRISM;
      else {
        if (cd[n].id[in1].fn == FN_NONE)
          cd[n].id[in1].fn = frand(6) + 1;
        cd[n].id[in1].fn++;
        if (cd[n].id[in1].fn > FN_NORMAL)
          cd[n].id[in1].fn = FN_EARTH;
      }
      break;
    case FN_MIRROR:
      if (cd[n].id[in1].fn == FN_MIRROR ||
         ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
          (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
           PER_DICE <  25))
        cd[n].id[in1].fn = FN_MIRROR;
      break;
    default:
      cd[n].id[in1].fn = cd[n].id[in2].fn;
      if (cd[n].id[in1].fn == FN_NONE) {
        if ((cd[n].id[in1].type & MT_MASK) == MT_MAGICAL &&
            (cd[n].id[in2].type & MT_MASK) == MT_MAGICAL &&
             PER_DICE <  25)
          cd[n].id[in1].fn = std::abs(frand(16) - 6);
        else
          cd[n].id[in1].fn = frand(6) + 1;
      }
  }
  std::snprintf(name, sizeof(name), "%s %s", (cd[n].id[in1].UE < 9 && cd[n].id[in1].fn <= FN_NORMAL
        ? fn_name[cd[n].id[in1].fn] : ci_name[cd[n].id[in1].UE])
    ,wt_name[(cd[n].id[in1].type & WT_MASK) / WT_PLACE]);
  break;
      }
      break;
    }
    switch (cd[n].id[in1].type & WT_MASK) {
    case WT_FIST:
    default:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 8
          ? cd[n].id[in1].wp * 2 : (16 - cd[n].id[in1].wp) * 2);
      break;
    case WT_SWORD:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 32
         ? cd[n].id[in1].wp : (96 - cd[n].id[in1].wp) / 2);
      break;
    case WT_BLADE:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 32
        ? cd[n].id[in1].wp * 3 / 4 : (96 - cd[n].id[in1].wp) * 3 / 8);
      break; /* bug? fix 20000722 */
    case WT_STINGER:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 16
        ? cd[n].id[in1].wp * 3 / 2 : (32 - cd[n].id[in1].wp) * 3 / 2);
      break; /* bug? fix 20000327 */
    case WT_SPEAR:
      if (cd[n].id[in1].wp < 16)
  cd[n].id[in1].ATP = cd[n].id[in1].wp * 3 / 2;
      else if (cd[n].id[in1].wp < 32)
  cd[n].id[in1].ATP = (cd[n].id[in1].wp - 16) / 2 + 24;
      else
  cd[n].id[in1].ATP = (96 - cd[n].id[in1].wp) / 2;
      break; /* bug? fix 20000327 */
    case WT_AXE:
      if (cd[n].id[in1].wp < 16)
  cd[n].id[in1].ATP = cd[n].id[in1].wp / 2;
      else if (cd[n].id[in1].wp < 32)
  cd[n].id[in1].ATP = (cd[n].id[in1].wp - 16) * 3 / 2 + 8;
      else
  cd[n].id[in1].ATP = (96 - cd[n].id[in1].wp) / 2;
      break;
    case WT_STRIKER:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 32
          ? cd[n].id[in1].wp / 2 : (96 - cd[n].id[in1].wp) / 4);
      break;
    case WT_WHIP:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 16
         ? cd[n].id[in1].wp : (32 - cd[n].id[in1].wp));
      break;
    case WT_FANG:
      cd[n].id[in1].ATP = (cd[n].id[in1].wp < 4
           ? cd[n].id[in1].wp * 4 : (8 - cd[n].id[in1].wp) * 4);
      break;
    }
    if (sef == 0)
      cd[n].id[in1].SE = SE_NONE;
    cd[n].id[in1].ATP = (int) ((double) cd[n].id[in1].ATP
          * kMtAtp[(cd[n].id[in1].type & MT_MASK) / MT_PLACE]);
    if ((cd[n].id[in1].type & IT_MASK) == IT_WEAPON)
      cd[n].id[in1].ER = kMtEr[(cd[n].id[in1].type & MT_MASK) / MT_PLACE];

    cd[n].id[in1].name = findstr(name);
  }
  cd[n].id[in1].active_time = 0;
  if (cd[n].id[in1].wp <= 0) {
    delete_item(&(cd[n].id[in1]));
  }
  if (cd[n].id[in1].wp > 99)
    cd[n].id[in1].wp = 99;
  if (cd[n].id[in1].ATP < 1)
    cd[n].id[in1].ATP = 1;
  if (cd[n].id[in1].ATP > 99)
    cd[n].id[in1].ATP = 99;
  if (cd[n].id[in1].op[1] > 99)
    cd[n].id[in1].op[1] = 99;
  if (cd[n].id[in1].op[2] > 99)
    cd[n].id[in1].op[2] = 99;
  delete_item(&(cd[n].id[in2]));
  if (cd[n].id[in1].wp <= 0) {
    dmprintf(n, 0, buf.data(), buf.size(),DMM_CREATE_NOTHING);
    cd[n].weq = nullptr;
  } else {
    if (cd[n].oeq == &(cd[n].id[in1])) {  /* niju soubi kinsi */
      cd[n].oeq = nullptr;
      cd[n].osep = 0;
      cd[n].at = AT_NONE;
    }
    if (cd[n].aeq == &(cd[n].id[in1])) {
      cd[n].aeq = nullptr;
      cd[n].asep = 0;
    }
    cd[n].weq = &(cd[n].id[in1]);
    cd[n].wsep = sep_calc(n, cd[n].weq);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_CREATE);
  }
  write_companion(n, 0, buf.data(), 0);
  lv_up(n, n, LV_CREATER, 0);
  item_gc(n);
  set_fn(n);
  return;
}

/* add 20020528 */
int detect_fn(int n,int fn,int vsfn,char *fmt)
{
   std::array<char, 256> buf;

#ifdef NO_SKILL
    if (cd[n].exp == 99999999 || fn <= FN_NORMAL ||
        ( ((vsfn == -2) || (PER_DICE < cd[n].lv[LV_MERCHANT] / 1000)) &&
          ((vsfn == -1) || (PER_DICE < cd[n].lv[LV_WIZARD] / 1000))))
        fn = fn;
    else {
      if (fn == FN_RAINBOW)
        fn = frand(FN_NORMAL+1);
      else if (vsfn < FN_NONE) {
        if (PER_DICE <= 50)
          fn = FN_NONE;
        else
          fn = frand(FN_NORMAL/2) * 2 + (fn == FN_MIRROR ? 1 : 0);
	  } else {
	    if (vsfn == FN_NONE || vsfn > FN_NORMAL)
          fn = FN_NONE;
        else if (fn == FN_MIRROR)
          fn = vsfn;
        else if (fn == FN_PRISM) {
          fn = vsfn + 1;
          if (fn > FN_NORMAL)
                fn = FN_EARTH;
        }
      }
    }
#else
    if (cd[n].exp != 99999999 && fn >= 0) {
      if (fn <= FN_NORMAL) {
        /* Normal */
        if (PER_DICE >= (100-100/(cd[n].skill[SKL_MAGIC_FN][fn]/100+1)))
          fn = -1;
      } else {
        /* Variable */
        if (PER_DICE >= (100-100/(cd[n].skill[SKL_MAGIC_FN][fn]/100+1))) {
          if (fn == FN_RAINBOW)
            fn = frand(FN_NORMAL+1);
          else if (vsfn < FN_NONE) {
            if (PER_DICE <= 50)
              fn = FN_NONE;
            else
              fn = frand(FN_NORMAL/2) * 2 + (fn == FN_MIRROR ? 1 : 0);
          } else {
            if (vsfn == FN_NONE || vsfn > FN_NORMAL)
              fn = FN_NONE;
            else if (fn == FN_MIRROR)
              fn = vsfn;
            else if (fn == FN_PRISM) {
              fn = vsfn + 1;
              if (fn > FN_NORMAL)
                       fn = FN_EARTH;
            }
          }

          if (PER_DICE >= (100-100/(cd[n].skill[SKL_MAGIC_FN][fn]/100+1)))
            fn = -1;        
        }
      }
   }
#endif

   std::snprintf(buf.data(), buf.size(), fmt, (fn >= 0 ? fn_name[fn] : "????" ));

   check_write(n, buf.data());
   return (fn);
}

void detect_item(int n, int in)
{
  /* buf は CPYSTR/CATSTR (sizeof(d) を使うマクロ) で使うため C 配列のまま */
  char   buf[256];
  int    i;
  int    ok;
  ItemData *id;
  int    dtfn;

  id = &(cd[n].id[in]);
  i = cd[n].id[in].wp * ((id->type & MT_MASK)/MT_PLACE+1)
       * (10000-cd[n].lv[LV_MERCHANT])/10000;
  if (nciflag) {
    i = i * cirate / 100 + 1;
  } else {
    i = i + 1;
  }
  if (tax < 0)
    i = i * (10000 - tax) / 10000;
  if (cd[n].gp < i) {
    dmcheck_write(n, DMM_DETECT_NO_TAX);
    return;
  }
  cd[n].gp -= i;
  tax += i;
  if (PER_DICE > (cd[n].lv[LV_MERCHANT] + 600) / 20)
    dmcheck_write(n, DMM_DETECT_NO);
  else {
#ifndef NO_SKILL
    ok = (PER_DICE < 10 + 45 * std::log10(cd[n].skill[SKL_DETECT_IT][id->type & IT_MASK]/100+1.0));
#endif
    std::snprintf(buf, sizeof(buf), "  Name  : %-31s   Wp  : %2i \n", id->name, id->wp);
    check_write(n, buf);
#ifdef NO_SKILL
    std::snprintf(buf, sizeof(buf), "  Type  : %-15s \n", it_name[cd[n].id[in].type & IT_MASK]);
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "        : %-15s \n", mt_name[(cd[n].id[in].type & MT_MASK) / 0x100]);
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "        : %-15s \n", wt_name[(cd[n].id[in].type & WT_MASK) / 0x10]);
    check_write(n, buf);
#else
    std::snprintf(buf, sizeof(buf), "  Type  : %-15s \n", (ok ? it_name[id->type & IT_MASK] : "????"));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "        : %-15s \n", 
      (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_MT][(id->type & MT_MASK)/MT_PLACE]/100+1.0) ?
       mt_name[(id->type & MT_MASK) / MT_PLACE] : "????"));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "        : %-15s \n", 
      (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_WT][(id->type & WT_MASK)/WT_PLACE]/100+1.0) ?
       wt_name[(id->type & WT_MASK) / WT_PLACE] : "????"));
    check_write(n, buf);
#endif
    /* change for master detect 20000607 for skill 20020527 */
    dtfn = detect_fn(n, id->fn, -1, "   Fn   : %-15s \n");
#ifdef NO_SKILL
      std::snprintf(buf, sizeof(buf), "   ATP  : %2i \n", id->ATP);
#else
    if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_WEAPON]/100+1.0))
      std::snprintf(buf, sizeof(buf), "   ATP  : %2i \n", id->ATP);
    else
      std::snprintf(buf, sizeof(buf), "   ATP  : ?? \n");
#endif
    check_write(n, buf);
    if ((id->type & WD_MASK) == WD_FORE)
      check_write(n, "          not weapon\n");
    else {
      CPYSTR(buf, "          attack to ");
#ifndef NO_SKILL
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_WEAPON]/100+1.0)) {
#endif
        if (!(id->type & WD_FORE))
    CATSTR(buf, "FORE ");
        if (id->type & WD_BACK)
    CATSTR(buf, "BACK ");
        if (id->type & WD_LEFT)
    CATSTR(buf, "LEFT ");
        if (id->type & WD_RIGHT)
    CATSTR(buf, "RIGHT ");
        CATSTR(buf, "\n");
#ifndef NO_SKILL
      } else {
    CATSTR(buf, "?? ");
      }
#endif
      check_write(n, buf);
    }
#ifndef NO_SKILL
    if (ok)
#endif
    switch (id->type & IT_MASK) {
    case IT_ARMOR:
#ifdef NO_SKILL
        std::snprintf(buf, sizeof(buf), "   DFP  : %2i%%  MRP  : %2i%% \n"
          ,id->DFP, id->MRP);
#else
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_ARMOR]/100+1.0))
        std::snprintf(buf, sizeof(buf), "   DFP  : %2i%%  MRP  : %2i%% \n"
          ,id->DFP, id->MRP);
      else
        CPYSTR(buf, "   DFP  : ??%  MRP  : ??% \n");
#endif
      check_write(n, buf);
      break;
    case IT_WEAPON:
#ifdef NO_SKILL
        std::snprintf(buf, sizeof(buf), "   ER   : %2i%% \n", id->ER);
#else
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_WEAPON]/100+1.0))
        std::snprintf(buf, sizeof(buf), "   ER   : %2i%% \n", id->ER);
      else
        CPYSTR(buf, "   ER   : ??% \n");
#endif
      check_write(n, buf);
#ifdef NO_SKILL
        std::snprintf(buf, sizeof(buf), " Effect : %-15s \n", ae_name[id->AE]);
#else
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_WEAPON]/100+1.0))
        std::snprintf(buf, sizeof(buf), " Effect : %-15s \n", ae_name[id->AE]);
      else
        CPYSTR(buf, " Effect : ???? \n");
#endif
      check_write(n, buf);
      break;
    case IT_FOOD:
#ifndef NO_SKILL
      i = cd[n].skill[SKL_DETECT_IT][IT_FOOD]/100;
      if (PER_DICE < 100 - (id->VALUE2 - i)) {
#endif
        std::snprintf(buf, sizeof(buf), "  Value : %2i \n", id->VALUE);
        check_write(n, buf);
        if (id->VALUE2 > 0) {
    std::snprintf(buf, sizeof(buf), "Food Lv : %2d \n", id->VALUE2);
    check_write(n, buf);
        }
#ifndef NO_SKILL
      } else {
        CPYSTR(buf, "  Value : ?? \n");
        check_write(n, buf);
      }
#endif
      if (id->ENTROP == 0)
  i = 0;
      else
  i = id->wp - id->ENTROP;
      if (i < -10)
  check_write(n, "          spoiled food. \n");
      else if (i > 10)
  check_write(n, "          flesh food. \n");
      else if (frand(20) + i > 10)
  check_write(n, "          flesh food ? \n");
      else
  check_write(n, "          spoiled food ? \n");
      break;
    case IT_RESOURCE:
#ifndef NO_SKILL
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_RESOURCE]/100+1.0))
#endif
      std::snprintf(buf, sizeof(buf), " Effect : %-15s \n", rs_effect[id->VALUE]);
      check_write(n, buf);
      break;
    case IT_JEWEL:
#ifndef NO_SKILL
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_JEWEL]/100+1.0)) {
#endif
      i = id->VALUE * 100 + id->VALUE2;
	  i *= cirate;
	  i /= 100;
      std::snprintf(buf, sizeof(buf), "  Value : %10i %s \n", i, cirunit);
#ifndef NO_SKILL
      } else
        std::snprintf(buf, sizeof(buf), "  Value : ???? %s \n", cirunit);
#endif
      check_write(n, buf);
      break;
    case IT_SPELL_JEWEL:
      std::snprintf(buf, sizeof(buf), "  Spell : %-31s \n", id->name);
      check_write(n, buf);
      std::snprintf(buf, sizeof(buf), "  Power : %3i \n", mgstrlen(id->name));
      check_write(n, buf);

#ifdef NO_SKILL
      detect_fn(n, id->fn, -1,   "  Fn    : %-31s \n");
#else
      /* detect skill bug fix 20021119 */
      std::snprintf(buf, sizeof(buf), "  Fn    : %-31s \n", (dtfn >= 0 ? fn_name[dtfn] : "????" ));
      check_write(n, buf);
#endif
      i = (cd[n].skill[SKL_DETECT_IT][IT_SPELL_JEWEL] +
           cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_CHARGE] +
           cd[n].lv[LV_WIZARD])/3;
      if (PER_DICE < 10+45*std::log10(i/100+1.0)) {
        std::snprintf(buf, sizeof(buf), " Effect : %-31s \n", use_effect[id->UE]);
        skillup(n,SKL_MAGIC_SP,SKL_MGSP_ANALYZE,0,30,1);
        skillup(n,SKL_MAGIC_SP,SKL_MGSP_CHARGE,0,30,1);
      } else
#ifdef NO_SKILL
      std::snprintf(buf, sizeof(buf), " Effect : %-31s \n", use_effect[id->UE]);
#else
        CPYSTR(buf, " Effect : ???? \n");
#endif
      check_write(n, buf);
      break;
    case IT_CHARGE:
#ifndef NO_SKILL
      i = (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_CHARGE]/100+1.0));
      if (!i || PER_DICE >= cd[n].skill[SKL_DETECT_IT][IT_CHARGE]/100)
  CPYSTR(buf, " Charge : ?? \n");
      else
#endif
      if (id->CHARGE > 0)
  std::snprintf(buf, sizeof(buf), " Charge : %2i \n", id->CHARGE);
      else
  std::snprintf(buf, sizeof(buf), " Charge : Infinity \n");
      check_write(n, buf);
#ifndef NO_SKILL
      if (!i) {
  CPYSTR(buf, " Effect : ???? \n");
      } else
#endif
      if (id->UE == EF_APPEAR) {
  check_write(n, " Effect : Appear\n");
      } else if (id->UE == EF_DISAPPEAR) {
  check_write(n, " Effect : Disappear\n");
      } else {
  std::snprintf(buf, sizeof(buf), " Effect : %-31s \n", use_effect[id->UE]);
      }
  check_write(n, buf);
      break;
    case IT_ACCESSORY:
#ifndef NO_SKILL
      if (PER_DICE >= 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_ACCESSORY]/100+1.0))
  CPYSTR(buf, " Effect : ???? \n");
      else
#endif
      if (id->AT > MAX_AT)
  std::snprintf(buf, sizeof(buf), " Effect : None \n");
      else
  std::snprintf(buf, sizeof(buf), " Effect : %-15s \n", at_name[id->AT]);
      check_write(n, buf);
      break;
    case IT_FOREIGN:
#ifndef NO_SKILL
      if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_FOREIGN]/100+1.0))
#endif
      std::snprintf(buf, sizeof(buf), "  Value : %4i gp \n",
        id->VALUE * 100 + id->VALUE2);
      check_write(n, buf);
      break;
    case IT_SCROLL:
            check_write(n, "  please use to reading this. \n");
            break;
    case IT_EVENTLY:
            check_write(n, "  please use if you want. \n");
            break;
    }
    if ((id->type & IT_MASK) != IT_FOOD) {
      if (id->SE > SE_NONE) {
  if ((id->SE <= MAX_SE) &&
      (cd[n].exp == 99999999
       || (id->SE != SE_PASSWALL
     && id->SE != SE_NOMOVE))){
    if (curse_check(n, id->SE) < 0)
      std::snprintf(buf, sizeof(buf), "  Curse : %-15s \n", se_name[id->SE]);
    else
      std::snprintf(buf, sizeof(buf), "   S.E. : %-15s \n", se_name[id->SE]);
    check_write(n, buf);
  } else if (id->SE == SE_APPEAR) {
    check_write(n,
          " ** and ,you detect a touch of special power! **\n");
  }
      }
    }
    if (is_need_charge_time){
      if((id->type & IT_MASK) == IT_CHARGE) {
#ifndef NO_SKILL
        if (PER_DICE < 10+45*std::log10(cd[n].skill[SKL_DETECT_IT][IT_CHARGE]/100+1.0))
#endif
        {
  if(id->active_time > time(nullptr))
    check_write(n, " Status : charging \n");
  else
    check_write(n, " Status : ready \n");
        }
      }
    }
  }
  lv_up(n, n, LV_MERCHANT, 0);
  skillup(n,SKL_DETECT_IT,id->type & IT_MASK,99,50,1);
  skillup(n,SKL_DETECT_WT,(id->type & WT_MASK)/WT_PLACE,99,30,1);
  skillup(n,SKL_DETECT_MT,(id->type & MT_MASK)/MT_PLACE,99,30,1);
  return;
}

void   detect_magic(int n, int in)
{
  std::array<char, 256> buf;
  int    i, l;
  MagicData *m;

  m = &(cd[n].md[in]);
  l = mgstrlen(m->spell);

  i = l * (10000 - cd[n].lv[LV_MERCHANT]) / 10000;
  if (nciflag) {
    i = i * cirate / 100 + 1;
  } else {
    i = i + 1;
  }

  if (tax < 0)
    i = i * (10000 - tax) / 10000;
  if (cd[n].gp < i) {
    dmcheck_write(n, DMM_ANALYZE_NO_TAX);
    return;
  }
  cd[n].gp -= i;
  tax += i;

#ifdef NO_SKILL
  if (l * l * 4 > cd[n].lv[LV_WIZARD])
#else
  if (l * l * 4 > (cd[n].lv[LV_WIZARD]+cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_ANALYZE])/2)
#endif
  {
    dmcheck_write(n, DMM_ANALYZE_NO);
  } else {
    std::snprintf(buf.data(), buf.size(), "  Spell : %-31s \n", m->spell);
    check_write(n, buf.data());
    std::snprintf(buf.data(), buf.size(), "  Power : %3i \n", mgstrlen(m->spell));
    check_write(n, buf.data());
    /* add for new fn 20000327 change for master detect 20000607 */
#ifdef NO_SKILL
    detect_fn(n, m->fn, -2, "  Fn    : %-31s \n");
    std::snprintf(buf.data(), buf.size(), " Effect : %-31s \n", use_effect[m->effect]);
#else
    detect_fn(n, m->fn, -1, "  Fn    : %-31s \n");
    std::snprintf(buf.data(), buf.size(), " Effect : %-31s \n",
        (cd[n].exp != 99999999 &&
         PER_DICE < 100-100/(skill_mg_calc(n,&(cd[n].md[in]))+1) ? use_effect[(int)(m->effect)] : "????"));
#endif
    check_write(n, buf.data());
  }

  lv_up(n, n, LV_WIZARD, 0);
  skillup(n,SKL_MAGIC_SP,SKL_MGSP_ANALYZE,(l * l * 4 * 2.6 / 100),50,1);
  skillup_magic(n, 0, m, 0);

  return;
}

void   ident_to(int n, int m)
{
  int    i, l;
  /* buf は CPYSTR/CATSTR (sizeof(d) を使うマクロ) で使うため C 配列のまま */
  char   buf[256];
  int    is_master;

  l = (cd[m].lv[LV_FIGHTER]+cd[m].lv[LV_WIZARD])/100;

  i = l * (10000 - cd[n].lv[LV_MERCHANT]) / 10000;
  if (nciflag) {
    i = i * cirate / 100 + 1;
  } else {
    i = i + 1;
  }

  if (tax < 0)
    i = i * (10000 - tax) / 10000;
  if (cd[n].gp < i) {
    dmcheck_write(n, DMM_IDENTIFY_NO_TAX);
    return;
  }
  cd[n].gp -= i;
  tax += i;

  is_master =0;

  l = cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_WIZARD]
    - cd[m].lv[LV_FIGHTER] - cd[m].lv[LV_WIZARD];
  skillup(n,SKL_MAGIC_SP,SKL_MGSP_IDENT,(l < 0 ? -l : 0)/200,50,1);
#ifndef NO_SKILL
  l = (l + cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_IDENT]) / 2;
#endif
  if (l < 0)
    l = 0;

  if (cd[n].exp == 99999999) {
    l = 20000;
    is_master = 1;
  } else if ((cd[m].at == AT_STEALTH) || (cd[m].undetect == ID_ALL)
       || ((cd[m].weq != nullptr ? cd[m].weq->SE : SE_NONE) == SE_STEALTH)
       || ((cd[m].aeq != nullptr ? cd[m].aeq->SE : SE_NONE) == SE_STEALTH)
   || ((cd[m].oeq != nullptr ? cd[m].oeq->SE : SE_NONE) == SE_STEALTH)) {
    dmprintf(n, m, buf, sizeof(buf),DMM_IDENTIFY_NO);
    check_write(n, buf);
    return;
  }

  std::snprintf(buf, sizeof(buf), " Name  : %s \n", cd[m].name);
  check_write(n, buf);
  if (l >= IN_HPMP) {
    if ((cd[m].undetect & ID_HP) && (cd[m].undetect & ID_MP) && !is_master)
      std::snprintf(buf, sizeof(buf), "   Hp : ???? / ????   Mp  : ???? / ???? \n");
    else if ((cd[m].undetect & ID_HP) && !is_master)
      std::snprintf(buf, sizeof(buf), "   Hp : ???? / ????   Mp  : %4li / %4li \n", cd[m].mp, cd[m].mmp);
    else if ((cd[m].undetect & ID_MP) && !is_master)
      std::snprintf(buf, sizeof(buf), "   Hp : %4li / %4li   Mp  : ???? / ???? \n", cd[m].hp, cd[m].mhp);
    else
      std::snprintf(buf, sizeof(buf), "   Hp : %4li / %4li   Mp  : %4li / %4li \n", cd[m].hp, cd[m].mhp, cd[m].mp, cd[m].mmp);
    check_write(n, buf);
  }
  if (l >= IN_LEVEL) {
    check_write(n, " Level :\n");
    if ((cd[m].undetect & ID_LEVEL) && !is_master)
      std::snprintf(buf, sizeof(buf), "    Fighter  ????    Wizard   ???? \n");
    else
      std::snprintf(buf, sizeof(buf), "    Fighter  %4i    Wizard   %4i \n", cd[m].lv[LV_FIGHTER], cd[m].lv[LV_WIZARD]);
    check_write(n, buf);
  }
  if (l >= IN_ATP) {
    if ((cd[m].undetect & ID_ATP) && !is_master)
      std::snprintf(buf, sizeof(buf), " ATP   : ??? \n");
    else
      std::snprintf(buf, sizeof(buf), " ATP   : %3i \n", (cd[m].weq != nullptr ? cd[m].weq->ATP : 0));
    check_write(n, buf);
  }
  if (l >= IN_AFN) {
    if ((cd[m].undetect & ID_AFN) && !is_master) {
      std::snprintf(buf, sizeof(buf), " ATT Fn: ???? \n");
      check_write(n, buf);
    } else {
      /* change for master detect 20000607 */
      detect_fn(n, cd[m].afn, cd[n].dfn, " ATT Fn: %s \n");
      if (cd[m].weq != nullptr)
  if ((cd[m].weq->type & IT_MASK) == IT_WEAPON) {
    std::snprintf(buf, sizeof(buf), " Effect: %s \n", ae_name[cd[m].weq->AE]);
    check_write(n, buf);
  }
    }
  }
  if (l >= IN_DEFMRP) {
    if ((cd[m].undetect & ID_DEFMRP) && !is_master)
      std::snprintf(buf, sizeof(buf), " DFP   : ??? \n MRP    : ??? \n");
    else {
      std::snprintf(buf, sizeof(buf), " DFP   : %3i \n", (cd[m].aeq != nullptr ? cd[m].aeq->DFP : 0));
      check_write(n, buf);
      std::snprintf(buf, sizeof(buf), " MRP   : %3i \n", (cd[m].aeq != nullptr ? cd[m].aeq->MRP : 0));
    }
    check_write(n, buf);
  }
  if (l >= IN_DFN) {
    if ((cd[m].undetect & ID_DFN) && !is_master) {
      std::snprintf(buf, sizeof(buf), " DEF Fn: ???? \n");
      check_write(n, buf);
    } else {
      /* change for master detect 20000607 */
      detect_fn(n, cd[m].dfn, cd[n].afn, " DEF Fn: %s \n");
    }
  }
  if (l >= IN_KARMA) {
    if (is_master)
      std::snprintf(buf, sizeof(buf), " Karma : %s (%2i)\n", (cd[m].align < 50 ? "Law" : "Chaos"),cd[m].align);
    else if ((cd[m].undetect & ID_KARMA))
      std::snprintf(buf, sizeof(buf), " Karma : ???? \n");
    else
      std::snprintf(buf, sizeof(buf), " Karma : %s \n", (cd[m].align < 50 ? "Law" : "Chaos"));
    check_write(n, buf);
  }
  if (m >= MAX_NTTYS)
    if (l >= IN_SPEED) {
      if ((cd[m].undetect & ID_SPEED) && !is_master)
  std::snprintf(buf, sizeof(buf), " Speed : ?? \n");
      else
  std::snprintf(buf, sizeof(buf), " Speed : %2i \n", cd[m].sp);
      check_write(n, buf);
    }
  if (l >= IN_STATUS) {
    if ((cd[m].undetect & ID_STATUS) && !is_master)
      std::snprintf(buf, sizeof(buf), " Status: ???? \n");
    else {
      if (cd[m].type < CT_MAGICAL)
  std::snprintf(buf, sizeof(buf), " Status: %s \n", at_name[cd[m].at]);
      else if ((cd[m].at < AT_VISIBLE) || (cd[m].at == AT_IMMORTAL))
  std::snprintf(buf, sizeof(buf), " Status: Immortal \n"); /* 結局これ */
      else
  std::snprintf(buf, sizeof(buf), " Status: Immortal %s \n", at_name[cd[m].at]);
      check_write(n, buf);
      std::snprintf(buf, sizeof(buf), "Special: ");
      i = 0;
      if (cd[m].weq != nullptr)
  if (((cd[m].weq->type) & IT_MASK) != IT_FOOD)
    if (cd[m].weq->SE != SE_NONE) {
      CATSTR(buf, se_name[cd[m].weq->SE]);
      CATSTR(buf, " ");
      i++;
    }
      if (cd[m].aeq != nullptr)
  if (cd[m].aeq->SE != SE_NONE) {
    CATSTR(buf, se_name[cd[m].aeq->SE]);
    CATSTR(buf, " ");
    i++;
  }
      if (cd[m].oeq != nullptr)
  if (cd[m].oeq->SE != SE_NONE) {
    CATSTR(buf, se_name[cd[m].oeq->SE]);
    CATSTR(buf, " ");
    i++;
  }
      if (cd[m].innate & IA_STONEHEAD) {
  CATSTR(buf, "Stonehead ");
  i++;
      }
      if (cd[m].innate & IA_UMBRELLA) {
  CATSTR(buf, "Umbrella ");
  i++;
      }
      if (cd[m].innate & IA_FLYING) {
  CATSTR(buf, "Flying ");
  i++;
      }
      if (cd[m].innate & IA_RICK) {
  CATSTR(buf, "Rick ");
  i++;
      }
      if (cd[m].innate & IA_DRAGON) {
  CATSTR(buf, "Dragon ");
  i++;
      }
      if (cd[m].innate & IA_GOD) {
  CATSTR(buf, "Godhead ");
  i++;
      }
      if (cd[m].innate & IA_ELEMENT) {
  CATSTR(buf, "Element ");
  i++;
      }
      if (cd[m].innate & IA_DEMON) {
  CATSTR(buf, "Demon ");
  i++;
      }
      if (i == 0) {
  CATSTR(buf, "None ");
      }
      CATSTR(buf, "\n");
    }
    check_write(n, buf);
  }
#ifdef DEBUG
  if (l == 20000) {
    /* insert master detective data */
    check_write(n, "debug: \n");
    std::snprintf(buf, sizeof(buf), "afn: %i dfn: %i sfn: %i \n", cd[m].afn, cd[m].dfn, cd[m].sfn);
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "wd: %04x IA: %04x UD: %04x AT: %i \n", cd[m].wd, cd[m].innate, cd[m].undetect, cd[m].at);
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "wSE: %i aSE: %i oSE: %i \n", cd[m].wsep, cd[m].asep, cd[m].osep);
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "regene:  %4i will:     %4i \n", TCN_REGENE(m), TCN_WILL(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "coat:    %4i spell:    %4i \n", TCN_COAT(m), TCN_SPELL(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "protect: %4i shell:    %4i \n", TCN_PROTECT(m), TCN_SHELL(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "haste:   %4i illusion: %4i \n", TCN_HASTE(m), TCN_ILLUSION(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "poison:  %4i palsy:    %4i \n", TCN_POISON(m), TCN_PALSY(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "panic:   %4i confuse:  %4i \n", TCN_PANIC(m), TCN_CONFUSE(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "berserk: %4i silence:  %4i \n", TCN_BERSERK(m), TCN_SILENCE(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "karma:   %4i vanish:   %4i \n", TCN_KARMA(m), TCN_VANISH(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "blind:   %4i mirror:   %4i \n", TCN_BLIND(m), TCN_MIRROR(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "paswall: %4i gigant:   %4i \n", TCN_PASSWALL(m), TCN_GIGANT(m));
    check_write(n, buf);
    std::snprintf(buf, sizeof(buf), "nomove:  %4i raise:    %4i \n", TCN_NOMOVE(m), TCN_RAISEDEAD(m));
    check_write(n, buf);

    /* add for personal flags 20010627 */
    std::snprintf(buf, sizeof(buf), "flags: \n");
    check_write(n, buf);
    CPYSTR(buf,"  P:");
    p = buf + 4;
    for(i=0; i<PFLAG_MAX; i++) {
	  if (i==SFLAG_MAX) {
        *p++ = ' ';
        *p++ = 'S';
        *p++ = ':';
      }
      *p++ = ((cd[m].pflag & (1 << i)) ? '1' : '0');
	}
	*p = '\0';
    check_write(n, buf);

  }
#endif

/* Identify でレベルアップを削除 deleted by NEO 20120212 
  lv_up(n, m, LV_FIGHTER, 0);
  lv_up(n, m, LV_WIZARD, 0);
  lv_up(n, n, LV_MERCHANT, 0);
*/
  return;
}

void effect_analyze(int n, MagicData *m, int f)
{
  std::array<char, 256> buf;
  int    mdp;
  int    i, j, k;
  double    mdn; /* add for separate effect power/time 20000707 */
  MapData *pmap;

  pow_mg_msg(n, *m);
  mdp = mdp_calc(n, m, 1, f, &mdn);
  skillup_magic(n, 0, m, f); /* 空撃ちでも少しは上がる */
  switch (m->effect) {
  case EF_LINE:
    mg_line_attack(n, m, 0, f);
    return;
  case EF_PIERCE:
    mg_line_attack(n, m, 1, f);
    return;
  case EF_EXPLODE:
    mg_line_attack(n, m, 2, f);
    return;
  case EF_SPHERE:
    mg_sphere_attack(n, m, 0, f);
    return;
  case EF_MLINE:
    mg_line_attack(n, m, 4, f);
    return;
  case EF_MPIERCE:
    mg_line_attack(n, m, 4 | 1, f);
    return;
  case EF_MEXPLODE:
    mg_line_attack(n, m, 4 | 2, f);
    return;
  case EF_MSPHERE:
    mg_sphere_attack(n, m, 4, f);
    return;
  case EF_ROOF:
    mg_area_attack(n, m, 1, f);
    return;
  case EF_UNROOF:
    mg_area_attack(n, m, 2, f);
    return;
  case EF_AREA:
    mg_area_attack(n, m, 3, f);
    return;
  case EF_MROOF:
    mg_area_attack(n, m, 4 | 1, f);
    return;
  case EF_MUNROOF:
    mg_area_attack(n, m, 4 | 2, f);
    return;
  case EF_MAREA:
    mg_area_attack(n, m, 4 | 3, f);
    return;
/*  case EF_HEAL:*/
  case EF_HEALS:
    if (cd[n].party && !f)
      mg_heal_friend_all(n, m); /* add for party heal 20021216 */
    else
      mg_heal(n, m, f);
    return;
  case EF_SAVELIFE: /* add for save life 20000612 */
    mg_save_life(n, m, f);
    return;
  /* change for party cure 20021225 */
  case EF_ANTIDOTE:
    if (cd[n].party && !f)
      mg_antidote_friend_all(n, m);
    else
      mg_effect(n, m, f);
    return;
  case EF_REMOVE:
  case EF_AWAKEN:
    if (cd[n].party && !f) {
      mg_cure_friend_one(n, m);
      return;
    }
  case EF_COAT_:
  case EF_SPELL_: /* bug fix 20020529 */
  case EF_PROTECT_:
  case EF_SHELL_:
  case EF_HASTE_:
  case EF_ILLUSION_: /* add 20010226 */
  case EF_POISON:
  case EF_PANIC:
  case EF_PALSY:
  case EF_CONFUSE:
  case EF_BERSERK:
  case EF_SILENCE:
  case EF_BLIND:
  case EF_HPDIVIDE:
  case EF_MPDIVIDE:
    mg_effect(n, m, f);
    return;
  case EF_ERASE:
    mg_erase(n, m, f);
    return;
  /* add for healing mp 20000822 */
  case EF_ENCOURAGE:
    mg_encourage(n, m, f);
    return;
  }

  /* add for support party magic 20021224 */
  k = 0;
  if (m->effect != EF_SPELL &&
      m->effect >= EF_REGENE &&
      m->effect <= EF_ILLUSION) {
    pmap = &(map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]]);
    if (!f && cd[n].party)
      for(i=0;i<MAX_CHRS_INONE && !k;i++) {
        if ((j = pmap->chr[i] - 1) < 0)
          continue;
        if (cd[n].party != cd[j].party)
          continue;
        if (fld[pmap->area].flag & FF_MAGICEFFECT) {
          dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
          return;
        }
        k = j;
      }
  }

  /* added by Qajin 20000115 */
  if (!k && ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGICEFFECT)) {
    dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
    return;
  }
  switch (m->effect) {
  /* 判定位置変更 20020516 */
  case EF_HEAL:
    if (cd[n].party && !f)
      mg_heal_friend_one(n, m); /* add for party heal 20021216 */
    else
      mg_heal(n, m, f);
    break;
  /* add for remote get & put 20000612 */
  case EF_R_GET:
    /* add permission control 20000804 */
    if (map[cd[n].y][cd[n].x].area != map[cd[n].iy][cd[n].ix].area &&
        ((fld[map[cd[n].y][cd[n].x].area].flag & FF_REMOTEGET) ||
         (fld[map[cd[n].iy][cd[n].ix].area].flag & FF_REMOTEPUT))) {
        dmcheck_write(n, DMM_RGET_FAIL);
        return;
    }
    remote_get(n);
    break;
  case EF_R_PUT:
    if (map[cd[n].y][cd[n].x].area != map[cd[n].iy][cd[n].ix].area &&
        ((fld[map[cd[n].y][cd[n].x].area].flag & FF_REMOTEPUT) ||
         (fld[map[cd[n].iy][cd[n].ix].area].flag & FF_REMOTEGET))) {
        dmcheck_write(n, DMM_RPUT_FAIL);
        return;
    }
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type != DT_NONE)
        break;
    if (i >= MAX_ITEMS) {
      dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
      break;
    }
    list_inv(n);
    cd[n].st = ST_R_PUT;
    break;
  case EF_REGENE:
    display_single_magic(!k?n:k,MHeal);
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_REGENE_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_REGENE, mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_REGENE(!k?n:k) = mdp;
    break;
  case EF_COAT:
    /* add for fighter/wizard option 20000713 */
    if (CN_SPELL(n)>0) {
      if (fighter_wizard > 0) {
          CN_SPELL(n) = 0;
      } else if (fighter_wizard < 0) {
          dmcheck_write(n,DMM_COAT_NO);
          return;
      }
    }
    display_single_magic(!k?n:k,MCoat);
    mdp = mdp_calc(n, m, 1, 1, nullptr);
    /* change for separate effect power/time 20000707 */
    mdn = 100 / (double)fn(m->fn, (cd[!k?n:k].weq != nullptr ? cd[!k?n:k].weq->fn : FN_NONE), 100);
    mdp = mdp * mdn / 2;
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_COAT_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    if (!spellup_only_time)
      mdn = 1;
    if(mdn == 1)
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_COAT, mdp);
    else
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_COAT_WITH_TIME, (int)(mdp / mdn), mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_COAT(!k?n:k) = mdp;
    cd[!k?n:k].afn = m->fn;
    NCN_COAT(!k?n:k) = mdn;
    break;
  case EF_SPELL:
    /* add for fighter/wizard option 20000713 */
    if (CN_COAT(n)>0) {
      if (fighter_wizard > 0) {
          CN_COAT(n) = 0;
      } else if (fighter_wizard < 0) {
          dmcheck_write(n,DMM_SPELLUP_NO);
          return;
      }
    }
    display_single_magic(n,MCoat);
    mdp = mdp_calc(n, m, 1, 1, nullptr);
    mdp = mdp * mdp / 83; /* ballance test 20010221 */
    dmprintf(n, 0, buf.data(), buf.size(),DMM_SPELLUP, mdp);
    check_write(n, buf.data());
    CN_SPELL(n) = mdp;
    cd[n].sfn = m->fn;
    break;
  case EF_PROTECT:
    display_single_magic(!k?n:k,MProtect);
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_PROTECT_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    if(mdn == 1)
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_PROTECT, mdp);
    else
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_PROTECT_WITH_TIME, (int) (mdp / mdn), mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_PROTECT(!k?n:k) = mdp;
    cd[!k?n:k].dfn = m->fn;
    NCN_PROTECT(!k?n:k) = mdn;
    break;
  case EF_SHELL:
    display_single_magic(!k?n:k,MProtect);
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_SHELL_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    if(mdn == 1)
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_SHELL, mdp);
    else
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_SHELL_WITH_TIME, (int) (mdp / mdn), mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_SHELL(!k?n:k) = mdp;
    cd[!k?n:k].dfn = m->fn;
    NCN_SHELL(!k?n:k) = mdn;
    break;
  case EF_HASTE:
    display_single_magic(!k?n:k,MIllusion);
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_HASTE_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    if(mdn == 1)
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_HASTE, mdp);
    else
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_HASTE_WITH_TIME, (int) (mdp / mdn), mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_HASTE(!k?n:k) = mdp;
    cd[!k?n:k].dfn = m->fn;
    NCN_HASTE(!k?n:k) = mdn;
    break;
  case EF_ILLUSION:
    display_single_magic(!k?n:k,MIllusion);
    if (k) {
      dmprintf(n, k, buf.data(), buf.size(),DMM_ILLUSION_PARTY, m->spell);
      write_companion(n, k, buf.data(), 0);
    }
    if(mdn == 1)
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_ILLUSION, mdp);
    else
      dmprintf(!k?n:k, 0, buf.data(), buf.size(),DMM_ILLUSION_WITH_TIME, (int) (mdp / mdn), mdp);
    check_write(n, buf.data());
    if (k)
      check_write(k, buf.data());
    CN_ILLUSION(!k?n:k) = mdp;
    NCN_ILLUSION(!k?n:k) = mdn;
    break;
  case EF_FORCEFIELD:
    display_single_magic(n,MProtect);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_FORCEFIELD, m->spell, mdp);
    check_write(n, buf.data());
    CN_FORCE(n) = mdp;
    break;
  case EF_REFLEXFIELD:
    display_single_magic(n,MProtect);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_REFREXFIELD, m->spell, mdp);
    check_write(n, buf.data());
    CN_REFLEX(n) = mdp;
    break;
  case EF_MIRROR:
    display_single_magic(n,MProtect);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_MIRROR, m->spell, mdp);
    check_write(n, buf.data());
    CN_MIRROR(n) = mdp;
    break;
  case EF_VANISH:
    display_single_magic(n,MIllusion);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_VANISH, m->spell, mdp);
    check_write(n, buf.data());
    CN_VANISH(n) = mdp;
    break;
  }
  skillup_magic(n, n, m, f); /* 自分掛けは限界まで上がる */
  return;
}

static int big_already(int *list, int *count, int j)
/* 範囲・直線魔法用: 2x2 の大きな敵は体の複数マスが範囲に入っても
   1回だけ適用する。j をこの詠唱で処理済みなら 1、未処理なら
   記録して 0 を返す */
{
  int i;

  for (i = 0; i < *count; i++)
    if (list[i] == j)
      return (1);
  if (*count < 8)
    list[(*count)++] = j;
  return (0);
}

void   mg_heal(int n, MagicData * m, int f)
{
  int    mdp;
  std::array<char, 256> buf;
  int    x, y;
  int    i, j;
  int    backup;
  int    xx,yy,d;
  int    s;
  int    big_done[8]; int nbig = 0;  /* 2x2 の大きな敵の二重適用防止 */


  if (m->effect == EF_HEAL) {
    display_single_magic(n,MHeal);
    mdp = mdp_calc(n, m, 0, f, nullptr);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_CURE_SELF, m->spell, mdp);
    write_companion(n, 0, buf.data(), 0);
    (void) hp_dmg(n, mdp, 0);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    check_write(n, buf.data());
  } else {

	d = cd[n].d;
	x = cd[n].x;
	y = cd[n].y;
#ifndef NO_SKILL
	s = cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_SPHERE];
#endif
    for (xx = x-1; xx <= x+1; xx++)
      for (yy = y-1; yy <= y+1; yy++) {

#ifndef NO_SKILL
	if (std::abs(xx-x)+std::abs(yy-y) <= s/500)
#endif
	{

#if MAX_OBJECTS > 0
        if (is_see_magic_effects) {
			backup = dobj[yy][xx]-1;
			move_obj(MHeal,xx,yy,-1);
			display_magics();
			move_obj(MHeal,-1,-1,-1);
			move_obj(backup,xx,yy,-1);
		}
#endif

  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = map[yy][xx].chr[i] - 1) < 0)
      continue;
    if (chr_big(j) && big_already(big_done, &nbig, j))
      continue;
    if (CN_REFLEX(j) || CN_GASIFY(j) >= 60) {
      dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
      continue;
    }
    /* added by Qajin 20000115 */
    if ((fld[map[yy][xx].area].flag) & FF_MAGICEFFECT) {
      if(j==n)
        dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
      else
        dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
      continue;
    }
    mdp = mdp_calc(n, m, 0, f, nullptr);
    if (x==xx || y==yy)
      mdp /= 2;
    if (cd[j].type >= CT_MAGICAL) {
      dmprintf(n, j, buf.data(), buf.size(),DMM_INVALID_MAGICAL);
      check_write(n, buf.data());
      continue;
    } else {
      if (j == n) {
        dmprintf(n, 0, buf.data(), buf.size(),DMM_CURE_SELF
          , m->spell, mdp);
        write_companion(n, 0, buf.data(), 0);
      } else {
        dmprintf(n, j, buf.data(), buf.size(),DMM_CURE_OTHER
          , m->spell, mdp);
        if (cd[j].type == CT_NORMAL) {
    if (j >= MAX_NTTYS)
      cd[j].vf |= VF_HEAL;
    lv_up(n, j, LV_WIZARD, -1);
    skillup_magic(n, j, m, f);
        }
        write_companion(j, n, buf.data(), 0);
      }
    }
    (void) hp_dmg(j, mdp, 0);
    dmprintf(j, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    check_write(j, buf.data());
  }
      }
   }
  }
  return;
}

void   mg_heal_friend_all(int n, MagicData * m)
{
  int    mdp;
  std::array<char, 256> buf;
  int    x, y;
  int    i, j;
  double    s;

  set_view(n);

  for (x = 0; x <= 4; x++)
    for (y = 0; y <= 4; y++) {

      for (i = 0; i < MAX_CHRS_INONE; i++) {
        if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
          continue;
        if (cd[n].party != cd[j].party)
          continue;
        if (CN_REFLEX(j) || CN_GASIFY(j) >= 60) {
          dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
          continue;
        }
        if ((fld[cd[n].view[y][x].area].flag) & FF_MAGICEFFECT) {
          if(j==n)
            dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
          else
            dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
          continue;
        }
        mdp = mdp_calc(n, m, 0, 0, nullptr);

#ifdef NO_SKILL
        mdp /= 1 + (std::abs(y-1) + std::abs(x-2))/2;
#else
        s = (cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_SPHERE] +
             cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_HEAL]) / 2.0;
        mdp *= (1.0 - (2.0/3.0 - s*7/150000.0) * (std::abs(y-1)+std::abs(x-2))/5.0);
#endif

        if (cd[j].type >= CT_MAGICAL) {
          dmprintf(n, j, buf.data(), buf.size(),DMM_INVALID_MAGICAL);
          check_write(n, buf.data());
          continue;
        } else {

          display_single_magic(j, MHeal);

          if (j == n) {
            dmprintf(n, 0, buf.data(), buf.size(),DMM_CURE_SELF
              , m->spell, mdp);
            write_companion(n, 0, buf.data(), 0);
          } else {
            dmprintf(n, j, buf.data(), buf.size(),DMM_CURE_OTHER
              , m->spell, mdp);
            if (cd[j].type == CT_NORMAL) {
              if (j >= MAX_NTTYS)
                cd[j].vf |= VF_HEAL;
              lv_up(n, j, LV_WIZARD, -1);
              skillup_magic(n, j, m, 0);
            }
            write_companion(j, n, buf.data(), 0);
          }
        }

        (void) hp_dmg(j, mdp, 0);
        dmprintf(j, 0, buf.data(), buf.size(),DMM_LEFT_HP);
        check_write(j, buf.data());
      }
  }

  return;
}

void   mg_heal_friend_one(int n, MagicData * m)
{
  int    mdp;
  std::array<char, 256> buf;
  int    x, y;
  int    xx,yy;
  int    i, j, k;
  double    s, p;

  set_view(n);

  k = n; xx = yy = 0; p = 1.0;
  for (x = 0; x <= 4; x++)
    for (y = 0; y <= 4; y++) {

      for (i = 0; i < MAX_CHRS_INONE; i++) {
        if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
          continue;
        if (cd[n].party != cd[j].party)
          continue;
        if (CN_REFLEX(j) || CN_GASIFY(j) >= 60)
          continue;
        if (cd[j].type >= CT_MAGICAL)
          continue;
        if ((fld[cd[n].view[y][x].area].flag) & FF_MAGICEFFECT)
          continue;

        if ((double)cd[j].hp / cd[j].mhp < p) {
          p = (double)cd[j].hp / cd[j].mhp;
          k = j;
          xx = x; yy = y;
        }
      }
    }

    mdp = mdp_calc(n, m, 0, 0, nullptr);

#ifdef NO_SKILL
    mdp /= 1 + (std::abs(yy-1) + std::abs(xx-2))/2;
#else
    s = (cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_SPHERE] +
         cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_HEAL]) / 2.0;
    mdp *= (1.0 - (2.0/3.0 - s*7/150000.0) * (std::abs(yy-1)+std::abs(xx-2))/5.0);
#endif

    display_single_magic(k, MHeal);

    if (k == n) {
      dmprintf(n, 0, buf.data(), buf.size(),DMM_CURE_SELF
        , m->spell, mdp);
      write_companion(n, 0, buf.data(), 0);
    } else {
      dmprintf(n, k, buf.data(), buf.size(),DMM_CURE_OTHER
        , m->spell, mdp);
      if (cd[k].type == CT_NORMAL) {
        if (k >= MAX_NTTYS)
          cd[k].vf |= VF_HEAL;
        lv_up(n, k, LV_WIZARD, -1);
        skillup_magic(n, k, m, 0);
      }
      write_companion(k, n, buf.data(), 0);
    }

    (void) hp_dmg(k, mdp, 0);
    dmprintf(k, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    check_write(k, buf.data());

  return;
}

/* add for antidote friend 20021225 */
void   mg_antidote_friend_all(int n, MagicData * m)
{
  int    mdp;
  std::array<char, 256> buf;
  int    x, y;
  int    i, j;
  double    s;

  set_view(n);

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_EFFECT_APPEAR, m->spell);
  check_write(n, buf.data());

  for (x = 0; x <= 4; x++)
    for (y = 0; y <= 4; y++) {

      for (i = 0; i < MAX_CHRS_INONE; i++) {
        if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
          continue;
        if (cd[n].party != cd[j].party)
          continue;
        if (CN_REFLEX(j) || CN_GASIFY(j) >= 60) {
          dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
          continue;
        }
        if ((fld[cd[n].view[y][x].area].flag) & FF_MAGICEFFECT) {
          if(j==n)
            dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
          else
            dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
          continue;
        }
        mdp = mdp_calc(n, m, 1, 0, nullptr);

#ifdef NO_SKILL
        mdp /= 1 + (std::abs(y-1) + std::abs(x-2))/2;
#else
        s = cd[n].skill[SKL_MAGIC_EF1][SKL_MGEF1_ANTIDOTE];
        mdp *= (1.0 - (2.0/3.0 - s*7/150000.0) * (std::abs(y-1)+std::abs(x-2))/5.0);
#endif

        if (cd[j].type >= CT_MAGICAL) {
          dmprintf(n, j, buf.data(), buf.size(),DMM_INVALID_MAGICAL);
          check_write(n, buf.data());
          continue;
        } else {

          display_single_magic(j, MHeal);

          if (mdp < CN_POISON(j)) {
			if (j==n)
              dmprintf(n, 0, buf.data(), buf.size(),DMM_ANTIDOTE_SELF_NO);
            else
              dmprintf(n, j, buf.data(), buf.size(),DMM_ANTIDOTE_OTHER_NO);
            check_write(n, buf.data());
            continue;
          } else {
            if (CN_POISON(j)) {
              if (j == n) {
                dmprintf(n, 0, buf.data(), buf.size(),DMM_ANTIDOTE_SELF);
                check_write(n, buf.data());
              } else {
                dmprintf(n, j, buf.data(), buf.size(),DMM_ANTIDOTE_OTHER
                  , m->spell);
                write_companion(j, n, buf.data(), 0);
              }
            }
            if (j!=n && cd[j].type == CT_NORMAL)
              lv_up(n, j, LV_WIZARD, -1);
            CN_POISON(j) = 0;
            skillup_magic(n, j, m, 0);
          }
        }
      }
    }

  return;
}


/* add for cure friend 20021225 */
void   mg_cure_friend_one(int n, MagicData * m)
{
  int    mdp;
  std::array<char, 256> buf;
  int    x, y;
  int    xx,yy;
  int    i, j, k, p;
  double    s;

  set_view(n);

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_EFFECT_APPEAR, m->spell);
  check_write(n, buf.data());

  k = n; xx = yy = 0; p = 0;
  for (x = 0; x <= 4; x++)
    for (y = 0; y <= 4; y++) {

      for (i = 0; i < MAX_CHRS_INONE; i++) {
        if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
          continue;
        if (j == n)
          continue;
        if (cd[n].party != cd[j].party)
          continue;
        if (CN_REFLEX(j) || CN_GASIFY(j) >= 60)
          continue;
        if (cd[j].type >= CT_MAGICAL)
          continue;
        if ((fld[cd[n].view[y][x].area].flag) & FF_MAGICEFFECT)
          continue;

		switch (m->effect) {
		  case EF_REMOVE:
            if (CN_PALSY(j) < p)
              continue;
            p = CN_PALSY(j);
            break;
		  case EF_AWAKEN:
            if (p > CN_PANIC(j) && p > CN_CONFUSE(j) && p > CN_BERSERK(j))
              continue;
            if (p < CN_PANIC(j))
              p = CN_PANIC(j);
            if (p < CN_CONFUSE(j))
              p = CN_CONFUSE(j);
            if (p < CN_BERSERK(j))
              p = CN_BERSERK(j);
            break;
        }
        k = j;
        xx = x; yy = y;
      }
    }

  if (k == n)
    return;

  mdp = mdp_calc(n, m, 1, 0, nullptr);

#ifdef NO_SKILL
  mdp /= 1 + (std::abs(yy-1) + std::abs(xx-2))/2;
#else
  s = cd[n].skill[SKL_MAGIC_EF1][m->effect == EF_REMOVE ? SKL_MGEF1_REMOVE :
                                                          SKL_MGEF1_AWAKEN];
  mdp *= (1.0 - (2.0/3.0 - s*7/150000.0) * (std::abs(yy-1)+std::abs(xx-2))/5.0);
#endif

  display_single_magic(k, MHeal);

  switch (m->effect) {
    case EF_REMOVE:
      mdp = mdp / 5 + 1;
      if (mdp < CN_PALSY(k)) {
        dmprintf(n, k, buf.data(), buf.size(),DMM_REMOVE_NO);
        check_write(n, buf.data());
        break;
      }
      if (CN_PALSY(k)) {
        dmprintf(n, k, buf.data(), buf.size(),DMM_REMOVE
          , m->spell);
        write_companion(k, n, buf.data(), 0);
      }
      CN_PALSY(k) = 0;
      break;
    case EF_AWAKEN:
      if (mdp < CN_PANIC(k) || mdp < CN_CONFUSE(k) || mdp < CN_BERSERK(k)) {
        dmprintf(n, k, buf.data(), buf.size(),DMM_AWAKEN_NO);
        check_write(n, buf.data());
        break;
      }
      if (CN_PANIC(k) || CN_CONFUSE(k) || CN_BERSERK(k)) {
        dmprintf(n, k, buf.data(), buf.size(),DMM_AWAKEN
          , m->spell);
        write_companion(k, n, buf.data(), 0);
      }
      CN_PANIC(k) = 0;
      CN_CONFUSE(k) = 0;
      CN_BERSERK(k) = 0;
      break;
  }

  lv_up(n, k, LV_WIZARD, -1);
  skillup_magic(n, k, m, 0);

  return;
}


void   mg_erase(int n, MagicData * m, int f)
{
  int    mdp;
  std::array<char, 256> buf;
  int    i, j, k;
  MapData *pmap;

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_EFFECT_APPEAR, m->spell);
  check_write(n, buf.data());
  mdp = mdp_calc(n, m, 1, f, nullptr);
  /* added by Qajin 20000115 */
  if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGICEFFECT) {
    dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
  } else {
  dmcheck_write(n, DMM_ERASE_SELF);
  for (i = 0; i <= ERASE_CN; i++) {
    if (cd[n].cn[i] < 0 && cd[n].cn[i] > -0x10000) { /* change for eternal blind :-P 20000814 */
      cd[n].cn[i] += mdp;
      if (cd[n].cn[i] > 0)
  cd[n].cn[i] = 0;
    }
    if (cd[n].cn[i] > 0 && cd[n].cn[i] < 0x10000) {
      cd[n].cn[i] -= mdp;
      if (cd[n].cn[i] < 0)
  cd[n].cn[i] = 0;
    }
  }
  skillup_magic(n, n, m, f);
  }
  pmap = &(map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]]);
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (CN_GASIFY(j) >= 60) {
      dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
      continue;
    }
    /* added by Qajin 20000115 */
    if ((fld[pmap->area].flag) & FF_MAGICEFFECT) {
      dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
      continue;
    }
    for (k = 0; k <= ERASE_CN; k++) {
      dmprintf(n, j, buf.data(), buf.size(),DMM_ERASE_OTHER,
        m->spell);
      if (cd[j].cn[k] < 0 && cd[j].cn[k] > -0x10000) {
  cd[j].cn[k] += mdp;
  if (cd[j].cn[k] > 0)
    cd[j].cn[k] = 0;
      }
      if (cd[j].cn[k] > 0 && cd[j].cn[k] < 0x10000) {
  cd[j].cn[k] -= mdp;
  if (cd[j].cn[k] < 0)
    cd[j].cn[k] = 0;
      }
      skillup_magic(n, j, m, f);
    }
  }
  return;
}

/* add for healing mp 20000822 */
void   mg_encourage(int n, MagicData * m, int f)
{
  int    mdp;
  std::array<char, 256> buf;
  int    i, j, k;
  MapData *pmap;

  mdp = mgstrlen(m->spell)+4;
  mdp = mdp * mdp / 2;
  if (!f && cd[n].mp <= mdp) {
    mdp = cd[n].mp - 1;
  }
  k = 0;
  pmap = &(map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]]);
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (CN_GASIFY(j) >= 60) {
      dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
      continue;
    }
    k++;
  }
  if (k == 0)
    return;
  if ((fld[pmap->area].flag) & FF_MAGICEFFECT) {
    dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
    return;
  }
  if (!f)
    mp_dmg(n, -mdp, 0);
  mdp *= (10000 + cd[n].lv[LV_WIZARD]) / 5000;
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (CN_GASIFY(j) >= 60)
      continue;
    if (!f) {
      dmprintf(n, j, buf.data(), buf.size(),DMM_CUREM_FLOW);
      check_write(n, buf.data());
    }
    dmprintf(n, j, buf.data(), buf.size(),DMM_CUREM
            , m->spell, mdp * 5000 / (10000 + cd[j].lv[LV_WIZARD]) / k);
    write_companion(j, n, buf.data(), 0);
    mp_dmg(j, mdp * 5000 / (10000 + cd[j].lv[LV_WIZARD]) / k, 0);
    dmprintf(j, 0, buf.data(), buf.size(),DMM_LEFT_MP);
    check_write(j, buf.data());
    if (cd[j].type == CT_NORMAL) {
      lv_up(n, j, LV_WIZARD, -1);
      skillup_magic(n, j, m, f);
    }
  }
  if (!f) {
    dmprintf(n, 0, buf.data(), buf.size(),DMM_LEFT_MP);
    check_write(n, buf.data());
  }
  return;
}

void   mg_effect(int n, MagicData * m, int f)
{
  int    mrr;
  int    mdp;
  std::array<char, 256> buf;
  int    i, j;
  double    mdn;
  MapData *pmap;

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_EFFECT_APPEAR, m->spell);
  check_write(n, buf.data());
  if (m->effect == EF_ANTIDOTE) {
    display_single_magic(n,MHeal);
    /* added by Qajin 20000115 */
    if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGICEFFECT) {
      dmcheck_write(n, DMM_MAGIC_BLOCKED_HERE);
    } else {
    mdp = mdp_calc(n, m, 1, f, nullptr);
    if (mdp < CN_POISON(n)) {
      dmprintf(n, 0, buf.data(), buf.size(),DMM_ANTIDOTE_SELF_NO);
      check_write(n, buf.data());
    } else {
      if (CN_POISON(n)) {
  dmprintf(n, 0, buf.data(), buf.size(),DMM_ANTIDOTE_SELF);
  check_write(n, buf.data());
      }
      CN_POISON(n) = 0;
      skillup_magic(n, n, m, f);
    }
    }
  }
  pmap = &(map[cd[n].y+py[cd[n].d]][cd[n].x+px[cd[n].d]]);
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (CN_REFLEX(j) || CN_GASIFY(j) >= 60) {
      dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
      continue;
    }
    /* add for deny PK 20020204 */
    if (deny_pk && n < MAX_NTTYS && j < MAX_NTTYS &&
        (m->effect < EF_ANTIDOTE || m->effect > EF_AWAKEN)) {
      dmprintf(n,j,buf.data(), buf.size(),DMM_DENY_PK);
      check_write(n, buf.data());
      continue;
    }
    /* added by Qajin 20000115 */
    if ((fld[pmap->area].flag) & FF_MAGICEFFECT) {
      dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
      continue;
    }
    mrr = 50;
    if (cd[j].aeq != nullptr)
      mrr = (100 - (100 - mrr) * (100 - cd[j].aeq->MRP) / 100);
    if (TCN_ILLUSION(j) < 0)
      mrr = mrr * (100 + TCN_ILLUSION(j)) / 100;
    else if (TCN_ILLUSION(j) > 0)
      mrr = 100 - (100 - mrr) * (100 - TCN_ILLUSION(j)) / 100; /* bug(?) fix 20000825 */
    if (m->effect < EF_ANTIDOTE || m->effect > EF_AWAKEN) { /* bug(?) fix 20000821 */
      if (cd[j].type >= CT_MAGICAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_INVALID_MAGICAL);
  check_write(n, buf.data());
  continue;
      }
      if (PER_DICE + lv_dice(cd[n].lv[LV_WIZARD])
    < mrr + lv_dice(cd[j].lv[LV_WIZARD])) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_MAGIC_EFFECT_NO);
  check_write(n, buf.data());
  skillup_magic(n, j, m, f); /* 失敗でも上がる */
  continue;
      }
    }
    mdp = mdp_calc(n, m, 1, f, &mdn);
    switch (m->effect) {
    case EF_COAT_:
      display_single_magic(j,MDown);
      mdn = 100 / fn(m->fn, (cd[n].weq != nullptr ? cd[n].weq->fn : FN_NONE), 100) / 2.0;
      mdp = mdp * mdn;
      if (!spellup_only_time || !mdn) /* bug fix 20010430 */
        mdn = 1;
      if (mdn == 1)
        dmprintf(n, j, buf.data(), buf.size(),DMM_DEGRADE
          , mdp, m->spell);
      else
        dmprintf(n, j, buf.data(), buf.size(),DMM_DEGRADE_WITH_TIME
          , (int)(mdp / mdn), mdp, m->spell);
      CN_COAT(j) = -mdp;
      cd[j].afn = m->fn;
      NCN_COAT(j) = mdn;
      break;
    case EF_SPELL_:
      display_single_magic(j,MDown);
      mdp = mdp * mdp / 83; /* ballance test 20010221 */
      dmprintf(n, j, buf.data(), buf.size(),DMM_SPELLDOWN,
        mdp, m->spell);
      CN_SPELL(j) = -mdp;
      cd[j].sfn = m->fn;
      break; /* bug fix 20020516 */
    case EF_PROTECT_:
      display_single_magic(j,MDown);
      if (mdn == 1)
        dmprintf(n, j, buf.data(), buf.size(),DMM_APROTECT
          , mdp, m->spell);
      else
        dmprintf(n, j, buf.data(), buf.size(),DMM_APROTECT_WITH_TIME
          , (int) (mdp / mdn), mdp, m->spell);
      CN_PROTECT(j) = -mdp;
      cd[j].dfn = m->fn;
      NCN_PROTECT(j) = mdn;
      break;
    case EF_SHELL_:
      display_single_magic(j,MDown);
      if (mdn == 1)
        dmprintf(n, j, buf.data(), buf.size(),DMM_ASHELL
          , mdp, m->spell);
      else
        dmprintf(n, j, buf.data(), buf.size(),DMM_ASHELL_WITH_TIME
          , (int) (mdp / mdn), mdp, m->spell);
      CN_SHELL(j) = -mdp;
      cd[j].dfn = m->fn;
      NCN_SHELL(j) = mdn;
      break;
    case EF_HASTE_:
      display_single_magic(j,MDown);
      if (mdn == 1)
        dmprintf(n, j, buf.data(), buf.size(),DMM_SLOW
          , mdp, m->spell);
      else
        dmprintf(n, j, buf.data(), buf.size(),DMM_SLOW_WITH_TIME
          , (int) (mdp / mdn), mdp, m->spell);
      CN_HASTE(j) = -mdp;
      cd[j].dfn = m->fn;
      NCN_HASTE(j) = mdn;
      break;
    case EF_ILLUSION_: /* add 20010226 */
      display_single_magic(j,MDown);
      if (mdn == 1)
        dmprintf(n, j, buf.data(), buf.size(),DMM_CONTRAST
          , mdp, m->spell);
      else
        dmprintf(n, j, buf.data(), buf.size(),DMM_CONTRAST_WITH_TIME
          , (int) (mdp / mdn), mdp, m->spell);
      CN_ILLUSION(j) = -mdp;
      cd[j].dfn = m->fn;
      NCN_ILLUSION(j) = mdn;
      break;
    case EF_POISON:
      display_single_magic(j,MDown);
      if (cd[j].at == AT_POISONPROOF || cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_POISON_NO);
  check_write(n, buf.data());
  continue;
      }
      dmprintf(n, j, buf.data(), buf.size(),DMM_POISON
        , mdp, m->spell);
      CN_POISON(j) = mdp;
      break;
    case EF_PALSY:
      display_single_magic(j,MDown);
      mdp = mdp / 5 + 1;
      if (cd[j].at == AT_PALSYPROOF || cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_PALSY_NO);
  check_write(n, buf.data());
  continue;
      }
      if (CN_PALSY(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_PALSY_ALREADY);
  check_write(n, buf.data());
  continue;
      } else {
  dmprintf(n, j, buf.data(), buf.size(),DMM_PALSY
    , mdp, m->spell);
  CN_PALSY(j) = mdp;
      }
      break;
    case EF_PANIC:
      display_single_magic(j,MDown);
      if (cd[j].at == AT_MEPROOF || cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_PANIC_NO);
  check_write(n, buf.data());
  continue;
      }
      dmprintf(n, j, buf.data(), buf.size(),DMM_PANIC
        , mdp, m->spell);
      CN_PANIC(j) = mdp;
      break;
    case EF_CONFUSE:
      display_single_magic(j,MDown);
      mdp = mdp / 5 + 1;
      if (cd[j].at == AT_MEPROOF || cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_CONFUSE_NO);
  check_write(n, buf.data());
  continue;
      }
      if (CN_CONFUSE(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_CONFUSE_ALREADY);
  check_write(n, buf.data());
  continue;
      } else {
  dmprintf(n, j, buf.data(), buf.size(),DMM_CONFUSE
    , mdp, m->spell);
  CN_CONFUSE(j) = mdp;
      }
      break;
    case EF_BERSERK:
      display_single_magic(j,MDown);
      mdp = mdp / 5 + 1;
      if (cd[j].at == AT_MEPROOF || cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_BERSERK_NO);
  check_write(n, buf.data());
  continue;
      }
      if (CN_BERSERK(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_BERSERK_ALREADY);
  check_write(n, buf.data());
  continue;
      } else {
  dmprintf(n, j, buf.data(), buf.size(),DMM_BERSERK
    , mdp, m->spell);
  CN_BERSERK(j) = mdp;
      }
      break;
    case EF_SILENCE:
      display_single_magic(j,MDown);
      mdp = mdp / 5 + 1;
      if (cd[j].at == AT_IMMORTAL) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_SILENT_NO);
  check_write(n, buf.data());
  continue;
      }
      if (CN_SILENCE(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_SILENT_ALREADY);
  check_write(n, buf.data());
  continue;
      } else {
  dmprintf(n, j, buf.data(), buf.size(),DMM_SILENT
    , mdp, m->spell);
  CN_SILENCE(j) = mdp;
      }
      break;
    case EF_ANTIDOTE:
      display_single_magic(j,MHeal);
      if (mdp < CN_POISON(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_ANTIDOTE_OTHER_NO);
  check_write(n, buf.data());
  continue;
      }
      buf[0]='\0';/* bug fix 20021225 */
      if (CN_POISON(j))
  dmprintf(n, j, buf.data(), buf.size(),DMM_ANTIDOTE_OTHER
    , m->spell);
      CN_POISON(j) = 0;
      break;
    case EF_REMOVE:
      display_single_magic(j,MHeal);
      mdp = mdp / 5 + 1;
      if (mdp < CN_PALSY(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_REMOVE_NO);
  check_write(n, buf.data());
  continue;
      }
      buf[0]='\0';/* bug fix 20021225 */
      if (CN_PALSY(j))
  dmprintf(n, j, buf.data(), buf.size(),DMM_REMOVE
    , m->spell);
      CN_PALSY(j) = 0;
      break;
    case EF_AWAKEN:
      display_single_magic(j,MHeal);
      if (mdp < CN_PANIC(j) || mdp < CN_CONFUSE(j) || mdp < CN_BERSERK(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_AWAKEN_NO);
  check_write(n, buf.data());
  continue;
      }
      buf[0]='\0';/* bug fix 20021225 */
      if (CN_PANIC(j) || CN_CONFUSE(j) || CN_BERSERK(j))
  dmprintf(n, j, buf.data(), buf.size(),DMM_AWAKEN
    , m->spell);
      CN_PANIC(j) = 0;
      CN_CONFUSE(j) = 0;
      CN_BERSERK(j) = 0;
      break;
    case EF_BLIND:
      display_single_magic(j,MDown);
      mdp = mdp / 5 + 1;
      if (CN_BLIND(j) < 0) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_BLIND_ALREADY);
  check_write(n, buf.data());
  continue;
      } else {
  dmprintf(n, j, buf.data(), buf.size(),DMM_BLIND
    , mdp, m->spell);
  CN_BLIND(j) = -mdp;
      }
      break;
    case EF_HPDIVIDE:
      display_single_magic(j,MDown);
      mdp = mdp / 16 + 1;
      /* change for sheil cheat 20000522 */
      if ((cd[j].mhp == 0 && (cd[n].exp == 99999999 || n >= MAX_NTTYS)) || mdp <= 1) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEHP_NO);
  check_write(n, buf.data());
  continue;
      }
      dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEHP_ATTACK
        , m->spell);
      write_companion(j, n, buf.data(), 0);
      dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEHP
        , mdp);
      cd[j].hp = cd[j].hp / mdp + 1;
      break;
    case EF_MPDIVIDE:
      display_single_magic(j,MDown);
      mdp = mdp / 16 + 1;
      /* change for sheil cheat 20000522 */
      if ((cd[j].mmp == 0 && (cd[n].exp == 99999999 || n >= MAX_NTTYS)) || mdp <= 1) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEMP_NO);
  check_write(n, buf.data());
  continue;
      }
      dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEMP_ATTACK
        , m->spell);
      write_companion(j, n, buf.data(), 0);
      dmprintf(n, j, buf.data(), buf.size(),DMM_DIVIDEMP
        , mdp);
      cd[j].mp = cd[j].mp / mdp + 1;
      break;
    }
    if (buf[0]) /* bug fix 20021225 */
    write_companion(j, n, buf.data(), 0);
    /* 回復系以外も lvup の可能性追加 20020516 */
    if (m->effect != EF_HPDIVIDE && m-> effect != EF_MPDIVIDE) /* DIVIDEはなし :-P */
      lv_up(n, j, LV_WIZARD, (m->effect >= EF_ANTIDOTE && m->effect <= EF_AWAKEN ? -1:1));
    skillup_magic(n, j, m, f);
  }
  return;
}

namespace {
int friend_magic; /* add for party attack 20020926 */
} // namespace

/* f = 0 / line */
/* f = 1 / pierce */
/* f = 2 / explode */
void   mg_line_attack(int n, MagicData * m, int f, int f2)
{
  std::array<char, 256> buf;
  int    x, y;
  int    i, j;
  int   xx,yy;
  int    d;
  int    range,hit;
  int    big_done[8]; int nbig = 0;  /* 2x2 の大きな敵の二重適用防止 */

  d = cd[n].d;
  range = hit = 0;
#if MAX_OBJECTS > 0
  if(is_see_magic_effects) {
    move_obj(MHit,-1,-1,cd[n].d);
    move_obj(MLine[0],-1,-1,cd[n].d);
  }
#endif

  friend_magic = 0;
  if (cd[n].party) {
    set_view(n);
    for(y=0;y<3 && !friend_magic;y++) {
	  for(i=0;i<MAX_CHRS_INONE && !friend_magic;i++) {
        if ((j = cd[n].view[4-y][2].chr[i] - 1) < 0) continue;
	    if (cd[n].party == cd[j].party) friend_magic = 1;
	  }
    }
  }

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_FIRE, m->spell);
  check_write(n, buf.data());
  for (x = cd[n].x, y = cd[n].y;;) {
    if (x + px[d] < 1 || x + px[d] >= MAX_X - 1
  || y + py[d] < 1 || y + py[d] >= MAX_Y - 1
  || (map[y + py[d]][x + px[d]].tip & T_ANTIMAGIC)
#ifndef NO_SKILL
  || range-1 > cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_LINE] / 100
#endif
  ) {
      if (f & 2) {
#if MAX_OBJECTS > 0
        move_obj(MHit,-1,-1,-1);
#endif
  mg_explode(n, m, x, y, f, f2);
    }
      break;
    }

    x += px[d];
    y += py[d];
    range++;

#if MAX_OBJECTS > 0
    if(is_see_magic_effects) {
		if (f & 2) {
			move_obj(MHit,x,y,-1);
		} else {
			xx = od[MLine[0]].x;
			yy = od[MLine[0]].y;
			i = od[MLine[1]].x;
			j = od[MLine[1]].y;
			move_obj(MLine[0],x,y,-1);
			move_obj(MLine[1],xx,yy,od[MLine[0]].dir);
			move_obj(MLine[2],i,j,od[MLine[1]].dir);
		}
        display_magics();
    }
#endif

    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = map[y][x].chr[i] - 1) < 0)
  continue;
      if (chr_big(j) && big_already(big_done, &nbig, j))
  continue;
      if (!(f & 16) && TCN_MIRROR(j)) {
  dmprintf(n, j, buf.data(), buf.size(),DMM_MAGIC_REFLECT,
    m->spell);
  write_companion(j, n, buf.data(), 0);
  mg_line_attack(j, m, f | 16, f2);
#if MAX_OBJECTS > 0
    clear_magic_obj();
#endif
  return;
      }
      if (f & 2) {
#if MAX_OBJECTS > 0
          move_obj(MHit,-1,-1,-1); /* bug fix */
#endif
  mg_explode(n, m, x, y, f, f2);
#if MAX_OBJECTS > 0
    clear_magic_obj();
#endif
  return;
      }
#if MAX_OBJECTS > 0
      if(is_see_magic_effects) {
        move_obj(MHit,x,y,-1);
        display_magics();
      }
#endif
      mg_attack_to(n, j, m, f, f2);
      hit++;
      if (!(f & 1)
#ifndef NO_SKILL
       || (hit > cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_PIERCE] / 100)
#endif
         ) {
#if MAX_OBJECTS > 0
      if(is_see_magic_effects) {
			xx = od[MLine[1]].x;
			yy = od[MLine[1]].y;
			move_obj(MLine[2],xx,yy,od[MLine[1]].dir);
			display_magics();
			for(i=0;i<3;i++)
				move_obj(MLine[i],-1,-1,-1);
	        display_magics();
			move_obj(MHit,-1,-1,-1);
  			clear_magic_obj();
		}
#endif

  return;
      }
    }
  }
#if MAX_OBJECTS > 0
  clear_magic_obj();
#endif

}

void   mg_sphere_attack(int n, MagicData * m, int f, int f2)
{
  std::array<char, 256> buf;
  int    x, y;
  int    i, j;
  int    s;
  int    big_done[8]; int nbig = 0;  /* 2x2 の大きな敵の二重適用防止 */


  s = cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_SPHERE];

#if MAX_OBJECTS > 0
  if (is_see_magic_effects) {

#ifndef NO_SKILL	
    if (s / 500 > 0)
#endif
    {
      for(y=-1;y<2;y++)
        for(x=-1;x<2;x++) {
#ifndef NO_SKILL	
          if (std::abs(x)+std::abs(y) <= s/500)
#endif
          {
            move_obj(MExplode[y][x],cd[n].x+x,cd[n].y+y,-1);
          }
        }
    }
#ifndef NO_SKILL
     else {
      move_obj(MHit,cd[n].x,cd[n].y,-1);
    }
#endif

    display_magics();
  }
#endif

  /* add for party attack 20021119 */
  friend_magic = 0;
  if (cd[n].party) {
    for(y=-1;y<2 && !friend_magic;y++) {
      for(x=-1;x<2 && !friend_magic;x++) {
	    for(i=0;i<MAX_CHRS_INONE && !friend_magic;i++) {
          if ((j = map[cd[n].y+y][cd[n].x+x].chr[i] - 1) < 0 || j == n)
          	continue;
	      if (cd[n].party == cd[j].party) friend_magic = 1;
	    }
	  }
    }
  }

  dmprintf(n, 0, buf.data(), buf.size(),DMM_MAGIC_SPHERE, m->spell);
  check_write(n, buf.data());
  for (x = -1; x <= 1; x++)
    for (y = -1; y <= 1; y++) {
#ifndef NO_SKILL
     if (std::abs(x)+std::abs(y) <= s/500)
#endif
      for (i = 0; i < MAX_CHRS_INONE; i++) {
  if ((j = map[cd[n].y+y][cd[n].x+x].chr[i] - 1) < 0)
    continue;
  if (j == n)
    continue;
  if (chr_big(j) && big_already(big_done, &nbig, j))
    continue;
  mg_attack_to(n, j, m, f | (x != 0 || y != 0 ? 8 : 0), f2);
      }
    }

#if MAX_OBJECTS > 0
  if(is_see_magic_effects)
  clear_magic_obj();
#endif
}

void   mg_explode(int n, MagicData * m, int x0, int y0, int f, int f2)
{
  int    x, y;
  int    i, j;
  int    s;
  int    big_done[8]; int nbig = 0;  /* 2x2 の大きな敵の二重適用防止 */


  s = cd[n].skill[SKL_MAGIC_AT][SKL_MGAT_EXPLODE];
#if MAX_OBJECTS > 0
  if(is_see_magic_effects) {

#ifndef NO_SKILL
    if (s / 500 > 0)
#endif
    {
      for(y=-1;y<2;y++)
        for(x=-1;x<2;x++) {
#ifndef NO_SKILL
          if (std::abs(x)+std::abs(y) <= s/500)
#endif
          {
            move_obj(MExplode[y][x],x0+x,y0+y,-1);
          }
        }
    }
#ifndef NO_SKILL
     else {
      move_obj(MHit,cd[n].x,cd[n].y,-1);
    }
#endif

    display_magics();
  }
#endif

  for (x = -1; x <= 1; x++)
    for (y = -1; y <= 1; y++) {
#ifndef NO_SKILL
     if (std::abs(x)+std::abs(y) <= s/500)
#endif
      for (i = 0; i < MAX_CHRS_INONE; i++) {
  if ((j = map[y+y0][x+x0].chr[i] - 1) < 0)
    continue;
  if (chr_big(j) && big_already(big_done, &nbig, j))
    continue;
  mg_attack_to(n, j, m, f | (x != 0 || y != 0 ? 8 : 0), f2);
      }
    }

#if MAX_OBJECTS > 0
  if(is_see_magic_effects)
  clear_magic_obj();
#endif
}

void   mg_area_attack(int n, MagicData * m, int f, int f2)
{
  std::array<char, 256> buf;
  int    i, j;
  int    ar1, ar2;

  dmprintf(n, 0, buf.data(), buf.size(),DMM_AREA_EXCHANGE);
  check_write(n, buf.data());

  friend_magic = 0;

  if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_AREAMAGIC) {
    dmcheck_write(n, DMM_AREA_EXCHANGE_NO);
    dmcheck_write(n, DMM_AREA_EXCHANGE_STOP);
    return;
  }
  if (!(f2)) {
    i = (mgstrlen(m->spell) + 4) * 29;
    if (cd[n].weq != nullptr) {
      if (PER_DICE < 60) {
  if (degrade(cd[n].weq)==2) {
    dmprintf(n, 0, buf.data(), buf.size(),DMM_BROKEN_WEAPON);
    write_companion(n, 0, buf.data(), 0);
    cd[n].weq = nullptr;
    cd[n].wsep = 0;
    item_gc(n);
    set_fn(n);
  }
      }
    }
    if (cd[n].mmp > 0 && cd[n].mhp > 0) {
      if (i >= cd[n].mp) {
  dmcheck_write(n, DMM_MORE_MP);
  cd[n].mp = 1;
  return;
      }
      cd[n].mp -= (i - 1);  /* MaxMP上昇しない */
      (void) mp_dmg(n, -1, 0);  /* 死なないことが保証されている */
      dmprintf(n, 0, buf.data(), buf.size(),DMM_LEFT_MP);
      check_write(n, buf.data());
    }
  }
  ar1 = (int) (unsigned int) map[cd[n].y][cd[n].x].area;

  dmprintf(n, 0, buf.data(), buf.size(),DMM_AREA_EXCHANGE_COMP, m->spell);
  check_write(n, buf.data());

  for (i = 1; i < MAX_NTTYS + MAX_ENEMIES; i++)
    if (cd[i].type) {
      ar2 = (int) (unsigned int) map[cd[i].y][cd[i].x].area;
      if (fld[ar1].floor == fld[ar2].floor) {
  j = f & 3; /* bug fix 20000613 */
  if (j == 3)
    j++;
  if (cd[i].innate & j)
    j = 2;
  else {
    j = 0;
    switch (f & 3) {
    case 1:
      if (map[cd[i].y][cd[i].x].tip & T_ROOF) {
        if (cd[i].at != AT_HELMET)
    j = 1;
        else
    j = 2;
      }
      break;
    case 2:
      if (!(map[cd[i].y][cd[i].x].tip & T_ROOF)) {
        if (cd[i].at != AT_UMBRELLA)
    j = 1;
        else
    j = 2;
      }
      break;
    case 3:
      if (cd[i].at != AT_FLY)
        j = 1;
      else
        j = 2;
      break;
    }
  }
  if (j == 2) {
    dmprintf(n, i, buf.data(), buf.size(),DMM_AREA_ATTACK, m->spell);
    write_companion(n, i, buf.data(), 0);
    dmprintf(n, i, buf.data(), buf.size(),DMM_AREA_DEFEND, (((f & 3) == 1) ? ((i < MAX_NTTYS) ? DMM_AREA_DEF_HELMET : DMM_AREA_DEF_STONEHEAD) : (((f & 3) == 2) ? DMM_AREA_DEF_UMBRELLA : DMM_AREA_DEF_FLY)));
    write_companion(n, i, buf.data(), 0);
  }
  if (j != 1)
    continue;

  if (TCN_MIRROR(i)) {
    dmprintf(n, i, buf.data(), buf.size(),DMM_MAGIC_REFLECT,
      m->spell);
    mg_attack_to(i, n, m, (f & 4) | 8, f2);
  } else {
    mg_attack_to(n, i, m, (f & 4) | 8, f2);
  }
      }
    }
}

void   mg_attack_to(int as, int rs, MagicData * m, int f, int f2)
{
  int    mdp, hr, ae, tfn;
  std::array<char, 256> buf;
  int    target; /* ADD スイッチデータ追加 2005/11/26 Savarl.F */
  int    mpr, avr,il; /* add illusion defence at 20020615 from JETDM by NEO 20120206 */

/*  if ((m->effect != EF_AREA) ||
      (m->effect != EF_ROOF) ||
      (m->effect != EF_UNROOF) ||
      (m->effect != EF_MAREA) ||
      (m->effect != EF_MROOF) ||
      (m->effect != EF_MUNROOF))
    angry_check(as, rs);
*/

  angry_check(as, rs);

  mdp = mdp_calc(as, m, 0, f2, nullptr);
  if (friend_magic && !f2)
  	mdp /= 3;

  if (CN_GASIFY(rs) >= 60 || (rs < MAX_NTTYS && CN_ALAG(rs))) {
    dmcheck_write(as, DMM_INVALID_ANY_MAGIC);
    return;
  }
  /* add for deny PK 20020204 */
  if (deny_pk && as < MAX_NTTYS && rs < MAX_NTTYS && as != rs) {
    dmprintf(as,rs,buf.data(), buf.size(),DMM_DENY_PK);
    check_write(as, buf.data());
	return;
  }
  /* added by Qajin 20000115 */
  if ((fld[map[cd[rs].y][cd[rs].x].area].flag) & FF_MAGICEFFECT) {
    dmcheck_write(as, DMM_MAGIC_BLOCKED_THERE);
    return;
  }
  switch (m->fn) {
        case FN_RAINBOW:
            tfn = frand(7);
            break;
        case FN_MIRROR:
            tfn = cd[rs].dfn;
            break;
        case FN_PRISM:
            tfn = cd[rs].dfn + 1;
            if (tfn > FN_NORMAL)
              tfn = FN_EARTH;
            break; /* 2026-11-16 修正：以前は break が無く default へ
                      落ち、計算した tfn が m->fn で上書きされていた */
        default:
            tfn = m->fn;
            break;
    }
  if (!(tfn % 2)) {
    if (CN_REFLEX(rs)) {
      dmcheck_write(as, DMM_INVALID_ENG_MAGIC);
      return;
    }
    mdp = mdp * (100 - mr_rate(rs)) / 100;
  } else {
    if (CN_FORCE(rs) && !(f & 4)) {
      dmcheck_write(as, DMM_INVALID_PHY_MAGIC);
      return;
    }
    mdp = mdp * (100 - df_rate(rs)) / 100;
  }
  mdp = fn(m->fn, cd[rs].dfn, mdp);
  if (f & 8) {
    if ((m->effect != EF_AREA) &&
		(m->effect != EF_ROOF) &&
		(m->effect != EF_UNROOF) &&
		(m->effect != EF_MAREA) &&
		(m->effect != EF_MROOF) &&
		(m->effect != EF_MUNROOF))
      mdp /= 2;
    else
      mdp /= 7;
  }

  /* ダメージ変化属性確認 */
  if (cd[rs].innate > IA_RICK) {/* Richに魔法は効く */
    hr = cd[rs].innate;
    ae = 0;
    if (cd[as].weq != nullptr)
      if ((cd[as].weq->type & IT_MASK) == IT_WEAPON)
  ae = cd[as].weq->AE;

    if (hr & IA_SAGA) {   /* SaGaにはSaGaを媒体とした魔法が必要 */
      if (!(((ae == AE_SAGA1) && (hr & IA_SAGA1))
      || ((ae == AE_SAGA2) && (hr & IA_SAGA2))
      || ((ae == AE_SAGA3) && (hr & IA_SAGA3))
      || ((ae == AE_SAGA4) && (hr & IA_SAGA4))))
  mdp = 0;
    } else if (hr & IA_GURDIAN) { /* Gurdも同様 */
      if (!(((ae == AE_DRAGON) && (hr & IA_DRAGON))
      || ((ae == AE_GOD) && (hr & IA_GOD))
      || ((ae == AE_ELEMENT) && (hr & IA_ELEMENT))
      || ((ae == AE_DEMON) && (hr & IA_DEMON))
      || ((ae == AE_GURD1) && (hr & IA_GURD1))
      || ((ae == AE_GURD2) && (hr & IA_GURD2))
      || ((ae == AE_GURD3) && (hr & IA_GURD3))
      || ((ae == AE_GURD4) && (hr & IA_GURD4))))
  mdp = mdp / 2;
    }
  }
  /* 変化してもメッセージはなし */

  /* add illusion defence at 20020615 from JETDM by NEO 20120206 */
	il = TCN_ILLUSION(rs);
	if(il > 0){
		mpr = mdp;
		if(mpr > 100) mpr = 100; 
		mpr = 100 - (100 - mpr)*(100 - frand(mpr)) / 100;
		avr = il;
		if (cd[rs].aeq != nullptr)
		    avr = avr * (100 - cd[rs].aeq->wp) / 100;
	    	mpr = frand(mpr);
	    	avr = frand(avr);

		if ((mpr + lv_dice(cd[as].lv[LV_WIZARD])) < (avr + lv_dice(cd[rs].lv[LV_WIZARD]))){
			dmprintf( as, rs, buf.data(), buf.size(), DMM_MAGIC_EFFECT_NO_IL);
			write_companion(rs, as, buf.data(), 0);
			return;
		}
	}
  /* ここまで NEO 20120206 */

  if (f & 4) {
    /* change for sheil cheat 20000522 */
    if (cd[rs].mmp == 0 && (cd[rs].exp == 99999999 || rs >= MAX_NTTYS)) {
      dmcheck_write(as, DMM_INVALID_ANIMATED);
      return;
    }
    if (cd[rs].at == AT_ABSORB && fn(m->fn,cd[rs].dfn,100) == 25) {
      dmprintf(as, rs, buf.data(), buf.size(),(char*)(as ? DMM_ABS_FROM_MAGIC : DMM_ABS_FROM_TRAP),
        m->spell, mdp);
      write_companion(rs, as, buf.data(), 0);
      f = mp_dmg(rs, mdp, (as == rs ? 0 : 1));
      dmprintf(rs, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    } else {
      dmprintf(as, rs, buf.data(), buf.size(),(char*)(as ? DMM_MAGIC_MP : DMM_TRAP_MP)
        , m->spell, mdp);
      write_companion(rs, as, buf.data(), 0);
      f = mp_dmg(rs, -mdp, (as == rs ? 0 : 1));
      dmprintf(rs, 0, buf.data(), buf.size(),DMM_LEFT_MP);
    }
  } else {
    /* change for sheil cheat 20000522 */
    if (cd[rs].mhp == 0 && (cd[rs].exp == 99999999 || rs >= MAX_NTTYS)) {
      dmcheck_write(as, DMM_INVALID_ASTRAL);
      return;
    }
    if (cd[rs].at == AT_ABSORB && fn(m->fn,cd[rs].dfn,100) == 25) {
      dmprintf(as, rs, buf.data(), buf.size(),(char*)(as ? DMM_ABS_FROM_MAGIC : DMM_ABS_FROM_TRAP),
        m->spell, mdp);
      write_companion(rs, as, buf.data(), 0);
      f = mp_dmg(rs, mdp, (as == rs ? 0 : 1));
      dmprintf(rs, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    } else {
      dmprintf(as, rs, buf.data(), buf.size(),(char*)(as ? DMM_MAGIC_HP : DMM_TRAP_HP)
        , m->spell, mdp);
      write_companion(rs, as, buf.data(), 0);
      f = hp_dmg(rs, -mdp, (as == rs ? 0 : 1));
      dmprintf(rs, 0, buf.data(), buf.size(),DMM_LEFT_HP);
    }
  }
  check_write(rs, buf.data());
/* ↓スイッチデータ追加 2005/11/26 Savarl.F */
/*  if (f)	*/
/*    kill_other(as, rs, LV_WIZARD);	*/
  if (f) {
    kill_other(as, rs, LV_WIZARD);

    if (f != 2) {
      if (as == rs) {
        /* 自爆除く */
      } else if (as < MAX_NTTYS && rs < MAX_NTTYS) {  /* PCに限定　Bugfix by NEO 2008/03/30 */
        load_switch(as,cd[as].x,cd[as].y,22);  /* PC殺害時 */
      } else if (as < MAX_NTTYS) {
        target = cd[as].target;
        cd[as].target = rs;
        load_switch(as,cd[as].x,cd[as].y,21);  /* 指定NPC殺害時 */
        cd[as].target = target;
      }
    }
  }
/* ↑CHG */
  if (f == 2)     /* cd[rs] has come back */
    raise_dead(rs);

  if (rs != as) {
    lv_up(as, rs, LV_WIZARD, 1);
    skillup_magic(as, rs, m, f2);
    
    if (mmp_up_for_attack == 1) { /* 魔法攻撃によって最大MPを増加させる added by NEO 20120212 */
	    if (cd[as].mmp > 0 && cd[as].mmp < 9999
	        && !death_penalty /* add for control death penalty (lv up) 20020204 */
	        && PER_DICE < ( 1 + ( 10000 - cd[as].mmp ) / cd[as].mmp ))  /* 最大MPによりランダムで最大MPが増加 */
	        /* MMP別増加確率
	        1 + (10000 - 1000) / 1000 = 10     約1/10
	        1 + (10000 - 2000) / 2000 = 5      約1/20
	        1 + (10000 - 3000) / 3000 = 3.33   約1/30
	        1 + (10000 - 4000) / 4000 = 2.5    約1/40
	        1 + (10000 - 5000) / 5000 = 2      約1/50
	        1 + (10000 - 6000) / 6000 = 1.66   約1/60
	        1 + (10000 - 7000) / 7000 = 1.43   約1/70
	        1 + (10000 - 8000) / 8000 = 1.25   約1/80
	        1 + (10000 - 9000) / 9000 = 1.11   約1/90
	        */
	    { 
	        if (cd[as].mmp < cd[rs].lv[LV_WIZARD]) /* 上昇するのは敵のWlvまで */
	        cd[as].mmp++;
	    }
	} else if (cd[as].mmp > 0 && cd[as].mmp < 9999 /* 従来のMP増加 */
	        && !death_penalty /* add for control death penalty (lv up) 20020204 */
	        && PER_DICE < mdp * 10 / cd[as].mmp) { /* 敵に大ダメージを与えると最大MPが増加 */
	        cd[as].mmp++;
	/*    if (cd[as].lv[LV_WIZARD] >= cd[as].mmp) {
	      if(cd[as].mmp>0 && cd[as].mmp<9999 &&
	         PER_DICE<mdp*10/cd[as].mmp) {
	        cd[as].mmp++;
	      }
	    } else {
	      cast_mmpup(n, m->spell, mdp);
	    }*/
	}
  }
  if (rs < MAX_NTTYS) {
    check_write(rs, "#lag\n");
    if (CN_LAG(rs) < 0) {
      CN_ALAG(rs) = 2;
    } else {
      CN_ALAG(rs) = 1;
    }
  }
  
  if ( degrade_by_magic == 1) /* 魔法ダメージでも鎧・アクセが欠ける設定ならば */
  DegradeArmorAndAccesory( as, rs );	/* 魔法ダメージでも鎧・アクセが欠ける処理 added by NEO 20120212 */
  
  return;
}

/* add 20000612 */
int    remote_get(int n)
{
  int x, y;

  cd[n].vf = 0;
  check_write(n, "#list\n");
  x = cd[n].x;
  y = cd[n].y;
  cd[n].x = cd[n].ix;
  cd[n].y = cd[n].iy;
  cd[n].vf2 = 1;
  list_item(n, 1);
  check_write(n, "#end-list\n");
  if (map[cd[n].y][cd[n].x].tip & T_ITEM) {
    dmcheck_write(n, DMM_GET_SELECT);
    cd[n].st = ST_R_GET;
    check_write(n, "#s-edit\n");
  } else
    dmcheck_write(n, DMM_NO_ITEM_THERE);
  cd[n].x = x;
  cd[n].y = y;
  return (0);
}

int   list_inv(int n)
{
  int i, j;
  std::array<char, 256> buf;

    check_write(n, "#list\n");
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
        continue;
      j++;
      std::snprintf(buf.data(), buf.size(), "[%2i] %s \n", j, cd[n].id[i].name);
      check_write(n, buf.data());
    }
    check_write(n, "#end-list\n");
    dmcheck_write(n, DMM_PUT_SELECT);
    check_write(n, "#s-edit\n");
    return (0);
}

/* add for save life 20000612 */
void    mg_save_life(int n, MagicData * m, int f)
{
  std::array<char, 256> buf;
  int mdp;
  int i, j, k;
  MapData *pmap;

  dmprintf(n, 0, buf.data(), buf.size(),DMM_SAVELIFE_EXCHANGE);
  check_write(n, buf.data());

  mdp = mdp_calc(n, m, 1, 1, nullptr);
  if (!f) {
    k = (mgstrlen(m->spell) + 4) * 29;
    if (cd[n].weq != nullptr) {
      if (PER_DICE < 60) {
        if (degrade(cd[n].weq)==2) {
          dmprintf(n, 0, buf.data(), buf.size(),DMM_BROKEN_WEAPON);
          write_companion(n, 0, buf.data(), 0);
          cd[n].weq = nullptr;
          cd[n].wsep = 0;
          item_gc(n);
          set_fn(n);
        }
      }
    }
    if (cd[n].mhp > 0 || (n < MAX_NTTYS && cd[n].exp < 99999999)) {
      cd[n].hp -= k;
      if (cd[n].hp < 0)
        cd[n].hp = 0;
      dmprintf(n, 0, buf.data(), buf.size(),DMM_LEFT_HP);
      check_write(n, buf.data());
    }
  }
  if (!(cd[n].hp))
    mdp *= 5;

  pmap = &(map[cd[n].y+py[cd[n].d]][cd[n].x+px[cd[n].d]]);
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (CN_REFLEX(j) || CN_GASIFY(j) >= 60) {
      dmcheck_write(n, DMM_INVALID_EFC_MAGIC);
      continue;
    }
    if ((fld[pmap->area].flag) & FF_MAGICEFFECT) {
      dmcheck_write(n, DMM_MAGIC_BLOCKED_THERE);
      continue;
    }
    if (cd[j].type >= CT_MAGICAL) {
      dmprintf(n, j, buf.data(), buf.size(),DMM_INVALID_MAGICAL);
      check_write(n, buf.data());
      continue;
    }

    display_single_magic(j,MHeal);
    if (j < MAX_NTTYS) {
      cd[j].think = 0;
      if ((!(cd[n].hp)
#ifndef NO_SKILL
       || cd[n].skill[SKL_MAGIC_EF3][SKL_MGEF3_SAVELIFE]/100==99
#endif
       ) && n < MAX_NTTYS && std::strcmp(ip_address_check(n),ip_address_check(j)))
        cd[j].think = VF_HEAL;
    }
    if (!CN_RAISEDEAD(j) || !(cd[n].hp)) {
      dmprintf(n, j, buf.data(), buf.size(),DMM_SAVELIFE
                 , mdp);
      check_write(n, buf.data());
      check_write(j, buf.data());
      CN_RAISEDEAD(j) = mdp;
      lv_up(n, j, LV_WIZARD, -1);
      skillup_magic(n, j, m, f);
    }
  }

  if (!(cd[n].hp)) {
    dmprintf(n, 0, buf.data(), buf.size(),DMM_SACRIFICE);
    write_companion(n, 0, buf.data(), (n < MAX_NTTYS ? access_dm : 0));
    if (log_op || n < MAX_NTTYS) {
      std::snprintf(buf.data(), buf.size(), "sacrifice: %s ", cd[n].name);
      log_print(buf.data());
    }
    dead(n);
  }
}

int    mdp_calc(int n, MagicData * m, int f, int f2, double *mdn)
{
  int    mdp;
  int    l;

  if (adding_power_spell_flag == 0) {
    int rate;
    rate = pow_mg_multiplier(*m);
    if (rate < 0) {
      if (!f)
        mdp = mgstrlen(m->spell) / (-rate) / 2 + 4;
      else
        mdp = mgstrlen(m->spell) / (-rate) / 4 + 4;
    } else {
      if (!f)
        mdp = mgstrlen(m->spell) * rate / 2 + 4;
      else
        mdp = mgstrlen(m->spell) * rate / 4 + 4;
    }
  } else {
    if (!f)
      mdp = (mgstrlen(m->spell) + pow_mg_multiplier(*m)) / 2 + 4;
    else
      mdp = (mgstrlen(m->spell) + pow_mg_multiplier(*m)) / 4 + 4;
    if (mdp < 4)
      mdp = 4;
  }

  /* bug fix 20010107 */
  if (mdn != nullptr)
    *mdn = 1;
  if (!f2) {
    l = TCN_SPELL(n) * 25 / fn(m->fn, cd[n].sfn, 100);
    if (mdn != nullptr && spellup_only_time) {
      *mdn = (mdp + l) / (double)mdp;
      if (*mdn == 0) /* bug fix 20010315 */
        *mdn = 1;
    }
    mdp = mdp + l;
    if (mdp < 0)	/* bug(?) fix 20041031 */
    	mdp = 0;
  }

  if (!f)
    mdp = mdp * mdp;

  mdp = frand(mdp) + mdp;

#ifdef NO_SKILL

  mdp = mdp * (10000 + cd[n].lv[LV_WIZARD]) / 5000;  /* Wlv によるダメージ加算 old 乗数２～４　ダメージ差２倍 */

#else
  mdp = mdp * (5000 + cd[n].lv[LV_WIZARD] + skill_mg_calc(n,m)) / 5000;
#endif
  return (mdp);
}

void copy_spell(int n, MagicData *m)
{
  int    x, y;
  int    i, j, k;

/* 魔法ラーニング設定 added by NEO 20120212 */
	if (spell_learning == 1 ||  /* 魔法ラーニング完全禁止の場合 */
	   (spell_learning == 2 &&  n < MAX_NTTYS ) )  /* NPCからのみ魔法ラーニング許可で魔法を唱えたのがPCの場合 */
		return; /* ラーニングしない */
/* ここまで by NEO 20120212 */

  if (m->effect > NORMAL_EF)
    return;
  if (m->is_inherence)
    return;
  set_view(n);
  for (y = 0; y < 5; y++)
    for (x = 0; x < 5; x++) {
      if (cd[n].view[y][x].tip == T_UNKNOWN)
  continue;
      for (i = 0; i < MAX_CHRS_INONE; i++) {
  if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
    continue;
  if (j == n)
    continue;
  if (cd[j].type != CT_NORMAL)
    continue;
  for (k = 0; k < MAX_MAGICS - 1 && cd[j].md[k].effect != EF_NONE; k++)
    if (!std::strcmp(m->spell, cd[j].md[k].spell))
      break;
  if (k < MAX_MAGICS - 1 && cd[j].md[k].effect != EF_NONE)
    continue;
  cd[j].md[k].spell = m->spell;
  cd[j].md[k].effect = m->effect;
  cd[j].md[k].fn = m->fn;
  cd[j].md[k].is_inherence = 0;
  cd[j].md[k].extension = m->extension;
      }
    }
  return;
}

void exchange_magic(int n, MagicData *m)
{
  MagicData md;
  int    i;

  md.spell = m->spell;
  md.effect = m->effect;
  md.fn = m->fn;
  md.is_inherence = m->is_inherence;
  md.extension = m->extension;
  for (i = 0; i < MAX_MAGICS - 1 && cd[n].md[i].effect != EF_NONE; i++)
    if (m->spell == cd[n].md[i].spell)
      break;
  for (; i > 0; i--) {
    cd[n].md[i].spell = cd[n].md[i - 1].spell;
    cd[n].md[i].effect = cd[n].md[i - 1].effect;
    cd[n].md[i].fn = cd[n].md[i - 1].fn;
    cd[n].md[i].is_inherence = cd[n].md[i - 1].is_inherence;
    cd[n].md[i].extension = cd[n].md[i - 1].extension;
  }
  cd[n].md[0].spell = md.spell;
  cd[n].md[0].effect = md.effect;
  cd[n].md[0].fn = md.fn;
  cd[n].md[0].is_inherence = md.is_inherence;
  cd[n].md[0].extension = md.extension;
  return;
}

void   forget_magic(int n, int in)
{
  int    i;

  for (i = in; i < MAX_MAGICS - 1 && cd[n].md[i + 1].effect != EF_NONE; i++) {
    cd[n].md[i].spell = cd[n].md[i + 1].spell;
    cd[n].md[i].effect = cd[n].md[i + 1].effect;
    cd[n].md[i].fn = cd[n].md[i + 1].fn;
    cd[n].md[i].is_inherence = cd[n].md[i + 1].is_inherence;
    cd[n].md[i].extension = cd[n].md[i + 1].extension;
  }
  cd[n].md[i].spell = nullptr;
  cd[n].md[i].effect = EF_NONE;
  cd[n].md[i].fn = FN_NONE;
  cd[n].md[i].is_inherence = 0;
  cd[n].md[i].extension = nullptr;
  return;
}

void   learn_magic(int n, MagicData * m)
{
  int    k;

  for (k = 0; k < MAX_MAGICS - 1 && cd[n].md[k].effect != EF_NONE; k++)
    if (!std::strcmp(m->spell, cd[n].md[k].spell))
      break;
  if (k < MAX_MAGICS - 1 && cd[n].md[k].effect != EF_NONE)
    return;
  cd[n].md[k].spell = m->spell;
  cd[n].md[k].effect = m->effect;
  cd[n].md[k].fn = m->fn;
  cd[n].md[k].is_inherence = m->is_inherence;
  cd[n].md[k].extension = m->extension;
  skillup_magic(n,0,m,0); /* A good candleholder proves a good gamester... */
}

void   charge_spell(int n, int m)
{
  std::array<char, 256> buf;
  int    mpow;
  int    i, lv;

  if(cd[n].md[m].is_inherence){
    dmcheck_write(n, DMM_CHARGESPL_NO_INHRNT);
    return;
  }

  lv = mgstrlen(cd[n].md[m].spell);
  mpow = lv * lv * 4;   /* 魔法の一般的、基礎的な強さ */

  i = mpow * (10000 - cd[n].lv[LV_MERCHANT]) / 10000;
  if (nciflag) {
    i = i * cirate / 100 + 1;
  } else {
    i = i + 1;
  }

  if (tax < 0)
    i = i * (10000 - tax) / 10000;
  if (cd[n].gp < i) {
    dmcheck_write(n, DMM_CHARGESPL_NO_TAX);
    return;
  }
  cd[n].gp -= i;
  tax += i;
  lv_up(n, n, LV_MERCHANT, 0);
  lv_up(n, n, LV_WIZARD, 0);
  skillup(n,SKL_MAGIC_SP,SKL_MGSP_CHARGE,mpow * 2.6 / 100,50,1);
  skillup_magic(n, 0, &(cd[n].md[m]), 0);

  if (PER_DICE >
#ifdef NO_SKILL
      100 * cd[n].lv[LV_WIZARD]
#else
      100 * (cd[n].lv[LV_WIZARD] + cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_CHARGE]) / 2
#endif
      / (mpow
   * (cd[n].weq->fn == cd[n].md[m].fn ? 3 : 6) * lv / cd[n].weq->wp + 1)
  /* 属性と求める宝石の重さ */
      * 5 / ((cd[n].weq->type & MT_MASK) / MT_PLACE + 1)) {
    /* 宝石の材質、Silver が基本 */
/* それ相当の宝石の重さと材質があれば、 */
/* mpow <= wizard level なら 100 % 成功 */
/* そうでない方はそれなりに…           */
    dmprintf(n, 0, buf.data(), buf.size(),DMM_CHARGESPL_FAIL);
    check_write(n, buf.data());
    delete_item(cd[n].weq);
    cd[n].weq = nullptr;
    cd[n].wsep = 0;
    item_gc(n);
    set_fn(n);
  } else {
    dmprintf(n, 0, buf.data(), buf.size(),DMM_CHARGESPL_SUCCESS);
    check_write(n, buf.data());
    cd[n].weq->name = cd[n].md[m].spell;
    cd[n].weq->type = (cd[n].weq->type ^ IT_JEWEL) ^ IT_SPELL_JEWEL;
    /* add new fn 20000324 */
    if (cd[n].md[m].fn <= FN_NORMAL || cd[n].weq->fn == cd[n].md[m].fn)
      cd[n].weq->fn = cd[n].md[m].fn;
    else if (cd[n].md[m].fn == FN_PRISM && cd[n].weq->fn != FN_NONE) {
      if (++(cd[n].weq->fn)>FN_NORMAL)
        cd[n].weq->fn = FN_EARTH;
    } else if (cd[n].md[m].fn == FN_RAINBOW)
        cd[n].weq->fn = frand(7);
    cd[n].weq->UE = cd[n].md[m].effect;
    cd[n].weq->op[1] = 0;
    cd[n].weq->active_time = 0;
    cd[n].wsep = sep_calc(n, cd[n].weq);
    if (cd[n].md[m].extension != nullptr) {
      if (cd[n].weq->extension != nullptr) {
        /* 2026-11-16 修正：以前は cd[n].md[n]（キャラ番号 n）と
           書かれており、他人の呪文の extension が混ざっていた。
           呪文番号 m が正しい。 */
        std::snprintf(buf.data(), buf.size(),"%s %s",cd[n].weq->extension,cd[n].md[m].extension);
        cd[n].weq->extension = findstr(buf.data());
      } else {
        cd[n].weq->extension = cd[n].md[m].extension;
      }
    }
    set_fn(n);
#ifndef NO_SKILL
    /* un-lose magic 20020527 :-P */
    if (!(cd[n].skill[SKL_MAGIC_SP][SKL_MGSP_CHARGE]/100==99 &&
          PER_DICE < skill_mg_calc(n,&(cd[n].md[m]))/1000))
#endif
    forget_magic(n, m);
  }
}

int    create_special_item(int n, int in1, int in2, int is_success)
{
  ItemData *item;
  int wp, wp_rate;
  int i;

  if (!(i=match_special_item(n, in1, in2, &item, is_success))) return 0;
#ifdef DEBUG
  std::snprintf(debug_buf, sizeof(debug_buf),"special create %s and %s to %s",cd[n].id[in1].name,cd[n].id[in2].name,item->name);
  log_print(debug_buf);
#endif
  cd[n].id[in1].name = item->name;
  cd[n].id[in1].type = item->type;
  cd[n].id[in1].fn = item->fn;
  cd[n].id[in1].extension = item->extension;
  if (i == 2) {
    cd[n].id[in1].wp /= 2;
    cd[n].id[in2].wp /= 2;
  }
  wp = cd[n].id[in1].wp + cd[n].id[in2].wp;
  cd[n].id[in1].wp = wp;
  for (i = 1; i < 4; i++) /* op[0] は 以下に設定する ATP */
    cd[n].id[in1].op[i] = item->op[i];
  cd[n].id[in1].active_time = 0;

  wp_rate = ((wp <= item->wp)? wp : item->wp - (wp - item->wp));
  if(wp_rate<0) wp_rate=0;

  cd[n].id[in1].ATP = item->ATP * wp_rate / item->wp;
  if ((item->type & IT_MASK) == IT_ARMOR)
    cd[n].id[in1].DFP = item->DFP * wp_rate / item->wp;
  else if ((item->type & IT_MASK) == IT_FOOD) {
    cd[n].id[in1].VALUE = item->VALUE * wp_rate / item->wp;
    cd[n].id[in1].ENTROP = wp - (item->wp - item->ENTROP);
    if(cd[n].id[in1].ENTROP < 0)
      cd[n].id[in1].ENTROP = 0;
    if(cd[n].id[in1].ENTROP > 99)
      cd[n].id[in1].ENTROP = 99;
  } else if ((item->type & IT_MASK) == IT_JEWEL) {
    cd[n].id[in1].VALUE = item->VALUE * wp_rate / item->wp;
    cd[n].id[in1].VALUE2= item->VALUE2* wp_rate / item->wp;
  }
  return 1;
}

/* 返り値 変更 20020529 */
/* 0 : no match */
/* 1 : match (success) */
/* 2 : match (fail) */
int    match_special_item(int n, int in1, int in2, ItemData **item, int is_success)
{
  CrtData *p;

  for (p = first_create_data; p != nullptr; p=p->next)
    if(   (match_item(*(p->staff1), cd[n].id[in1])
        && match_item(*(p->staff2), cd[n].id[in2]))
       || (match_item(*(p->staff2), cd[n].id[in1])
        && match_item(*(p->staff1), cd[n].id[in2]))){
      if(p->level < 0 && !is_success) {
  if(p->fail_product!=nullptr) {
    *item= p->fail_product;
    skillup_create(n,*item,0);
    return 2;
  }
      } else if(p->level <= 0
    || (cd[n].lv[LV_CREATER] >= p->level
        && PER_DICE < (cd[n].lv[LV_CREATER]-p->level)*100/(p->level))) {
  if(p->product!=nullptr){
    *item= p->product;
    skillup_create(n,*item,(p->level < 0 ? 0 : p->level / 1000));
    return 1;
  }
      } else {
  if(p->fail_product!=nullptr){
    *item= p->fail_product;
    skillup_create(n,*item,p->level / 1000);
    return 2;
  }
      }
    }
  return 0;
}

#if MAX_OBJECTS > 0

void display_magics()
{
    int i;

    if (!is_see_magic_effects)
        return;

    reflesh_objects = 1;

    for(i=1;i<MAX_NTTYS;i++)
    	if (cd[i].type)
	        print_map(i,cd[i].modef & MD_CHAT);

    reflesh_objects = 0;
}

void display_single_magic(int n,int Mobj)
{
    int backup;

    if (!is_see_magic_effects)
        return;
      if (n <= 0 || n >= MAX_NTTYS)
        return;

    backup = dobj[cd[n].y][cd[n].x]-1;
    move_obj(Mobj,cd[n].x,cd[n].y,-1);
    display_magics();
    move_obj(Mobj,-1,-1,-1);
    move_obj(backup,cd[n].x,cd[n].y,-1);
}

void clear_magic_obj()
{
	int i;

    /* bug fix 20020715 */
    if (!is_see_magic_effects)
        return;

	for(i=0;i<3;i++)
		move_obj(MLine[i],-1,-1,-1);
	move_obj(MHit,-1,-1,-1);
	for(i=0;i<9;i++)
		move_obj(MExplode[i/3][i%3],-1,-1,-1);
	for(i=0;i<MAX_OBJECTS;i++)
		if (od[i].gname != nullptr && od[i].x >= 0 && od[i].y >= 0) /* bug fix 20010507 */
			dobj[od[i].y][od[i].x] = i+1;

}

#else
void display_single_magic(int n,int Mobj) {}
#endif
