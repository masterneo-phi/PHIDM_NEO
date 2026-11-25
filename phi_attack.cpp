
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

/* 物理攻撃まわり。attack_other() が武器の攻撃方向にいる相手全員へ
   attack_to() を呼び、attack_to() が命中・ダメージ・装備の劣化・
   追加効果・殺害までを処理する。guard_self() は防御の構え、
   spat_check() はコマンド列からの技の判定、degrade() と
   speed_degrade() は装備の劣化。
   第9段階で C++ らしく書き換えたが、計算式・乱数の消費順序・
   癖はすべて従来どおりに保ってある。
   詳細は docs/cxx-port-notes.md の第9段階を参照。 */

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "phi_dm.h"
#include "phi_attack.h"
#include "phi_calc.h"
#include "phi_data.h"
#include "phi_enemy.h"
#include "phi_use.h"
#include "phi_str.h"
#include "phi_class.h" /* PHIDM NEO extension: phi.jb job/gender */

#ifdef DEBUG
char debug_buf[256];
#endif

extern int px[];
extern int py[];

namespace {

/* パーティ攻撃の印 (add for party attack 20020917)。
   attack_other() が「前方セルに同じパーティのメンバーがいる」場合に
   1 にし、attack_to() がダメージ 1/3 とクリティカル率の割算に使う。
   前方セルしか見ないため、横・後ろだけを攻撃する武器でも、味方が
   前に立っていれば立つ。attack_to() の呼び出しは attack_other()
   だけなので、常にここで設定した直後の値が読まれる。 */
int friend_attack;

} // namespace

/* 1 回の attack_other で同じ相手を二度攻撃しないための入口。
   槍の 2 マス先・複数方向の武器・2x2 の大きな敵の前縁など、攻撃セルが
   重なると同じ大きな敵が複数のセルに現れる (魔法側の big_already と
   同じ対策)。j == n (大きな敵が自分の体のあるセルを攻撃方向に含む
   場合) もここで弾く */
static int attack_once(int n, int j, int *hit_list, int *nhit, int *asf)
{
  int i;

  if (j < 0 || j == n)
    return (0);
  for (i = 0; i < *nhit; i++)
    if (hit_list[i] == j)
      return (0);
  if (*nhit < 32)
    hit_list[(*nhit)++] = j;
  attack_to(n, j);
  *asf = 1;
  return (1);
}

static void attack_line(int n, int tx, int ty, int v, int w, int spear, int *asf, int *hit_list, int *nhit)
/* (tx,ty) のマスのキャラ全員を攻撃する。槍ならバリアのない限り
   その1マス先 (v,w 方向) も攻撃する */
{
  MapData *pmap;
  int    i, j;

  if (tx < 0 || tx >= MAX_X || ty < 0 || ty >= MAX_Y)
    return;
  pmap = &(map[ty][tx]);
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = pmap->chr[i] - 1) < 0)
      continue;
    if (j == n)
      continue;
    attack_once(n, j, hit_list, nhit, asf);
  }
#ifndef OLDWEAPON
  if (spear && !(pmap->tip & T_SPEARBARRIER)) {
    tx += v;
    ty += w;
    if (tx < 0 || tx >= MAX_X || ty < 0 || ty >= MAX_Y)
      return;
    pmap = &(map[ty][tx]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
        continue;
      if (j == n)
        continue;
      attack_once(n, j, hit_list, nhit, asf);
    }
  }
#endif
}

void   attack_other(int n)
{
  int    i, j;
  int    tp, asf;
  int    spear;
  MapData *pmap;
  int    x,y,v,w;

  asf = 0;      /* 攻撃成功？ */
  int hit_list[32]; int nhit = 0;  /* 同じ相手への二重攻撃防止 */

  if (cd[n].weq == nullptr)
    tp = 0x0000;
  else {
    tp = ((cd[n].weq->type) & WD_MASK);

    if ((tp == 0) && (((cd[n].weq->type) & WT_MASK) == WT_WIDE))
      tp = 0x3000;
    if ((tp == 0) && (((cd[n].weq->type) & WT_MASK) == WT_AROUND))
      tp = 0x7000;
  }

  tp = tp ^ 0x8000;

#ifndef OLDWEAPON
  /* add for long range spear 20000518 */
  spear = ((cd[n].weq != nullptr && (cd[n].weq->type & WT_MASK) == WT_SPEAR)? 1 : 0);
#endif

  if (cd[n].st == ST_ATTACK && cd[n].wt > 0) {
    dmcheck_write(n,DMM_WAIT_ATTACK);
    cd[n].wt += 2;
    return;
  }
  /* for lag defence 20010717 */
  if (n < MAX_NTTYS && CN_ALAG(n) && CN_DLAG(n)) {
    dmcheck_write(n,DMM_ATTACK_WITH_LAG);
    return;
  }
  /* add for defense 20010703 */
  if (cd[n].st == ST_DEFEND && cd[n].wt > 0) {
    dmcheck_write(n,DMM_WAIT_ATTACK);
    return;
  }
  
  cd[n].st = ST_COMMAND;
  x = cd[n].x; y = cd[n].y;
  v = px[cd[n].d]; w = py[cd[n].d];

  friend_attack = 0; /* add for party attack 20020917 */
  if (cd[n].party) {
    pmap = &(map[y+w][x+v]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
        continue;
      if (cd[j].party == cd[n].party) {
        friend_attack = 1;
        break;
      }
    }
  }
  
  pet_attack_begin(n);  /* ペットのプール消費は1コマンド1回分まで (PHIDM NEO extension) */

  /* 4方向のブロックは座標式が違うだけで同形。槍の2セル先は、
     バリア判定が「1セル先」の tip を見る点も含め、元の形の
     まま残してある (phi_m57.cpp のオブジェ走査と同じ判断)。 */
  if ((tp & WD_FORE) && chr_big(n)) {
	/* 2x2 の大きな敵: 正面は体の前縁の外側2マス。
	   基準マスの正面は自らの体なので、通常の正面処理は行わない */
	int ci, tx, ty;
	for (ci = 0; ci < 4; ci++) {
	  tx = x + BIG_OFF[ci][0] + v;
	  ty = y + BIG_OFF[ci][1] + w;
	  if (tx >= x && tx <= x + 1 && ty >= y && ty <= y + 1)
		continue;   /* 体内に戻る方向は前縁ではない */
	  attack_line(n, tx, ty, v, w, spear, &asf, hit_list, &nhit);
	}
  } else if (tp & WD_FORE) {
	pmap = &(map[y+w][x+v]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
  continue;
      attack_once(n, j, hit_list, &nhit, &asf);
    }
#ifndef OLDWEAPON
    /* add for long range spear 20000518 */
    if (spear && !(pmap->tip & T_SPEARBARRIER)
     && y+w*2 >= 0 && x+v*2 >= 0 && y+w*2 <= MAX_Y && x+v*2 <= MAX_X) {
	pmap = &(map[y+w*2][x+v*2]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
          continue;
        attack_once(n, j, hit_list, &nhit, &asf);
      }
    }
#endif
  }
  if (tp & WD_BACK) {
	pmap = &(map[y-w][x-v]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
  continue;
      attack_once(n, j, hit_list, &nhit, &asf);
    }
#ifndef OLDWEAPON
    /* add for long range spear 20000518 */
    if (spear && !(pmap->tip & T_SPEARBARRIER)
     && y-w*2 >= 0 && x-v*2 >= 0 && y-w*2 <= MAX_Y && x-v*2 <= MAX_X) {
	pmap = &(map[y-w*2][x-v*2]);
      for (i = 0; i < MAX_CHRS_INONE; i++) {
        if ((j = pmap->chr[i] - 1) < 0)
          continue;
        attack_once(n, j, hit_list, &nhit, &asf);
      }
    }
#endif
  }
  if (tp & WD_LEFT) {
	pmap = &(map[y-v][x+w]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
  continue;
      attack_once(n, j, hit_list, &nhit, &asf);
    }
#ifndef OLDWEAPON
    /* add for long range spear 20000518 */
    if (spear && !(pmap->tip & T_SPEARBARRIER)
     && y-v*2 >= 0 && x+w*2 >= 0 && y-v*2 <= MAX_Y && x+w*2 <= MAX_X) {
	pmap = &(map[y-v*2][x+w*2]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
          continue;
        attack_once(n, j, hit_list, &nhit, &asf);
      }
    }
#endif
  }
  if (tp & WD_RIGHT) {
	pmap = &(map[y+v][x-w]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
  continue;
      attack_once(n, j, hit_list, &nhit, &asf);
    }
#ifndef OLDWEAPON
    /* add for long range spear 20000518 */
    if (spear && !(pmap->tip & T_SPEARBARRIER)
     && y+v*2 >= 0 && x-w*2 >= 0 && y+v*2 <= MAX_Y && x-w*2 <= MAX_X) {
	pmap = &(map[y+v*2][x-w*2]);
    for (i = 0; i < MAX_CHRS_INONE; i++) {
      if ((j = pmap->chr[i] - 1) < 0)
          continue;
        attack_once(n, j, hit_list, &nhit, &asf);
      }
    }
#endif
  }
  if (asf) {
    if (cd[n].weq != nullptr) {
	  /* re-sequence for bug fix 20030529 */
      if ((cd[n].weq->type & IT_MASK) == IT_WEAPON) {
        skillup(n,SKL_DETECT_IT,IT_WEAPON,99,50,1);
        skillup(n,SKL_DETECT_WT,(cd[n].weq->type & WT_MASK)/WT_PLACE,99,50,1);
      }
      if (((cd[n].weq->type & IT_MASK) == IT_CHARGE) && (cd[n].weq->SE == SE_HITTOUSE)) {
  for (j = 0; j < MAX_ITEMS; j++) {
    if (&(cd[n].id[j]) == cd[n].weq)
      break;
  }
  if (j < MAX_ITEMS)
    (void) use_item(n, j);
      }
      /* add for lag attack 20020115 */
      if (n < MAX_NTTYS && CN_ALAG(n)) {
		CN_DLAG(n) = 1;
	  }
    }
  }
  /* add for hit switch 20020530 */
  if (map[y+w][x+v].tip & T_SWITCH)
    load_switch(n, x + v, y + w, 17);
  return;
}

void attack_to(int as, int ds)
{
  std::array<char, 256> buf;
  int    special;
  int    hr, avr;
  int    atp;
  int    tp, ae, otp; /* add for knucle special 20000530 */
  int    wtp;
  long long dmg; /* bug(?) fix 20011231 */
  int    i;
  int    fwd;
  int    counter; /* add for counter attack 20000518 */
  int    weight;  /* add for hammer 20000526 */
  int    skill_w,skill_o,skill_ds;  /* add for weapon skill 20020501 */
  int    target; /* ADD スイッチデータ追加 2005/11/26 Savarl.F */
  int    mrr, il; /* add for defence by illusion at 20020615 from JETDM by NEO 20120206 */
  int    bs;      /* add for バーサクによる攻撃力アップ by NEO 20120206 */

  angry_check(as,ds);

  if (cd[as].weq == nullptr) {
    cd[as].wt = 0;    /*- TCN_HASTE(as);*/
    atp = TCN_COAT(as);
    otp = tp = DT_NONE & WT_MASK;
    wtp = DT_NONE & IT_MASK;
    ae = 0;
    weight = 0; /* add for hammer */
  } else {
    cd[as].wt = cd[as].weq->wp - TCN_HASTE(as);
    atp = cd[as].weq->ATP + TCN_COAT(as);
    otp = tp = (cd[as].weq->type & WT_MASK);
    wtp = (cd[as].weq->type & IT_MASK);
    ae = (wtp == IT_WEAPON ? cd[as].weq->AE : 0);
    weight = cd[as].weq->wp; /* add for hammer */
  }
/* add for バーサクによる攻撃力アップ by NEO 20120206 */
    bs = TCN_BERSERK(as);
    if ( bs > 0 )
    {
        if ( bs > 50 )	/* バーサクによる攻撃力アップは */
        bs = 50;	/* 最大で 1.5 倍まで */
        atp = (int)std::floor( (double)atp * ( (double)( 100 + bs ) / 100. ) );
    }
/* ここまで by NEO 20120206 */
#ifndef OLDWEAPON
  /* add for counter attack 20000518 */
  counter = 0;
  if (counter_effect != 0) { /* カウンター効果係数が0だとカウンター処理しない added by NEO 2011/12/10 */
  	if (cd[ds].st == ST_ATTACK && !TCN_BERSERK(as) && (as < MAX_NTTYS || cd[as].at == AT_COUNTER)) {
  	  /* com の中で最初に 'A' が出る位置でカウンター係数が決まる。
  	     switch は字下げに反してループの外にある (ループ本体は
  	     if の break だけ)。防御側が PC のときは添字 1 から探す。 */
  	  for (i=(ds<MAX_NTTYS?1:0);i<10;i++) /* balance change 20001013 */
  	    if (cd[ds].com[i] == 'A')
  	      break;
  	    switch(i) {  /* balance change 20001004 */
  	      case 0:
  	          counter = 4;
  	          break;
  	      case 1:
  	          counter = 3;
  	          break;
  	      case 2:
  	      case 3:
  	          counter = 2;
  	          break;
  	      case 4:
  	      case 5:
  	          counter = 1;
  	          break;
  	      default:
  	          if (as < MAX_NTTYS && cd[as].at == AT_COUNTER)
  	            counter = 1;
  	          break;
  	    }
  	  if (counter > 0 && as >= MAX_NTTYS) /* balance change 20000728/bug fix 20001115 */
  	     counter = 1;
  
    if (counter > 0) { /* カウンターでない場合も素手だと属性が変化していた bugfix by NEO 20120212 */
  	  if (otp == WT_FIST) { /* 素手の場合カウンター係数が２倍 */
  	    counter *= 2;
  	    if (cd[ds].weq == nullptr) {
  	      tp = DT_NONE & WT_MASK;
  	    /*  ae = 0; */
  	    } else {
  	      tp = (cd[ds].weq->type & WT_MASK);
  	    /*  ae = ((cd[ds].weq->type & IT_MASK) == IT_WEAPON ? cd[ds].weq->AE : 0);*/
  	    }
  	  }
/*  if (counter > 0) {   delete for bugfix by NEO 20120212 */
  	    dmprintf(as,ds, buf.data(), buf.size(),DMM_COUNTER_ATTACK);
  	    write_companion(as,ds, buf.data(), 0);
/*      atp = atp + ((cd[ds].weq != NULL ? cd[ds].weq->ATP : 1) + TCN_COAT(ds)) * counter / 2;  カウンター効果演算 old */
  	    atp = atp + ((cd[ds].weq != nullptr ? cd[ds].weq->ATP : 1) + TCN_COAT(ds)) * counter / (2 * counter_effect); /* カウンター効果係数　changed by NEO 2011/12/6 */
  	  }
  	}
  }
#endif

#ifdef NO_SKILL
  cd[as].wt = cd[as].wt * (10000.0 - cd[as].lv[LV_FIGHTER]) / 5000;
#else
  /* add for weapon skill 20020501 */
  skill_w = cd[as].skill[SKL_WEAPON][tp/WT_PLACE];
  skill_o = cd[as].skill[SKL_WEAPON][otp/WT_PLACE];
  skill_ds = cd[ds].skill[SKL_WEAPON][tp/WT_PLACE];

  cd[as].wt *= (10000.0 - cd[as].lv[LV_FIGHTER]) / 5000;
  /* add for weapon skill 20020501 */
  cd[as].wt *= (10000.0 - skill_o) / 5000;
#endif

  if (jb_job(as) == 6)  /* 盗賊の攻撃ウェイト軽減 (PHIDM NEO extension) */
    cd[as].wt = cd[as].wt * (100 - jb_rate(6, 0)) / 100;
  if (cd[as].wt < 5)
    cd[as].wt = 5;
  cd[as].st = ST_ATTACK;
  check_write(as, (char *)"#attack\n");
  if (atp < 1)
    atp = 1;
#ifdef NO_SKILL
  special = (as < MAX_NTTYS ? spat_check(cd[as].com, otp / WT_PLACE, cd[as].str, sizeof(cd[as].str),0) : 0); /* 技のチェック */
#else
  special = (as < MAX_NTTYS ? spat_check(cd[as].com, otp / WT_PLACE, cd[as].str, sizeof(cd[as].str),skill_o) : 0);
#endif
  if (special) {
    dmprintf(as,ds, buf.data(), buf.size(), DMM_SPECIAL_ATTACK, cd[as].str);
    write_companion(ds, as, buf.data(), 0);
  }
  avr = 50;
  if (cd[ds].aeq != nullptr)
    avr = avr * (100 - cd[ds].aeq->wp) / 100;
  if (TCN_HASTE(ds) < 0)
    avr = avr * (100 + TCN_HASTE(ds)) / 100;
  else if (TCN_HASTE(ds) > 0)
    avr = 100 - (100 - avr) * (100 - TCN_HASTE(ds)) / 100; /* bug? fix 20001206 */
  if (TCN_PALSY(ds))
    avr = 0;
  /* add for defence 20010703 */
  if (cd[ds].st == ST_DEFEND)
    avr = avr * 3 / 2;
  if (tp != WT_STINGER && tp != WT_SPEAR)
    dmg = frand(atp * 5 + 1) + (atp * 5 + 1); /* STINGER と SPEAR以外のダメージ設定 */
  else
    dmg = (std::abs(frand(24) - 9) + 1) * atp; /* STINGER と SPEAR の場合 */
  /* change for technical sword 20000518 */
#ifdef OLDWEAPON
  dmg = dmg * (100 + special) / 100;
#else

#ifdef NO_SKILL
  dmg = dmg * (100 + special * (tp == WT_SWORD ? 5 : (otp == WT_BLADE ? 1 : 2))) / 100; /* 剣(5倍),ブレード(等倍),その他(2倍)の技効果を加算 */
#else
  /* change for weapon skill 20020501 */
  dmg *= (100 + special * ((tp == WT_SWORD ? 5 : (otp == WT_BLADE ? 1 : 2)) * ((5000.0 + skill_o)/10000.0))) / 100;
#endif

#endif

#ifdef NO_SKILL

if (attack_effect == 1) { /* 新攻撃効果設定 added by NEO 2011/12/6 */
  dmg = dmg * (5000 + cd[as].lv[LV_FIGHTER] * 2) / 5000; /* 乗数１～５ ダメージ差５倍 2011/12/6 */
} else if (attack_effect == 2) {
  dmg = dmg * (5000 + cd[as].lv[LV_FIGHTER] * 4) / 5000; /* 乗数１～９ ダメージ差９倍 2011/12/6 */
} else
  dmg = dmg * (10000 + cd[as].lv[LV_FIGHTER]) / 5000;  /* Flv によるダメージ加算 old 乗数２～４　ダメージ差２倍 */

#else
  /* change for weapon skill 20020501 */
  dmg = dmg * (5000 + cd[as].lv[LV_FIGHTER] + skill_o) / 5000;
#endif
  dmg = fn(cd[as].afn, cd[ds].dfn, dmg); /* ダメージに新属性(レインボー,ミラー,プリズム)の係数を加算 */
  if (ae == AE_LAW) {
    dmg = dmg * (150-cd[as].align)*((cd[ds].align+1)*2-cd[as].align)/10000; /* Holy attack のダメージ加算 */
  } else if (ae == AE_CHAOS) {
    dmg = dmg * (100 + cd[as].align) / 100; /* Chaos attack のダメージ加算 */
  }
  if (friend_attack)
    dmg = dmg / 3; /* Party の場合はダメージを1/3に */
  if (cd[as].weq != nullptr) {
    /* change for weapon skill 20020501 */
#ifdef NO_SKILL
    if (PER_DICE < cd[as].weq->wp / 10 + 1 && PER_DICE >= cd[as].lv[LV_FIGHTER] / 100
#else
    if (PER_DICE < cd[as].weq->wp / 10 + 1 && PER_DICE >= (cd[as].lv[LV_FIGHTER] + skill_o)/ 200
#endif
  && otp != WT_FIST && otp != WT_FANG
  && cd[as].type == CT_NORMAL &&
  (cd[as].weq->type & MT_MASK) != MT_NOBREAK) {
      dmprintf(as, ds, buf.data(), buf.size(),DMM_FUMBLE);
      write_companion(as, ds, buf.data(), 0);
      for (i = 0; i < MAX_ITEMS; i++)
  if (&(cd[as].id[i]) == cd[as].weq)
    break;
      if (degrade(cd[as].weq)==2) {
  dmprintf(as, ds, buf.data(), buf.size(),DMM_BROKEN_WPN);
  write_companion(as, ds, buf.data(), 0);
      } else if (i < MAX_ITEMS)
  save_id(as, i, 1);
      item_gc(as);
      set_fn(as);
      wtp = DT_NONE & 0xf;
      return;
    }
  }
  if (CN_FORCE(as) & 0xffff) {
    dmprintf(as, ds, buf.data(), buf.size(),DMM_ATTACK_WITH_FORCE);
    check_write(as, buf.data());
    return;
  }
  if (CN_GASIFY(ds) >= 60 || (ds < MAX_NTTYS && CN_ALAG(ds))) {
    dmprintf(as,ds, buf.data(), buf.size(),DMM_INVALID_ANY_ATTACK);
    check_write(as, buf.data());
    return;
  }
  /* add for deny PK 20020204 */
  if (deny_pk && as < MAX_NTTYS && ds < MAX_NTTYS) {
    dmprintf(as,ds, buf.data(), buf.size(),DMM_DENY_PK);
    check_write(as, buf.data());
	return;
  }
  if (ae == AE_MENTAL || ae == AE_ENERGY) {
    if (CN_REFLEX(ds)) {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_INVALID_ENG_ATTACK);
      check_write(as, buf.data());
      return;
    }
  } else {
    if (CN_FORCE(ds)) {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_INVALID_PHY_ATTACK);
      check_write(as, buf.data());
      return;
    }
  }
  hr = PER_DICE;
  /* 武闘家のクリティカル率ボーナス (PHIDM NEO extension)。
     閾値式は NO_SKILL 版とスキル版で異なるため、ダイス側に加算して
     両分岐 (と OLDWEAPON) に一様に効かせる。 */
  int jb_crit = (jb_job(as) == 3) ? jb_rate(3, 0) : 0;
  /* change for technical criticalize 20000518 */
  /* change for defence against blade critical 20010703 */
#ifdef OLDWEAPON
  if (hr + jb_crit >= (tp != WT_BLADE ? 99 : 95 - (cd[as].lv[LV_FIGHTER] / 150)))
#else

#ifdef NO_SKILL
  if (hr + jb_crit >= 100 - ((tp != WT_BLADE || cd[ds].st == ST_DEFEND) ? (1+special/10) : (5*(20+special)*(cd[as].lv[LV_FIGHTER]+5001   )/20/5000))/(friend_attack?3:1) )
#else
  if (hr + jb_crit >= 100 - ((tp != WT_BLADE || cd[ds].st == ST_DEFEND) ? (1+special/10) : (5*(20+special)*(cd[as].lv[LV_FIGHTER]+skill_w)/20/5000))/(friend_attack?3:1) )
#endif

#endif
  {
    dmprintf(as,ds, buf.data(), buf.size(),DMM_CRITICAL);
    write_companion(ds, as, buf.data(), 0);
#ifndef OLDWEAPON
    /* add for blade weak point 20000519 */
    if (otp == WT_BLADE) {
      if (axe_blade_setting == 1) /* 斧＆ブレード補正がONの場合 added by NEO 2011/12/8 */
      cd[as].wt *= (2 + (((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) / 50)); /* Wait時間を敵DEFに応じて設定 changed by NEO 2011/12/10 */
      else
      cd[as].wt *= 2; /* Wait時間を2倍に default */
    }     
#endif
  } else {
    if (cd[as].weq == nullptr) {
      hr = hr * (100 / (tp == WT_WHIP ? 1 : 2)) / 100;
    } else {
      hr = hr * (100 - cd[as].weq->wp / (tp == WT_WHIP ? 1 : 2)) / 100;
    }
    hr += (100 - hr) * (100 + special) / 100;

#ifdef NO_SKILL
    if (hr + lv_dice(cd[as].lv[LV_FIGHTER]) < avr + lv_dice(cd[ds].lv[LV_FIGHTER]))
#else
    if (hr + lv_dice((cd[as].lv[LV_FIGHTER]+skill_w)/2) < avr + lv_dice((cd[ds].lv[LV_FIGHTER])+skill_ds)/2)
#endif
    {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_MISS_ATTACK);
      write_companion(ds, as, buf.data(), 0);
      return;
    }
    if (cd[ds].wt == 0 || cd[ds].st == ST_DEFEND)
      if ((cd[ds].weq == nullptr) || ((cd[ds].weq->type & WT_MASK) == WT_FIST))
  if (tp != WT_STRIKER && tp != WT_WHIP
      && tp != WT_STINGER && tp != WT_SPEAR
      && tp != WT_FANG){
    fwd = (4 + cd[as].d - cd[ds].d) % 4;
    if ((cd[ds].weq == nullptr && fwd == 2)
        || (cd[ds].weq != nullptr &&
        ((fwd == 2 && (cd[ds].weq->type ^ 0x8000) & WD_FORE)
         || (fwd == 3 && (cd[ds].weq->type ^ 0x8000) & WD_RIGHT)
         || (fwd == 0 && (cd[ds].weq->type ^ 0x8000) & WD_BACK)
         || (fwd == 1 && (cd[ds].weq->type ^ 0x8000) & WD_LEFT))))
/* cd[ds].wd is *potential* attack direction,                  */
/* so, if you use cd[ds].wd instead of cd[ds].weq->type,       */
/* he can caught attack by direction of weapon he don't equip. */

      if (PER_DICE < ((cd[ds].lv[LV_FIGHTER] < 2000 ? cd[ds].lv[LV_FIGHTER] / 100 : 20)
          *(cd[ds].st == ST_DEFEND ? 3 : 2) / 2)) {
        dmprintf(as,ds, buf.data(), buf.size(),DMM_CATCH_ATTACK);
        write_companion(ds, as, buf.data(), 0);
        return;
      }
  }
#ifdef OLDWEAPON
    if (tp == WT_AXE || tp == WT_STRIKER)
      if (tp == WT_AXE)
  dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) / ((cd[as].lv[LV_FIGHTER] > 1999) ? 2 : 1.25 + cd[as].lv[LV_FIGHTER] / 3000.0)) / 100;
      else
  dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) / ((cd[as].lv[LV_FIGHTER] > 1999) ? 2 : 1.5 + cd[as].lv[LV_FIGHTER] / 4000.0)) / 100;
#else
    /* change for technical axe 20000530 */
    if (tp == WT_AXE)

#ifdef NO_SKILL
     if (axe_blade_setting == 1) { /* 斧＆ブレード補正がONの場合 added by NEO 2011/12/8 */
  dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) * (100 - ((5 + 5 * (cd[as].lv[LV_FIGHTER]) / 5000.0)*(1 + special/25.0)) / 2 )/100) / 100; /* 斧の場合の減算を１／２に changed by NEO 2011/12/8 */
     } else
  dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) * (100 - (5 + 5 * (cd[as].lv[LV_FIGHTER]) / 5000.0)*(1 + special/25.0))/100) / 100; /* 斧の場合 default */
/* ダメージを敵の防御率分減少(メンタル,エナジーの場合はMrpで) */
#else
  dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds)) * (100 - (5 + 5 * (cd[as].lv[LV_FIGHTER]+skill_w) / 10000.0)*(1 + special/25.0))/100) / 100;
#endif

#endif
    else
      dmg = dmg * (100 - ((ae == AE_MENTAL || ae == AE_ENERGY) ? mr_rate(ds) : df_rate(ds))) / 100; /* その他の武器の場合 */
  }

  /* ダメージ変化属性確認 */
  if (cd[ds].innate >= IA_RICK) {
    hr = cd[ds].innate;

    if (hr & IA_SAGA) {
      if (!(((ae == AE_SAGA1) && (hr & IA_SAGA1))
      || ((ae == AE_SAGA2) && (hr & IA_SAGA2))
      || ((ae == AE_SAGA3) && (hr & IA_SAGA3))
      || ((ae == AE_SAGA4) && (hr & IA_SAGA4))))
  dmg = 0; /* SAGA武器で無い場合はダメージ0 */
    } else if (hr & IA_GURDIAN) {
      if (!(((ae == AE_DRAGON) && (hr & IA_DRAGON))
      || ((ae == AE_GOD) && (hr & IA_GOD))
      || ((ae == AE_ELEMENT) && (hr & IA_ELEMENT))
      || ((ae == AE_DEMON) && (hr & IA_DEMON))
      || ((ae == AE_GURD1) && (hr & IA_GURD1))
      || ((ae == AE_GURD2) && (hr & IA_GURD2))
      || ((ae == AE_GURD3) && (hr & IA_GURD3))
      || ((ae == AE_GURD4) && (hr & IA_GURD4))))
  dmg = dmg / 2; /* xxxx killer系以外の武器ではダメージ半減 */
    } else if (hr & IA_RICK) {
      if ((ae < AE_RICK) || (ae > AE_SAGA4)) {
  if (cd[as].weq == nullptr)
    dmg = 0; /* ライカンスロープ,SAGA4に武器を持たない場合はダメージ0 */
  else
    switch ((cd[as].weq->type) & MT_MASK) {
    case MT_SILVER:
    case MT_MYTHRIL:
    case MT_MAGICAL:
    case MT_NOBREAK:
      break;
    default:
      dmg = 0; /* ライカンスロープ,SAGA4に銀,ミスリル,マジック,NOBREAK以外の場合はダメージ0 */
    }
      }
    }
  }
  /* 変化してもメッセージはなし */

#ifndef OLDWEAPON
  /* add for spear knock back 20000601 */
  if (tp == WT_SPEAR) {
    if (std::abs(cd[as].x-cd[ds].x) < 2 && std::abs(cd[as].y-cd[ds].y) < 2 &&

#ifdef NO_SKILL
        PER_DICE < 15 + cd[as].lv[LV_FIGHTER] / 500 + weight / 5)
#else
        PER_DICE < 15 + (cd[as].lv[LV_FIGHTER] + skill_w) / 1000 + weight / 5)
#endif
        {
          move_chr(ds, cd[ds].x, cd[ds].y,
             cd[ds].x * 2 - cd[as].x,
             cd[ds].y * 2 - cd[as].y, 0);
      /* bug(?) fix 20020531 */
      if (std::abs(cd[as].x-cd[ds].x) > 1 || std::abs(cd[as].y-cd[ds].y) > 1)
        return;
    }
  }
#endif

  if (dmg < 0)
    dmg = 0;
  if (jb_job(as) == 1)  /* 戦士の物理ダメージボーナス (PHIDM NEO extension) */
    dmg = dmg * (100 + jb_rate(1, 0)) / 100;
  if (wtp == IT_WEAPON && ae == AE_MENTAL) {
    /* change for sheil cheat 20000522 */
    if (cd[ds].mmp == 0 && (cd[ds].exp == 99999999 || ds >= MAX_NTTYS)) {
      dmprintf(as, ds, buf.data(), buf.size(),DMM_INVALID_ANIMATED);
      check_write(as, buf.data());
      return;
    }
    if (cd[ds].at == AT_ABSORB && fn(cd[as].afn,cd[ds].dfn,100) == 25) {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_ABS_FROM_ATTACK,dmg);
      write_companion(ds, as, buf.data(), 0);
      i = hp_dmg(ds, dmg, 1);
      dmprintf(ds,as, buf.data(), buf.size(),DMM_LEFT_HP);
    } else {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_ATTACK_MP,dmg);
      write_companion(ds, as, buf.data(), 0);
      i = mp_dmg(ds, -dmg, 1);
      dmprintf(ds,as, buf.data(), buf.size(),DMM_LEFT_MP);
    }
  } else {
    /* change for sheil cheat 20000522 */
    if (cd[ds].mhp == 0 && (cd[ds].exp == 99999999 || ds >= MAX_NTTYS)) {
      dmprintf(as, ds, buf.data(), buf.size(), DMM_INVALID_ASTRAL);
      check_write(as, buf.data());
      return;
    }
    if (cd[ds].at == AT_ABSORB && fn(cd[as].afn,cd[ds].dfn,100) == 25) {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_ABS_FROM_ATTACK,dmg);
      write_companion(ds, as, buf.data(), 0);
      i = hp_dmg(ds, dmg, 1);
    } else {
      dmprintf(as,ds, buf.data(), buf.size(),DMM_ATTACK_HP,dmg);
      write_companion(ds, as, buf.data(), 0);
      i = hp_dmg(ds, -dmg, 1);
      /* ペットの追加攻撃: 命中確定後 (この時点の dmg は Flv 倍率・
         df 軽減まで適用済みの最終ダメージ) に差し込む。attack_to の
         外に置くと外れにも追随し、deny_pk の早期 return も通らない
         (PHIDM NEO extension) */
      pet_assist(as, ds, dmg, friend_attack);
    }
    dmprintf(ds,as, buf.data(), buf.size(),DMM_LEFT_HP);
  }
  check_write(ds, buf.data());
  if (cd[as].weq != nullptr) {
    atp = cd[as].weq->ATP + OCN_COAT(as); /* bug fix 20010429 */
    atp = (atp == 0 ? 1 : atp);
	if (PER_DICE < (magic_anti_degrade ? (10 * cd[as].weq->ATP / atp) : 10))
    if (degrade(cd[as].weq)==2) {
      dmprintf(as, ds, buf.data(), buf.size(),DMM_BROKEN_WEAPON);
      write_companion(as, ds, buf.data(), 0);
      cd[as].afn = FN_NONE;
      cd[as].weq = nullptr;
      cd[as].wsep = 0; /* bug fix 20000525 */
      item_gc(as);
      wtp = DT_NONE & 0xf;
    }
  }
  if (cd[ds].aeq != nullptr) {
    skillup(ds,SKL_DETECT_IT,IT_ARMOR,0,50,1);
    if (PER_DICE < (magic_anti_degrade ? 10 * (100 - OCN_PROTECT(ds)) / 100 : 10))
    if (degrade(cd[ds].aeq)==2) {
      dmprintf(ds, as, buf.data(), buf.size(),DMM_BROKEN_ARMOR);
      write_companion(ds, as, buf.data(), 0);
      cd[ds].aeq = nullptr;
      cd[ds].asep = 0;
      if (ds >= MAX_NTTYS)
  select_armor(ds);
      item_gc(ds);
      set_fn(ds);
    }
  }
  if (cd[ds].oeq != nullptr) {
    skillup(ds,SKL_DETECT_IT,IT_ACCESSORY,0,50,1);
    if (PER_DICE < (magic_anti_degrade ? 10 * (100 - OCN_SHELL(ds)) / 100 : 10))
    if (degrade(cd[ds].oeq)==2) {
      dmprintf(ds, as, buf.data(), buf.size(),DMM_BROKEN_ACCESORY);
      write_companion(ds, as, buf.data(), 0);
      cd[ds].oeq = nullptr;
      cd[ds].at = AT_NONE;
      cd[ds].osep = 0;
      if (ds >= MAX_NTTYS) {
  select_accessory(ds);
  select_armor(ds);
      }
      item_gc(ds);
      set_fn(ds);
    }
  }
#ifndef OLDWEAPON
  /* add for Weapon break 20000525 */
  if (tp == WT_STRIKER) {
    /* add ATP Down 20000526 */
#ifdef NO_SKILL
    if (PER_DICE < (10 + special / 10) * weight / 24)
#else
    if (PER_DICE < (10 + special / 10) * weight * ((5000+skill_w)/5000.0) / 24 /(friend_attack+1))
#endif
    {
        if (CN_COAT(ds) == 0)
          NCN_COAT(ds) = 1;
#ifdef NO_SKILL
        CN_COAT(ds) -= (frand(weight / 2 + 1) + weight / 2 + 1) / 5;
#else
        CN_COAT(ds) -= (frand(weight / 2 + 1) + weight / 2 + 1) / 5 * ((5000+skill_w)/5000.0);
#endif
        dmprintf(as,ds, buf.data(), buf.size(),DMM_HAMMER_ATPDOWN);
        write_companion(ds, as, buf.data(), 0);
    }
#ifdef NO_SKILL
    if (cd[ds].weq != nullptr && PER_DICE < (10 + special/10) * weight / 32)
#else
    if (cd[ds].weq != nullptr && PER_DICE < (10 + special/10) * weight * ((5000+skill_w)/5000.0) / 32 /(friend_attack+1))
#endif
    {
      if (degrade(cd[ds].weq)==2) {
        dmprintf(ds, as, buf.data(), buf.size(),DMM_BROKEN_WEAPON);
        write_companion(ds, as, buf.data(), 0);
        cd[ds].weq = nullptr;
        cd[ds].wsep = 0;
        item_gc(ds);
        set_fn(ds);
      }
    }
    /* add Slow 20000526 */
#ifdef NO_SKILL
    if (PER_DICE < (10 + special / 10) * weight / 32 && CN_HASTE(ds) >=0)
#else
    if (PER_DICE < (10 + special / 10) * weight * ((5000+skill_w)/5000.0) / 32 /(friend_attack+1) && CN_HASTE(ds) >=0)
#endif
    {
        if (CN_HASTE(ds) == 0)
          NCN_HASTE(ds) = 1;
#ifdef NO_SKILL
        CN_HASTE(ds) -= (frand(weight / 2 + 1) + weight / 2 + 1) / 3;
#else
        CN_HASTE(ds) -= (frand(weight / 2 + 1) + weight / 2 + 1) / 3 * ((5000+skill_w)/5000.0) ;
#endif
        dmprintf(as,ds, buf.data(), buf.size(),DMM_HAMMER_SLOW);
        write_companion(ds, as, buf.data(), 0);
    }
    /* add Confuse 20000526 */
#ifdef NO_SKILL
    if (PER_DICE < (10 + special / 10) * weight / 32 && !CN_CONFUSE(ds))
#else
    if (PER_DICE < (10 + special / 10) * weight * ((5000+skill_w)/5000.0) / 32 /(friend_attack+1) && !CN_CONFUSE(ds))
#endif
    {
#ifdef NO_SKILL
        CN_CONFUSE(ds) = (frand(weight / 2 + 1) + weight / 2 + 1) / 8;
#else
        CN_CONFUSE(ds) = (frand(weight / 2 + 1) + weight / 2 + 1) / 8 * (5000+skill_w)/5000.0 ;
#endif
        dmprintf(as,ds, buf.data(), buf.size(),DMM_HAMMER_CONFUSE);
        write_companion(ds, as, buf.data(), 0);
    }
  }
#endif
/* ↓スイッチデータ追加 2005/11/26 Savarl.F */
/*  if (i)	*/
/*    kill_other(as, ds, LV_FIGHTER);	*/
  if (i) {
    kill_other(as, ds, LV_FIGHTER);

    if (i != 2) {
      if (as == ds) {
        /* 自爆除く */
      } else if (as < MAX_NTTYS && ds < MAX_NTTYS) {  /* PCに限定　Bugfix by NEO 2008/03/24 */
        load_switch(as,cd[as].x,cd[as].y,22);  /* PC殺害時 */
      } else if (as < MAX_NTTYS) {
        target = cd[as].target;
        cd[as].target = ds;
        load_switch(as,cd[as].x,cd[as].y,21);  /* 指定NPC殺害時 */
        cd[as].target = target;
      }
    }
  }
/* ↑CHG */
  if (i == 2)     /* cd[ds] has come back */
    raise_dead(ds);
  lv_up(as, ds, LV_FIGHTER, 1);
  skillup_weapon(as,ds,tp/WT_PLACE,special);
  
  if (mhp_up_for_attack == 1) { /* 攻撃によって最大HPを増加させる added by NEO 20120212 */
    if (cd[as].mhp > 0 && cd[as].mhp < 9999
        && !death_penalty /* add for control death penalty (lv up) 20020204 */
/*      && PER_DICE < dmg * 10 / cd[as].mhp)  敵に大ダメージを与えると最大HPが増加 */
        && PER_DICE < ( 1 + ( 10000 - cd[as].mhp ) / cd[as].mhp ))  /* 最大HPによりランダムで最大HPが増加 */
        /* MHP別増加確率
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
    if (cd[as].mhp < cd[ds].lv[LV_FIGHTER]) /* 上昇するのは敵のFlvまで */
      cd[as].mhp++;
    }
  } /* ここまで added by NEO 20120212 */

  if (tp == WT_FANG)
  /* change for technical drain 20000518 */
#ifdef OLDWEAPON
    (void) hp_dmg(as, dmg / 10, 0);
#else

#ifdef NO_SKILL
    (void) hp_dmg(as, dmg / 10 * (1 + special / 20), 0);
#else
    (void) hp_dmg(as, dmg / 10 * (1 + special / 20) * ((5000+skill_w)/5000.0) , 0);
#endif

#endif
  if (!i && tp == WT_WHIP && !CN_PALSY(ds) && dmg >= 100
#ifdef NO_SKILL
      && 10 + lv_dice(cd[ds].lv[LV_FIGHTER]) < PER_DICE + lv_dice(cd[as].lv[LV_FIGHTER]))
#else
      && 10 + lv_dice((cd[ds].lv[LV_FIGHTER]+skill_ds)/2) < PER_DICE/(friend_attack+1) + lv_dice((cd[as].lv[LV_FIGHTER]+skill_w)/2))
#endif
     {
    /* change for technical bind 20000526 */
#ifdef OLDWEAPON
    CN_PALSY(ds) = dmg / 100 + 1;
#else

#ifdef NO_SKILL
    CN_PALSY(ds) = (dmg / 100 + 1) * (1 + special/20);
#else
    CN_PALSY(ds) = (dmg / 100 + 1) * (1 + special/20) * ((5000+skill_w)/5000.0) ;
#endif

#endif
    dmprintf(as,ds, buf.data(), buf.size(),DMM_WHIP_PALSY);
    write_companion(ds, as, buf.data(), 0);
  }
#ifndef OLDWEAPON
  /* add for strike back 20000601 */
  if (tp == WT_STINGER && cd[ds].weq != nullptr && (cd[ds].weq->type & MT_MASK) != MT_NOBREAK) {
#ifdef NO_SKILL
    if (PER_DICE < (cd[as].lv[LV_FIGHTER] / 500) * (1 + special / 20))
#else
    if (PER_DICE < ((cd[as].lv[LV_FIGHTER] + skill_w) / 1000) * (1 + special / 20)/(friend_attack+1))
#endif
    {
      dmprintf(ds, as, buf.data(), buf.size(),DMM_RAPIER_DROP);
      write_companion(ds, as, buf.data(), 0);
      for (i = 0; i < MAX_ITEMS; i++)
  if (&(cd[ds].id[i]) == cd[ds].weq)
    break;
      if (degrade(cd[ds].weq)==2) {
  dmprintf(ds, as, buf.data(), buf.size(),DMM_BROKEN_WPN);
  write_companion(ds, as, buf.data(), 0);
      } else if (i < MAX_ITEMS)
  save_id(ds, i, 1);
      item_gc(ds);
      set_fn(ds);
    }
  }
#endif
  /* ここの i は本来ダメージの結果だが、直前のスティンガー処理が
     アイテム探しの添字に i を使い回すため、tp == WT_STINGER の
     ときは中身がそちらに差し替わっている (元のまま)。 */
  if (!i && wtp == IT_WEAPON && cd[ds].type < CT_MAGICAL
      && 10 + lv_dice(cd[ds].lv[LV_FIGHTER]) < PER_DICE + lv_dice(cd[as].lv[LV_FIGHTER])) {
    dmg = frand(cd[as].weq->ATP / 2 + 1) + (cd[as].weq->ATP / 2 + 1);
    /* add for defence 20010703 */
    if (cd[ds].st == ST_DEFEND)
      dmg = dmg / 2;
    if (ae == AE_RANDOM) {
      ae = frand(8);
      if (ae == 7)
  ae = 13;
    }
	/* ここから add defence by Illusion at 20020615 from JETDM by NEO 20120206 */ 
	switch(ae){
	case AE_POISON:
	case AE_PALSY:
	case AE_PANIC:
	case AE_CONFUSE:
	case AE_BERSERK:
	case AE_SILENCE:
	case AE_DRAIN:
	case AE_BLIND:
	case AE_RANDOM:
	case AE_ALL:
		il = TCN_ILLUSION(ds);
		if(il!=0){
			mrr = 50;
			if (cd[ds].aeq != nullptr)
				mrr = (100 - (100 - mrr) * (100 - cd[ds].aeq->MRP) / 100);
			if (il < 0)
				mrr = mrr * (100 + il) / 100;
			else if (il > 0)
				mrr = 100 - (100 - mrr) * (100 - il) / 100;
			if (PER_DICE + lv_dice(cd[as].lv[LV_WIZARD])
				< mrr + lv_dice(cd[ds].lv[LV_WIZARD])) {
				ae = AE_NONE;
				dmprintf(as,ds, buf.data(), buf.size(),DMM_ILLUSION_EFFECT);
				write_companion(ds, as, buf.data(), 0);
			}
		}
		break;
	default:
		;
	}
	/* ここまで by NEO 20120206 */ 
    switch (ae) {
    case AE_POISON:
      if (cd[ds].at == AT_POISONPROOF || cd[ds].at == AT_IMMORTAL)
  break;
      CN_POISON(ds) = dmg;
      dmprintf(as,ds, buf.data(), buf.size(),DMM_POISON_BY_ATTACK
        , CN_POISON(ds));
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_PALSY:
      dmg = dmg / 5 + 1;
      if (cd[ds].at == AT_PALSYPROOF || cd[ds].at == AT_IMMORTAL)
  break;
      if (!CN_PALSY(ds)) {
  CN_PALSY(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_PALSY_BY_ATTACK
    , CN_PALSY(ds));
      } else
  dmprintf(as,ds, buf.data(), buf.size(),DMM_PALSY_ALREADY);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_PANIC:
      if (cd[ds].at == AT_MEPROOF || cd[ds].at == AT_IMMORTAL)
  break;
      CN_PANIC(ds) = dmg;
      dmprintf(as,ds, buf.data(), buf.size(),DMM_PANIC_BY_ATTACK
        , CN_PANIC(ds));
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_CONFUSE:
      dmg = dmg / 5 + 1;
      if (cd[ds].at == AT_MEPROOF || cd[ds].at == AT_IMMORTAL)
  break;
      if (!CN_CONFUSE(ds)) {
  CN_CONFUSE(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_CONFUSE_BY_ATTACK
    , CN_CONFUSE(ds));
      } else
  dmprintf(as,ds, buf.data(), buf.size(),DMM_CONFUSE_ALREADY);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_BERSERK:
      dmg = dmg / 5 + 1;
      if (cd[ds].at == AT_MEPROOF || cd[ds].at == AT_IMMORTAL)
  break;
      if (!CN_BERSERK(ds)) {
  CN_BERSERK(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_BERSERK_BY_ATTACK
    , CN_BERSERK(ds));
      } else
  dmprintf(as,ds, buf.data(), buf.size(),DMM_BERSERK_ALREADY);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_SILENCE:
      dmg = dmg / 5 + 1;
      if (cd[ds].at == AT_IMMORTAL)
  break;
      if (!CN_SILENCE(ds)) {
  CN_SILENCE(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_SILENCE_BY_ATTACK
    , CN_SILENCE(ds));
      } else
  dmprintf(as,ds, buf.data(), buf.size(),DMM_SILENCE_ALREADY);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_DRAIN:
      if (cd[ds].at == AT_DRAINPROOF)
  break;
      i = frand(MAX_LEVEL) + LV_FIGHTER;
      if (cd[ds].lv[i] < 1000)
  dmg = (dmg * cd[ds].lv[i]) / 1000 + 1;
      if (dmg > cd[ds].lv[i])
  dmg = cd[ds].lv[i];
      cd[ds].lv[i] -= dmg;
      (void) mp_dmg(as, dmg, 0);
      dmprintf(as,ds, buf.data(), buf.size(),DMM_LEVEL_DRAIN,dmg);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_THIEF:
      if (cd[ds].at == AT_THEFTGUARD)
  break;
      if (dmg > cd[ds].gp)
  dmg = cd[ds].gp;
      cd[ds].gp -= dmg;
      if (cd[ds].gp < 0)
  cd[ds].gp = 0;
      cd[as].gp += jb_gp_gain(as, dmg);  /* 商人ボーナス (PHIDM NEO extension) */
      if (cd[as].gp > 9999){
  tax+=(cd[as].gp-9999);
  cd[as].gp = 9999;
      }
      dmprintf(as,ds, buf.data(), buf.size(),DMM_ROBERY_MONEY,dmg);
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_BLIND:
      dmg = dmg / 5 + 3;
      if (CN_BLIND(ds) < 0) {
  dmprintf(as,ds, buf.data(), buf.size(),DMM_BLIND_ALREADY);
      } else {
  CN_BLIND(ds) = -dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_BLIND_BY_ATTACK
    , dmg);
      }
      write_companion(ds, as, buf.data(), 0);
      break;
    case AE_KNOCKBACK:
      if (cd[ds].st == ST_DEFEND)
        break;
      dmprintf(as,ds, buf.data(), buf.size(),DMM_KNOCK_BACK);
      write_companion(ds, as, buf.data(), 0);
      move_chr(ds, cd[ds].x, cd[ds].y,
         /* change for spear */
         cd[ds].x + fsign(cd[ds].x - cd[as].x),
         cd[ds].y + fsign(cd[ds].y - cd[as].y), 0);
      break;
    case AE_ALL:
      if (cd[ds].at != AT_POISONPROOF
    && cd[ds].at != AT_IMMORTAL) {
  CN_POISON(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_POISON_BY_ATTACK
    , CN_POISON(ds));
  write_companion(ds, as, buf.data(), 0);
      }
      if (cd[ds].at != AT_PALSYPROOF
    && cd[ds].at != AT_IMMORTAL) {
  if (!CN_PALSY(ds)) {
    CN_PALSY(ds) = (int) (dmg / 5);
    dmprintf(as,ds, buf.data(), buf.size(),DMM_PALSY_BY_ATTACK
      , CN_PALSY(ds));
  write_companion(ds, as, buf.data(), 0);
  }
      }
      if (cd[ds].at != AT_MEPROOF
    && cd[ds].at != AT_IMMORTAL) {
  CN_PANIC(ds) = dmg;
  dmprintf(as,ds, buf.data(), buf.size(),DMM_PANIC_BY_ATTACK
    , CN_PANIC(ds));
  write_companion(ds, as, buf.data(), 0);
      }
      if (cd[ds].at != AT_MEPROOF
    && cd[ds].at != AT_IMMORTAL) {
  if (!CN_CONFUSE(ds)) {
    CN_CONFUSE(ds) = (int) (dmg / 5) + 1;
    dmprintf(as,ds, buf.data(), buf.size(),DMM_CONFUSE_BY_ATTACK
      , CN_CONFUSE(ds));
  write_companion(ds, as, buf.data(), 0);
  }
      }
      if (cd[ds].at != AT_MEPROOF
    && cd[ds].at != AT_IMMORTAL) {
  if (!CN_BERSERK(ds)) {
    CN_BERSERK(ds) = (int) (dmg / 5) + 1;
    dmprintf(as,ds, buf.data(), buf.size(),DMM_BERSERK_BY_ATTACK
      , CN_BERSERK(ds));
  write_companion(ds, as, buf.data(), 0);
  }
      }
      if (cd[ds].at != AT_IMMORTAL) {
  if (!CN_SILENCE(ds)) {
    CN_SILENCE(ds) = dmg;
    dmprintf(as,ds, buf.data(), buf.size(),DMM_SILENCE_BY_ATTACK
      , CN_SILENCE(ds));
  write_companion(ds, as, buf.data(), 0);
  }
      }
      if (CN_BLIND(ds) > 0) {
  CN_BLIND(ds) = ((int) (dmg / 5) + 3) * (-1);
  dmprintf(as,ds, buf.data(), buf.size(),DMM_BLIND_BY_ATTACK
    , dmg);
      write_companion(ds, as, buf.data(), 0);
      }
      break;
    }
  }
  if (ds < MAX_NTTYS) {
    check_write(ds, (char *)"#lag\n");
    if (CN_LAG(ds) < 0) {
      CN_ALAG(ds) = 2;
    } else {
      CN_ALAG(ds) = 1;
    }
  }
  return;
}

/* add 20010703 */
void   guard_self(int n)
{
  if (cd[n].weq == nullptr) {
    cd[n].wt = 0;    /*- TCN_HASTE(n);*/
  } else {
    cd[n].wt = cd[n].weq->wp - TCN_HASTE(n);
  }
  cd[n].wt = cd[n].wt * (10000.0 - cd[n].lv[LV_FIGHTER]) / 5000;
  if (cd[n].wt < 5)
    cd[n].wt = 5;
  if (cd[n].st == ST_ATTACK)
    check_write(n, (char *)"#end-at\n");
  cd[n].st = ST_DEFEND;
  check_write(n, (char *)"#guard\n");
}

/* 技のコマンドパターン。ext_skills.cpp が extern 宣言して
   参照するため、型 (char * の配列) は変えない。 */
char *special_com[16][13] =
{
  {"AFBLRRL", "AFBRLLR", "AFBFB", "AFB", "AF", "", "", "", "", "", "", "", ""},
  {"AFBrlFBlr", "AFBlrFBrl", "Alrrllr", "Arllrrl", "AFFFF", "Alrrl", "Arllr", "AFF", "Alr", "Arl", "AF", "Al", "Ar"},
  {"Allll", "Arrrr", "All", "Arr", "Al", "Ar", "", "", "", "", "", "", ""},
  {"AFBFB", "AFB", "AF", "", "", "", "", "", "", "", "", "", ""},
  {"AFFFF", "AFF", "AF", "", "", "", "", "", "", "", "", "", ""},
  {"AFFFF", "Allll", "Arrrr", "AFF", "All", "Arr", "AF", "Al", "Ar", "", "", "", ""},
  {"Allll", "Arrrr", "All", "Arr", "Al", "Ar", "", "", "", "", "", "", ""},
  {"Alrrllr", "Arllrrl", "Alrrl", "Arllr", "Alr", "Arl", "", "", "", "", "", "", ""},
  {"AFFFF", "AFF", "AF", "", "", "", "", "", "", "", "", "", ""},
  {"AFLRRL", "AFRLLR", "", "", "", "", "", "", "", "", "", "", ""},
  {"AFLBR", "AFRBL", "Arrrr", "Allll", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""}
};

namespace {

/* 技の表示名。special_com と同じ並びで、spat_check() が使う。 */
constexpr std::array<std::array<const char *, 13>, 16> kSpecialAt =
{{
  /* Knocle */
  {"Illusion flush", "Illusion flush", "Double Impact smash", "Impact smash", "Straight smash", "", "", "", "", "", ""},
  /* Sword */
  {"9-headed dragon crush", "9-headed dragon crush", "Sigma slash", "Sigma slash", "Omega blast",
  "Z-slash", "Z-slash", "Buster charge", "Double slash", "Double slash", "Head crush", "Side slash", "Side slash"},
  /* Blade */
  {"Full moon slash", "Full moon slash", "Crescent slash", "Crescent slash",
  "Side slash", "Side slash", "", "", "", "", "", "", ""},
  /* Rapier */
  {"Lightning thrust", "Sonic thrust", "Hard thrust", "", "", "", "", "", "", "", "", "", ""},
  /* Spear */
  {"Meteo strike", "Thruster charge", "Hard thrust", "", "", "", "", "", "", "", "", "", ""},
  /* Axe */
  {"Omega blast", "Berserker brandish", "Berserker brandish", "Buster charge",
  "Rolling crush", "Rolling crush", "Head crush", "Arm crush", "Arm crush", "", "", "", ""},
  /* Hammer */
  {"Berserker brandish", "Berserker brandish", "Rolling crush", "Rolling crush",
  "Arm crush", "Arm crush", "", "", "", "", "", "", ""},
  /* Whip */
  {"Avalanche rush", "Avalanche rush", "Snake shot", "Snake shot",
  "Wave shot", "Wave shot", "", "", "", "", "", "", ""},
  /* Fang */
  {"Fang crush", "Crusher fang", "Dragon fang", "", "", "", "", "", "", "", "", "", ""},
  /* Wide */
  {"Trihorn", "Trihorn", "", "", "", "", "", "", "", "", "", "", ""},
  /* Around */
  {"Typhoon", "Typhoon", "Tornado", "Tornado", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""},
  {"", "", "", "", "", "", "", "", "", "", "", "", ""}
}};

} // namespace

int spat_check(char *com, int tp, char *str, int len, int skill)
{
  int    dr;
  std::array<char, 16> buf;
  char  *p;
  int    i,j;

  /* com の先頭10文字を buf へ写す。'\0' では止まらず飛ばす
     ため、途中に空きがあっても詰めてコピーされる。 */
  for (i = 0, p = buf.data(); i < 10; i++)
    if (com[i])
      *(p++) = com[i];
  *p = 0;
  dr = 0;
#ifndef NO_SKILL
  skill /= 100;
  skill = fisqrt(skill+1);
#endif
  /* 空文字列のパターンは strncmp の長さ 0 で常に一致するため、
     実パターンが無ければ末尾の空欄で break する。その場合 dr は
     0 のままで、技名だけが str へ写る (呼び出し側は special が
     0 のとき使わない)。 */
  for (i = 0; i < 13; i++)
#ifdef NO_SKILL
    if (!std::strncmp(buf.data(), special_com[tp][i], std::strlen(special_com[tp][i])))
#else
    if (!std::strncmp(buf.data(), special_com[tp][i], (j=std::strlen(special_com[tp][i]))) && j<=skill)
#endif
    {
      strlcpy(str, kSpecialAt[tp][i],len);
      dr = std::strlen(special_com[tp][i]) * 10;
      break;
    }
  return (dr);
}

namespace {

/* 素材ごとの劣化率 (%)。添字は (type & MT_MASK) / MT_PLACE で 0..15。 */
constexpr std::array<int, 16> kDegradeRate =
{10, 10, 8, 5, 5, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 0};

} // namespace

/* 返却値: 劣化が起こらなかったときは           0 */
/*         劣化が起こったがまだ残っているときは 1 */
/*         劣化が起こって消滅した時は           2 */
int degrade(ItemData *id)
{
  if(is_speed_degrade)
    return(speed_degrade(id));
  if (PER_DICE >= kDegradeRate[(id->type & MT_MASK) / MT_PLACE])
    return 0;
  switch (id->type & IT_MASK) {
  default:
    id->wp /= 2;
    break;
  case IT_ARMOR:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->DFP > 0)
      id->DFP = id->DFP * 3 / 4;
    if (id->DFP <= 0)
      id->DFP = 0;
    if (id->MRP > 0)
      id->MRP /= 2;
    if (id->MRP <= 0)
      id->MRP = 0;
    if (id->DFP == 0 && id->MRP == 0)
      id->wp = 0;
    break;
  case IT_WEAPON:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->ER > 0)
      id->ER /= 2;
    break;
  case IT_FOOD:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    if ((id->wp) > (id->ENTROP)) {
      id->VALUE -= 2;
    } else {
      id->VALUE += 2;
    }
    if (id->VALUE <= 0) {
      id->wp = 0;
    } else if (id->VALUE > 99) {
      id->VALUE = 99;
    }
    break; /* bug(?) fix 20010220 */
  case IT_JEWEL:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->VALUE > 0)
      id->VALUE /= 2;
    if (id->VALUE <= 0)
      id->wp = 0;
    break;
  case IT_SPELL_JEWEL:
    id->wp--;
    if(id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    break;
  case IT_RESOURCE:
    id->wp = 0;
    break;
  case IT_CHARGE:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->CHARGE > 0) {
      id->CHARGE--;
      if (id->CHARGE <= 0)
  id->wp = 0;
    }
    break;
  case IT_ACCESSORY:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 3 / 4;
    if (id->ATP <= 0)
      id->ATP = 1;
    break;
  case IT_FOREIGN:
    id->wp--;
    if ((id->wp) == 0)
      tax += (((id->VALUE) * 100 + (id->VALUE2))
        * cirate / 100);
  }
  if (id->wp <= 0) {
    id->type = DT_NONE;
    id->wp = 0;
    id->fn = FN_NONE;
    id->ATP = 1;
    id->ER = 0;
    id->AE = AE_NONE;
    id->SE = SE_NONE;
    id->active_time = 0;
    id->extension = nullptr; /* bug fix 20000204 */
    return 2;
  }
  return 1;
}

int speed_degrade(ItemData *id)
{
  if (PER_DICE >= kDegradeRate[(id->type & MT_MASK) / MT_PLACE] * 4)
    return 0;
  switch (id->type & IT_MASK) {
  default:
    id->wp = id->wp * 7 / 8 ;
    break;
  case IT_ARMOR:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->DFP > 0)
      id->DFP = id->DFP * 15 / 16;
    if (id->DFP <= 0)
      id->DFP = 0;
    if (id->MRP > 0)
      id->MRP = id->MRP * 7 / 8;
    if (id->MRP <= 0)
      id->MRP = 0;
    if (id->DFP == 0 && id->MRP == 0)
      id->wp = 0;
    break;
  case IT_WEAPON:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->ER > 0)
      id->ER  = id->ER * 7 / 8;
    break;
  case IT_FOOD:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    if ((id->wp) > (id->ENTROP)) {
      id->VALUE -= 1;
    } else {
      id->VALUE += 1;
    }
    if (id->VALUE <= 0) {
      id->wp = 0;
    } else if (id->VALUE > 99) {
      id->VALUE = 99;
    }
    break; /* 2026-11-16 修正：以前は break が無く IT_JEWEL へ
              落ちていた。degrade() 側は 20010220 の修正で break が
              入っており、こちらも合わせた。 */
  case IT_JEWEL:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    if (id->VALUE > 0)
      id->VALUE = id->VALUE * 7 / 8;
    if (id->VALUE <= 0)
      id->wp = 0;
    break;
  case IT_SPELL_JEWEL:
    id->wp--;
    if(id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    break;
  case IT_RESOURCE:
    id->wp = 0;
    break;
  case IT_CHARGE:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->wp = 0;
    if (id->CHARGE > 0) {
      id->CHARGE--;
      if (id->CHARGE <= 0)
  id->wp = 0;
    }
    break;
  case IT_ACCESSORY:
    id->wp--;
    if (id->ATP > 0)
      id->ATP = id->ATP * 15 / 16;
    if (id->ATP <= 0)
      id->ATP = 1;
    break;
  case IT_FOREIGN:
    id->wp--;
    if ((id->wp) == 0)
      tax += (((id->VALUE) * 100 + (id->VALUE2))
        * cirate / 100);
  }
  if (id->wp <= 0) {
    id->type = DT_NONE;
    id->wp = 0;
    id->fn = FN_NONE;
    id->ATP = 1;
    id->ER = 0;
    id->AE = AE_NONE;
    id->SE = SE_NONE;
    id->active_time = 0;
    id->extension = nullptr; /* bug fix 20000204 */
    return 2;
  }
  return 1;
}
