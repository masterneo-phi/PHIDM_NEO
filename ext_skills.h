
#ifndef __PHI_SKILL_H__
#define __PHI_SKILL_H__

#include "phi_dm.h"

#define MAX_SKILL_TYPES	14

#define SKL_WEAPON	0
#define SKL_MAGIC_AT	1
#define SKL_MAGIC_EF1	2
#define SKL_MAGIC_EF2	3
#define SKL_MAGIC_EF3	4
#define SKL_MAGIC_FN	5
#define SKL_MAGIC_SP	6
#define SKL_DETECT_IT	7
#define SKL_DETECT_WT	8
#define SKL_DETECT_MT	9
#define SKL_CREATE_IT	10
#define SKL_CREATE_WT	11
#define SKL_CREATE_MT	12
#define SKL_SPECIAL	13

#define SKL_WPN_MAX		0x10

#define SKL_MGAT_MAX	10	/* 01～10 40～45 */
#define SKL_MGEF1_MAX	29	/* 11～39 */
#define SKL_MGEF2_MAX	15	/* 46～60 */
#define SKL_MGEF3_MAX	33	/* 61～93 */
#define SKL_MGFN_MAX	FN_MAX
#define SKL_MGSP_MAX	7

#define SKL_MGAT_HP	0
#define SKL_MGAT_MP	1
#define SKL_MGAT_HEAL	2
#define SKL_MGAT_LINE	3
#define SKL_MGAT_PIERCE	4
#define SKL_MGAT_EXPLODE	5
#define SKL_MGAT_SPHERE	6
#define SKL_MGAT_ROOF	7
#define SKL_MGAT_UNROOF	8
#define SKL_MGAT_AREA	9

#define SKL_MGEF1_COAT	1
#define SKL_MGEF1_SPELL	3
#define SKL_MGEF1_ANTIDOTE	18
#define SKL_MGEF1_REMOVE	19
#define SKL_MGEF1_AWAKEN	20

#define SKL_MGEF3_SAVELIFE	7

#define SKL_MGSP_RETURN	0
#define SKL_MGSP_EAGLE	1
#define SKL_MGSP_ANALYZE	2
#define SKL_MGSP_CHARGE	3
#define SKL_MGSP_IDENT	4
#define SKL_MGSP_APPEAR	5
#define SKL_MGSP_DISAPR	6

#define SKL_DTIT_MAX	0x10
#define SKL_DTWP_MAX	0x10
#define SKL_DTMT_MAX	0x10

#define SKL_CRIT_MAX	0x10
#define SKL_CRWP_MAX	0x10
#define SKL_CRMT_MAX	0x10

int skill_init();
int skill_clear(int);
int load_skl_type(int,int,char *);
int load_skills(int,char *);
int save_skl_type(int,char *,int,int);
int save_skills_cd(int,FILE *);
int put_skills(int,int);
int status_skills(int,char *);
int check_lvup_pc(int,int);
int skillup(int,int ,int ,int, int, int);
int skillup_weapon(int,int ,int ,int);
int skillup_magic_one(int,int ,int type,int ,int );
int skillup_magic(int,int ,MagicData *,int );
int skillup_create(int ,ItemData *,int );
int skill_down_dead(int);
int skill_down_idle(int);
int skill_mg_calc(int, MagicData *);
int skill_crt_check(int , ItemData *);

#endif
