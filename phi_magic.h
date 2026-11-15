
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38  */

#ifndef __PHI_MAGIC_H__
#define __PHI_MAGIC_H__

#include  "phi_dm.h"

extern int MLine[3];
extern int MHit;
extern int MExplode[3][3];
extern int MHeal;
extern int MCoat;
extern int MProtect;
extern int MIllusion;
extern int MDown;

int    mgstrlen(char *);
int    pow_mg_multiplier(MagicData);
void   pow_mg_msg(int, MagicData);
void   spell_analyze(int, char *);
void   user_lists(int n);
void   master_lists(int n);
void   wizard_light(int);
void   wizard_eye_f(int, int);
void   create_item(int, int);
int    detect_fn(int, int, int, char *);
void   detect_item(int, int);
void   detect_magic(int, int);
void   ident_to(int, int);
void   effect_analyze(int, MagicData *, int);
void   mg_heal(int, MagicData *, int);
void   mg_heal_friend_all(int, MagicData *);
void   mg_heal_friend_one(int, MagicData *);
void   mg_antidote_friend_all(int, MagicData *);
void   mg_cure_friend_one(int, MagicData *);
void   mg_erase(int, MagicData *, int);
void   mg_encourage(int, MagicData *, int);
void   mg_effect(int, MagicData *, int);
void   mg_line_attack(int, MagicData *, int, int);
void   mg_sphere_attack(int, MagicData *, int, int);
void   mg_explode(int, MagicData *, int, int, int, int);
void   mg_area_attack(int, MagicData *, int, int);
void   mg_attack_to(int, int, MagicData *, int, int);
int    remote_get(int);
int    list_inv(int);
void   mg_save_life(int, MagicData *, int);
int    mdp_calc(int, MagicData *, int, int, double *);
void   copy_spell(int, MagicData *);
void   exchange_magic(int, MagicData *);
void   forget_magic(int, int);
void   learn_magic(int, MagicData *);
void   charge_spell(int, int);
int    create_special_item(int, int, int, int);
int    match_special_item(int, int, int, ItemData **, int);
void   display_magics();
void   display_single_magic(int,int);
void   clear_magic_obj();

#endif
