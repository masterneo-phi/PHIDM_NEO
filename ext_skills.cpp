/* written by Qajin 2002/03/07 */
/* last update 2002/05/02 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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
static int skl_max[] =
{SKL_WPN_MAX,
 SKL_MGAT_MAX,SKL_MGEF1_MAX,SKL_MGEF2_MAX,SKL_MGEF3_MAX,SKL_MGFN_MAX,SKL_MGSP_MAX,
 SKL_DTIT_MAX,SKL_DTWP_MAX,SKL_DTMT_MAX,
 SKL_CRIT_MAX,SKL_CRWP_MAX,SKL_CRMT_MAX,
 0};

static char *skl_id[] =
{"wp",
 "mgA","mgE1","mgE2","mgE3","mgF","mgS",
 "dtI","dtW","dtM",
 "crI","crW","crM",
 "sp"};

extern int enmax;

int skill_init()
{
	int i,j;
	int *p;
	int **npc_skill;

	for (i=0;i<MAX_NTTYS;i++) {
		if ((cd[i].skill = (int **) malloc(sizeof(int *) * MAX_SKILL_TYPES)) == NULL)
			return (-1);
		for (j=0;j<MAX_SKILL_TYPES;j++) {
			if (skl_max[j] > 0) {
				if ((p = (int *) malloc(sizeof(int) * skl_max[j])) == NULL)
					return (-1);
				cd[i].skill[j] = p;
			} else {
				cd[i].skill[j] = NULL;
			}
		}
		skill_clear(i);
	}
	if ((npc_skill = (int **) malloc(sizeof(int *) * MAX_SKILL_TYPES)) == NULL)
		return (-1);
	for (j=0;j<MAX_SKILL_TYPES;j++) {
		if (skl_max[j] > 0) {
			if ((p = (int *) malloc(sizeof(int) * skl_max[j])) == NULL)
				return (-1);
			npc_skill[j] = p;
			for(i=0;i<skl_max[j];i++)
				npc_skill[j][i] = 5000; /* 取り合えず 一律 5000 */
		} else {
			npc_skill[j] = NULL;
		}
	}
	for (i=MAX_NTTYS;i<MAX_NTTYS+MAX_ENEMIES;i++) {
		cd[i].skill = npc_skill;
	}
	return (0);
}

int skill_clear(int n)
{
	int i;
	
	if (n >= MAX_NTTYS)
		return(0);

	for (i=0;i<MAX_SKILL_TYPES;i++) {
		if (cd[n].skill[i] != NULL && skl_max[i] > 0) {
			memset(cd[n].skill[i],0,sizeof(int)*skl_max[i]);
		}
	}
	return(0);
}

int load_skl_type(int n,int type,char *p)
{
	int i,j;
	
	if (n >= MAX_NTTYS || cd[n].skill[type] == NULL)
		return(1);

	for(i=0;i<skl_max[type];i++) {
		if (*p != ':') {
			if (*p != '\n' && *p != '\0')
				return(-1);
			for(;i<skl_max[type];i++)
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
	char buf[256];

	for(i=0;i<MAX_SKILL_TYPES;i++) {
		if (!strncmp(p,skl_id[i],strlen(skl_id[i]))) {
			if (!load_skl_type(n,i,p+strlen(skl_id[i])))
				break;
		}
	}
	if (i == MAX_SKILL_TYPES) {
		if (lcnt > 0) {
			snprintf(buf, sizeof(buf),"%s at line %d: no parameter.(load_skills)", pfname, lcnt);
			log_print(buf);
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
	
	bp = buf; bs = size;
	snprintf(buf,size,":S%s",skl_id[type]);
	l = 2+strlen(skl_id[type]);
	buf += l; size -= l;

	if (cd[n].skill[type] == NULL || skl_max[type] == 0)
		return(-1);

	for (i=k=0;i<skl_max[type];i++) {
		snprintf(buf,size,":%.0i",cd[n].skill[type][i]);
		l = strlen(buf);
		buf += l; size -= l;
		if (cd[n].skill[type][i]==0)
			k++;
		else {
			bp = buf; bs = size;
		}
	}
	strlcpy(bp,"\n",bs);
	return ( k==skl_max[type] ? 1 : 0 );
}

int save_skills_cd(int n,FILE *f)
{
	int i;
	char buf[256];

	for (i=0;i<MAX_SKILL_TYPES;i++) {
		if(!save_skl_type(n,buf,sizeof(buf),i))
			fputs(buf,f);
	}

	return (0);
}

int put_skills(int n,int m)
{
	int i;
	char buf[256];
	
	snprintf(buf,sizeof(buf),"#chr-D %-15s VX",cd[0].uid);

	for (i=0;i<MAX_SKILL_TYPES;i++) {
		if(!save_skl_type(n,buf+25,sizeof(buf)-25,i))
			check_write(m,buf);
	}

	return (0);
}

static char *skl_mgc_regions[] =
{ "Hp damage", "Mp damage", "Heal",
 "Line", "Pierce", "Explode", "Sphere",
 "Roof Shake", "Rain", "Quake"};

static char *skl_mgc_cmnspl[] =
{ "Return", "Eagle eye", "Analyze", "Charge spell",
  "Identify", "Appear", "Disappear"};

int status_skills(int n,char *p)
{
	char buf[256],buf2[256];
	int i,j,k;

	if (fisspace(*p))
		SKIP_SPACE(p);
	else if (*p == 's' && *(p+1) == '\0')
		p++;
	else
		return (1);

	dmcheck_write(n,DMM_SKILLS_SKILL);
	if (*p == '\0' || !strcmp(p,"weapon")) {
		dmcheck_write(n,DMM_SKILLS_WEAPON);
		CPYSTR(buf2,"  ");
		for (i=0;i<SKL_WPN_MAX;i++) {
			if ((j=cd[n].skill[SKL_WEAPON][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 4 == 3 && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
	}
	if (*p == '\0' || !strcmp(p,"spell")) {
		dmcheck_write(n,DMM_SKILLS_SPELL);
		CPYSTR(buf2,"  ");
		for (i=0;i<SKL_MGAT_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_AT][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-9.9s : %2i ",skl_mgc_regions[i],j);
				CATSTR(buf2,buf);
			}
			if ((i==2 || i==6 || i==9) && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
		for (i=k=0;i<SKL_MGEF1_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_EF1][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+11],j);
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
				snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+46],j);
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
				snprintf(buf,sizeof(buf)," %-13.13s: %2i ",use_effect[i+61],j);
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
				snprintf(buf,sizeof(buf)," %-12.12s : %2i ",skl_mgc_cmnspl[i],j);
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
		dmcheck_write(n,DMM_SKILLS_FN);
		for (i=0;i<SKL_MGFN_MAX;i++) {
			if ((j=cd[n].skill[SKL_MAGIC_FN][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-7.7s : %2i ",fn_name[i],j);
				CATSTR(buf2,buf);
			}
			if (i % 3 == 0 && *(buf2+2) != '\0') {
				CATSTR(buf2,"\n");
				check_write(n,buf2);
				CPYSTR(buf2,"  ");
			}
		}
	}
	if (*p == '\0' || !strcmp(p,"detect")) {
		dmcheck_write(n,DMM_SKILLS_DETECT);
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_DTIT_MAX;i++) {
			if ((j=cd[n].skill[SKL_DETECT_IT][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-12.12s: %2i ",it_name[i],j);
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
				snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
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
				snprintf(buf,sizeof(buf)," %-8.8s: %2i ",mt_name[i],j);
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
	if (*p == '\0' || !strcmp(p,"create")) {
		dmcheck_write(n,DMM_SKILLS_CREATE);
		CPYSTR(buf2,"    ");
		for (i=k=0;i<SKL_CRIT_MAX;i++) {
			if ((j=cd[n].skill[SKL_CREATE_IT][i]/100) > 0) {
				snprintf(buf,sizeof(buf)," %-12.12s: %2i ",it_name[i],j);
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
				snprintf(buf,sizeof(buf)," %-6.6s : %2i ",wt_name[i],j);
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
				snprintf(buf,sizeof(buf)," %-8.8s: %2i ",mt_name[i],j);
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
    if (!strcmp(ip_address_check(n),ip_address_check(m)))
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

  i = (int) (log10((double) (cd[n].exp + 10.0)) * 10.0);
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
  if (strlen(special_com[type][0]) == sp / 10)  /* 0番が最長… */
    limit = 99;

  /* 同じ武器を使う相手なら相手の Skill LV が高い程上がり易い */
  if (type == WT_FIST && m < MAX_NTTYS && cd[m].weq == NULL) {
    vslv = cd[m].skill[SKL_WEAPON][WT_FIST/WT_PLACE];
  } else if (cd[m].weq != NULL && (cd[m].weq->type & WT_MASK) / WT_PLACE == type) {
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
		if (cd[n].skill[i] != NULL && skl_max[i] > 0) {
			for (j=0;j<skl_max[i];j++) {
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
		if (cd[n].skill[i] != NULL && skl_max[i] > 0) {
			for (j=0;j<skl_max[i];j++) {
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
	return ((cd[n].skill[SKL_CREATE_IT][id->type & IT_MASK] +
	         cd[n].skill[SKL_CREATE_WT][id->type & WT_MASK]/WT_PLACE +
             /* 素材のウェイトが大きい… */
	     2 * cd[n].skill[SKL_CREATE_MT][id->type & MT_MASK]/MT_PLACE) / 4);
}
