/* phi_class.h - PHIDM NEO extension: phi.cls (Lua) switch programs.
 *
 * A switch record with action type 62 carries a program name (a quoted
 * string). When the switch fires, the same-named Lua function defined
 * in the world's phi.cls file is executed and control returns to the
 * normal switch processing.
 *
 * class_load()  : (re)load phi.cls into a fresh Lua state. Called at
 *                 server startup and by the master spell "Load class".
 * class_call()  : run one program for player n; sx, sy are the switch
 *                 cell coordinates that fired.
 * class_tick()  : run the phi.cls global "on_second" for player n.
 *                 Called once per second from the main loop; a no-op
 *                 unless phi.cls defines on_second.
 *
 * This file also hosts the per-character string store (phi.qst),
 * the persistence back end of the dm.get_str / dm.set_str /
 * dm.del_str Lua API:
 * qstr_load()   : load phi.qst into memory. Called once at startup.
 * qstr_save(n)  : write the store back. Called on the same occasions
 *                 as save_sav(): character logout and gate transfer.
 *                 Returns 1 on success (or when there is nothing to
 *                 do), 0 on a write failure.
 *
 * This file also hosts the job / gender system (phi.jb), a light
 * title-like attribute for player characters:
 * jb_load()     : load phi.jb at startup. Without the file the whole
 *                 system is disabled and behavior is unchanged.
 * jb_job(n)     : character n's job (0 = unset, 1-9), read from the
 *                 "job:" ChrEx property stored in phi.cd.
 * jb_sex(n)     : character n's gender (0 = unset, 1, 2), likewise
 *                 from the "sex:" ChrEx property. Display/test only;
 *                 gender has no numeric effect.
 * jb_set_job() / jb_set_sex() : write the ChrEx property (switch
 *                 action 80). No-ops while the system is disabled.
 * jb_rate(job, slot) : effective rate of one job effect, after the
 *                 phi.jb override and the engine cap (base x 1.5).
 * jb_gp_gain(n, gp)  : a GP income amount with the merchant bonus
 *                 applied (identity for other jobs / when disabled).
 * jb_job_name() / jb_sex_name() : display names for %job / %sex.
 * jb_send_self(n) / jb_send_chr(n) / jb_send_chr_all(n) :
 *                 #ex-job frames for MD_JOB clients (display only;
 *                 names come from the current world's phi.jb, so in
 *                 a world without phi.jb the names are empty).
 *
 * This file also hosts the pet system (phi.pet / phi.pt), a
 * permanent, entity-less companion for player characters:
 * pet_load()    : load phi.pet (records) and phi.pt (tuning) at
 *                 startup. Without phi.pt the built-in defaults of
 *                 the design table apply; the system itself is not
 *                 gated on the file (pets never leave the world:
 *                 phi.pet is per-world, unlike the ChrEx job data).
 * pet_save(n)   : write the store back. Called on the same
 *                 occasions as qstr_save(): logout and gate exit.
 * pet_tick(n)   : once per second per player: pool regeneration,
 *                 healer pets' HP watch, bird support casts.
 * pet_attack_begin(n) : reset the per-attack-command pool budget;
 *                 called at the top of attack_other().
 * pet_assist(as, ds, dmg, friend) : the pet's follow-up attack,
 *                 called inside attack_to() right after the owner's
 *                 hit damage was applied. dmg is the owner's final
 *                 damage of that hit; friend is attack_other()'s
 *                 friend_attack flag.
 * pet_on_damaged(n, p, f) : hook called after every hp_dmg() on a
 *                 player: combat timer and the cat's emergency heal.
 * pet_grant(n, type) / pet_rename_start(n) / pet_release_start(n) /
 * pet_restore(n) : switch action 92 and the dm.pet_* Lua API.
 * pet_name_input(n, s) / pet_release_input(n, s) : the ST_PETNAME /
 *                 ST_PETREL input states, fed from input_analyze().
 * pet_type(n)   : 0 = no pet, 1 fairy, 2 cat, 3 dog, 4 bird.
 * pet_active(n) : the pet can act (its main pool is not empty).
 * pet_send_self(n) / pet_send_chr(n) / pet_send_chr_all(n) :
 *                 #ex-pet frames for MD_PET clients.
 */
#ifndef PHI_CLASS_H
#define PHI_CLASS_H

void class_load(void);
void class_call(int n, const char *prog, int sx, int sy);
void class_tick(int n);
void qstr_load(void);
int qstr_save(int n);
void jb_load(void);
int jb_enabled(void);
int jb_job(int n);
int jb_sex(int n);
void jb_set_job(int n, int v);
void jb_set_sex(int n, int v);
int jb_rate(int job, int slot);
int jb_heal_rate(int n);
int jb_matk_rate(int n);
int jb_gp_gain(int n, int gp);
const char *jb_job_name(int job);
const char *jb_sex_name(int sex);
void jb_send_self(int n);
void jb_send_chr(int n);
void jb_send_chr_all(int n);
void pet_load(void);
int pet_save(int n);
void pet_tick(int n);
void pet_attack_begin(int n);
void pet_assist(int as, int ds, int owner_dmg, int friend_attack);
void pet_on_damaged(int n, int p, int f);
int pet_grant(int n, int type);
int pet_rename_start(int n);
int pet_release_start(int n);
int pet_release_now(int n);
int pet_restore(int n);
const char *pet_name_of(int n);
void pet_name_input(int n, char *s);
void pet_release_input(int n, char *s);
int pet_type(int n);
int pet_active(int n);
void pet_send_self(int n);
void pet_send_chr(int n);
void pet_send_chr_all(int n);

#endif /* PHI_CLASS_H */
