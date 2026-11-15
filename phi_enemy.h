
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#ifndef __PHI_ENEMY_H__
#define __PHI_ENEMY_H__

#include  "phi_dm.h"

#define MC_F  0x1
#define MC_L  0x2
#define MC_R  0x4
#define MC_B  0x8

#define STEP_GO     0
#define STEP_GO_L   1
#define STEP_GO_R   2
#define STEP_GO_B   3
#define STEP_TURN_L 4
#define STEP_TURN_R 5
#define STEP_TURN_B 6

int    try_to_attackstep(int, int, int, int, int);
int    try_to_wizardstep(int, int, int, int, int);
int    try_to_tracestep(int, int, int, int, int);
int    try_to_runstep(int, int, int, int, int, int);
int    auto_action(int);
void   random_walk(int, int);
int    run_walk(int, int);
int    move_check(int, int, int);
int    enemy_check(int, int);
int    attack_check(int, int);
int    magic_check(int, int);
int    line_search(int);
void   select_magic(int, int);
void   select_armor(int);
void   select_accessory(int);
int    curse_check(int, int);
int    str_search(int, char *);
int    next_char(char *, char *);
int    direction_check(int, int, int, int, int);
int    is_effective_effect(int, int, int, int, int);
int    use_charged_item_check(int, int);
int    view_location(int, int, int *, int *);
int    attack_angry(int, int);
int    attack_enemy(int, int);
int    best_er_weapon(int);
int    is_left_mp_moral(int, int);
int    useable_long_range_magic(int, int);
int    npc_talk(int, int);
int    temptation_to_attack(int);
void   angry_check(int, int);
int    accept_master(int,int);
void   cooldown_check();

#endif
