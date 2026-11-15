
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#ifndef __PHI_CALC_H__
#define __PHI_CALC_H__

#include  "tcp.h"
#include  "phi_dm.h"

int    sum_wp(int);
int    df_rate(int);
int    mr_rate(int);
int    hp_dmg(int, int, int);
int    mp_dmg(int, int, int);
void   cast_mmpup(int, char *, int);
void   dead(int);
void   raise_dead(int);
void   kill_other(int, int, int);
void   kill_self_msg(int, char *);
void   lv_up(int, int, int, int);
int    fn(int, int, int);
int    dice(int);
int    lv_dice(int);
int    fisqrt(int);
void   set_fn(int);
int    ecn(int, int);
int    sep_calc(int, ItemData *);
int    gigant_check(int);
int    sg_check(int);
int    analyze_numbers(int *, int, char *);
int    analyze_single_number(char *);
int    analyze_hexnumbers(int *, int, char *);

#endif
