/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#ifndef __PHI_ATTACK_H__
#define __PHI_ATTACK_H__

#include  "phi_dm.h"

void   attack_other(int n);
void   attack_to(int ,int);
void   guard_self(int);
int    spat_check(char *,int ,char *,int,int);
int    degrade(ItemData *id);
int    speed_degrade(ItemData *id);

#endif
