/* written by Qajin 2002/03/07 */
/* last update 2002/05/02 */

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "phi_dm.h"
#include "ext_skills.h"
#include "phi_calc.h"
#include "phi_name.h"
#include "phi_data.h"
#include "phi_magic.h"
#include "phi_str.h"
#include "tcp.h"

/* 注) 送受信文字長の都合上 1つのスキル種につき 熟練度は41個まで */
/* 熟練度数を後から増加させる事は出来ないので */
/* スキル種増設時は必ず最大数を確保すること */
namespace {

/* SKL_SPECIAL (13) の最大数は 0 で、格納場所を持たない
   （skill_init() が nullptr を入れ、:Ssp 行は読み込み時に
   エラー扱いになる）。 */
constexpr std::array<int, MAX_SKILL_TYPES> kSkillMax =
{SKL_WPN_MAX,
 SKL_MGAT_MAX,SKL_MGEF1_MAX,SKL_MGEF2_MAX,SKL_MGEF3_MAX,SKL_MGFN_MAX,SKL_MGSP_MAX,
 SKL_DTIT_MAX,SKL_DTWP_MAX,SKL_DTMT_MAX,
 SKL_CRIT_MAX,SKL_CRWP_MAX,SKL_CRMT_MAX,
 0};

constexpr std::array<const char *, MAX_SKILL_TYPES> kSkillId =
{"wp",
 "mgA","mgE1","mgE2","mgE3","mgF","mgS",
 "dtI","dtW","dtM",
 "crI","crW","crM",
 "sp"};

/* 1キャラ分のスキル配列を確保する。fill が 0 以上なら確保直後に
   その値で埋める。確保に失敗したら false を返し、確保済みの
   部分はそのまま残す（呼び出し元が即座に -1 を返す元の挙動
   と同じ）。 */
bool alloc_skill_row(int **row, int fill)
{
    for (int j = 0; j < MAX_SKILL_TYPES; j++) {
        if (kSkillMax[j] <= 0) {
            row[j] = nullptr;
            continue;
        }
        int *p = static_cast<int *>(std::malloc(sizeof(int) * kSkillMax[j]));
        if (p == nullptr)
            return false;
        row[j] = p;
        if (fill >= 0) {
            for (int i = 0; i < kSkillMax[j]; i++)
                row[j][i] = fill;
        }
    }
    return true;
}

} // namespace

/* enmax は phi_data.h で宣言済み。 */

int skill_init()
{
    for (int i = 0; i < MAX_NTTYS; i++) {
        cd[i].skill = static_cast<int **>(std::malloc(sizeof(int *) * MAX_SKILL_TYPES));
        if (cd[i].skill == nullptr)
            return (-1);
        if (!alloc_skill_row(cd[i].skill, -1))
            return (-1);
        skill_clear(i);
    }
    int **npc_skill = static_cast<int **>(std::malloc(sizeof(int *) * MAX_SKILL_TYPES));
    if (npc_skill == nullptr)
        return (-1);
    /* 取り合えず 一律 5000 */
    if (!alloc_skill_row(npc_skill, 5000))
        return (-1);
    /* 敵は全員で1つのスキル表を共有する。 */
    for (int i = MAX_NTTYS; i < MAX_NTTYS + MAX_ENEMIES; i++) {
        cd[i].skill = npc_skill;
    }
    return (0);
}

int skill_clear(int n)
{
    if (n >= MAX_NTTYS)
        return(0);

    for (int i = 0; i < MAX_SKILL_TYPES; i++) {
        if (cd[n].skill[i] != nullptr && kSkillMax[i] > 0) {
            std::memset(cd[n].skill[i], 0, sizeof(int) * kSkillMax[i]);
        }
    }
    return(0);
}

int load_skl_type(int n,int type,char *p)
{
	int i,j;
	
	if (n >= MAX_NTTYS || cd[n].skill[type] == nullptr)
		return(1);

	/* 途中で : が無くなったら、行末なら残りを 0 で埋めて 0、
	   それ以外（数字でも : でもない文字）は -1 を返す。 */
	for(i=0;i<kSkillMax[type];i++) {
		if (*p != ':') {
			if (*p != '\n' && *p != '\0')
				return(-1);
			for(;i<kSkillMax[type];i++)
				cd[n].skill[type][i]=0;
			return(0);
		}
		p++;
		if (*p >= '0' && *p <= '9')
			j = decimal_load_arbitrarily(&p);
		else
			j = 0;
		cd[n].skill[type][i] = (j < 0 ? 0 : j);
	}

	return (0);
}

extern int lcnt;
extern char pfname[128];

int load_skills(int n,char *p)
{
    int i;
    std::array<char, 256> buf;

    /* load_skl_type() が 0 以外を返した場合は break せず走査を
       続け、末尾まで行くと no parameter 扱いになる。:Ssp 行
       （格納場所が無い）と書式エラー行がここに落ちる。 */
    for (i = 0; i < MAX_SKILL_TYPES; i++) {
        if (!std::strncmp(p, kSkillId[i], std::strlen(kSkillId[i]))) {
            if (!load_skl_type(n, i, p + std::strlen(kSkillId[i])))
                break;
        }
    }
    if (i == MAX_SKILL_TYPES) {
        if (lcnt > 0) {
            std::snprintf(buf.data(), sizeof(buf), "%s at line %d: no parameter.(load_skills)", pfname, lcnt);
            log_print(buf.data());
        }
        return (-1);
    }
    return (0);
}

/* 指定typeのスキル値を出力  */
/* 全て 0 の時 返値  1       */
/* 1つでも出力したら 0       */
/* 指定typeが存在しない時 -1 */
int save_skl_type(int n,char *buf,int size,int type)
{
	int i,l,k;
	char *bp;
	char bs;
	
	/* bs は char のため size の下位 8 ビットだけが残る。残りが
	   128〜255 のとき負値になり、strlcpy の第3引数では巨大値
	   として扱われるが、その位置には改行2バイト分の余裕が
	   必ずあるため結果は同じ。全て 0 のときは何も書かないが、
	   返値 1 の場合呼び出し元は buf を読まない。元のまま。 */
	bp = buf; bs = size;
	std::snprintf(buf,size,":S%s",kSkillId[type]);
	l = 2+std::strlen(kSkillId[type]);
	buf += l; size -= l;

	if (cd[n].skill[type] == nullptr || kSkillMax[type] == 0)
		return(-1);

	/* %.0i は 0 を空文字にするため、0 の項目は : だけになる。
	   読み込み側は : の後が数字でなければ 0 と読む。 */
	for (i=k=0;i<kSkillMax[type];i++) {
		std::snprintf(buf,size,":%.0i",cd[n].skill[type][i]);
		l = std::strlen(buf);
		buf += l; size -= l;
		if (cd[n].skill[type][i]==0)
			k++;
		else {
			bp = buf; bs = size;
		}
	}
	strlcpy(bp,"\n",bs);
	return ( k==kSkillMax[type] ? 1 : 0 );
}

int save_skills_cd(int n,FILE *f)
{
    std::array<char, 256> buf;

    for (int i = 0; i < MAX_SKILL_TYPES; i++) {
        if (!save_skl_type(n, buf.data(), sizeof(buf), i))
            fputs(buf.data(), f);
    }

    return (0);
}

int put_skills(int n,int m)
{
    std::array<char, 256> buf;

    /* 先頭の UID は対象ではなく常に cd[0] のもの。元の挙動のまま。 */
    std::snprintf(buf.data(), sizeof(buf), "#chr-D %-15s VX", cd[0].uid);

    for (int i = 0; i < MAX_SKILL_TYPES; i++) {
        if (!save_skl_type(n, buf.data() + 25, sizeof(buf) - 25, i))
            check_write(m, buf.data());
    }

    return (0);
}

namespace {

constexpr std::array<const char *, SKL_MGAT_MAX> kSkillMagicRegions =
{ "Hp damage", "Mp damage", "Heal",
 "Line", "Pierce", "Explode", "Sphere",
 "Roof Shake", "Rain", "Quake"};

constexpr std::array<const char *, SKL_MGSP_MAX> kSkillMagicCommonSpells =
{ "Return", "Eagle eye", "Analyze", "Charge spell",
  "Identify", "Appear", "Disappear"};

} // namespace

int status_skills(int n,char *p)
{
	char buf[256],buf2[256];
	int i,j,k;

	/* 引数の扱い：空白で始まれば空白を読み飛ばし、単独の s なら
	   1 文字読み飛ばす（skills と打った場合）。それ以外は 1 を
	   返し、呼び出し元が次のコマンド分岐へ落とす。引数なしの
	   skill はここで 1 を返し、発言として扱われる。 */
	if (fisspace(*p))
		SKIP_SPACE(p);
	else if (*p == 's' && *(p+1) == '\0')
		p++;
	else
		return (1);

	dmcheck_write(n,DMM_SKILLS_SKILL);
	if (*p == '\0' || !std::strcmp(p,"weapon")) {
		dmcheck_write(n,DMM_SKILLS_WEAPON);
		CPYSTR(buf2,"  ");
		for (i=0;i<SKL_WPN_MAX;i++) {
			if ((j=cd[n].skill[SKL_WEAPON][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
	}
	if (*p == '\0' || !std::strcmp(p,"spell")) {
		dmcheck_write(n,DMM_SKILLS_SPELL);
		CPYSTR(buf2,"  ");
		for (i=0;i<SKL_MGAT_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_AT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-9.9s : %2i ",kSkillMagicRegions[i],j);
				CATSTR(buf2,buf);
			}
			if ((i==2 || i==6 || i==9) && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
		/* k は EF1/EF2/EF3 の3ループで通し番号。表示した項目が
		   4 個たまるごとに改行し、最後の端数もまとめて出す。
		   use_effect の添字 +11/+46/+61 は EF の範囲
		   11〜39/46〜60/61〜93 に対応する。 */
		for (i=k=0;i<SKL_MGEF1_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_EF1][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+11],j);
				CATSTR(buf2,buf);
				if(++k % 4 == 0) {
					CATSTR(buf2,"\n");
					check_write(n,buf2);
					CPYSTR(buf2,"  ");
				}
			}
		}
		for (i=0;i<SKL_MGEF2_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_EF2][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+46],j);
				CATSTR(buf2,buf);
				if(++k % 4 == 0) {
					CATSTR(buf2,"\n");
					check_write(n,buf2);
					CPYSTR(buf2,"  ");
				}
			}
		}
		for (i=0;i<SKL_MGEF3_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_EF3][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+61],j);
				CATSTR(buf2,buf);
				if(++k % 4 == 0) {
					CATSTR(buf2,"\n");
					check_write(n,buf2);
					CPYSTR(buf2,"  ");
				}
			}
		}
		if(k % 4 != 0) {
			CATSTR(buf2,"\n");
			check_write(n,buf2);
			CPYSTR(buf2,"  ");
		}
		for (i=0;i<SKL_MGSP_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_SP][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-12.12s : %2i ",kSkillMagicCommonSpells[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
		if (*(buf2+2) != '\0') {
			CATSTR(buf2,"\n");
			check_write(n,buf2);
			CPYSTR(buf2,"  ");
		}
		/* FN は i が 3 の倍数のときに中身があれば改行して出す。 */
		dmcheck_write(n,DMM_SKILLS_FN);
		for (i=0;i<SKL_MGFN_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_FN][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-7.7s : %2i ",fn_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 3 == 0 && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
	}
	/* detect/create の各小見出しは、最初の flush のときに遅れて
	   出す（中身が無ければ見出しも出ない）。 */
	if (*p == '\0' || !std::strcmp(p,"detect")) {
		dmcheck_write(n,DMM_SKILLS_DETECT);
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_DTIT_MAX;i++) {
			if ((j=cd[n].skill[SKL_DETECT_IT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-12.12s: %2i ",it_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_DTITM);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_DTWP_MAX;i++) {
			if ((j=cd[n].skill[SKL_DETECT_WT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_DTWPN);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_DTMT_MAX;i++) {
			if ((j=cd[n].skill[SKL_DETECT_MT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-8.8s: %2i ",mt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_DTMTR);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
	}
	if (*p == '\0' || !std::strcmp(p,"create")) {
		dmcheck_write(n,DMM_SKILLS_CREATE);
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_CRIT_MAX;i++) {
			if ((j=cd[n].skill[SKL_CREATE_IT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-12.12s: %2i ",it_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_CRTITM);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_CRWP_MAX;i++) {
			if ((j=cd[n].skill[SKL_CREATE_WT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_CRTWPN);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_CRMT_MAX;i++) {
			if ((j=cd[n].skill[SKL_CREATE_MT][i]/100) > 0) {
				std::snprintf(buf,sizeof(buf)," %-8.8s: %2i ",mt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+4) != '\0') {
				if (k++ == 0) {
					dmcheck_write(n,DMM_SKILLS_CRTMTR);
				}
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"    ");
			}
		}
	}
	check_write(n,"\n");
	return (0);
}

extern char *special_com[16][13];

int check_lvup_pc(int n,int m)
{
  int pc;

  if (death_penalty)
    return(0);

  if(!is_kill_master
     && ((n < MAX_NTTYS && cd[n].exp == 99999999)
   || (m < MAX_NTTYS && cd[m].exp == 99999999)))
    return(0);

  if (n < MAX_NTTYS && m < MAX_NTTYS) {
    pc = 1;
    if (!std::strcmp(ip_address_check(n),ip_address_check(m)))
      pc = 2;
  } else
    pc = 0;

  if (lv_up_between_pc && ((lv_up_between_pc & 3) <= pc))
    return(0);

  return(1);
}

int skillup(int n,int type,int idx,int limit,int per,int e)
{
  int i, lv, exp;

  if (n >= MAX_NTTYS)
    return (0);

  lv  = cd[n].skill[type][idx] / 100;
  exp = cd[n].skill[type][idx] % 100;

  /* PER_DICE は評価のたびに乱数を振り直す。下の3回の評価は
     それぞれ別の出目であり、変数にまとめてはいけない。 */
  i = (int) (std::log10((double) (cd[n].exp + 10.0)) * 10.0);
  if (PER_DICE < i) {
      if (PER_DICE > lv &&
          PER_DICE < per) {
          exp += e;
          if (exp >= 100 && lv >= limit)
            exp = 99;
          cd[n].skill[type][idx] = lv * 100 + exp;
          if (cd[n].skill[type][idx] > 9999)
            cd[n].skill[type][idx] = 9999;
      }
  }

  return (1);
}

int skillup_weapon(int n,int m,int type,int sp)
{
  int limit,vslv;

  if (!check_lvup_pc(n,m))
    return(0);

  /* 技によって上限 */
  limit = 10 + sp;
  if (type == WT_WIDE || type == WT_AROUND)
    limit = 50;
  if (std::strlen(special_com[type][0]) == sp / 10)  /* 0番が最長… */
    limit = 99;

  /* 同じ武器を使う相手なら相手の Skill LV が高い程上がり易い */
  if (type == WT_FIST && m < MAX_NTTYS && cd[m].weq == nullptr) {
    vslv = cd[m].skill[SKL_WEAPON][WT_FIST/WT_PLACE];
  } else if (cd[m].weq != nullptr && (cd[m].weq->type & WT_MASK) / WT_PLACE == type) {
    vslv = cd[m].skill[SKL_WEAPON][type];
  } else
    vslv = 0;

  skillup(n,SKL_WEAPON,type,limit,vslv+30,(sp ? sp / 10 : 1));

  /* 攻撃を受けた側にも Skill Up の可能性… */
  if (cd[n].skill[SKL_WEAPON][type] > cd[m].skill[SKL_WEAPON][type])
    skillup(m,SKL_WEAPON,type,0,30,(sp ? (sp+10)/20 : 1));

  return(1);
}

/* f = 0 exp only     */
/* f = 1 enable lv up */
int skillup_magic_one(int n,int m,int type,int idx,int f)
{
    int j;

    if (m && type == SKL_MAGIC_EF3 && idx == SKL_MGEF3_SAVELIFE) {
      if (!(cd[n].hp))
        skillup(n,type,idx,99,90,20); /* …10以上ないと…(汗 */
      else
        skillup(n,type,idx,98,50,1);
    }

    /* NCN_COAT() は double のため、100 / NCN_COAT は浮動小数点
       の割り算になり、int へ切り捨てて代入される。 */
    /* coat,spell upに頼ると未熟なまま… */
    if (type == SKL_MAGIC_EF1 && idx == SKL_MGEF1_COAT) {
      j = 100 / NCN_COAT(n);
    } else if (type == SKL_MAGIC_EF1 && idx == SKL_MGEF1_SPELL) {
      j = 50;
    } else {
      j = 100-TCN_SPELL(n);
    }

    if (type >= SKL_MAGIC_FN || PER_DICE < j)
      skillup(n,type,idx,(!f ? 0 : (!m ? 10 : 99)),50,1);

    return (1);
}

/* m==0 は「相手なし」（詠唱のみ・空撃ち）の合図で、lvup 判定を
   素通りし、上限は 10 になる。m>0 のときだけ check_lvup_pc()
   で PC 同士の制限を見る。 */
int skillup_magic(int n,int m,MagicData *md,int chrg)
{
  int ef;
  int tp,idx;
  int rgn,dmg;

  if (m > 0 && !check_lvup_pc(n,m))
    return(0);

  ef = md->effect;
  if (ef >= EF_TRAPHP || chrg) {
    /* chargeは無関係 */
    return(0);
  } else if (ef <= EF_HEALS || (ef >= EF_ROOF && ef <= EF_MAREA)) {
    /* 攻撃/回復系 */
    if (ef < EF_HEALS) {
      rgn = (ef-EF_LINE) % 4 + SKL_MGAT_LINE; dmg = (ef-EF_LINE) / 4;
    } else if (ef == EF_HEAL || ef == EF_HEALS) {
      rgn = (ef == EF_HEALS ? SKL_MGAT_SPHERE : 0); dmg = SKL_MGAT_HEAL;
    } else {
      rgn = (ef-EF_ROOF) % 3 + SKL_MGAT_ROOF; dmg = (ef-EF_ROOF) / 3;
    }

    /* 攻撃範囲 */
    if (rgn >= SKL_MGAT_LINE && rgn <= SKL_MGAT_EXPLODE)
      /* Pierce/Explode は Line を兼ねる */
      skillup_magic_one(n,m,SKL_MAGIC_AT,SKL_MGAT_LINE,(rgn == SKL_MGAT_LINE ? 1 : 0));
    if (rgn)
      skillup_magic_one(n,m,SKL_MAGIC_AT,rgn,1);

    /* ダメージタイプ */
    skillup_magic_one(n,m,SKL_MAGIC_AT,dmg,1);

  } else {
    /* 状態変化系 */
    tp = idx = 0;
    if (ef > EF_HEALS && ef < EF_ROOF) {
      tp = SKL_MAGIC_EF1; idx = ef - EF_HEALS -1;
    } else if (ef > EF_MAREA && ef <= NORMAL_EF) {
      tp = SKL_MAGIC_EF2; idx = ef - EF_MAREA -1;
    } else if (ef > NORMAL_EF && ef < EF_TRAPHP) {
      tp = SKL_MAGIC_EF3; idx = ef - NORMAL_EF -1;
    }
    skillup_magic_one(n,m,tp,idx,1);
  }

  /* 属性 */
  skillup_magic_one(n,m,SKL_MAGIC_FN,md->fn,1);
  return (1);
}

int skillup_create(int n,ItemData *id,int l)
{
	skillup(n,SKL_CREATE_IT,id->type & IT_MASK,99,50,1+l);
	skillup(n,SKL_CREATE_WT,(id->type & WT_MASK)/WT_PLACE,99,50,1+l);
	skillup(n,SKL_CREATE_MT,(id->type & MT_MASK)/MT_PLACE,99,50,1+l);

	/* 見てるだけより… */
	skillup(n,SKL_DETECT_IT,id->type & IT_MASK,99,50,2+l);
	skillup(n,SKL_DETECT_WT,(id->type & WT_MASK)/WT_PLACE,99,50,2+l);
	skillup(n,SKL_DETECT_MT,(id->type & MT_MASK)/MT_PLACE,99,70,5+l);
	
	return(0);
}

int skill_down_dead(int n)
{
	int i,j;

	if (n >= MAX_NTTYS)
		return(0);

	for (i=0;i<MAX_SKILL_TYPES;i++) {
		if (cd[n].skill[i] != nullptr && kSkillMax[i] > 0) {
			for (j=0;j<kSkillMax[i];j++) {
				cd[n].skill[i][j] -= 10;
				/* 一度習得した Skill LV は 0にはならない */
				if (cd[n].skill[i][j] >= 90 && cd[n].skill[i][j] < 100)
					cd[n].skill[i][j] = 100;
				if (cd[n].skill[i][j] < 0)
					cd[n].skill[i][j] = 0;
			}
		}
	}

	return(1);
}

int skill_down_idle(int n)
{
	int i,j;

	if (n >= MAX_NTTYS)
		return(0);

	for (i=0;i<MAX_SKILL_TYPES;i++) {
		if (cd[n].skill[i] != nullptr && kSkillMax[i] > 0) {
			for (j=0;j<kSkillMax[i];j++) {
				if (PER_DICE < cd[n].skill[i][j]/100) {
					cd[n].skill[i][j]--;
					/* 一度習得した Skill LV は 0にはならない */
					if (cd[n].skill[i][j] == 99)
						cd[n].skill[i][j] = 100;
					if (cd[n].skill[i][j] < 0)
						cd[n].skill[i][j] = 0;
				}
			}
		}
	}

	return(1);
}

/* skillup_magic() と同じ効果振り分けで、対応する熟練度を返す。
   ただし EF_NONE はここでは 0 を返す（skillup_magic() 側は
   効果 0 でも AT 2/0 の熟練度を振る経路に入る）。添字の
   3/7/2 は SKL_MGAT_LINE/SKL_MGAT_ROOF/SKL_MGAT_HEAL の
   値そのもの。 */
int skill_mg_calc(int n, MagicData *md)
{
  int ef;
  int tp,idx;
  int dmg,rgn;

  ef = md->effect;

  if (ef >= EF_TRAPHP || ef == EF_NONE) {
    return(0);
  } else if (ef == EF_HEAL || ef == EF_HEALS) {
      return(cd[n].skill[SKL_MAGIC_AT][2]);
  } else if (ef < EF_HEAL || (ef >= EF_ROOF && ef <= EF_MAREA)) {
    if (ef < EF_HEAL) {
      rgn = (ef-EF_LINE) % 4 + 3; dmg = (ef-EF_LINE) / 4;
    } else {
      rgn = (ef-EF_ROOF) % 3 + 7; dmg = (ef-EF_ROOF) / 3;
    }

    return((cd[n].skill[SKL_MAGIC_AT][dmg]+cd[n].skill[SKL_MAGIC_AT][rgn])/2);
  } else {
	tp = idx = 0;
    if (ef > EF_HEALS && ef < EF_ROOF) {
      tp = SKL_MAGIC_EF1; idx = ef - EF_HEALS -1;
    } else if (ef > EF_MAREA && ef <= NORMAL_EF) {
      tp = SKL_MAGIC_EF2; idx = ef - EF_MAREA -1;
    } else if (ef > NORMAL_EF && ef < EF_TRAPHP) {
      tp = SKL_MAGIC_EF3; idx = ef - NORMAL_EF -1;
    }

    return(cd[n].skill[tp][idx]);
  }
}

int skill_crt_check(int n, ItemData *id)
{
	/* WT/MT の添字は skillup_create() と同じく PLACE で割ってから
	   使う。以前は割らずに添字にしていたため、type & WT_MASK は
	   最大 0xf0、type & MT_MASK は最大 0xf00 になり、16 個しかない
	   配列の範囲外を読んでいた（2026-11-16 修正）。 */
	return ((cd[n].skill[SKL_CREATE_IT][id->type & IT_MASK] +
	         cd[n].skill[SKL_CREATE_WT][(id->type & WT_MASK)/WT_PLACE] +
             /* 素材のウェイトが大きい… */
	     2 * cd[n].skill[SKL_CREATE_MT][(id->type & MT_MASK)/MT_PLACE]) / 4);
}
