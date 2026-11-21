
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38  */

#include "phi_dm.h"

#ifndef __PHI_DATA_H__
#define __PHI_DATA_H__

#define  chartoint(x) ((int)(unsigned int)(unsigned char)(x))

#define ATOMBUF_SIZE 500

extern ItemData putbuf;
extern int    gfbuf[GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX];  /* 前回変更されたフラグ */
extern int    gfbuf2[GFLAG_MAX + PFLAG_MAX + GVAR_MAX + PVAR_MAX + SVAR_MAX]; /* 今回変更したフラグ */
extern int    gfn;
extern int    gfn2;

extern CHROData *ed_o;

extern int enmax;     /* クリーチャーののべ数 */

extern int    match_n;   /* 条件に合致した数 */

extern int	flr_max;  /* 使用floor max */

/* add for menu 20041212 */
typedef struct menu_t {
   int  menu_num;
   char *sel;
   char *caption;
   int  act_type;
   int  act_param;
} MENUData;

extern int	menun;
extern MENUData  *menud;

#define MENU_ACT_NULL	0x00
#define MENU_ACT_FLAG	0x01
#define MENU_ACT_MESG	0x02
#define MENU_ACT_MENU	0x03


#define CMD_MAXLEN		32

typedef struct cmd_t {
	char	type;
	char	com[CMD_MAXLEN];
	char	act;
	int		param;
} CMDData;

#define CMD_MAX		16

extern CMDData cmdd[CMD_MAX];


int  fsign(int);
int    fsps(int, int, int);
char  *fstrstr(char *, char *);
char   ftoupper(char);
char   ftolower(char);
int    fisspace(char);
void   fecrypt(char *, int);
void   item_gc(int);
void   swap_item(ItemData *, ItemData *);

char  *efgets(char *, int, FILE *);

FILE  *efopen(char *, char *);
void    efclose(FILE *);
void   additem(int, int, ItemData *, int);
ItemOData **finditem(int, int, ItemOData **);
char  *findstr(char *);
StringData *findstrdata(char *);
void   importcir(int);
void   exportcir(int);
void   load_mesg(int);
void   display_menu(int,int);
void   select_menu(int,char *);
int    authorize(char *);
int    shift_gate(int *, int *, int, char *, char *);
int    ip_banned(int);
int    ip_multi(int);
int    id_banned(int );
int id_ban_add(char *);
char  *select_password(char *, char *);
CHRExData **load_chrex(CHRExData **,char *);
void erase_chrex(CHRExData *);
int    init_cd(int);
int    load_cd(int, int);
int    save_cd(int, int);
int    backup_cd(int);
int    get_cd(int, char *, char *);
void   put_cd(int, int, char *);
int    load_sav(int);
int    save_sav(int);
int    load_authority(int);
int    have_authority(int,int,int,int);
void   load_ed_o();
void    free_ed_o();
void   load_ed(int);
void   load_id();
int    savef_id();
int  savef_id_in_circle();
int  load_id_in_circle(int,int);
void   load_fd();
void   free_fd();
void   load_td_gate();
void   free_gatedata();
void   free_outgatedata();
void   load_td_swt();
void   load_td_ban();
void   load_cmd();
int    check_and_process_cmd(int, char, char *);
void   free_swtd();
void   load_dd();
void   remap_lock_and_board(int);
void   extension_dd(int);
unsigned short set_dd(char *);
int    save_dd();
void   list_gate(int);
void   load_gate(int, int);
int    get_gate(char *, int, int);
void  get_gaten(int, int,int,int,int,int,
                char *,char *,int);
void   put_gaten(int, int);
void   put_gate(int);
void   send_down();
int   load_switch(int, int, int, int);

/* DM演出 (FX) 拡張 (PHIDM NEO extension, phi_data.cpp) */
int   fx_send_payload(int, const char *);
int   fx_send_img(int, const char *, const char *, long);
void  fx_send_image_data(int, const char *);
void  fx_clear_image_cache(void);
unsigned int fx_cn_mask(int);
void   change_flag(int, int, int, int);
int   load_sd(int, int, int, int, int);
int    save_sd(int, int, int, int, char *, int);
int    set_title(int, int, int, int);
void   board_title(int,char *,int);
int    list_board(int, int);
int    search_board(int, char *);
void   get_title(char *);
void   load_board(int, char *);
void year_format(char *, int);
void   append_file(int, char *, char *);
void   id2f(int, char *);
int    save_id(int, int, int);
void   save_gp(int);
int    save_store(int, int, int, char *);
void   str_gc();
void   item_destruct();
void   min_proc();
void    clear_gate_and_switch_on_map();
void    remap_gate_and_switch();
int    list_item(int, int);
int    list_store(int, int);
int    item_number_on_floor(int, char *, int);
void   item_name_on_floor(char *, ItemOData *, int);
ItemOData  *investigate_item_on_floor(int, int);
int    load_item(ItemData *, char *);
void   save_item(ItemData *, char *, int);
int    match_item(ItemData, ItemData);
int   load_spell(MagicData *, char *);
void   save_spell(MagicData *, char *, int);
int match_spell(MagicData, MagicData);
void   load_talk(TalkData *, char *);
void   save_talk(TalkData *, char *, int);
void   load_app(AppData *, char *);
void   bulletin_board(int, char *);
int    decimal_load_arbitrarily(char **);
int    decimal_load_p(int n, char **p); /* Wildcard error 対策 added by NEO 20060707 */
int    decimal_load(int, char **);
int    hexadecimal_load_arbitrarily(char **);
int    hexadecimal_load(int, char **);
int    flagno_load(char **);
int    varno_load(char **);
int    var_value(int,int);
void   loadstr(char *, char **, int, int);
int    wildcmp(char *, char *);
int    fn_load(char);
void load_crt();
void clear_crt();
int    get_server_port();
int    server_version_check(int, int, char *, int);
void    load_ex();
void   free_ex();
int    export_item_check(int, int, int, char *, char *, char);
int str_to_obj(char **);
void clear_obj();
void load_obj();
int save_obj();
int search_obj(int,char *);
int move_obj(int,int,int,int);
void init_magic_objects();

ItemPData *additempos(int,int);
ItemPData *finditempos(int,int);
void delitempos(int,int);

void check_id_in_circle();

void DegradeArmorAndAccesory( int as, int ds );  /* 防御側の鎧・アクセの欠け処理 added by NEO 20120212 */

#endif
