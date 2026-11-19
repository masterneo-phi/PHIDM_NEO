
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

/* 計算系: 攻撃力・防御率・HP/MP の増減と死亡処理、レベル変動、属性
 * 相性・ダイス・平方根、装備による属性設定、数値列のパース。
 * 乱数 (PER_DICE / frand / RAND) の消費順序はセッションの挙動に
 * 直結するため、条件式の短絡評価も含めて元の評価順序を保つ。
 * 呼び出し側は phi_attack.cpp / phi_magic.cpp / phi_dm.cpp など。
 */

#include  <array>
#include  <climits>
#include  <cmath>
#include  <cstdio>
#ifndef Mips
#include  <cstdlib>
#endif
#include  <cstring>

#include  "tcp.h"
#include  "phi_dm.h"
#include  "phi_attack.h"
#include  "phi_calc.h"
#include  "phi_data.h"
#include  "phi_enemy.h"
#include  "phi_magic.h"
#include  "phi_use.h"
#include  "phi_str.h"
#include  "ext_skills.h"

#ifdef DEBUG
char debug_buf[256];
#endif

namespace {

/* デスペナリミッタ (NEO 2011/11/12): レベル帯ごとの切り捨て単位。
 * 元の if-else 連鎖と同じ順序・同じ境界。最後の INT_MAX は
 * 「7000 以上は 1000 単位」の受け皿。 */
struct LevelLimit {
  int threshold;
  int step;
};

constexpr std::array<LevelLimit, 5> kLevelLimits = {{
  {1000, 50},    /* limit 50 */
  {3000, 100},   /* limit 100 */
  {5000, 200},   /* limit 200 */
  {7000, 500},   /* limit 500 */
  {INT_MAX, 1000}, /* limit 1000 */
}};

/* SE (特殊効果) から EF (魔法効果) への変換表。add 20010705。
 * このファイルの中でしか参照されないためファイルローカルにした。
 * 添字は 0..MAX_SE (34)。sep_calc() が範囲を検査してから引く。 */
constexpr std::array<int, 35> se2ef = {
  EF_NONE,    EF_COAT,      EF_COAT_,    EF_PROTECT, EF_PROTECT_,
  EF_SHELL,   EF_SHELL_,    EF_HASTE,    EF_HASTE_,  EF_ILLUSION,
  EF_REGENE,  EF_ENCOURAGE, EF_POISON,   EF_PALSY,   EF_PANIC,
  EF_CONFUSE, EF_BERSERK,   EF_SILENCE,  EF_NONE,    EF_NONE,
  EF_NONE,    EF_BLIND,     EF_SAVELIFE, EF_NONE,    EF_NONE,
  EF_VANISH,  EF_MIRROR,    EF_SPELL,    EF_SPELL_,  EF_NONE,
  EF_NONE,    EF_NONE,      EF_NONE,     EF_NONE,    EF_ILLUSION_
};

}  /* namespace */

int sum_wp(int n)
{
  int wp = 0;
  for (int i = 0; i < MAX_ITEMS; i++)
    wp += cd[n].id[i].wp;
  return (wp);
}

int df_rate(int n)
{
  int dfp = (cd[n].aeq == nullptr ? 0 : cd[n].aeq->DFP);
  /* add for defense 20010703 */
  if (cd[n].st == ST_DEFEND)
    dfp = (100 - (100 - dfp) * 8 / 10);
  if (TCN_PROTECT(n) < 0)
    dfp = dfp * (100 + TCN_PROTECT(n)) / 100;
  else if (TCN_PROTECT(n) > 0)
    dfp = (100 - (100 - dfp) * (100 - TCN_PROTECT(n)) / 100);
  return (dfp);
}

int mr_rate(int n)
{
  int mrp = (cd[n].aeq == nullptr ? 0 : cd[n].aeq->MRP);
  if (TCN_SHELL(n) < 0)
    mrp = mrp * (100 + TCN_SHELL(n)) / 100;
  else if (TCN_SHELL(n) > 0)
    mrp = (100 - (100 - mrp) * (100 - TCN_SHELL(n)) / 100);
  return (mrp);
}

/* 戻り値: 0 = 生存、1 = 死亡 (dead() 済み)、2 = 復活処理へ (raise_dead() 呼び出し側)。
 * f == 1 のとき最大 HP の成長判定と死亡判定を行い、f == 2 のとき
 * 最大 MP の成長判定 (魔法バースト) を行う。f == 2 の成長式は
 * mhp / hp を使う (元のままの癖。mp_dmg() の f == 1 と対照)。
 * PER_DICE の評価回数と短絡順序は元のままにしている。 */
int hp_dmg(int n, int p, int f)
{
  int    c;

  if (p < 0 && n < MAX_NTTYS && TCN_AEGIS(n)) /* add for Aegis of Athena */
    p = 0;

  cd[n].hp += p;
  if (cd[n].hp < 0)
    cd[n].hp = 0;

/*  load_switch(n,cd[n].x,cd[n].y,25); ADD スイッチデータ追加(HP/MP変動時) 2005/11/26 Savarl.F */

  /* change for sheil cheat 20000522 bug fix 20000623 */
  if (cd[n].mhp > 0 || (cd[n].exp < 99999999 && n < MAX_NTTYS)) {
    /* mhp == 0 のときは短絡で除算を避ける。 */
    if (!(cd[n].mhp) || PER_DICE < -p * 100 / cd[n].mhp) {
      if ((cd[n].st & 0xf0) == ST_SPELL) {
        dmcheck_write(n, DMM_IMCOMPLETE_CAST);
        cd[n].st = ST_COMMAND;
      }
    }
    if (f == 1 && cd[n].mhp < 9999
        && !death_penalty /* add for control death penalty 20020204 */
        && (is_kill_master || cd[n].exp < 99999999)) {
      if (cd[n].mhp >= cd[n].lv[LV_FIGHTER]) {
        if ((!(cd[n].mhp) || PER_DICE < -p * 100 / cd[n].mhp) &&
            (PER_DICE < -p * 100 / (cd[n].hp + 1))) {
          cd[n].mhp++;
        }
      } else {
        c = -p * 25 * (cd[n].mhp + cd[n].hp)
          / (cd[n].mhp * cd[n].hp + 1);
        if (PER_DICE < c) {
          cd[n].mhp++;
        }
      }
    }
    if (f == 2 && cd[n].mmp < 9999 && cd[n].mmp > 0
        && !death_penalty /* add for control death penalty 20020204 */
        && (is_kill_master || cd[n].exp < 99999999)) {
      /* 魔法バースト */
      if (cd[n].mmp < cd[n].lv[LV_WIZARD]) {
        c = -p * 25 * (cd[n].mhp + cd[n].hp)
          / (cd[n].mhp * cd[n].hp + 1);
        if (PER_DICE < c) {
          cd[n].mmp++;
        }
      }
    }
    if (cd[n].hp == 0) {
      print_status(n); /* added by Qajin 19991224 */
      if (f == 1 && TCN_RAISEDEAD(n))
        return (2);
      else if (f == 1 && dp_beginner == 1 && cd[n].exp < 1440) { /* 初心者加護 added by NEO 2011/11/13 */
        CN_RAISEDEAD(n) = 1;
        return (2);
      }
      dead(n);
      return (1);
    }
  }
  if (cd[n].hp > cd[n].mhp)
    cd[n].hp = cd[n].mhp;
  print_status(n); /* added by Qajin 19991224 */
  return (0);
}

/* hp_dmg() の MP 版。f == 2 で MP が 0 になっても死亡せず MP を 1 に
 * 戻す。f == 1 の最大 MP 成長判定は、hp_dmg() と違って詠唱中断判定の
 * 内側にある (元のままの構造の違い)。成長式は 50 倍で mmp / mp を使う。 */
int mp_dmg(int n, int p, int f)
{
  int    c;

  cd[n].mp += p;
  if (cd[n].mp < 0)
    cd[n].mp = 0;

/* load_switch(n,cd[n].x,cd[n].y,25); ADD スイッチデータ追加(HP/MP変動時) 2005/11/26 Savarl.F */

  /* change for sheil cheat 20000522 bug fix 20000623 */
  if (cd[n].mmp > 0 || (cd[n].exp < 99999999 && n < MAX_NTTYS)) {
    if (!(cd[n].mmp) || PER_DICE < -p * 100 / cd[n].mmp) {
      if ((cd[n].st & 0xf0) == ST_SPELL) {
        dmcheck_write(n, DMM_IMCOMPLETE_CAST);
        cd[n].st = ST_COMMAND;
      }
      if (f == 1 && cd[n].mmp < 9999
          && !death_penalty /* add for control death penalty 20020204 */
          && (is_kill_master || cd[n].exp < 99999999)) {
        if (cd[n].mmp >= cd[n].lv[LV_WIZARD]) {
          if (PER_DICE < -p * 100 / (cd[n].mp + 1))
            cd[n].mmp++;
        } else {
          c = -p * 50 * (cd[n].mmp + cd[n].mp)
            / (cd[n].mmp * cd[n].mp + 1);
          if (PER_DICE < c) {
            cd[n].mmp++;
          }
        }
      }
    }
    if (cd[n].mp == 0) {
      if (f == 2) {
        cd[n].mp = 1;
        print_status(n); /* added by Qajin 19991224 */
        return (0);
      }
      print_status(n); /* added by Qajin 19991224 */
      if (f == 1 && TCN_RAISEDEAD(n))
        return (2);
      else if (f == 1 && dp_beginner == 1 && cd[n].exp < 1440) { /* 初心者加護 added by NEO 2011/11/13 */
        CN_RAISEDEAD(n) = 1;
        return (2);
      }
      dead(n);
      return (1);
    }
  }
  if (cd[n].mp > cd[n].mmp)
    cd[n].mp = cd[n].mmp;
  print_status(n); /* added by Qajin 19991224 */
  return (0);
}

void cast_mmpup(int n, char *spell, int mdp)
{
  int l = static_cast<int>(std::strlen(spell));
  if (!death_penalty && /* add for control death penalty 20020204 */
      frand(1000) <= (l + mdp) * 3) {
    cd[n].mmp++;
  }
}

void dead(int n)
{
  std::array<char, 256> buf;

  /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
  load_switch(n,cd[n].x,cd[n].y,24);  /* キャラクタ死亡時 */
  /* ↑ADD */

/*  if (n >= MAX_NTTYS) { delete for lose talk 20001212 */
  int i = 0;
  for (; i < MAX_TALKS; i++)
    if (cd[n].td[i].type == TT_DYING)
      break;
  if (i < MAX_TALKS)
    talk_analyze(n,cd[n].td[i].string); /* change for npc dead psychogram 20001212 */
/*  } */
  if (!is_kill_master && n < MAX_NTTYS && cd[n].exp == 99999999) {
    /* 経験値カンストの PC はペナルティなしで全快する (GP は全額 tax へ)。 */
    tax += cd[n].gp;
    cd[n].gp = 0;
    cd[n].hp = cd[n].mhp;
    cd[n].mp = cd[n].mmp;
    clear_chr(n);
    cd[n].x = cd[n].ix;
    cd[n].y = cd[n].iy;
    snprintf(buf.data(), buf.size(), "dead     : %s ", cd[n].name);
    log_print(buf.data());
    dmcheck_write(n, DMM_DEAD);
    dmcheck_write(n, DMM_TRYAGAIN);
    dmcheck_write(n, DMM_SAVEDATA);
    check_write(n, "#close\n");
    nfflush(n, 1000);   /* 送信バッファーフラッシュ bugfix 2011/11/26 */
  } else {
    /* change for control death penalty 20020204 */
    if (death_penalty < 3 || n >= MAX_NTTYS) {
      /* tax には切り上げた半分が入り、残り (切り捨てた半分) が手元に残る。 */
      tax += cd[n].gp - cd[n].gp / 2;
      cd[n].gp /= 2;
    }
    if (death_penalty < 2 || n >= MAX_NTTYS) {
      save_gp(n);
      cd[n].gp = 0;
    }
    if (death_penalty < 1 && n < MAX_NTTYS) {
      for (int lv = LV_FIGHTER; lv < MAX_LEVEL + LV_FIGHTER; lv++) {
        /* change for デスペナ割合設定 by NEO 2008/03/27 */
        int penalty_nlv = (cd[n].lv[lv] * (100 - penalty_rate)) / 100;
        /* change for デスペナリミッタ by NEO 2011/11/12 */
        int limit_nlv = 0;
        for (const auto &limit : kLevelLimits) {
          if (cd[n].lv[lv] < limit.threshold) {
            limit_nlv = (cd[n].lv[lv] / limit.step) * limit.step;
            break;
          }
        }
        if (dp_limitter == 1) {  /* add for デスペナリミッタ設定 by NEO 2011/12/4 */
          cd[n].lv[lv] = penalty_nlv > limit_nlv ? penalty_nlv : limit_nlv; /* limit_nlv で リミッタをかける */
        } else {
          cd[n].lv[lv] = penalty_nlv; /* リミッタをかけない */
        }
      }

      skill_down_dead(n);
      cd[n].mhp -= 2;
      cd[n].mmp -= 2;
      if (cd[n].exp < 1440) {
        if (cd[n].mhp < 1000)
          cd[n].mhp = 1000;
        if (cd[n].mmp < 1000)
          cd[n].mmp = 1000;
      }
      /* change for sheil cheat 20000522 */
      if (cd[n].mhp < 1)
        cd[n].mhp = 1;
      if (cd[n].mmp < 1)
        cd[n].mmp = 1;
    }
    /* add for control re-birth life 20020304 */
    switch (rebirth_life) {
      case 0:
      default:
        cd[n].hp = cd[n].mhp;
        cd[n].mp = cd[n].mmp;
        break;
      case 1:
        cd[n].hp = 1;
        cd[n].mp = 1;
        break;
      case 2:
        cd[n].hp = cd[n].mhp / 2;
        cd[n].mp = cd[n].mmp / 2;
        break;
    }
    cd[n].align = (cd[n].align * 75) / 100;
    if (n < MAX_NTTYS) {
      /* change for control death penalty 20020204 */
      if (death_penalty < 2) {
        for (int j = 0; j < MAX_ITEMS; j++) {
          if (cd[n].id[j].type != DT_NONE)
            save_id(n, j, 1);
          delete_item(&(cd[n].id[j]));
        }
      }
      clear_chr(n);
      cd[n].x = cd[n].ix;
      cd[n].y = cd[n].iy;
      snprintf(buf.data(), buf.size(), "dead     : %s ", cd[n].name);
      log_print(buf.data());
      dmcheck_write(n, DMM_DEAD);
      dmcheck_write(n, DMM_TRYAGAIN);
      dmcheck_write(n, DMM_SAVEDATA);
      check_write(n, "#close\n");
      nfflush(n, 1000);   /* 送信バッファーフラッシュ bugfix 2011/11/26 */
    }
  }
  close_connect(n);
  return;
}

void raise_dead(int n)
{
  std::array<char, 256> buf;
  int f = 0;
  ItemData *id = nullptr;

  if (dp_beginner == 1 && cd[n].exp < 1440) { /* 初心者加護 added by NEO 2011/11/13 */
    dmcheck_write(n, DMM_BEGINNER_DEAD);
    cd[n].mhp -= 2;  /* HPを犠牲に復活 by NEO 2011/11/25 */
    if (cd[n].mhp < 1000)
      cd[n].mhp = 1000;
  }

  /* add for save life 20000612 */
  if (CN_RAISEDEAD(n)) {
    f = 4;
    if (n >= MAX_NTTYS || cd[n].think != VF_HEAL) {
      CN_RAISEDEAD(n) = 0;
    } else
      f = 5;
  } else if (cd[n].oeq != nullptr && cd[n].oeq->SE == SE_RAISEDEAD) {
    id = cd[n].oeq;
    f = 1;
  } else if (cd[n].aeq != nullptr && cd[n].aeq->SE == SE_RAISEDEAD) {
    id = cd[n].aeq;
    f = 2;
  } else if (cd[n].weq != nullptr && (cd[n].weq->type & IT_MASK) != IT_FOOD && cd[n].weq->SE == SE_RAISEDEAD) {
    id = cd[n].weq;
    f = 3;
  }
  if (!f) {     /* error */
    dead(n);
    return;
  }
  dmprintf(n, 0, buf.data(), buf.size(), DMM_RAISEDEAD,
      (f < 4 ? id->name : (f == 4 ? DMM_RAISEDEAD_MAGIC : DMM_RAISEDEAD_SACRIFICE)));
  if (cd[n].type == CT_NORMAL)
    write_companion(n, 0, buf.data(), 1);/* 飛んだライズアイテム名を全域に表示　20050306 by NEO */
  else
    write_companion(n, 0, buf.data(), 0);/* NPCの場合は全域にしない　20050309 by NEO */
  if (f < 4 && (id->type & MT_MASK) != MT_NOBREAK) {
    dmprintf(n, 0, buf.data(), buf.size(), DMM_BROKEN_ITEM, id->name);
    delete_item(id);
    write_companion(n, 0, buf.data(), 0);
    id = nullptr;
    if (n >= MAX_NTTYS) {
      if (f == 1) {
        select_accessory(n);
        select_armor(n);
      } else if (f == 2) {
        select_armor(n);
      }
    }
    item_gc(n);
    set_fn(n);
  }
  /*dmprintf(n,0,buf, sizeof(buf),DMM_COMEBACK);
  write_companion(n, 0, buf, (n < MAX_NTTYS ? access_dm : 0));*/
  cd[n].hp = cd[n].mhp;
  cd[n].mp = cd[n].mmp;
  CN_GASIFY(n) = 0x10000;
  if (f >= 4) {
    cd[n].align -= 10; /* add with save life :-P */
    if (cd[n].align < 0)
      cd[n].align = 0;
  }
  print_status(n); /* added by Qajin 19991224 */

  /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
  load_switch(n,cd[n].x,cd[n].y,23);  /* キャラクタ復活時 */
  /* ↑ADD */

  return;
}

void kill_other(int as, int ds, int lv)
{
  std::array<char, 256> buf;

  if (as < MAX_NTTYS && ds < MAX_NTTYS) {
    dmprintf(ds, as, buf.data(), buf.size(), DMM_KILL_BY_PK); /* PKメッセージ by NEO 20050315 */
    write_companion(ds, as, buf.data(), 1);
  } else {
    dmprintf(ds, as, buf.data(), buf.size(), DMM_KILL_BY_OTHER);
    write_companion(ds, as, buf.data(), (ds < MAX_NTTYS ? access_dm : 0));
  }
  if (as < MAX_NTTYS) {
    int i = 0;
    for (; i < MAX_TALKS; i++)
      if (cd[as].td[i].type == TT_KILL) break;
    if (i < MAX_TALKS)
      talk_analyze(as,cd[as].td[i].string); /* change for psychogram wintalk 20001212 */
  }
  lv_up(as, ds, lv, 1);
  if (log_op || ds < MAX_NTTYS) {
    snprintf(buf.data(), buf.size(), "kill     : %s <- %s ", cd[ds].name, cd[as].name);
    log_print(buf.data());
  }
  if (as >= MAX_NTTYS) {
    cd[as].think = TH_WIN;
    if (ds == cd[as].angry) {
      cd[as].angry = -1;
#ifdef DEBUG_ANGRY
      /* 注意: debug_buf は DEBUG のときだけ定義される (このファイル先頭)。
       * DEBUG_ANGRY だけを定義したビルドでは未定義になる。元のまま。 */
      snprintf(debug_buf, sizeof(debug_buf), "no angry : %s(%d) <- %s(%d) ", cd[ds].name, ds, cd[as].name, as);
      log_print(debug_buf);
#endif
    }
  }
  return;
}

void   kill_self_msg(int n, char *s)
{
  std::array<char, 256> buf;

  dmprintf(n, 0, buf.data(), buf.size(), DMM_KILL_SELF, s);
  write_companion(n, 0, buf.data(), (n < MAX_NTTYS ? access_dm : 0));
  if (log_op || n < MAX_NTTYS) {
    snprintf(buf.data(), buf.size(), "kill self: %s ", cd[n].name);
    log_print(buf.data());
  }
}

void   lv_up(int n, int m, int lv, int f)
/* f == 0 :カルマの変化なし */
/* f > 0  :攻撃に関するカルマの変化 */
/* f < 0  :回復に関するカルマの変化 */
{
  int    pc; /* pc間 level変動制御 */

  if (!is_kill_master
      && ((n < MAX_NTTYS && cd[n].exp == 99999999)
          || (m < MAX_NTTYS && cd[m].exp == 99999999)))
    return;

  /* add for level control between pc 20000829 */
  if (n < MAX_NTTYS && m < MAX_NTTYS) {
    pc = 1;
    if (!std::strcmp(ip_address_check(n), ip_address_check(m)))
      pc = 2;
  } else
    pc = 0;

  if ((lv_up_between_pc & 4) && ((lv_up_between_pc & 3) <= pc))
    f = 0;

  if (cd[n].type == CT_NORMAL && (lv == LV_FIGHTER || lv == LV_WIZARD)) {
    if (cd[m].type == CT_NORMAL) {
      if (f < 0 && cd[m].align >= 50 && cd[n].align < 99)
        cd[n].align++;
      else if (f > 0 && cd[m].align < 50 && cd[n].align < 99)
        cd[n].align++;
    } else if (f > 0 && cd[m].type >= CT_MAGICAL) {
      if (PER_DICE < 5 && cd[n].align > 0)
        cd[n].align--;
    }
  }

  if (lv_up_between_pc && ((lv_up_between_pc & 3) <= pc))
    return;

  /* add for control death penalty (lv up) 20020204 */
  if (death_penalty)
    return;

  /* 元のコードは変数 i を3役 (上昇判定のしきい値・合計40000超えの
   * フラグ・減少ループの添字) に使い回していた。評価順序は変えず、
   * フラグだけ別名にして読みやすくした。 */
  int i = static_cast<int>(std::log10(static_cast<double>(cd[n].exp + 10.0)) * 10.0);
  if (PER_DICE < i) {
    int over_limit;
    if (40000 > (cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_WIZARD]
                 + cd[n].lv[LV_MERCHANT] + cd[n].lv[LV_CREATER]))
      over_limit = 0;
    else
      over_limit = 1;
    switch (lv) {
      default:
        return;
      case LV_FIGHTER:
        if (cd[m].lv[LV_FIGHTER] > cd[n].lv[LV_FIGHTER]
            && PER_DICE > cd[n].lv[LV_FIGHTER] / 100) {
          cd[n].lv[LV_FIGHTER]++;
          level_up_print(n); /* Add for レベルアップ表示 by NEO 2008/04/29 */
        }
        break;
      case LV_WIZARD:
        if (cd[m].lv[LV_WIZARD] > cd[n].lv[LV_WIZARD]
            && PER_DICE > cd[n].lv[LV_WIZARD] / 100) {
          cd[n].lv[LV_WIZARD]++;
          level_up_print(n); /* Add for レベルアップ表示 by NEO 2008/04/29 */
        }
        break;
      case LV_MERCHANT:
        if (PER_DICE > cd[n].lv[LV_MERCHANT] / 100) {
          cd[n].lv[LV_MERCHANT]++;
          level_up_print(n); /* Add for レベルアップ表示 by NEO 2008/04/29 */
        }
        break;
      case LV_CREATER:
        if (PER_DICE > cd[n].lv[LV_CREATER] / 100) {
          cd[n].lv[LV_CREATER]++;
          level_up_print(n); /* Add for レベルアップ表示 by NEO 2008/04/29 */
        }
        break;
    }

    if (over_limit) {
      /* 合計 40000 超え: ランダムな位置から他のレベルを1つ下げて
       * 帳尻を合わせる。下げる対象が無い (他が全部 0) と、今上げた
       * レベル自身を下げる。 */
      int j;
      for (i = frand(MAX_LEVEL) + LV_FIGHTER, j = LV_FIGHTER; j < MAX_LEVEL; i++, j++) {
        if (i >= MAX_LEVEL)
          i = LV_FIGHTER;
        if (i != lv && cd[n].lv[i] > 0) {
          cd[n].lv[i]--;
          break;
        }
      }
      if (j >= MAX_LEVEL)
        cd[n].lv[lv]--;
    }
  }
  return;
}

/* 新属性追加 20000324 */
int fn(int afn, int dfn, int p)
{
  static constexpr std::array<int, 6> fn_rate = {25, 50, 100, 100, 100, 200};
  int    i;

  if (afn == FN_RAINBOW)
    afn = frand(FN_NORMAL + 1);
  if (dfn == FN_RAINBOW)
    dfn = frand(FN_NORMAL + 1);

  if (!afn || !dfn)
    return (p);
  if (afn == FN_MIRROR || dfn == FN_MIRROR)
    i = (afn == dfn ? 2 : 0);
  else if (afn == FN_PRISM)
    i = (afn == dfn ? 2 : 5);
  else if (dfn == FN_PRISM)
    i = 1;
  else
    i = afn - dfn;
  i %= 6;
  if (i < 0)
    i += 6;
  /* i を 0..5 に丸めてから表を引く。afn / dfn が FN_NONE..FN_RAINBOW
   * (0..9) の範囲内なら従来と結果は同じ。範囲外のデータ由来の値でも
   * 境界外読みにならないよう、6 で割った余りに正規化する（属性の
   * 相性が 6 つで循環する、という元の i += 6 の意図の延長。
   * 2026-11-16 修正）。 */
  p = p * fn_rate[i] / 100;
  return (p);
}

int dice(int n)
{
  int sum = 0;
  for (; n > 0; n--)
    sum += frand(6) + 1;
  return (sum);
}

int lv_dice(int lv)
{
  /* RAND % (lv + 1) を 100 で割るため、lv が 99 以下だと常に 0。 */
  int sum = RAND % (lv + 1) / 100;
  return (sum);
}

/*----------------------------------
  平方根の代替関数
  ----------------------------------*/
int    fisqrt(int x)
{
  std::array<double, 5> d;
  double xd, yd;

  xd = static_cast<double>(x);
  yd = 1.0;
  while (xd >= 2.0) {
    xd /= 4.0;
    yd *= 2.0;
  }
  d[0] = xd - 1.0;
  for (int i = 1; i < 5; i++) {
    d[i] = d[i - 1] * (xd - 1.0);
  }
  yd *= 1.0 + d[0] / 2.0 - d[1] / 8.0 + d[2] / 16.0
    - d[3] * 5.0 / 128.0 + d[4] * 7.0 / 256.0;
  return (static_cast<int>(yd));
}

void set_fn(int n)
{
  if (!CN_COAT(n)) {
    if (cd[n].weq != nullptr) {
      cd[n].afn = cd[n].weq->fn;
    } else {
      cd[n].afn = FN_NONE;
    }
    if (cd[n].aeq != nullptr) {
      if (cd[n].aeq->SE == SE_ATPUP
          || cd[n].aeq->SE == SE_ATPDOWN)
        cd[n].afn = cd[n].aeq->fn;
    }
    if (cd[n].oeq != nullptr) {
      if (cd[n].oeq->SE == SE_ATPUP
          || cd[n].oeq->SE == SE_ATPDOWN)
        cd[n].afn = cd[n].oeq->fn;
    }
  }
  if (!CN_PROTECT(n) && !CN_SHELL(n) && !CN_HASTE(n)) {
    cd[n].dfn = (cd[n].aeq == nullptr ? FN_NONE : cd[n].aeq->fn);
    if (cd[n].weq != nullptr) {
      if ((cd[n].weq->type & IT_MASK) != IT_FOOD)
        if (cd[n].weq->SE == SE_DEFUP
            || cd[n].weq->SE == SE_DEFDOWN
            || cd[n].weq->SE == SE_MRPUP
            || cd[n].weq->SE == SE_MRPDOWN
            || cd[n].weq->SE == SE_HASTE
            || cd[n].weq->SE == SE_SLOW)
          cd[n].dfn = cd[n].weq->fn;
    }
  }
  if (!CN_SPELL(n)) {
    cd[n].sfn = FN_NONE;
    if (cd[n].weq != nullptr) {
      if ((cd[n].weq->type & IT_MASK) != IT_FOOD)
        if (cd[n].weq->SE == SE_SPELLUP
            || cd[n].weq->SE == SE_SPELLDOWN)
          cd[n].sfn = cd[n].weq->fn;
    }
    if (cd[n].aeq != nullptr) {
      if (cd[n].aeq->SE == SE_SPELLUP
          || cd[n].aeq->SE == SE_SPELLDOWN)
        cd[n].sfn = cd[n].aeq->fn;
    }
    if (cd[n].oeq != nullptr) {
      if (cd[n].oeq->SE == SE_SPELLUP
          || cd[n].oeq->SE == SE_SPELLDOWN)
        cd[n].sfn = cd[n].oeq->fn;
    }
  }
  if (cd[n].oeq != nullptr)
    if (cd[n].oeq->AT == AT_ABSORB)
      if (cd[n].oeq->fn != FN_NONE)
        cd[n].dfn = cd[n].oeq->fn;

  if (n >= MAX_NTTYS) {
    cd[n].wd = WD_NONE;
    for (int i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type != DT_NONE && cd[n].id[i].active_time != ULONG_MAX)
        if ((cd[n].id[i].type & IT_MASK) == IT_WEAPON ||
            (npc_use_charged_item &&
             (cd[n].id[i].type & IT_MASK) == IT_CHARGE &&
             (cd[n].id[i].SE == SE_HITTOUSE ||
              cd[n].id[i].SE == SE_HITISUSE)))
          cd[n].wd |= ((cd[n].id[i].type & WD_MASK) ^ WD_FORE);
  } else {
    if (cd[n].weq == nullptr) cd[n].wd = WD_FORE;
    else cd[n].wd = ((cd[n].weq->type & WD_MASK) ^ WD_FORE);
  }
  return;
}

int    ecn(int n, int se_t)
{
  int secn = 0;
  if (curse_check(n, se_t) == 0)/* 効果がないので ecn 0 */
    return (0);
  if (cd[n].weq != nullptr)
    if (((cd[n].weq->type) & 0xf) != IT_FOOD)
      if (se_t == cd[n].weq->SE)
        secn += cd[n].wsep;
  if (se_t == (cd[n].aeq != nullptr ? cd[n].aeq->SE : SE_NONE))
    secn += cd[n].asep;
  if (se_t == (cd[n].oeq != nullptr ? cd[n].oeq->SE : SE_NONE))
    secn += cd[n].osep;
  return (secn);
}

/* add 20010705 */
int sep_calc(int n, ItemData *id)
{
  int    sep;
  MagicData md;

  if ((id->type & IT_MASK) == IT_FOOD)
    return 0;      /* food は SE に対応するものがない */
  if (id->SE <= SE_NONE || id->SE > MAX_SE) /* S.E. janai */
    return (0);
  md.spell = id->name;
  md.effect = se2ef[id->SE]; /* bug(?) fix 20010705 */
  md.fn = id->fn;
  md.extension = id->extension;
  sep = mdp_calc(n, &md, 1, 1, nullptr) / 2;
  switch (id->SE) {
    case SE_ATPDOWN:
    case SE_DEFDOWN:
    case SE_MRPDOWN:
    case SE_SPELLDOWN:
    case SE_SLOW:
    case SE_KARMADOWN:
    case SE_BLIND:
    case SE_CONTRAST: /* add 20010226 */
      sep = -sep;
      break;
    default:
      break;
  }
  return (sep);
}

const int BIG_OFF[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};

char chr_marker(int n)
/* 地図行の大きさ印。'B' = 2x2 の大きな敵 (chr_big)、'*' = gigant、
   '#' = 通常。大きな敵は gigant より優先する (体が 2x2 の方を
   クライアントに伝えるため)。sg の隠蔽規則は従来どおり */
{
  if (!(cd[n].sg & 3) && chr_big(n))
    return ('B');
  if ((cd[n].sg & 1) || (!(cd[n].sg & 3) && gigant_check(n)))
    return ('*');
  return ('#');
}

int chr_big(int n)
{
  /* 2x2 の大きな敵か。gigant (表示の大きさ) とは別の設定で、
   * 敵データ phi.ed の :O B で指定された個体だけが対象。 */
  if (n < 0 || n >= MAX_NTTYS + MAX_ENEMIES)
    return (0);
  return (cd[n].big != 0);
}

int gigant_check(int n)
{
  /* NPC (または ggflag のときの PC) で mhp 5000 以上なら巨大。
   * LITTLE 状態なら打ち消される。元の2つの分岐は同じ結論だった
   * ため1つにまとめた。GIGANT 状態なら mhp に関係なく巨大。 */
  if (cd[n].mhp >= 5000 && (cd[n].type != CT_NORMAL || ggflag)) {
    if (TCN_LITTLE(n))
      return (0);
    else
      return (1);
  }

  if (TCN_GIGANT(n))
    return (1);
  return (0);
}

/* for switch action 17 add 20000106 */
int sg_check(int n)
{
  if (n < 0 || n >= (MAX_NTTYS + MAX_ENEMIES))  /* error */
    return (0);

  if (cd[n].sg & 4)
    return (1);
  if (cd[n].sg & 8)
    return (0);
  return (gigant_check(n));
}

int    analyze_numbers(int number[], int size, char *buf)
/* １以上の数字と '-' で構成された文字列 *buf を、*/
/* ばらばらにして number[] に格納。*/
/* '-' で終わっているものは、最後に -1 を格納する。*/
/* ただし、"-" だけの文字列に限って 1, -1 と格納する。*/
/* (つまり、-1 の前の数字から無限までということを表す) */
/* size は number[]の大きさ。返却値は、格納した数字の個数 */
/* 0 1つを返せるように変更 20000106 */
{
  /* 数字列の走査は元のポインタ歩行のまま。癖:
   * - 数字が直前以下 (k <= num) だと全体を捨てて 0 を返す。
   * - 範囲の後は num = to + 1 になるため、次の数字は範囲の末尾より
   *   大きくなければならない。
   * - "-" だけの入力は 1, -1 を格納する (size >= 2 が前提)。 */
  int i, k;
  char *p;
  int num;
  int from, to;

  for (i = 0, num = -1, p = buf, from = 0; i < size && *p != '\0';) {
    if (from == 0) {
      while ((*p < '0' || *p > '9') && *p != '-' && *p != '\0') p++;
      if (*p == '\0') return i;
      else if (*p == '-') from = (num > 0 ? num : 0) + 1;     /* "from -" 開始 */
      else {
        for (k = 0; '0' <= *p && *p <= '9'; p++)
          k = k * 10 + (*p - '0');
        if (k <= num) return 0;                 /* 小さな数字からの順番でない */
        number[i++] = num = k;
      }
    } else {
      while ((*p < '0' || '9' < *p) && *p != '\0') p++;
      if (*p == '\0') {                        /* "from -\0" */
        if (i == 0) number[i++] = 1;
        number[i++] = -1;
        return i;
      } else {
        for (to = 0; '0' <= *p && *p <= '9'; p++)
          to = to * 10 + (*p - '0');
        if (to < from) return 0;           /* "n - (n+1 以上)" でないとだめ */
        for (k = from; i < size && k <= to; i++, k++)
          number[i] = k;
        from = 0;
        num = to + 1;
      }
    }
  }
  return i;
}

int     analyze_single_number(char *buf)
/* 番号選択でひとつ指定する場合。*/
/* 数字が無かった場合は、0 を返す。読み込んだ数字を返す。*/
/* 数字 0 の指定は考慮外 */
/* テンキー選択に対応 added by Qajin 19991203 */
/* 数字がない時 -1 を返すよう変更 20000106 */
{
  char *p;
  int i;

  for (p = buf; *p != '\0' && (*p < '0' || '9' < *p); p++);
  if (*p == '\0') return -1;
  for (i = 0; '0' <= *p && *p <= '9'; p++)
    i = i * 10 + (*p - '0');
  return i;
}

int    analyze_hexnumbers(int number[], int size, char *buf)
/* analyze_numbers の 16進(char)版 added by Qajin 19991023 */
/* 00～ffが入る。終端値は00に変更。00は先頭にある時のみ値として有効 */
/* 終端を -1、int で格納するように変更 19991127 */
{
  /* 数字列の走査は元のポインタ歩行のまま。analyze_numbers との違い:
   * - 16進数字は小文字の a～f のみ (大文字 A～F は区切り扱い)。
   * - 順序検査は k < num (同値は許される。10進版は k <= num)。
   * - 末尾の -1 は無条件で書き込むため、呼び出し側は size より
   *   1 つ大きい配列を渡す必要がある (唯一の呼び出し側は
   *   ter[257] に size=256 を渡している)。
   * - 範囲の末尾が無い ("from -" で終端) と to = size-1 まで埋める。 */
  int i, k;
  char *p;
  int num;
  int from, to;

  for (i = 0, num = -1, p = buf, from = -1; i < size && *p != '\0';) {
    if (from == -1) {
      while ((*p < '0' || (*p > '9' && *p < 'a') || *p > 'f') && *p != '-' && *p != '\0') p++;
      if (*p == '\0')
        break;
      else if (*p == '-') from = num + 1;     /* "from -" 開始 */
      else {
        for (k = 0; ('0' <= *p && *p <= '9') || ('a' <= *p && *p <= 'f'); p++)
          k = k * 16 + (*p - (*p < 'a' ? '0' : 'a' - 10));
        if (k < num) return 0;                 /* 小さな数字からの順番でない */
        number[i++] = num = k;
      }
    } else {
      while ((*p < '0' || ('9' < *p && *p < 'a') || 'f' < *p) && *p != '\0') p++;
      if (*p == '\0') {                        /* "from -\0" */
        to = size - 1;
      } else {
        for (to = 0; ('0' <= *p && *p <= '9') || ('a' <= *p && *p <= 'f'); p++)
          to = to * 16 + (*p - (*p < 'a' ? '0' : 'a' - 10));
      }
      if (to < from) return 0;           /* "n - (n+1 以上)" でないとだめ */
      for (k = from; i < size && k <= to; i++, k++)
        number[i] = k;
      from = -1;
      num = to + 1;
    }
  }
  number[i++] = -1;
  return i;
}
