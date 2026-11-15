
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#include  <array>
#include  <cstddef>
#include  <cstdio>
#include  <cstdlib>
#include  <cstring>
#include  <ctime>

#include  "phi_dm.h"
#include  "phi_use.h"
#include  "phi_data.h"
#include  "phi_calc.h"
#include  "phi_magic.h"
#include  "phi_attack.h"
#include  "phi_str.h"

#include  "ext_data.h"
#include  "ext_item.h"

/* enmax / ed_o は phi_data.h で宣言済み。 */
extern int px[4];
extern int py[4];


namespace {

/* レベルエンハンサー (type 0xa000～) の1分岐分。元の6分岐は
 * 対象フィールドとメッセージ以外が同一だったため1つにまとめた。
 * VALUE==99 なら ENTROP を加算、それ以外は減算し、[1, 9999] で
 * 切り詰める。lv[] は int、mhp/mmp は long のため、加算結果を
 * フィールドへ代入してから (int への narrowing を含む) 比較する
 * 元の順序を保つ。使うのは int と long の実体化のみ。 */
template <typename T>
void apply_enhancer(int n, T &field, const ItemData &item,
                    const char *up_msg, const char *down_msg,
                    char *buf, std::size_t buf_size) {
  if (item.VALUE == 99) {
    dmprintf(n, 0, buf, buf_size, const_cast<char *>(up_msg), item.ENTROP);
    field = field + static_cast<long>(item.ENTROP);
    if (field > 9999) field = 9999;
  } else {
    dmprintf(n, 0, buf, buf_size, const_cast<char *>(down_msg), item.ENTROP);
    field = field - static_cast<long>(item.ENTROP);
    if (field < 1) field = 1;
  }
  write_companion(n, 0, buf, 0);
}

/* 資源の地形置換。UE 1～9 の9種は「地形ビットの置換 + メッセージ」
 * の同一手順だったため表にした。元の switch の case 順序とは無関係に
 * UE の値で引く。RS_RESOLVER (0) は load_id_in_circle() を伴い、
 * RS_ROOF (10) はトグル、RS_DUMMY (99) は無操作のため表に入れない。 */
struct ResourceTerrain {
  int tip;
  const char *message;
};

constexpr std::array<ResourceTerrain, 9> kResourceTerrains = {{
  {T_PLANT, DMM_RESOURCE_FLOWER},   /* RS_PLANT */
  {T_TREE, DMM_RESOURCE_TREE},      /* RS_TREE */
  {T_WATER, DMM_RESOURCE_WATER},    /* RS_WATER */
  {T_WALL, DMM_RESOURCE_WALL},      /* RS_WALL */
  {T_WINDOW, DMM_RESOURCE_WINDOW},  /* RS_WINDOW */
  {T_GATE, DMM_RESOURCE_DOOR},      /* RS_DOOR */
  {T_SGATE, DMM_RESOURCE_SECRET},   /* RS_SDOOR */
  {T_BOX, DMM_RESOURCE_CIRCLE},     /* RS_SAFE */
  {T_GLASS, DMM_RESOURCE_GLASS},    /* RS_GLASS */
}};

/* 敵スロットの実体を消滅させる。charge_appear() と
 * charge_disappear() で同一の塊だったため1つにまとめた。
 * 消滅させたら true を返す (disappear 側の k カウント用)。
 * appear 側は戻り値を捨てる (元も数えていなかった)。 */
bool clear_enemy_instance(int j) {
  if (cd[j].type != CT_NOEXIST) {
    for (int i = 0; i < MAX_ITEMS; i++) {
      delete_item(&(cd[j].id[i]));
    }
    tax += cd[j].gp;
    cd[j].type = CT_NOEXIST;
    cd[j].gp = 0;
    clear_chr(j);
    return true;
  }
  return false;
}

}  /* namespace */


int use_item(int n, int in)
{
  int    x, y;
  std::array<char, 256> buf;
  std::array<char, 256> buf2;
  int    tp;
  int    i;
  MagicData md;
  char  *p;

  tp = (cd[n].id[in].type & IT_MASK);
  switch (tp) {
  case IT_ARMOR:
    if (cd[n].weq == &(cd[n].id[in])) { /* niju soubi kinsi */
      cd[n].weq = nullptr;
      cd[n].wsep = 0;
    }
    cd[n].aeq = &(cd[n].id[in]);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_ARMOR);
    write_companion(n, 0, buf.data(), 0);
    cd[n].asep = sep_calc(n, cd[n].aeq);
    set_fn(n);
    return (0);
  case IT_WEAPON:
    cd[n].weq = &(cd[n].id[in]);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_WEAPON);
    write_companion(n, 0, buf.data(), 0);
    cd[n].wsep = sep_calc(n, cd[n].weq);
    set_fn(n);
    return (0);
  case IT_FOOD:
    /* レベルエンハンサー added by NEO 20120204 ここから */
    if (cd[n].id[in].type >= 0xa000 && cd[n].id[in].type < 0xb000) {
      apply_enhancer(n, cd[n].lv[LV_FIGHTER], cd[n].id[in],
                     DMM_EATFOOD_FLV_UP, DMM_EATFOOD_FLV_DOWN,
                     buf.data(), buf.size());
      break;
    } else if (cd[n].id[in].type >= 0xb000 && cd[n].id[in].type < 0xc000) {
      apply_enhancer(n, cd[n].lv[LV_WIZARD], cd[n].id[in],
                     DMM_EATFOOD_WLV_UP, DMM_EATFOOD_WLV_DOWN,
                     buf.data(), buf.size());
      break;
    } else if (cd[n].id[in].type >= 0xc000 && cd[n].id[in].type < 0xd000) {
      apply_enhancer(n, cd[n].lv[LV_MERCHANT], cd[n].id[in],
                     DMM_EATFOOD_MLV_UP, DMM_EATFOOD_MLV_DOWN,
                     buf.data(), buf.size());
      break;
    } else if (cd[n].id[in].type >= 0xd000 && cd[n].id[in].type < 0xe000) {
      apply_enhancer(n, cd[n].lv[LV_CREATER], cd[n].id[in],
                     DMM_EATFOOD_CLV_UP, DMM_EATFOOD_CLV_DOWN,
                     buf.data(), buf.size());
      break;
    } else if (cd[n].id[in].type >= 0xe000 && cd[n].id[in].type < 0xf000) {
      apply_enhancer(n, cd[n].mhp, cd[n].id[in],
                     DMM_EATFOOD_MHP_UP, DMM_EATFOOD_MHP_DOWN,
                     buf.data(), buf.size());
      break;
    } else if (cd[n].id[in].type >= 0xf000) {
      apply_enhancer(n, cd[n].mmp, cd[n].id[in],
                     DMM_EATFOOD_MMP_UP, DMM_EATFOOD_MMP_DOWN,
                     buf.data(), buf.size());
     break;
    /* レベルエンハンサー added by NEO 20120204 ここまで */
    } else if (cd[n].id[in].ENTROP <= cd[n].id[in].wp) {
      if ((*cd[n].name % 49) + 50 == cd[n].id[in].VALUE2) {
  i = 9999;
  if(is_see_burst)
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_ALL_SPH,
      cd[n].id[in].name);
  else
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_ALL_SLF);
      } else {
  i = dice(cd[n].id[in].VALUE) * 10;
  if (cd[n].id[in].VALUE2 > 0)
    i += cd[n].id[in].VALUE2;
  if(is_see_burst)
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_SPH,
      cd[n].id[in].name, i);
  else
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_SLF, i);
      }
    } else {
      if(is_see_burst)
  dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_SPOILE_SPH, cd[n].id[in].name);
      else
  dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_SPOILE_SLF, cd[n].id[in].name);
      write_companion(n, 0, buf.data(), 0);
      i = dice(cd[n].id[in].VALUE) * 10;
      if (cd[n].id[in].VALUE2 > 0)
  i += cd[n].id[in].VALUE2;
      if (cd[n].at == AT_IRONSTOMACH) {
  if(!is_see_burst){
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_IRNSTM_SLF);
    check_write(n, buf.data());
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_SPH,
      cd[n].id[in].name, i);
  } else
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_IRNSTM_SPH, i);
      } else {
/* message doubling bug(?) fix 20030605
  check_write(n, buf.data());
*/
  if(is_see_burst)
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_DAMAGE_SPH,
      cd[n].id[in].name, i);
  else
    dmprintf(n, 0, buf.data(), buf.size(),DMM_EATFOOD_DAMAGE_SLF, i);
  i = -i;
      }
    }
    write_companion(n, 0, buf.data(), 0);
    skillup(n,SKL_DETECT_IT,IT_FOOD,99,50,cd[n].id[in].VALUE2/10+1);
    if(mp_dmg(n, i, 2))
      kill_self_msg(n, DMM_KILL_SELF_BADFOOD);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_LEFT_MP);
    check_write(n, buf.data());
    break;
  case IT_RESOURCE:
    x = cd[n].x + px[cd[n].d];
    y = cd[n].y + py[cd[n].d];
    if (x < 1 || x >= MAX_X - 1 || y < 1 || y >= MAX_Y - 1) {
      dmcheck_write(n, DMM_RESOURCE_OUT);
      return (0);
    }
    if (cd[n].id[in].UE != RS_DUMMY && /* add for object resource 20011128 */
        cd[n].id[in].UE != RS_RESOLVER && (map[y][x].tip & TMASK) != T_SPACE) {
      dmcheck_write(n, DMM_RESOURCE_NO);
      return (0);
    }
    if (cd[n].id[in].UE == RS_RESOLVER && !(map[y][x].tip & T_PLANT)) {
      dmcheck_write(n, DMM_RESOURCE_RESOLVE_NO);
      return (0);
    }
      if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_RESOURCE) {
  dmcheck_write(n, DMM_RESOURCE_NO);
  return(0);
      }
    if (PER_DICE > (cd[n].lv[LV_CREATER] + 600) / 20)
      dmcheck_write(n, DMM_RESOURCE_MISS);
    else {
      if (cd[n].id[in].UE >= RS_PLANT && cd[n].id[in].UE <= RS_GLASS) {
        const ResourceTerrain &rule =
            kResourceTerrains[cd[n].id[in].UE - RS_PLANT];
        map[y][x].tip = ((map[y][x].tip & TCLEAR) | rule.tip);
        dmcheck_write(n, const_cast<char *>(rule.message));
      } else {
        switch (cd[n].id[in].UE) {
        case RS_RESOLVER:
          map[y][x].tip = ((map[y][x].tip & TCLEAR) | T_SPACE);
          dmcheck_write(n, DMM_RESOURCE_RESOLVE);
          load_id_in_circle(x,y);
          break;
        case RS_ROOF:
          map[y][x].tip = (map[y][x].tip ^ T_ROOF);
          if (map[y][x].tip & T_ROOF) {
            dmcheck_write(n, DMM_RESOURCE_ROOF);
          } else {
            dmcheck_write(n, DMM_RESOURCE_ROOF_BRK);
          }
          break;
        default:
          /* RS_DUMMY を含むその他の UE は何もしない。元の switch の
           * case RS_DUMMY: break; および case 不一致と同じ。 */
          break;
        }
      }

    skillup(n,SKL_DETECT_IT,IT_RESOURCE,99,50,5);
#if MAX_OBJECTS > 0
      /* add for object resource 20011128 */
      if (get_propertyvalue(cd[n].id[in].extension,DEII_RSCOBJ,buf.data(),buf.size())) {
		if (!*buf.data()) {
			/* object resolver */
			i=dobj[y][x]-1;
			if (i>=0) {
				move_obj(i,-1,-1,0);
				dmcheck_write(n, DMM_RSCOBJ_ERASE);
			} else {
				dmcheck_write(n, DMM_RSCOBJ_NOOBJ);
			}
		} else {
			dmprintf(n,0,buf2.data(),buf2.size(),buf.data());
            p = buf2.data();
			i = str_to_obj(&p);
			switch (i) {
				case -1:
					dmcheck_write(n, DMM_RSCOBJ_ERROR);
					break;
				case -2:
					dmcheck_write(n, DMM_RSCOBJ_NOSPACE);
					break;
				default:
					move_obj(i,x,y,cd[n].d ^ 2);
					dmcheck_write(n, DMM_RSCOBJ_CREATE);
					break;
			}
		}
		if (i<0 && cd[n].id[in].UE == RS_DUMMY)
			return (0);
	  }
#endif

      if (get_propertyvalue(cd[n].id[in].extension,DEII_RSCSFC,buf.data(),buf.size())) {
		if (!*buf.data()) {
			map[y][x].tip &= ~T_SURFACE;
			dmcheck_write(n, DMM_RSCSFC_ERASE);
		} else {
        	map[y][x].surface = (unsigned char)(set_dd(buf.data()));
        	map[y][x].tip |= T_SURFACE;
			dmcheck_write(n, DMM_RSCSFC_CREATE);
		}
	  }
	}
    print_map(n, 0);
    break;
  case IT_JEWEL:
    i = cd[n].id[in].VALUE * 100 + cd[n].id[in].VALUE2;
    i *= cirate;
    i /= 100;
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_JEWEL, i);
    check_write(n, buf.data());
    if (i > 9999 - cd[n].gp)
      i = 9999 - cd[n].gp;
    tax -= i;
    cd[n].gp += i;
    if (cd[n].gp > 9999)
      cd[n].gp = 9999;
    skillup(n,SKL_DETECT_IT,IT_JEWEL,0,30,1);
    break;
  case IT_SPELL_JEWEL:
    md.spell = cd[n].id[in].name;
    md.effect = cd[n].id[in].UE;
    md.fn = cd[n].id[in].fn;
    md.is_inherence = 0;
    md.extension = cd[n].id[in].extension;
    learn_magic(n, &md);
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_SPLJWL, md.spell);
    check_write(n, buf.data());
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_SPLJWL_UNSEALE, cd[n].id[in].name);
    write_companion(n, 0, buf.data(), 0);
    skillup(n,SKL_DETECT_IT,IT_SPELL_JEWEL,99,50,5);
    break;
  case IT_CHARGE:
    return (use_charge(n, &(cd[n].id[in])));
  case IT_ACCESSORY:
    if (cd[n].weq == &(cd[n].id[in])) { /* niju soubi kinsi */
      cd[n].weq = nullptr;
      cd[n].wsep = 0;
    }
    cd[n].oeq = &(cd[n].id[in]);
    cd[n].at = cd[n].id[in].AT;
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_ACCESSORY);
    write_companion(n, 0, buf.data(), 0);
    cd[n].osep = sep_calc(n, cd[n].oeq);
    set_fn(n);
    return (0);

  case IT_EVENTLY:
      i = cd[n].id[in].op[1];
      i = (i - (i % 10)) / 10  * 16 + (i % 10);

      if (i < GFLAG_MAX)
      {
        change_flag(n,i,FLAG_ON,1);
        load_switch(n, cd[n].x, cd[n].y, 10);
      }

      if (cd[n].id[in].op[2] == 0)
        return (0);

      cd[n].id[in].op[2] --;

      if (cd[n].id[in].op[2] > 0)
        return (0);

      break;

  case IT_SCROLL:
      cd[n].st = ST_MSGW;
      i  = cd[n].id[in].op[1] * 100 + cd[n].id[in].op[2];
      cd[n].anmf  = i;
      cd[n].anmt  = 0;
      return (0);

  default:
    dmcheck_write(n, DMM_USE_WHAT);
    return (0);
  }
  delete_item(&(cd[n].id[in]));
  item_gc(n);
  set_fn(n);
  return (1);
}

int use_charge(int n, ItemData *id)
{
  MagicData md;
  std::array<char, 100> buf;
  int is_use_fail;
  int deg;

  if (is_need_charge_time && id->active_time > std::time(nullptr)){
    dmprintf(n, 0, buf.data(), buf.size(),DMM_USE_CHARGE_STILL, id->name);
    check_write(n, buf.data());
    (id->active_time) ++;
    return 0;
  }
  skillup(n,SKL_DETECT_IT,IT_CHARGE,99,50,5);
  is_use_fail = 0;
  if (id->UE == EF_APPEAR) {
    if (!charge_appear(n, id)) is_use_fail = 1;
  } else if (id->UE == EF_DISAPPEAR) {
    if (!charge_disappear(n, id)) is_use_fail = 1;
  } else if (id->UE >= EF_TRAPHP && id->UE <= EF_MINEMP) {
	additem(cd[n].x+px[cd[n].d],cd[n].y+py[cd[n].d],id,-1);
	map[cd[n].y+py[cd[n].d]][cd[n].x+px[cd[n].d]].tip |= T_TRAP;
    dmprintf(n, 0, buf.data(), buf.size(),DMM_SET_TRAP, id->name);
    write_companion(n, 0, buf.data(), 0);
	delete_item(id);
	item_gc(n);
	set_fn(n);
	return(0);
  } else {
    md.spell = id->name;
    md.effect = id->UE;
    md.fn = id->fn;
    md.is_inherence = 1;
    md.extension = nullptr;
    effect_analyze(n, &md, 1);
  }
  id->active_time = std::time(nullptr) + frand(mgstrlen(id->name));
  if(is_use_fail) return 0;
  deg = 0;
  if(!is_degrade_charge || !(PER_DICE<10) || (deg=degrade(id)) == 0) /* PHI:00055 Bug Fix by EL */
    if(id->CHARGE>0){
      id->CHARGE--;
      if(id->CHARGE<=0){
  delete_item(id);
  deg = 2;
      }
    }
  if(deg == 2){
    dmprintf(n, 0, buf.data(), buf.size(),DMM_BROKEN_CHARGE, id->name);
    write_companion(n, 0, buf.data(), 0);
    item_gc(n);
    set_fn(n);
    return 1;
  }
  return 0;
}

int charge_appear(int n, ItemData *id)
{
  int    j, k, m;
  std::array<int, MAX_ENEMIES> cbuf;

  /* m は一致した定義の数合計だが、この後使われない (元のまま)。 */
  k = m = 0;
  for (j = 0; j < max_enemy; j++) {
    if ((ed_o[j].apc == 3) && ed_o[j].apnm == id->name) {
      cbuf[j] = -1;
      m += ed_o[j].nm;    /* 数合計 */
      ed_o[j].ix = cd[n].x;
      ed_o[j].iy = cd[n].y;
      k++;
    } else {
      cbuf[j] = 0;
    }
  }
  if (k == 0) {
    dmcheck_write(n, DMM_CHARGE_APPEAR_NO);
    return (0);
  }
  for (j = MAX_NTTYS; j < enmax; j++) {
    if ((cd[j].et >= 0) && cbuf[cd[j].et]) {
      /* 実体があれば消滅させる (戻り値は使わない)。 */
      clear_enemy_instance(j);
      cd[j].wt = fsps(80, ed_o[cd[j].et].apt, 990000);
      cd[j].ec = 1;   /* 発生 */
    }
  }
  dmcheck_write(n, DMM_CHARGE_APPEAR);
  return (1);
}

int charge_disappear(int n, ItemData *id)
{
  int    j, k;
  std::array<int, MAX_ENEMIES> cbuf;

  for (j = 0; j < max_enemy; j++) {
    if ((ed_o[j].apc == 3) && ed_o[j].apdm == id->name) {
      cbuf[j] = -1;
    } else {
      cbuf[j] = 0;
    }
  }
  k = 0;
  for (j = MAX_NTTYS; j < enmax; j++) {
    if ((cd[j].et >= 0) && cbuf[cd[j].et]) {
      if (clear_enemy_instance(j))
        k++;
      cd[j].wt = 0;
      cd[j].ec = 0;
    }
  }
  if (k == 0) {
    dmcheck_write(n, DMM_CHARGE_DISAPPEAR_NO);
    return (0);
  }
  dmcheck_write(n, DMM_CHARGE_DISAPPEAR);
  return (1);
}

/* 罠の発動 */
void effect_trap(int n)
{
	ItemOData **id,*id2;
	MagicData md;

	id = nullptr;
	while ((id = finditem(cd[n].x,cd[n].y,id))) {
		if (is_trap(*id)) {
			md.spell = (*id)->id.name;
			md.fn = (*id)->id.fn;
			md.is_inherence = 1;
			md.extension = nullptr;
			cd[0].lv[LV_WIZARD] = 0; /* 使用者のレベルに依存しない… */
			/* md.effect は各 case で設定してから使う。いずれの case にも
			 * 一致しなければ読まれない。 */
			switch ((*id)->id.UE) {
				case EF_TRAPHP:
					md.effect = EF_LINE;
					mg_attack_to(0,n,&md,0,1);
					break;
				case EF_TRAPMP:
					md.effect = EF_MLINE;
					mg_attack_to(0,n,&md,4,1);
					break;
				case EF_MINEHP:
					md.effect = EF_EXPLODE;
					mg_explode(0,&md,cd[n].x,cd[n].y,0,1);
				/* break が無いため EF_MINEMP へ落ち、両方の爆発が起きる。
				 * 元のまま残す。 */
				case EF_MINEMP:
					md.effect = EF_MEXPLODE;
					mg_explode(0,&md,cd[n].x,cd[n].y,4,1);
					break;
			}
		    if((*id)->id.CHARGE>0){
		        if(--((*id)->id.CHARGE)<=0){
			        id2 = (*id);
			        *id = (*id)->next;
			        std::free(id2);
		        }
		    }
		    return;
		}
	}
	map[cd[n].y][cd[n].x].tip &= ~T_TRAP;
	if (!(map[cd[n].y][cd[n].x].tip & (T_TRAP | T_ITEM)))
		delitempos(cd[n].x,cd[n].y);
}

/* スロットを空き (DT_NONE) にする。name や op[] は消さない
 * (元のまま)。ATP を 1 にするのも元の挙動。 */
void delete_item(ItemData *id)
{
  id->type = DT_NONE;
  id->wp = 0;
  id->fn = FN_NONE;
  id->ATP = 1;
  id->ER = 0;
  id->AE = AE_NONE;
  id->SE = SE_NONE;
  id->active_time = 0;
  id->extension = nullptr;
  return;
}

int item_count(int n, int in)
{
  int    i;

  if(in <= 0) return -1;
  for (i = 0; i < MAX_ITEMS; i++)
    if (cd[n].id[i].type != DT_NONE) {
      in--;
      if (!in)
  break;
    }
  if (i >= MAX_ITEMS)
    i = -1;
  return (i);
}

/* ぷれいや n の in 個目の it に適合するアイテムの番号 */
/* charge spell の処理参照、存在しない時は -1 を返す   */
/* 注意: DT_NONE の空きスロットも (type & IT_MASK)==0==IT_NOUSE で
 * 一致するため、it==IT_NOUSE のときは空きも数える (元のまま)。 */
int itemtype_count(int n, int in, int it)
{
  int i;

  if (in <=0) return -1;
  for (i=0; i<MAX_ITEMS; i++)
    if ((cd[n].id[i].type & IT_MASK) == it)
    {
      in--;
      if (in==0) break;
    }

  if (i < MAX_ITEMS)
    return i;
  return -1;
}
