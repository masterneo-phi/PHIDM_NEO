
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#ifndef __PHI_USE_H__
#define __PHI_USE_H__

#include  "phi_dm.h"

int    use_item(int, int);
int    use_charge(int, ItemData *);
int    charge_appear(int, ItemData *);
int    charge_disappear(int, ItemData *);
void   effect_trap(int);
void   delete_item(ItemData *);
int    item_count(int, int);
int    itemtype_count(int, int, int);

#endif
