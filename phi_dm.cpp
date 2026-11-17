

/*	Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38  */

#include  <cstdio>
#include  <cstring>
#include  <csignal>
#include  <ctime>
#ifdef Mips
#include  <sys/timeb.h>
#else
#include  <cstdlib>
#endif
#include  <cerrno>
#include  <unistd.h>
#include  <climits>
#include  <array>

#ifdef AIX
#include <sys/select.h>
#endif

#ifdef __CYGWIN32__
#include <process.h>
#endif

#include  "tcp.h"
#include  "phi_dm.h"
#include  "phi_attack.h"
#include  "phi_calc.h"
#include  "phi_data.h"
#include  "phi_enemy.h"
#include  "phi_magic.h"
#include  "phi_use.h"
#include  "phi_obj.h"
#include  "phi_str.h"
#include  "phi_m57.h"
#include  "phi_m81.h"
#include  "new_proto.h"
#include  "new_command.h"
#include  "ext_skills.h"
#include  "ext_party.h"

/* Single definitions of the globals declared extern in phi_dm.h.
   (C++ stage 0: the C build relied on -fcommon merging the
   tentative definitions from the header.) */
CrtData *first_create_data;
ItemPData *iphash[HASHMAX];
StringData *shash[HASHMAX];
int    generic_variable[GVAR_MAX];
char init_graphic[MAX_INIT_GRAPHIC][32];
int init_graph_n;
ExportItemData  *first_export_data;
char id_ban[BANNEDIDMAX][16];
ObjData od[MAX_OBJECTS];
#if MAX_OBJECTS > 255
unsigned short dobj[MAX_Y][MAX_X];
#else
unsigned char dobj[MAX_Y][MAX_X];
#endif
int    is_see_magic_effects;
FILE  *sfp;
char   tcp_addr[16];
char   land_name[32];
MapData map[MAX_Y][MAX_X];
long   tax;
int    tslide;
char   yname[32];
int    max_enemy;
CHRData cd[MAX_NTTYS + MAX_ENEMIES];
int    eflag;
int    log_op;
int    adding_power_spell_flag;
int    npc_use_charged_item;
int    ciflag;
int    jgflag;
int    nciflag;
int    ggflag;
char   cirunit[20];
int    cirate;
int    trnf;
int    addf, addx, addy;
char   addw[5];
int    wizeyef;
int    autoeqf;
char   roofoff[23];
int    sport;
long    s_id;
FLRData *fld;
FLRTData fldt[26];
ATOM **swtd;
int    swtn;
MSGData *msgd;
int      msgn;
ATOM **gatedata;
int    gatemax;
PowerSpellData power_spell[MAX_POWER_SPELL];
int    is_see_burst;
int    is_need_charge_time;
int    is_need_cast_time;
int    is_single_use;
int    is_destroy_companion;
int    is_kill_master;
int    is_speed_degrade;
int    is_degrade_charge;
int    is_limit_mrp;
BannedIP ip_ban[BANNEDIPMAX];
unsigned char wall_for_beast;
int    magic_anti_degrade;
int    spellup_only_time;
int    limit_addusers;
int    fighter_wizard;
int    multi_access;
int    save_condition;
int    lv_up_between_pc;
int    gift_tax;
int    offline_exp;
int    flagsave;
int    deny_pk;
int    death_penalty;
int    penalty_rate;
int    dp_limitter;
int    dp_beginner;
int    rebirth_life;
int    access_dm;
int    counter_effect;
int    attack_effect;
int    axe_blade_setting;
int    sys_magic_no_degrade;
int    mhp_up_for_attack;
int    mmp_up_for_attack;
int    spell_learning;
int    degrade_by_magic;
int    generic_flag[GFLAG_MAX];


time_t abstime;

struct tm *sttime;
char   timestr[32];
int    dsec = 0;
int    min = 0;
int    hour = 0;
int    oldh = 0; /* bug fix 20021130 */
int    yslide = 0;

/* px/py は他モジュールが extern int px[] / py[] として参照する
   共有インターフェースのため C 配列のまま */
int    px[4] = {0, 1, 0, -1}; /* 向きの識別番号と、実際の方向の関係 */
int    py[4] = {-1, 0, 1, 0};

/*	----->x   0:up		   */
/* |		  1:right	   */
/* |		  2:down	   */
/* |y		  4:left	   */
/* v					   */

/* テンキー選択の為のパース用 added by Qajin 19991203 */
namespace {

constexpr std::array<const char*, 10> kParsecom =
{"check",
 "hit"	 ,"go b"  ,"cast",
 "go l"  ,"turn b","go r",
 "turn l","go"	,"turn r"};

} // namespace

int reflesh_last_map[MAX_NTTYS];
static int reflesh_last_status[MAX_NTTYS];
int reflesh_objects;  /* add 20000107 */

/* add for refuse co-addusers 20000706 */
/* pre_adduser_ip は CPYSTR の宛先のため C 配列のまま */
static char pre_adduser_ip[32];

/* add for send logs 20010809 */
int log_send;

#ifdef DEBUG
char debug_buf[256];
#endif
#ifdef DEBUG_SEND_MSG
char debug_buf_msg[MAX_MESSAGE_SIZE];
#endif

extern int enmax;
extern CHROData *ed_o;
extern int lcnt;

int    main(int argc, char **argv)
{
  /* buf は CPYSTR の宛先とポインタ演算で使うため C 配列のまま */
  char	 buf[1024];
  std::array<char, 100> errmsg;
  std::array<char, 32> nm;
  char	 *p;
  int	 n;
  int	 i, j, k;
  int	 fd;
  int	**sklbk; /* bug fix for skill clush 20021031 */

  /* from psp13j7 */
  int	 server_lag_cnt;
  time_t server_lag_tmp; /* 時間計測テンポラリ */
  time_t server_lag; /* 10秒の実時間 */
  time_t server_lag_prev;
  time_t server_lag_prev2;
  /* ^^ */

  log_send = 0;

  log_op = 0;
  for (argc--; argc > 0; argc--) {
	if (!std::strcmp(argv[argc], "-logall"))
	  log_op = 1;
  }
  signal(SIGTERM, signal_trap);
  signal(SIGSEGV, signal_trap);
  signal(SIGFPE, signal_trap);
#ifndef __MINGW32__
  signal(SIGBUS, signal_trap);
  signal(SIGPIPE, SIG_IGN);
#endif
  tcp_initialize();
  for (n = 0; n < MAX_ENEMIES + MAX_NTTYS; n++) {
	cd[n].fd = -1;
	cd[n].uid[0] = 0;
	cd[n].type = CT_NOEXIST;
	cd[n].st = ST_COMMAND;
	cd[n].auth = nullptr;
	cd[n].extension = nullptr;
	cd[n].party = 0; /* add 20020822 */
  }
  tax = 0;
  tslide = 0;
  abstime = time(nullptr);
  sttime = (struct tm *) localtime(&abstime);
  CPYSTR(timestr, (char *) asctime(sttime));
  min =   (int) (timestr[14] - '0') * 10 + (int) (timestr[15] - '0');		/* bug fix 20000522 */
  dsec = ((int) (timestr[17] - '0') * 10 + (int) (timestr[18] - '0')) * 10;
  if (!set_server(get_server_port())){
	puts(" Server Set Error. ");
	exit(1);
	/* ポートが解放されていない等により終了してしまう場合、*/
	/* load_dd() 中の put_gate()による他世界とのゲートとの接続を */
	/* しないで欲しいため、先に行う。*/
  }
  load_dd();
  oldh = hour = (int) (timestr[11] - '0') * 10 + (int) (timestr[12] - '0') + tslide;
  log_print("start    : Dungeon Master ");
  std::snprintf(buf, sizeof(buf),"version  : %s ", PHI_VERSION);
  log_print(buf);
  std::snprintf(buf, sizeof(buf),"tax      : %li ", tax);
  log_print(buf);
  server_lag_cnt = 0;
  server_lag = server_lag_prev = server_lag_prev2 = 10;
  server_lag_tmp = time(nullptr) - 10;
  while (1) {
	/* from psp13j7 */
	server_lag_cnt = (server_lag_cnt + 1) % 100;
	if (server_lag_cnt == 1) {
	  server_lag_prev2 = server_lag_prev;
	  server_lag_prev = server_lag;
	  server_lag = time(nullptr) - server_lag_tmp;
	  server_lag_tmp = time(nullptr);
	}
	/* ^^ 13j7 */

	/* change for active wait 20000901 */
	/* 0とかマイナスになっちゃったらどうしよう… (汗 */
	connect_check(100 - ((server_lag+server_lag_prev+server_lag_prev2) / 3 - 10 )*10);
	/* nfclose() 後のエラー発生 */
	while((n=nfclose_error_check())>=0){
	  std::snprintf(buf, sizeof(buf),"exit-err : (%i:%i) = %s (%s)",
		  n, fd_check(n), ip_domain_check(n),
		  errnum_to_msg(connect_error_check(n)));
	  log_print(buf);
	}
	/* 新規接続受理 */
	while((n=new_connect_check())>=0){
	  if(connect_full()){
	std::snprintf(buf, sizeof(buf),"full connect: (%i:%i) = %s ",
		n, fd_check(n), ip_domain_check(n));
	log_print(buf);
	write_line(n, " No more connection. \n#x\n", 25, 0);
	nfflush(n, 1000);
	nclose(n);
	continue;
	  }
	  /* add for multi access control 20000713 */
	  if(multi_access && !ip_multi(n)) {
		std::snprintf(buf, sizeof(buf),"multi access refused: (%i:%i) = %s",
				n, fd_check(n), ip_domain_check(n));
		log_print(buf);
		write_line(n, "#x\n", 3, 0);
		nfflush(n, 1000);
		nclose(n);
		continue;
	  }
	  /* psp4 add - banned check - by aleph */
	  /* 新規接続の:Bスイッチ拒否 */
	  if(!ip_banned(n)){
		std::snprintf(buf, sizeof(buf),"connection refused: (%i:%i) = %s",
				n, fd_check(n), ip_domain_check(n));
		log_print(buf);
		write_line(n, "#x\n", 3, 0);
		nfflush(n, 1000);
		nclose(n);
		continue;
	  }
	  /* end */
	  init_connected_cd(n);
	  std::snprintf(buf, sizeof(buf),"connect  : (%i:%i) = %s ", n, cd[n].fd, cd[n].str);
	  log_print(buf);
	}
	/* 接続切断受理 */
	for(n=1; n<MAX_NTTYS; n++)
	  if(cd[n].fd>=0 && lost_connect(n))
	connection_error_process(n, connect_error_check(n));
	dsec++;
	if (dsec >= 600) {	  /* 1分おきに実行する手続き */
	  dsec = 0;
	  min++;
	  if (min >= 60)
	min = 0;
	  abstime = time(nullptr);
	  sttime = (struct tm *) localtime(&abstime);
	  CPYSTR(timestr, (char *) asctime(sttime));
	  min_proc();
/*		k = hour;*/
	  hour = (int) (timestr[11] - '0') * 10 +
	(int) (timestr[12] - '0') + tslide;
	  if(
#ifdef DEBUG_TIME
	 1
#else
	 oldh != hour	 /* 1時間おきに実行する手続き */
#endif
	 ) {
	if (
#ifdef DEBUG_TIME
	 1
#else
	min == 0 /* 負荷時差分散 20020516 */
#endif
	) {
	std::snprintf(buf, sizeof(buf),"tax      : %li ", tax);
	log_print(buf);
#ifdef DEBUG
	log_print("phi_dm:call save_sd()");
#endif
	save_sd(0, 0, 0, 'G', nullptr, 0);
	}
	if (
#ifdef DEBUG_TIME
	 1
#else
	min == 1
#endif
	) {
#ifdef DEBUG
	log_print("phi_dm:call save_dd()");
#endif
	save_dd();
	}

/* サークル内アイテムファイル追出し add 20020807 */
	if (
#ifdef DEBUG_TIME
	 1
#else
	min == 2
#endif
	) {
#ifdef DEBUG
	log_print("phi_dm:call savef_id_in_circle()");
#endif
	savef_id_in_circle();
	}

	if(
#ifdef DEBUG_TIME
	   1
#else
	   hour % 6 == 0  /* 6時間おきに実行する手続き */
#endif
	   ){
	  if (
#ifdef DEBUG_TIME
	   1
#else
	   min == 3
#endif
	  ) {
	  clear_gate_and_switch_on_map();
	  remap_gate_and_switch();
	  }
	  if (
#ifdef DEBUG_TIME
	   1
#else
	   min == 4
#endif
	  ) {
#ifdef DEBUG
	  log_print("phi_dm:call str_gc()");
#endif
	  str_gc();
	  }
	  if (
#ifdef DEBUG_TIME
	   1
#else
	   min == 5
#endif
	  ) {
#ifdef DEBUG
	  log_print("phi_dm:call savef_id()");
#endif
	  savef_id();
	  oldh = hour;
	  }
	}
	  }
	}
	if (dsec % 10 == 0) {
	  for (n = 1; n < enmax; n++) {
	/* n = 0 でないのは、cd[0] はもはや cd[i](i=1,2..) と */
	/* 同じ型の編集用の変数になってしまっているから。	  */
	if (cd[n].type) {
	  /* 時間当り送信量の平均化(微々たる物だが) 20020516 */
	  if(n < MAX_NTTYS && dsec / 10 == n % 60) {
		CN_LAG(n) = -1;  /* 負の場合はラグの測定 */
		check_write(n, "#lag\n");
	  }
	  /* change for sheil cheat 20000522 */
	  if (cd[n].mhp > 0 || (n < MAX_NTTYS && cd[n].exp < 99999999)) {
		if (TCN_POISON(n)) {
		  if (TCN_REGENE(n) < TCN_POISON(n))
		if(hp_dmg(n, TCN_REGENE(n) - TCN_POISON(n), 0))
		  kill_self_msg(n, DMM_KILL_SELF_POISON);
		} else if (TCN_REGENE(n))
		  hp_dmg(n, cd[n].mhp / 100 + 1, 0);
		else if (dsec == 0) {
		  hp_dmg(n, cd[n].mhp / 100 + 1, 0);
		}
	  }
	  if (dsec == 0) { /* bug fixed 20000302 */
		if (cd[n].exp < 99999998) {
			cd[n].exp++; /* add for idling skill down 20020507 */
			if (cd[n].exp % 10 == 0) /* Bugfix for レベルアップ表示 by NEO 2008/04/29 */
			level_up_print(n); /* Add for レベルアップ表示 by NEO 2008/04/04 */
			if (cd[n].exp % 60 == 0)
			skill_down_idle(n);
		}
		if (cd[n].align < 99 && TCN_KARMA(n) > 0)
		cd[n].align++;
		  else if (cd[n].align > 0 && TCN_KARMA(n) < 0)
		cd[n].align--;
	  }
	  /* change for sheil cheat 20000522 */
	  if (cd[n].mmp > 0 || (n < MAX_NTTYS && cd[n].exp < 99999999)) {
		if (TCN_PANIC(n)) {
		  if (TCN_PANIC(n) > TCN_WILL(n))
		if(mp_dmg(n, TCN_WILL(n) - TCN_PANIC(n), 0))
		  kill_self_msg(n, DMM_KILL_SELF_PANIC);
		} else if (TCN_WILL(n))
		  mp_dmg(n, cd[n].mmp / 1000 + 1, 0);
		else if (dsec % 60 == 0)
		  mp_dmg(n, cd[n].mmp / 1000 + 1, 0);
	  }
	  for (i = 0; i <= NORMAL_CN; i++) {
		if (cd[n].cn[i] < 0 && cd[n].cn[i] > -0x10000) /* change for eternal blind :-P 20000728 */
		  cd[n].cn[i]++;
		if (cd[n].cn[i] > 0 && cd[n].cn[i] < 0x10000)
		  cd[n].cn[i]--;
		if (i < MAX_NCN && cd[n].cn[i] == 0) /* add 20000725 */
		  cd[n].ncn[i] = 1;
	  }
		CN_IDLE(n)++;
		if (CN_IDLE(n) >= 60) {
		  CN_GASIFY(n) = 60; /* change for checkable idle 20010222 change 20020822 */
		}
		if (fld[map[cd[n].y][cd[n].x].area].flag & FF_GASIFY)
		  CN_GASIFY(n) = 0;
	  /* add for area flag 20000629 */
	  if ((j=fld[map[cd[n].y][cd[n].x].area].fexist) >= 0) {
		change_flag(n,j,FLAG_ON,1);
		load_switch(n,cd[n].x,cd[n].y,10);
	  }
	  set_fn(n);
	}
	  }
	}
	for (n = 1; n < enmax; n++) {
	  if (!cd[n].type) {
	if (cd[n].wt > 0) {
	  cd[n].wt--;
	  if (n < MAX_NTTYS && cd[n].wt <= 0) {
		cd[n].wt = 0;				 /* 連絡用ソケットのタイムアウト */
		connection_error_process(n, CONNECT_TIMEOUT);
	  }
	}
	continue;
	  }
	  for (i = 9; i > 0; i--)
	cd[n].com[i] = cd[n].com[i - 1];
	  cd[n].com[0] = 0;
	  if (cd[n].bx != cd[n].x || cd[n].by != cd[n].y) {
	if (cd[n].bx >= 0 && cd[n].by >= 0 &&
		cd[n].bx < MAX_X && cd[n].by < MAX_Y) {
	  if (map[cd[n].by][cd[n].bx].tip & T_SWITCH)
		load_switch(n, cd[n].bx, cd[n].by, 2);
	  /* add for area flag 20000629 */
	  if ((k = map[cd[n].by][cd[n].bx].area) != map[cd[n].y][cd[n].x].area &&
		  (j = fld[k].fout) >= 0) {
		change_flag(n,j,FLAG_ON,1);
		load_switch(n,cd[n].x,cd[n].y,10);
	  }
	}
	/* add for area flag 20000629 */
	if (cd[n].by < 0 || cd[n].bx < 0 ||
		 map[cd[n].by][cd[n].bx].area != map[cd[n].y][cd[n].x].area) {
	  if ((j = fld[map[cd[n].y][cd[n].x].area].fin) >= 0) {
		change_flag(n,j,FLAG_ON,1);
		load_switch(n,cd[n].x,cd[n].y,10);
	  }
	} else {
	  if ((j = fld[map[cd[n].y][cd[n].x].area].fmove) >= 0) {
		change_flag(n,j,FLAG_ON,1);
		load_switch(n,cd[n].x,cd[n].y,10);
	  }
	}
	cd[n].bx = cd[n].x;
	cd[n].by = cd[n].y;
	if (map[cd[n].y][cd[n].x].tip & T_SWITCH) {
	  load_switch(n, cd[n].x, cd[n].y, 1);
	}
	  }
	  if (cd[n].wt > 0) {
	cd[n].wt--;
	if (cd[n].wt <= 0)
	  switch (cd[n].st) {
	  case ST_CAST:
		cd[n].st = ST_CAST_COMPLETE;
		cd[n].parse = 0;
		input_analyze(n, "");
		break;
	  case ST_ATTACK:
		cd[n].st = ST_COMMAND;
		check_write(n, "#end-at\n");
		if(n>=MAX_NTTYS){
		  cd[n].wt +=
		frand(cd[n].sp / 2 + 1) + (cd[n].sp / 2) - TCN_HASTE(n);
		  if (cd[n].wt < 10)
		cd[n].wt = 10;
		}
		break;
	  case ST_DEFEND:
		cd[n].st = ST_COMMAND;
		check_write(n, "#end-gd\n");
		break;
	  }
	  }
	  if (cd[n].anmt > 0) /* add 20001122 */
		cd[n].anmt--;
	  /* bug fix 20020605 */
	  if (cd[n].st == ST_MSGW && cd[n].anmt == 0 && cd[n].anmf == 0) /* bug fix 20010221 20020417 */
		cd[n].st = ST_COMMAND;
	  if(cd[n].type && cd[n].st != ST_MENU && (cd[n].anmf || cd[n].st == ST_MSGW) && cd[n].anmt==0)
		load_mesg(n);
	  if (TCN_BERSERK(n) && cd[n].wt <= 0) /* PHI:0058 Bug Fix By EL */
	if(!( /* メッセージ中は hit しない */
			cd[n].st != ST_COMMAND /* 通常状態以外では hit しない */
		 || TCN_PALSY(n)		   /* 攻撃不可状態では hit しない */
		 || (fld[map[cd[n].y][cd[n].x].area].flag & FF_ATTACK)
		 || (is_need_charge_time   /* charged item の対応 */
		 && cd[n].weq!=nullptr
		 && (cd[n].weq->type & IT_MASK) == IT_CHARGE
		 && cd[n].weq->SE == SE_HITISUSE
		 && cd[n].weq->active_time < time(nullptr))))
	  {
		cd[n].parse = 0;
		CPYSTR(buf, "hit");
		input_analyze(n, buf);
	  }
	  if (CN_LAG(n) < 0)
	CN_LAG(n)--;

	  if (cd[n].m_wait && (++cd[n].m_wait_c) >= cd[n].m_wait)
	print_map(n,cd[n].modef & MD_CHAT);
	  if (cd[n].s_wait && (++cd[n].s_wait_c) >= cd[n].s_wait)
	print_status(n);
	}
	if(dsec % 100 == 0) cooldown_check();

	for (n = MAX_NTTYS; n < enmax; n++) {
	  if (cd[n].wt > 0)
	continue;
	  if (cd[n].type) {
	(void) auto_action(n);
	if(cd[n].st != ST_ATTACK && (cd[n].st & 0xf0) !=ST_SPELL){
	  cd[n].wt += frand(cd[n].sp / 2 + 1) + (cd[n].sp / 2) - TCN_HASTE(n);
	  if (cd[n].wt < 10)
		cd[n].wt = 10;
	}
	*(cd[n].str) = 0;
	  } else {
	load_ed(n);
	  }
	}
	for (n = 1; n < MAX_NTTYS; n++)
	  if (cd[n].fd != -1){
	if (cd[n].type) {
	  while(peek_protocol(n, buf, 240)) {/* protocol priority top 20010208 */
		func_analyze(n, buf);
	  }
	  if(!read_line(n, buf, 240))
	  	continue;
	  if (!*buf && cd[n].st != ST_ADDRESS_ && cd[n].st != ST_MENU) /* change for address no publish answer 20000901 */
		continue;
	  if (cd[n].st==ST_ATTACK || cd[n].st==ST_CAST || (cd[n].st!=ST_MSGW && cd[n].wt<=0))
		input_analyze(n, buf);
	  continue;
	} else {
	  if (!(i = read_line(n, buf, 240)))
	  	continue;
	  if (!*buf)
		continue;
	  if (!std::strncmp(buf, "#x", 2)) {
		std::snprintf(buf, sizeof(buf),"x-detect : %s ", cd[n].name);
		log_print(buf);
		close_connect(n);
		continue;
	  } else if (!std::strncmp(buf, "#open", 5) && CHKSP(buf+5) && buf[6] != '\0' && cd[n].st == ST_COMMAND) { /* check enhance 20000719 */
		p = buf + 6; /* bug fix for bad id 20020207 */
		loadstr(cd[n].uid, &p, ' ', 16);
		/* add for chrID ban 20030722 */
		if (id_banned(n)) {
		  std::snprintf(buf, sizeof(buf),"login refused: (%i:%i) = %s",
				  n, fd_check(n), cd[n].uid);
		  log_print(buf);
		  write_line(n, "#x\n", 3, 0);
		  nfflush(n, 1000);
		  nclose(n);
		  continue;
		}
		if (addf && !std::strcmp(cd[n].uid, "newuser")) {
		  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_VERSION);
		  check_write(n, buf);
		  check_write(n, "\n");
		  check_write(n, " Welcome to the world of Phantasmal Island(PHI).\n");
		  check_write(n, "\n");
		  check_write(n, "DM > First, you must create your account.\n");
		  check_write(n, "DM > Please enter your name (within 31 letters):\n");
		  cd[n].st = ST_USERNM;
		  /* snprintf(cd[n].uid, sizeof(cd[n].uid),"%s%05d", addw, s_id); */
		  /* fecrypt(cd[n].uid + strlen(cd[n].uid), 5); */
		  cd[n].cv = 0;
		  continue;
		}
		if (!(i = load_cd(n, 1))) {
		  dmcheck_write(n, DMM_NO_CHARACTER);
		  check_write(n, "#x\n");
		  std::snprintf(buf, sizeof(buf),"no-chr   : (%i:%i) ", n, cd[n].fd);
		  log_print(buf);
		  close_connect(n);
		  continue;
		}
		if (i < 0) {
		  if(!std::strcmp(cd[n].uid, "guest")){
		check_write(n, DMM_GUEST_USED);
		check_write(n, DMM_GUEST_TRYLATER);
		check_write(n, "#x\n");
		std::snprintf(buf, sizeof(buf),"w-access guest: (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		close_connect(n);
		continue;
		  } else {
		dmcheck_write(n, DMM_ACCESS_ALREADY);
		std::snprintf(buf, sizeof(buf),"w-access : (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		for(i = 1; i < MAX_NTTYS; i++)
		  if (i != n && cd[i].type && !std::strcmp(cd[n].uid, cd[i].uid))
			break;
		if (i < MAX_NTTYS) {
		  dmcheck_write(i, DMM_ACCESS_BY_OTHER);
		  dmcheck_write(i, DMM_CHANGE_CLIENT_OUT);
		  check_write(i, "#close\n");
		  std::snprintf(buf, sizeof(buf),"cli-chang: (%i:%i) -> (%i:%i) ", i, cd[i].fd, n, cd[n].fd);
		  log_print(buf);
		  std::snprintf(buf, sizeof(buf),"#name %s\n", cd[i].name); /* change for Rphi 20001018 */
		  check_write(n, buf);
		  if (IS_CGCHGED(i)) { /* キャラグラ変更 added by Qajin 19991224 */
			if(IS_CLIENT_RPHIWIN(n)) {				 /* phiwin/tphi 識別追加 added 20000101 */
			  std::snprintf(buf, sizeof(buf),"#name %s\n", CHRGRA(i));
			  check_write(n, buf);
			} else if(cd[i].cv >= 5085910)
			  check_write(n,"#name __\n");
		  }
		  fd = cd[n].fd;
		  sklbk = cd[n].skill; /* bug fix for skill clush 20021031 */
		  std::memcpy(&(cd[n]), &(cd[i]), sizeof(CHRData));
		  cd[i].skill = sklbk; /* bug fix for skill clush 20021031 */
		  cd[n].fd = fd;
		  cd[n].cv = 0;
		  cd[n].modef &= ~MD_M57; /* for client change from m57 */
		  cd[n].modef &= ~MD_M81;
		  cd[n].weq = nullptr;
		  cd[n].aeq = nullptr;
		  cd[n].oeq = nullptr;
		  if(cd[i].weq!=nullptr)
			for(j=0; j< MAX_ITEMS; j++)
			  if(cd[i].weq == &(cd[i].id[j])){
			cd[n].weq = &(cd[n].id[j]);
			break;
			  }
		  if(cd[i].aeq!=nullptr)
			for(j=0; j< MAX_ITEMS; j++)
			  if(cd[i].aeq == &(cd[i].id[j])){
			cd[n].aeq = &(cd[n].id[j]);
			break;
			  }
		  if(cd[i].oeq!=nullptr)
			for(j=0; j< MAX_ITEMS; j++)
			  if(cd[i].oeq == &(cd[i].id[j])){
			cd[n].oeq = &(cd[n].id[j]);
			break;
			  }
		  for(j=MAX_NTTYS; j<enmax; j++)
			if(cd[j].type != CT_NOEXIST){
			  if(cd[j].angry == i) cd[j].angry = n;
			  if(cd[j].trace == i) cd[j].trace = n;
			  if(cd[j].vf2	 == i) cd[j].vf2   = n; /* bug fix 20000621 */
			}
		  cd[i].auth = nullptr; /* bug fix by Qajin 19991208 */
		  cd[i].extension = nullptr; /* bug fix by Qajin 20010316 */
		  dmcheck_write(n, DMM_CHANGE_CLIENT_IN);
		  cd[i].type = CT_NOEXIST;
		  close_connect(i);
		  ex_notice(n,1); /* add for SD51 */
		  ex_notice(n,3);
		  set_chr(n); /* bug fix by Qajin 19991217 */
		  continue;
		} else {
		  check_write(n, DMM_CHANGE_CLIENT_FAIL);
		  check_write(n, "#x\n");
		  nfflush(n, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
		  close_connect(n);
		  continue;
		}
		  }
		}
		std::snprintf(buf, sizeof(buf),"#name %s\n", cd[n].name);
		check_write(n, buf);
		cd[n].type = CT_NORMAL;
		cd[n].cv = 0;
		ex_notice(n,1); /* add for SD51 */
		set_chr(n);
		std::snprintf(buf, sizeof(buf),"access   : %s (%i:%i) ", cd[n].name, n, cd[n].fd);
		log_print(buf);
		dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_VERSION);
		check_write(n, buf);
		
		if (dp_limitter == 1) /* デスペナリミット設定 added by NEO 2011/12/04 */
		dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY_LMT, penalty_rate); /* デスペナリミット付きの場合 */
		else
		dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY, penalty_rate); /* デスペナ設定表示　added by NEO 2008/04/01 */
		check_write(n, buf);

		if (counter_effect == 0) /* カウンター効果設定 added by NEO 2011/12/10 */
		dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_OFF); /* カウンター効果なしの場合 */
		else
		dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_ON); /* カウンター効果ありの場合 */
		check_write(n, buf);

		bulletin_board(n, "phi.bb");
		bulletin_board(n, "phi.inf");
		if (wizeyef)	wizard_light(n);
		else			wizard_eye_f(n,0);

		#ifdef ENPRN /* DM負荷表示 by NEO 2011/11/13 */
		  std::snprintf(buf, sizeof(buf)-1, 
		  " [DM LOAD: %i(%.2f) -> %i(%.2f) -> %i(%.2f)]\n", 
		  (int)server_lag_prev2, (long)server_lag_prev2 / 10.0, 
		  (int)server_lag_prev, (long)server_lag_prev / 10.0, 
		  (int)server_lag, (long)server_lag / 10.0);
		#else
		  std::snprintf(buf, sizeof(buf)-1, 
		  " [DM負荷: %i(%.2f) -> %i(%.2f) -> %i(%.2f)]\n", 
		  (int)server_lag_prev2, (long)server_lag_prev2 / 10.0, 
		  (int)server_lag_prev, (long)server_lag_prev / 10.0, 
		  (int)server_lag, (long)server_lag / 10.0);
		#endif
		  check_write(n, buf); /* ここまで */

		dmprintf(n, 0, buf, sizeof(buf),DMM_ACCESS);
		write_companion(n, 0, buf, access_dm);
		if (autoeqf) {
		  auto_equip(n);
		} else {
		  cd[n].weq = nullptr;
		  cd[n].aeq = nullptr;
		  cd[n].oeq = nullptr;
		}
		check_mail(n); /* Add for メールシステム:接続時メール件数チェック by NEO 2008/05/02 */
		
		CN_GASIFY(n) = 0x10000; /* add 20020908 */


/******************** 転送の仕組み	(サーバ１ -> サーバ２) ※追加改定 by Qajin
  (1)サーバ１がサーバ２に接続、バージョンを要求。
	 同時にキャラを ST_TRANSFER 状態、
	 サーバ２への接続用キャラを ST_CONNECTSERVER 状態に。
	 ここで接続用キャラの接続に、何らかのトラブルが生じると、
	 接続用キャラを閉じ、転送要求キャラを ST_TRANSFER 状態から解放する。
  (2)サーバ１が、バージョン確認後、
	クライアントへサーバ２への接続情報 #ch-srv を送り、
	サーバ２への接続用キャラを ST_CONNECTSERVER_ 状態に。
	(※追加:キャラをST_TRYRESERVE状態に)
  (3)#ch-srv を受取ったクライアントはサーバ２への接続を試みる
	成功: サーバ２へ #reserve を送信(キャラ情報の受取要求)
	失敗: サーバ１へ #no-srv を送信し、終了(転送失敗)
		  #no-srv を受け取ったサーバ１はキャラクタを
		  ST_TRANSFER 状態から解放し、
		  サーバ２との接続を #x を送信後切断。
  (4)#reserve を受け取ったサーバ２は、クライアントへ #rsv-ok を送信し、
	クライアントとの接続用キャラの上に転送されるキャラを構成するために
	初期化を行う(ST_RESERVE 状態)
	受取不可ならばクライアントへ #rsv-no を送信(実際には実装されていない)
  (5)#rsv-ok を受け取ったクライアントは
	サーバ１へ #trans を送信(サーバ２へのキャラ情報送信要求)
  (6)#trans を受け取ったサーバは
	#chr-D により情報をサーバ２に送信することを必要なだけ繰り返す。
	(※追加:接続用キャラをST_SENDCHR状態に)
  (7)#chr-D を受け取ったサーバ２は
	 クライアントとの接続用のキャラ上に情報を加えていく。
	 何らかの原因で、構成していたキャラが無くなったり
	 キャラ構成エラーやシフトゲートにより転送が失敗した場合、
	 サーバ１に #rcv-no を送信し、サーバ１との接続を切る。
		(a)#rcv-no を受け取ったサーバ１はクライアントに #trs-no を送信し、
		  サーバ２との接続を切る。(※追加:キャラをST_TRANSFAILE状態に)
		(b)#trs-no を受け取ったクライアントはサーバ１へ #ch-srv-no を送信し、
		  サーバ２への接続を切る(一方的に切るらしい(-.-))
		(c)#ch-srv-no を受け取ったサーバ１は
		  キャラが (CT_NOEXIST である場合のみ) ST_TRANSFER 状態から解放する
	 キャラ構成が完成した場合は、サーバ１に #rcv-ok を送信し、
	 キャラクタセーブを行い、サーバ１との接続を切る。(※追加:キャラをST_RECEIVECOMPLETE状態に)
		(a)#rcv-ok を受け取ったサーバ１は
		  クライアントに #trs-ok を送信し、
		  キャラのバックアップとセーブを行って
		  クライアントとの接続とサーバ２との接続を切る。
		(b)#trs-ok を受け取ったクライアントは
		  #ch-srv-ok をサーバ２に送信し、サーバ１との接続を切る。
		(c)#ch-srv-ok を受け取ったサーバ２は、
		  データ構築したキャラを ST_RESERVE から解放して転送完成。
*****************/


	} else if (!std::strncmp(buf, "#version-srv ", 13)
		 && cd[n].st == ST_CONNECTSERVER){
	  /* for gate auto delete added by Qajin 20000523 */
	  /* 接続成功時は時間をリセット */
	  if (gatedata[i=cd[n].vf2] != nullptr && gatedata[i][0].di == 'G') /* bug fix 20000803 fix2 20031007 */
		  gatedata[i][7].dl = 0;
	  /* バージョンチェックのために必要 */
	  if(std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
		check_write(n, "#x\n"); /* クライアントが落ち */
		close_connect(n);
	  } else if(!server_version_check(i, std::atoi(buf+13), errmsg.data(), errmsg.size())){
		cd[i].type = CT_NORMAL; /* バージョンチェックでだめ */
		cd[i].st = ST_COMMAND;
		cd[i].wt = 0;
		cd[i].ix = cd[i].x;
		cd[i].iy = cd[i].y;
		set_chr(i);
		print_map(i, 0);
		check_write(i, errmsg.data());
		check_write(n, "#x\n");
		close_connect(n);
	  } else {
		/* for character graphic added by Qajin 19991224 */
		/* バージョンチェック追加 changed by Qajin 19991228 */
		if(std::atoi(buf+13) < 5098003 && cd[i].cv >= 5085910 && IS_CGCHGED(i)) {
			std::snprintf(buf,sizeof(buf),"#name %s\n",CHRGRA(i));
		check_write(i,buf);
		}
		cd[n].wt = cd[i].wt; /* 残り時間を延長 */
		check_write(i, cd[n].str);
		/* load_gate()で仕組んで置いたやつ */
		/* クライアントに #ch-srv を送信 */
		cd[n].st = ST_CONNECTSERVER_;
	  }
	} else if (cd[n].st == ST_COMMAND && !std::strncmp(buf, "#reserve", 8) && CHKSP(buf+8) && buf[9] != '\0') { /* check enhance 20000719 */
		p = buf + 9;  /* bug fix for bad id 20020207 */
		loadstr(cd[n].uid, &p, ' ', 16);
		cd[n].name = findstr("????");
		/* add for chrID ban 20030722 */
		if (id_banned(n)) {
		  std::snprintf(buf, sizeof(buf),"reserve refused: (%i:%i) = %s",
				  n, fd_check(n), cd[n].uid);
		  log_print(buf);
		  write_line(n, "#rsv-no\n#x\n", 3, 0);
		  nfflush(n, 1000);
		  nclose(n);
		  continue;
		}
	  /* クライアントより他サーバからの転送を要求された */
	  /* cd[n] は接続成功後、要求されたキャラクタに変化する */
	  /**** 97/Jun/9 b-Yak-38 ADD ****/
	  for (i = 1; i < MAX_NTTYS; i++) {
		if (cd[i].st == ST_RESERVE
	  && (int) std::strlen(cd[i].uid) > 0
	  && !std::strncmp(cd[i].uid, buf + 9, std::strlen(cd[i].uid)))
	break;
	  }
	  if (i < MAX_NTTYS) {
		/* すでに同じキャラクタが、他のソケットから転送受理を要求し、*/
		/* そのソケットに対してこちら側は #rsv-ok を出している */
		/* 転送要求受理の再確認 */
		check_write(n, "#rsv-ok\n");
		close_connect(n);
		cd[i].wt = 600; /* 時間延長 */
	  } else {
		/*******************************/
		cd[n].cg = nullptr /*findstr("unknown")*/;
		cd[n].st = ST_RESERVE;
		cd[n].wt = 600;
		check_write(n, "#rsv-ok\n");
		std::snprintf(buf, sizeof(buf),"reserve  : (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		for (i = 0; i < MAX_TALKS; i++){
	cd[n].td[i].type = TT_NONE;
	cd[n].td[i].string = nullptr;
		}
		for (i = 0; i < MAX_MAGICS; i++){
	cd[n].md[i].spell = nullptr;
	cd[n].md[i].effect = EF_NONE;
	cd[n].md[i].fn = FN_NONE;
	cd[n].md[i].is_inherence = 0;
	cd[n].md[i].extension = nullptr; /* bug fix 20000320 */
		}
		for (i = 0; i < MAX_ITEMS; i++)
	delete_item(&(cd[n].id[i]));
	  }
	} else if (cd[n].st == ST_TRYRESERVE && !std::strcmp(buf, "#no-srv")) { /* check enhance for bug fix 20000710 */
	  /* クライアントより転送先サーバへの接続が失敗したとの報告 */
	  cd[n].type = CT_NORMAL;
	  cd[n].st = ST_COMMAND;
	  cd[n].wt = 0;
	  cd[n].ix = cd[n].x;
	  cd[n].iy = cd[n].y;
	  if (IS_CGCHGED(n)) { /* bug fix by Qajin 19991228 */
		 if (cd[n].cv >= 5090152) { 			/* phiwin/tphi 識別追加  added 20000101 */
		   std::snprintf(buf,sizeof(buf),"#name %s\n",CHRGRA(n));
		   check_write(n,buf);
		 } else if (cd[n].cv >= 5085910)
		   check_write(n,"#name __\n");
	  }
	  dmcheck_write(n, DMM_TRANS_NO_MISS_DEST);
	  if(!std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
		check_write(i, "#x\n");
		close_connect(i);
	  }
	} else if (cd[n].st == ST_TRYRESERVE && !std::strncmp(buf, "#trans", 6)) { /* check enhance for bug fix 20000710 */
	  /* クライアントより転送先サーバへのキャラクタ情報転送要求 */
	  if(std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
		check_write(n, "#trs-no\n");  /* 転送ソケット落ち */
		std::snprintf(buf, sizeof(buf),"trs-err  : %s -> %s ",cd[n].name, cd[n].str);
		log_print(buf);
		cd[n].type = CT_NORMAL;
		cd[n].st = ST_COMMAND;
		cd[n].wt = 0;
		cd[n].ix = cd[n].x;
		cd[n].iy = cd[n].y;
		dmcheck_write(n, DMM_TRANS_NO_MISS_DEST);
	  } else {
		std::snprintf(buf, sizeof(buf),"chr-trs  : (%i:%i) %s ", i,cd[i].fd,cd[n].name);
		log_print(buf);
		p = cd[i].str + 8;
		loadstr(nm.data(), &p, ' ', 32); /* ip */
		p = select_password(cd[n].str, nm.data());
		cd[n].st = ST_TRANSFER; /* add for checking abnomal process sequence 20000725 */
		cd[i].st = ST_SENDCHR;
		put_cd(n, i, p); /* #rcv-ok 待ちへ */
	  }
	} else if (!std::strncmp(buf, "#chr-D", 6)) {
	  /* 他サーバからキャラクターデータ転送 */
	  for (i = 1; i < MAX_NTTYS; i++)
		if (cd[i].st == ST_RESERVE
	  && (int) std::strlen(cd[i].uid) > 0
	  && !std::strncmp(cd[i].uid, buf + 7, std::strlen(cd[i].uid)))
	break;
	  if (i >= MAX_NTTYS) {
		/* 転送要求してたクライアントが見当たらない */
		log_print(buf);
		check_write(n, "#rcv-no\n");
		std::snprintf(buf, sizeof(buf),"rcv-err  : (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		close_connect(n);
		continue;
	  }
	  switch (get_cd(i, buf + 23, cd[n].str)) { /* キャラクター構築 */
	  case -3:
		log_print(buf);
		check_write(n, "#rcv-no\n");
		std::snprintf(buf, sizeof(buf),"refuse master in : (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		close_connect(n);
		continue;
	  case -2:	  /* shift_gate で蹴られた */
	  case -1:			/* エラー発生 */
		log_print(buf);
		check_write(n, "#rcv-no\n");
		std::snprintf(buf, sizeof(buf),"rcv-err  : (%i:%i) ", n, cd[n].fd);
		log_print(buf);
		close_connect(n);
	  case 1:
		continue;	   /* キャラクター構築途中 */
	  } 			   /* キャラクター構築完成 */
	  cd[i].wt = 600; /* 列車事故対策 20000718 */
	  check_write(n, "#rcv-ok\n");
	  while (!save_cd(i, 1));
	  std::snprintf(buf, sizeof(buf),"chr-rcv  : (%i:%i) ", n, cd[n].fd);
	  log_print(buf);
	  close_connect(n);
	} else if (cd[n].st == ST_SENDCHR && !std::strcmp(buf, "#rcv-no")){ /* check enhance 20000719 */
	  /* 転送先のサーバより、受け取り失敗連絡 */
	  if(std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
		close_connect(n); /* クライアント落ち */
	  } else {
		cd[i].st = ST_TRANSFAILE; /* add for checking abnomal process sequence 20000725 */
		check_write(i, "#trs-no\n");
		std::snprintf(buf, sizeof(buf),"trs-err  : %s -> %s ",cd[i].name, cd[i].str);
		log_print(buf);
		/* 転送用ソケットを閉じる */
		close_connect(n);
		/* クライアント側のソケットは、閉じない(#ch-srv-no 待ち) */
	  }
	} else if (cd[n].st == ST_SENDCHR && !std::strcmp(buf, "#rcv-ok")) { /* check enhance 20000719 */
	  while (!save_cd(n, 0)); /* vs Tphi dup 20010627 */
	  /* 転送先のサーバより、転送受渡し成功連絡 */
	  if(std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
		close_connect(n); /* クライアント落ち。今更… */
	  } else {
		check_write(i, "#trs-ok\n");
		close_connect(i);
		std::snprintf(buf, sizeof(buf),"out      : %s -> %s ",cd[i].name, cd[i].str);
		log_print(buf);
		dmprintf(i, 0, buf, sizeof(buf),DMM_OUT
		  , cd[i].str);
		write_companion(i, 0, buf, access_dm);
		close_connect(n);
	  }
	} else if (cd[n].st == ST_TRANSFAILE && !std::strcmp(buf, "#ch-srv-no")) { /* check enhance for bug fix 20000710 */
	  cd[n].type = CT_NORMAL;
	  cd[n].st = ST_COMMAND;
	  cd[n].wt = 0;
	  cd[n].ix = cd[n].x;
	  cd[n].iy = cd[n].y;
	  set_chr(n);
	  if (IS_CGCHGED(n)) { /* bug fix by Qajin 19991228 */
		 if (cd[n].cv >= 5090152) { 			/* phiwin/tphi 識別追加  added 20000101 */
		   std::snprintf(buf, sizeof(buf),"#name %s\n",CHRGRA(n));
		   check_write(n,buf);
		 } else if (cd[n].cv >= 5085910)
		   check_write(n,"#name __\n");
	  }
	  print_map(n, 0);
	  dmcheck_write(n, DMM_TRANS_FAIL);
	} else if (cd[n].st == ST_RECEIVECOMPLETE && !std::strcmp(buf, "#ch-srv-ok")) { /* check enhance for bug fix 20000710 */
	  std::snprintf(buf, sizeof(buf), "in       : %s from %s"
		,cd[n].name, cd[n].str);
	  log_print(buf);
	  cd[n].type = CT_NORMAL; /* change sequence for dmprintf 20021226 */
	  cd[n].st = ST_COMMAND;
	  cd[n].wt = 0;
	  dmprintf(n, 0, buf, sizeof(buf),DMM_COMEIN);
	  write_companion(n, 0, buf, access_dm);
	  ex_notice(n,1); /* add for SD51 */
	  set_chr(n);
	  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_VERSION);
	  check_write(n, buf);

	  if (dp_limitter == 1) /* デスペナリミット設定 added by NEO 2011/12/04 */
	  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY_LMT, penalty_rate); /* デスペナリミット付きの場合 */
	  else
	  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY, penalty_rate); /* デスペナ設定表示　added by NEO 2008/04/01 */
	  check_write(n, buf);

	  if (counter_effect == 0) /* カウンター効果設定 added by NEO 2011/12/10 */
	  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_OFF); /* カウンター効果なしの場合 */
	  else
	  dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_ON); /* カウンター効果ありの場合 */
	  check_write(n, buf);

	  bulletin_board(n, "phi.inf");
	  if (wizeyef)	  wizard_light(n);
	  else			  wizard_eye_f(n,0);
	  
		#ifdef ENPRN /* DM負荷表示 by NEO 2011/11/13 */
		  std::snprintf(buf, sizeof(buf)-1, 
		  " [DM LOAD: %i(%.2f) -> %i(%.2f) -> %i(%.2f)]\n", 
		  (int)server_lag_prev2, (long)server_lag_prev2 / 10.0, 
		  (int)server_lag_prev, (long)server_lag_prev / 10.0, 
		  (int)server_lag, (long)server_lag / 10.0);
		#else
		  std::snprintf(buf, sizeof(buf)-1, 
		  " [DM負荷: %i(%.2f) -> %i(%.2f) -> %i(%.2f)]\n", 
		  (int)server_lag_prev2, (long)server_lag_prev2 / 10.0, 
		  (int)server_lag_prev, (long)server_lag_prev / 10.0, 
		  (int)server_lag, (long)server_lag / 10.0);
		#endif
		  check_write(n, buf); /* ここまで */

	  print_map(n, 0);
	  dmcheck_write(n, DMM_TRANS_COMP);
	  if (autoeqf) {
		auto_equip(n);
	  } else {
		cd[n].weq = nullptr;
		cd[n].aeq = nullptr;
		cd[n].oeq = nullptr;
	  }
	  importcir(n);
	  load_sav(n); /* add for saved flags 20010622 */
	  /* exp max以外のmasterキャラ可能に 20010226 */
	  switch (load_authority(n)) {
		case 1:
		  dmcheck_write(n, DMM_MASTER_ACCESS_ERR1);
		  break;
		case 2:
		  dmcheck_write(n, DMM_MASTER_ACCESS_ERR2);
		  break;
		case 0:
		  break;
	  }
	  CN_GASIFY(n) = 0x10000; /* add 20020908 */
	} else if (!std::strncmp(buf, "#gate", 5)) {
	  if (get_gate(buf + 5, 0, n)) {
		CPYSTR(cd[n].str, buf + 22);
		std::snprintf(buf, sizeof(buf), "bad gate : %s ", cd[n].str);
		log_print(buf);
		close_connect(n);
		cd[n].str[31]='\0';
		dmprintf(n, 0, buf, sizeof(buf),DMM_ILLEGAL_GATE
		  ,cd[n].str);
		write_companion(0, 0, buf, 1);
	  } else {
		CPYSTR(cd[n].str, buf + 22);
		std::snprintf(buf, sizeof(buf), "gate     : %s ", cd[n].str);
		log_print(buf);
		close_connect(n);
		cd[n].str[31]='\0';
		dmprintf(n, 0, buf, sizeof(buf),DMM_GATE_NEW
		  ,cd[n].str);
		write_companion(0, 0, buf, 1);
	  }
	} else if (!std::strncmp(buf, "#g-close", 8)) {
	  get_gate(buf + 8, 1, n);
	  CPYSTR(cd[n].str, buf + 25);
	  std::snprintf(buf, sizeof(buf), "g-close  : %s ", cd[n].str);
	  log_print(buf);
	  close_connect(n);
	  cd[n].str[31]='\0';
	  dmprintf(n, 0, buf, sizeof(buf),DMM_GATE_CLOSE, cd[n].str);
	  write_companion(0, 0, buf, 1);
	} else if (!std::strncmp(buf, "#shutdown", 9)) {
	  nm[0] = '\0';
	  p = buf + 9;
	  SKIP_SPACE(p);
	  loadstr(cd[n].str, &p, ' ', 32);
	  SKIP_SPACE(p);
	  if (!std::strncmp(p, "P:", 2)) {
		loadstr(nm.data(), &p, '\n', 32);
	  }
	  if (!authorize(nm.data())) {
		std::snprintf(buf, sizeof(buf), "bad shut : %s", ip_domain_check(n));
		log_print(buf);
		/* psp5 fix ここから */
		dmprintf(n, 0, buf, sizeof(buf),DMM_ILLEGAL_SHUTDOWN,cd[n].str);
		write_companion(0, 0, buf, 1);
		close_connect(n);
		/* psp5 fix ここまで(shutdown失敗時のSEGV防止) */
	  } else {
		close_connect(n);
		signal_trap(0xff);
	  }
	} else if (!std::strcmp(buf, "#code-sjis")) {
	  check_write(n, "#code-sjis-ok\n");
	} else if (!std::strncmp(buf, "#version-cli ", 13)) {
	  /* for character change added by Qajin 19991228 */
	  i = std::atoi(buf + 13);
	  if (i != 0 && cd[n].cv == 0 && IS_CGCHGED(n) /* bug fix 20000321 fix2 0919 */
		  ) {
		if (i >= 5090152) {
		  std::snprintf(buf, sizeof(buf),"#name %s\n",CHRGRA(n));  /* phiwin/Tphi 識別追加 added by Qajin 20000101 */
		  check_write(n,buf);
		} else if (i >= 5085910)
		  check_write(n,"#name __\n");
	  }
	  cd[n].cv = i;
	  if (!i)
		check_write(n,"#code-sjis\n"); /* for old euc server ... 20010527 */
	  ex_version_dm(n);
   	  std::snprintf(buf, sizeof(buf), "#version-srv %s\n", PHI_PROT_VERS);
	  check_write(n, buf);
	} else if (!std::strncmp(buf,"#ex-register", 12)) {
		if (ex_register(n, buf+12)) {
			ex_notice(n,1); /* add for SD51 */
			dmprintf(n, 0, buf, sizeof(buf),DMM_ACCESS_NEW);
			write_companion(n, 0, buf, 1);
			dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_VERSION);
			check_write(n, buf);

			if (dp_limitter == 1) /* デスペナリミット設定 added by NEO 2011/12/04 */
			dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY_LMT, penalty_rate); /* デスペナリミット付きの場合 */
			else
			dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_PENARTY, penalty_rate); /* デスペナ設定表示　added by NEO 2008/04/01 */
			check_write(n, buf);

			if (counter_effect == 0) /* カウンター効果設定 added by NEO 2011/12/10 */
			dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_OFF); /* カウンター効果なしの場合 */
			else
			dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_COUNTER_ON); /* カウンター効果ありの場合 */
			check_write(n, buf);

			bulletin_board(n, "phi.bb");
			bulletin_board(n, "phi.inf");
			if (wizeyef) {
			  wizard_light(n);
			} else {
			  wizard_eye_f(n,0);
			}
			print_map(n, 0);
			auto_equip(n);
			CN_GASIFY(n) = 0x10000; /* add 20020908 */
			cd[n].st = ST_COMMAND;
		}
	} else if (!std::strncmp(buf,"#ex-get", 7)) {
		ex_get(n, buf+7);
	} else if (buf[0] != '#') {
	  if (cd[n].st == ST_USERID || cd[n].st == ST_USERNM ||
	cd[n].st == ST_USERAD || cd[n].st == ST_USERCF ||
	cd[n].st == ST_USERGRP) {
		adduser(n, buf);
	  }
	  if (cd[n].st == ST_USERREGISTER) {
		ex_adduser_v52(n, buf);
	  }
	}
  }
	}
  }
}

void   signal_trap(int sig)
{
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char	 buf[256];
#ifdef __CYGWIN32__
  char	 buf2[256];
#endif
  int	 i;

  /* sequence change against re-signal 20010602 */
  signal(SIGTERM, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGFPE, SIG_DFL);
#ifndef __MINGW32__
  signal(SIGBUS, SIG_DFL);
  signal(SIGPIPE, SIG_DFL);
#endif

  switch (sig) {
  default:
  case 0xff:
	log_print("shutdown : God Master");
	dmprintf(0, 0, buf, sizeof(buf),DMM_SHUT_VERUP);
	break;
  case SIGTERM:
	log_print("SIGTERM  : God Master");
	dmprintf(0, 0, buf, sizeof(buf),DMM_SHUT_SIGTERM);
	break;
  case SIGFPE:
	log_print("SIGFPE   : Outer God");
	dmprintf(0, 0, buf, sizeof(buf),DMM_SHUT_SIGFPE);
	break;
#ifndef __MINGW32__
  case SIGBUS:
	log_print("SIGBUS   : Outer God");
	dmprintf(0, 0, buf, sizeof(buf),DMM_SHUT_SIGBUS);
	break;
#endif
  case SIGSEGV:
	log_print("SIGSEGV  : Outer God");
	dmprintf(0, 0, buf, sizeof(buf),DMM_SHUT_SIGSEGV);
	break;
  }

  /* ファイルアクセス中のエラー対処 added by Qajin 20000205 */
  if (lcnt >= 0) {
	log_print("data files are back up...");
#ifndef __CYGWIN32__
	CPYSTR(buf, "cp phi.bd phi.bd.bak phi.cd phi.cd.bak phi.dd phi.id phi.od phi.sd phi.sd.bak phi.td phi.td.bak backup/");
	system(buf);
#else
	CPYSTR(buf, "phi.bd phi.bd.bak phi.cd phi.cd.bak phi.dd phi.id phi.od phi.sd phi.sd.bak phi.td phi.td.bak backup/");
	CPYSTR(buf2, "backup/");
	spawnlp(_P_WAIT, "cp", "cp", buf, buf2, 0);
#endif
  }


#ifdef DEBUG
  log_print("all creature's gold store to tax.");
#endif
  for (i = MAX_NTTYS; i < enmax; i++)
	if (cd[i].type)
	  tax += cd[i].gp;
#ifdef DEBUG
  log_print("close all client.");
#endif
  for (i = 1; i < MAX_NTTYS; i++) {
	if (cd[i].type) {
	  check_write(i, buf);
	  dmcheck_write(i, DMM_SAVEDATA);
	  check_write(i, "#close\n");
	  cd[i].x = cd[i].ix;
	  cd[i].y = cd[i].iy;
	  nfflush(i, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
	  close_connect(i);
	}
  }
#ifdef DEBUG
  log_print("save_sd");
#endif
  save_sd(0, 0, 0, 'G', nullptr, 0);
#ifdef DEBUG
  log_print("save_dd");
#endif
  save_dd();
#ifdef DEBUG
  log_print("savef_id");
#endif
  savef_id();
#if MAX_OBJECTS > 0
#ifdef DEBUG
  log_print("save_obj");
#endif
  save_obj();
#endif
#ifdef DEBUG
  log_print("put_gate(1)");
#endif
  put_gate(1); /* 他サーバ接続に時間がかかり緊急度も低いので最後 */
#ifdef DEBUG
  log_print("close listened socket.");
#endif
  close_server();
  connect_check(5000);
  log_print("end      : Dungeon Master");

  exit(0);
}

void   log_print(char *mes)
{
  FILE	*fp;
#ifdef SPLITLOG
  char tstr[TIMESTRMAX];
#endif

  abstime = time(nullptr);
  sttime = (struct tm *) localtime(&abstime);

#ifdef SPLITLOG
  strftime(tstr, TIMESTRMAX, "phi_logs/%Y%m.log", sttime);
  if ((fp = fopen(tstr, "ab")) == nullptr)
#else
  if ((fp = fopen("phi.log", "ab")) == nullptr)
#endif
	return;

  fprintf(fp, "[%-24.24s] %s \n", asctime(sttime), mes);
  fclose(fp);
  if (log_send)
  	check_write(log_send,mes);
  return;
}

int init_connected_cd(int n)
{
	init_cd(n); /* add 20030726 */
	cd[n].fd = fd_check(n);
	cd[n].wt = 600;
	cd[n].type = CT_NOEXIST;
	cd[n].st = ST_COMMAND;
	reflesh_last_map[n] = reflesh_last_status[n] = 1;
	CPYSTR(cd[n].str, ip_address_check(n));
	cd[n].extension = nullptr;
	cd[n].party = 0; /* add 20020822 */
	cd[n].pflag = 0; /* add 20030726 */
	return(0);
}

void   close_connect(int n)
{
  int	 i, j;

  if (n == log_send)
  	log_send = 0;
  for(i=MAX_NTTYS; i<enmax; i++)
	if(cd[i].type!=CT_NOEXIST){
	  if(cd[i].angry == n){
	cd[i].angry = -1;
#ifdef DEBUG_ANGRY
	std::snprintf(debug_buf, sizeof(debug_buf), "lost angry: %s(%d) ", cd[i].name,i);
	log_print(debug_buf);
#endif
	  }
	  if(cd[i].trace == n){
	cd[i].trace = -1;
	if(cd[i].think == TH_TALK
	   || cd[i].think == TH_TRACE
	   || cd[i].think == TH_WAIT)
	  cd[i].think = TH_NONE;
	  }
/* add for controlable sammon 20000614 */
	  if(cd[i].vf2 == n){
	cd[i].vf2 = -1; /* bug fix 20000727 */
#ifdef DEBUG_ANGRY_SAMMON
	std::snprintf(debug_buf, sizeof(debug_buf), "lost master: %s(%d) ", cd[i].name,i);
	log_print(debug_buf);
#endif
	  }
	}
  if (n < MAX_NTTYS) {
	cd[n].wt = 0;
	if (cd[n].fd != -1) nfclose(n);
	if (cd[n].type){
	  if(cd[n].exp == 99999999){
	tax += cd[n].gp;
	cd[n].gp = 0;
	  }
	  if (cd[n].auth != nullptr) { /* moved from phi_data.c 20000719 */
		if (cd[n].auth->territory != nullptr)
		  free(cd[n].auth->territory);
		free(cd[n].auth);
	  }
	  cd[n].auth = nullptr;
	  while (!save_cd(n, 1));
	  while (!save_sav(n)); /* for saved flags 20010622 */
	  skill_clear(n); /* for skills 20020307 */
	}
	erase_chrex(cd[n].extension); /* add for Chr Extension data 20000929 */
	cd[n].extension = nullptr;
	i = cd[n].party;
	cd[n].party = 0;
	renumber_party(i);
  } else {
	for (i = 0; i < MAX_ITEMS; i++) {
	  if (cd[n].id[i].type != DT_NONE
	  && (cd[n].id[i].type & MT_MASK) != MT_NOBREAK)
	save_id(n, i, 1);
	  delete_item(&(cd[n].id[i]));
	}
	i = cd[n].et;
	if (i >= 0) {
	ed_o[i].nr--;
	cd[n].wt = 0;
	if (ed_o[i].apc == 1 || ed_o[i].apc == 0) {
	  if (ed_o[i].nr <= ed_o[i].anm) {
		cd[n].type = CT_NOEXIST;
		for (j = MAX_NTTYS; j < enmax; j++) {
		  if (cd[j].et == i) {
		if (cd[j].type == CT_NOEXIST && cd[j].ec == 0) {
		  cd[j].wt = fsps(80, ed_o[i].apt, 990000);
		  cd[j].ec = 1;
		}
		  }
		}
	  }
	} else if (ed_o[i].apc == 4) { /* add for threatened species 20020520 */
		cd[n].type = CT_NOEXIST;
		j = time(nullptr) - cd[n].last;
	if (j < 1) {
		  cd[n].wt = fsps(80, ed_o[i].apt * ed_o[i].threaten * 2, 990000);
		} else if (j < ed_o[i].threaten) {
		  cd[n].wt = fsps(80, ed_o[i].apt * ed_o[i].threaten / j, 990000);
		} else {
		  cd[n].wt = fsps(80, ed_o[i].apt, 990000);
		}
		cd[n].ec = 1;
	}

    /* ↓スイッチデータ追加 2005/11/26 Savarl.F */
    load_switch(n,cd[n].x,cd[n].y,24);  /* キャラクタ死亡時 */
    /* ↑ADD */

    /* flag on when NPC disappeared added by Qajin 20000522 */
    if (ed_o[i].nr <= ed_o[i].fdn) {
      change_flag(n,ed_o[i].fdead,FLAG_ON,1);
      load_switch(n,cd[n].x,cd[n].y,10);
    }

	}
  }
  cd[n].fd = -1;
  cd[n].uid[0] = 0;
  cd[n].type = CT_NOEXIST;
  cd[n].st = ST_COMMAND;
  clear_chr(n);
  return;
  /* uid 以外の文字列はこの手続きの直後にのみ使うことを許すために残す */
}

void   check_write(int n, char *mes)
{
  int	 i;
  /* buf は CPYSTR の宛先のため C 配列のまま */
  char	 buf[MAX_MESSAGE_SIZE];

  if (n < MAX_NTTYS) {
	if (cd[n].fd == -1) return;
	if ((cd[n].st == ST_TRANSFER
	 || cd[n].st == ST_CONNECTSERVER
	 || cd[n].st == ST_CONNECTSERVER_
	 || cd[n].st == ST_SENDCHR
	 || cd[n].st == ST_TRYRESERVE
	 || cd[n].st == ST_TRANSFAILE) && *mes != '#') return;
#ifdef DEBUG_SEND_MSG
	std::snprintf(debug_buf_msg,sizeof(debug_buf_msg),"send following message to cd[%d]",n);
	log_print(debug_buf_msg);
	log_print(mes);
#endif
	/* added for Tphi suport by Qajin 20000228 */
	if (!IS_CLIENT_RPHIWIN(n) && !std::strcmp(mes,"#s-edit\n"))
	cd[n].parse = 1;
	CPYSTR(buf, mes);
	buf[MAX_MESSAGE_SIZE-2]='\0';
	/* mes が空だと strlen が 0 になり buf[-1] を読むため、空で
	   ないことを先に確かめる（2026-11-16 修正）。 */
	if(buf[0] != '\0' && buf[(i=std::strlen(buf)-1)]!='\n'){
	  buf[i+1]='\n';
	  buf[i+2]='\0';
	}
	  write_line(n, buf, std::strlen(buf), 0);
  }
  if(cd[n].exp == 99999999) nfflush(n, 1000);
  return;
}

void   write_companion(int n, int m, char *mes, int f)
/* 1) f == 1 の時は全てのクリーチャに話す。 							  */
/* 2) f == 2 の時は同一エリアのクリーチャに話す。(20010614追加) 		  */
/* 3) f == 3 の時はパーティーに話す。(20020823追加) 					  */
/* 4) f == 0 の時、n, m および、n の視界内のクリーチャにメッセージが届く。*/
/*	  (m == 0 は指定先なし。)											  */
/*	  NPC の知的生物の耳に入ると(キャスティング中以外は)				  */
/*	  情報がcd[].str に入る。											  */
{
  int	 x, y;
  int	 i, j;

  if (f==1) {
	for (i = 1; i < enmax; i++)
	  if (cd[i].type)
		check_write(i, mes);
  } else if (f==2) {
	j = map[cd[n].y][cd[n].x].area;
	for (i = 1; i < enmax; i++)
	  if (cd[i].type && j == map[cd[i].y][cd[i].x].area)
		check_write(i, mes);
  } else if (f==3) {
	j = cd[n].party;
	/* bug fix for no-joined 20030531 */
	if (!j)
	  dmcheck_write(n,DMM_PART_NO);
	else
	  for (i = 1; i < MAX_NTTYS; i++)
		if (cd[i].type && j == cd[i].party)
		  check_write(i, mes);
  } else {
	if(n<=0) return; /* 無意味 */
	check_write(n, mes);
	if (!(m<=0) && n != m)
	  check_write(m, mes);
	set_view(n);
	for (y = 0; y < 5; y++)
	  for (x = 0; x < 5; x++) {
  if (cd[n].view[y][x].tip == T_UNKNOWN)
	continue;
  for (i = 0; i < MAX_CHRS_INONE; i++) {
	if ((j = cd[n].view[y][x].chr[i] - 1) < 0)
	  continue;
	if (j != n && j != m) {
	  if (j < MAX_NTTYS)
		check_write(j, mes);
	  else if (cd[j].type == CT_NORMAL
		 && cd[j].st != ST_CAST
		 && std::strncmp(mes,DMM_DM,std::strlen(DMM_DM))) {
		cd[j].target = n;
		CPYSTR(cd[j].str, mes);
	  }
	}
  }
	  }
  }
  return;
}

void   set_view(int n)
{
  int	 x, y, d;
  int	 xx, yy;
  int	 blind;
  int	 i;

  d = cd[n].d;
  blind = TCN_BLIND(n);
  for (y = 3; y >= -1; y--)
	for (x = -2; x <= 2; x++) {
	  xx = cd[n].x + px[d] * y - py[d] * x;
	  yy = cd[n].y + py[d] * y + px[d] * x;
/*
 *	 y^--+			+------>xx
 *	  |  |			|  +-+		d= 0	1	 2	  3
 *	  |  |	  ==>	|  +-+		   up right down left
 *	  +-->x 	  yy|
 *	 (view) 		v  (map)
 *
 */

	  if (xx < 0 || xx >= MAX_X || yy < 0 || yy >= MAX_Y) {
		UNSEE(n,(y+1),(x+2));
  continue;
	  }
	  if (blind < 0 && (x != 0 || y != 0)) {
  cd[n].view[y + 1][x + 2].tip = (map[yy][xx].tip & TCLEAR) | T_MIST;
	} else if (blind > 0 && (map[yy][xx].tip & TMASK) == T_MIST) {
  cd[n].view[y + 1][x + 2].tip = (map[yy][xx].tip & TCLEAR) | T_SPACE;
	} else {
  cd[n].view[y + 1][x + 2].tip = map[yy][xx].tip;
	}
  cd[n].view[y + 1][x + 2].surface = map[yy][xx].surface;
  for (i = 0; i < MAX_CHRS_INONE; i++)
	cd[n].view[y + 1][x + 2].chr[i] = map[yy][xx].chr[i];

	/* 2x2 の大きな敵は基準マスのセルだけに残す (他の体のマスからは
	   消す)。地図送信で同じ敵が4回出るのを防ぐため。 */
	for (i = 0; i < MAX_CHRS_INONE; i++) {
	  int cj = cd[n].view[y + 1][x + 2].chr[i] - 1;
	  if (cj >= 0 && chr_big(cj) && (cd[cj].x != xx || cd[cj].y != yy))
	    cd[n].view[y + 1][x + 2].chr[i] = 0;
	}

	}

  if (cd[n].view[2][2].tip & T_MIST) {
		UNSEE(n,3,1);
		UNSEE(n,3,2);
		UNSEE(n,3,3);
		UNSEE(n,4,0);
		UNSEE(n,4,1);
		UNSEE(n,4,2);
		UNSEE(n,4,3);
		UNSEE(n,4,4);

  } else if (cd[n].view[3][2].tip & T_MIST) {
		UNSEE(n,4,1);
		UNSEE(n,4,2);
		UNSEE(n,4,3);

  }
  if (cd[n].view[1][1].tip & T_MIST) {
		UNSEE(n,2,0);
		UNSEE(n,1,0);
		UNSEE(n,0,0);

	if (cd[n].view[2][2].tip & T_MIST) {
		UNSEE(n,2,1);
		UNSEE(n,3,0);

	}
	if (cd[n].view[0][2].tip & T_MIST) {
		UNSEE(n,0,1);
	}
  }
  if (cd[n].view[1][3].tip & T_MIST) {
		UNSEE(n,2,4);
		UNSEE(n,1,4);
		UNSEE(n,0,4);

	if (cd[n].view[2][2].tip & T_MIST) {
		UNSEE(n,2,3);
		UNSEE(n,3,4);

	}
	if (cd[n].view[0][2].tip & T_MIST) {
		UNSEE(n,0,3);
	}
  }
  if (cd[n].view[2][1].tip & T_MIST) {
		UNSEE(n,3,0);
		UNSEE(n,4,0);

	if (cd[n].view[2][2].tip & T_MIST) {
		UNSEE(n,3,1);
	}
	if (cd[n].view[1][0].tip & T_MIST) {
		UNSEE(n,2,0);
	}
  }
  if (cd[n].view[2][3].tip & T_MIST) {
		UNSEE(n,3,4);
		UNSEE(n,4,4);

	if (cd[n].view[2][2].tip & T_MIST) {
		UNSEE(n,3,3);
	}
	if (cd[n].view[1][4].tip & T_MIST) {
		UNSEE(n,2,4);
	}
  }
  if (cd[n].view[3][1].tip & T_MIST) {
		UNSEE(n,4,0);
	if (cd[n].view[4][2].tip & T_MIST) {
		UNSEE(n,4,1);
	}
	if (cd[n].view[2][0].tip & T_MIST) {
		UNSEE(n,3,0);
	}
  }
  if (cd[n].view[3][3].tip & T_MIST) {
		UNSEE(n,4,4);
	if (cd[n].view[4][2].tip & T_MIST) {
		UNSEE(n,4,3);
	}
	if (cd[n].view[2][4].tip & T_MIST) {
		UNSEE(n,3,4);
	}
  }
  if ((cd[n].view[3][0].tip & T_MIST) && (cd[n].view[4][1].tip & T_MIST)) {
		UNSEE(n,4,0);
  }
  if ((cd[n].view[3][4].tip & T_MIST) && (cd[n].view[4][3].tip & T_MIST)) {
		UNSEE(n,4,4);
  }

  return;
}

void   dd_char(char *p, MapData d, int f)
{
  int	 i, j;

  *p = ' ';
  *(p + 1) = ' ';
  if (f & 0x1) {
	switch (((d.tip & T_SURFACE) ? d.surface : d.tip & TMASK)) {
	case T_PPLATE:
	  *p = 'o';
	  *(p + 1) = ' ';
	  break;
	case T_PLANT:
	  *p = ':';
	  *(p + 1) = ':';
	  break;
	case T_FLOWER:
	  *p = '+';
	  *(p + 1) = ':';
	  break;
	case T_WATER:
	case T_BWATER:
	  *p = '_';
	  *(p + 1) = '_';
	  break;
	case T_BOX:
	  *p = 'x';
	  *(p + 1) = 'x';
	  break;
	case T_MIST:
	  *p = '/';
	  *(p + 1) = '/';
	  break;
	case T_TGATE:
	  *p = '>';
	  *(p + 1) = '<';
	  break;
	case T_GATE:
	  *p = '[';
	  *(p + 1) = ']';
	  break;
	case T_JAIL:
	  *p = 'I';
	  *(p + 1) = 'I';
	  break;
	case T_WINDOW:
	  *p = '|';
	  *(p + 1) = '|';
	  break;
	case T_GLASS:
	  *p = '=';
	  *(p + 1) = '=';
	  break;
	case T_LBOX:
	  *p = '%';
	  *(p + 1) = '%';
	  break;
	case T_LGATE:
	  *p = '{';
	  *(p + 1) = '}';
	  break;
	case T_MWALL:
	case T_MSGATE:
	case T_LMSGATE:
	  *p = 'H';
	  *(p + 1) = 'H';
	  break;
	case T_SGATE:
	case T_LSGATE:
	case T_WALL:
	  *p = '#';
	  *(p + 1) = '#';
	  break;
	case T_ROCK:
	  *p = '@';
	  *(p + 1) = '@';
	  break;
	case T_TREE:
	  *p = 'T';
	  *(p + 1) = 'T';
	  break;
	case T_UNKNOWN:
	  *p = '?';
	  *(p + 1) = '?';
	  break;
	case T_STORE: /* add for storehouse 20020823 */
	  *p = 's';
	  *(p + 1) = 's';
	  break;
	default:
	  break;
	}
	if (d.tip & T_ROOF) {
	  if (f & 0x8) {
  switch (d.tip & TMASK) {
  case T_ANTIMAGIC:
  case T_EWALL:
  case T_MSGATE:
  case T_SGATE:
  case T_CWALL:
	for (i = 0; i < 23 && roofoff[i] != '\0'; i++) {
	  if ((d.tip & TMASK) == set_dd(&roofoff[i]))
		break;
	}
	if (roofoff[i] == '\0') {
	  *(p + 1) = '~';
	}
	break;
  case T_LGATE:
	if (std::strchr(&roofoff[0], '[') == nullptr)
	  *(p + 1) = '~';
	break;
  case T_LBOX:
	if (std::strchr(&roofoff[0], '%') == nullptr)
	  *(p + 1) = '~';
	break;
  case T_LSGATE:
  default:
	if (std::strchr(&roofoff[0], *p) == nullptr)
	  *(p + 1) = '~';
	break;
  }
	  } else {
  *(p + 1) = '~';
	  }
	}
  }
  if (f & 0x2) {
	if (d.tip & T_ITEM)
	  *p = '.';
	if (d.tip & T_MESSAGE)
	  *(p + 1) = '-';
  }
  if (f & 0x4) {
	/* change for variable multi characters in one space 20000721 */
	for (j = 0; j < MAX_CHRS_INONE; j++) {
	i = d.chr[j] - 1;
	if (i >= 0)
	  if ((i < MAX_NTTYS || *cd[i].name != ' ') && !TCN_VANISH(i)) {
  *(p + j % 2) = (i < MAX_NTTYS ? ftolower(*cd[i].name) : ftoupper(*cd[i].name));
  if (sg_check(i))
	*(p + (j + 1) % 2) = '*';
	  }
	}
  }
  return;
}

#define MAX_PRINT_NPC 30

void   print_map(int n, int f)
{
  static constexpr std::array<char, 4> kDc = {'N', 'E', 'S', 'W'};
  static constexpr std::array<char, 4> kRdc = {'B', 'R', 'F', 'L'};
  /* buf は dd_char へ buf + offset を渡すポインタ演算で使うため C 配列のまま */
  char	 buf[256];
  int	 x, y, xx, yy;
  int	 i, j, k, l;
  static int last_x[MAX_NTTYS], last_y[MAX_NTTYS], last_d[MAX_NTTYS];
  static time_t last_time[MAX_NTTYS];
  static MapData last_view[MAX_NTTYS][5][5];
  /* last_npc / last_board_title は CPYSTR の宛先・sizeof 依存のため C 配列のまま */
  static char last_npc[MAX_NTTYS][MAX_PRINT_NPC][256];
  static int  last_npc_num[MAX_NTTYS];
  static char last_board_title[MAX_NTTYS][256]; /* added by Qajin 20000102 */
#if MAX_OBJECTS > 0
  static int  obj_start_num[MAX_NTTYS];  /* add 20000107 */
  int obj;
#endif
  int need_check;

  if (n>= MAX_NTTYS || cd[n].fd == -1)
	return;

  need_check = 0;

#if MAX_OBJECTS > 0
  if(!reflesh_objects) {
#endif

  if(reflesh_last_map[n]){
	need_check = 1;
  }

  if(need_check
	 || cd[n].x!=last_x[n] || cd[n].y!=last_y[n] || cd[n].d!=last_d[n]){
	need_check = 1;
	if ((cd[n].modef & MD_BOARDSEND) && cd[n].d==last_d[n] && (map[cd[n].y][cd[n].x].tip & T_MESSAGE))	/* added by Qajin 20000102 */
	   board_title(n,last_board_title[n],sizeof(last_board_title[n]));
	last_x[n]=cd[n].x; last_y[n]=cd[n].y; last_d[n]=cd[n].d;
  }

  if((cd[n].modef & MD_BOARDSEND) && last_board_title[n][0] == '\0' && (map[cd[n].y][cd[n].x].tip & T_MESSAGE)) {
	   board_title(n,last_board_title[n],sizeof(last_board_title[n]));
  }
  if(!(map[cd[n].y][cd[n].x].tip & T_MESSAGE)) {
	   last_board_title[n][0] = '\0';
  }

  /* add for chat mode 20000629 */
  if(!need_check && ((f & MD_CHAT) || !cd[n].eye))
	return;

  abstime = time(nullptr);
  if(need_check
	 || abstime - last_time[n] >= 60	 /* 1分以上経った */
	 || abstime%60 < last_time[n]%60){ /* 分が変わった	*/
	need_check = 1;
	last_time[n] = abstime;
  }

  if (cd[n].modef & MD_M81) {
	print_m81(n,f);
	return;
  }
  if (cd[n].modef & MD_M57) {
	print_m57(n,f);
	return;
  }
  set_view(n);
  if(need_check
	 || std::memcmp(&(last_view[n][0][0]), &(cd[n].view[0][0]),sizeof(MapData)*5*5)){
	need_check = 1;
	std::memcpy(&(last_view[n][0][0]), &(cd[n].view[0][0]),sizeof(MapData)*5*5);
  }

  for (y=k=0; y<5 && k<MAX_PRINT_NPC; y++)
	for (x=0; x<5 && k<MAX_PRINT_NPC; x++) {
	  l = 0;
	  for (i = 0; i < MAX_CHRS_INONE && k < MAX_PRINT_NPC; i++) {
		if (IS_CLIENT_PHIWIN(n) && l >= 2) break;
  if ((j = cd[n].view[4-y][x].chr[i] - 1) < 0) continue;
  if (j != n && ((cd[j].name[0] != ' ' && !TCN_VANISH(j))
		   || cd[n].at == AT_VISIBLE)) {
	std::snprintf(buf, sizeof(buf),
	  "#map C%02i:%01i %01i %c %-31s %02x %-15s %c %02x \n",
	  k, x, y,
	  kRdc[(4 + cd[j].d - cd[n].d) & 3] , cd[j].name ,
	  (!(cd[j].st) && CN_GASIFY(j) >= 60 ? ST_GASIFY : cd[j].st),
	  CHRGRA(j),
	   chr_marker(j),
	  /* for change character graphic added by Qajin 19991224 */
	  (j < MAX_NTTYS ? ((cd[j].cg !=nullptr || cd[j].ocg != nullptr) ? 0x40: 0) : cd[j].type));

	if(need_check
	   || last_npc_num[n]<=k
	   || std::strcmp(last_npc[n][k]+6,buf+6)){
	  need_check = 1;
	  CPYSTR(last_npc[n][k],buf);
	}
	k++;
	l++;
  }
	  }
	}

  /* for change character graphic added by Qajin 19991224 changed 20000101 */
  if(cd[n].cv >= 5085910 && cd[n].cv < 5090152 && IS_CGCHGED(n)) {
	std::snprintf(buf, sizeof(buf),
	  "#map C%02i:2 3 B                                 %02x %-15s %c 40 \n",
	  k, (!(cd[n].st) && CN_GASIFY(n) >= 60 ? ST_GASIFY : cd[n].st),
	  CHRGRA(n),
	  ((!(cd[n].sg & 3) && gigant_check(n))||(cd[n].sg & 1)) ? '*' : '#');

	if(need_check
	   || last_npc_num[n]<=k
	   || std::strcmp(last_npc[n][k]+6,buf+6)){
	  need_check = 1;
	  CPYSTR(last_npc[n][k],buf);
	}
	k++;
  }


#if MAX_OBJECTS > 0
  obj_start_num[n] = k;
  } else {
	k = obj_start_num[n];
  }
#endif

  /* add for chat mode 20000629 */
  if(!need_check && ((f & MD_CHAT) || !cd[n].eye))
	return;

  if (cd[n].modef & MD_M81) {
	print_m81(n,f);
	return;
  }
  if (cd[n].modef & MD_M57) {
	print_m57(n,f);
	return;
  }

#if MAX_OBJECTS > 0

  for (y = 3; y >= -1 && k<MAX_PRINT_NPC; y--)
	for (x = -2; x <= 2 && k<MAX_PRINT_NPC; x++) {
	  if (cd[n].view[y + 1][x + 2].tip == T_UNKNOWN)
		 continue;

	  xx = cd[n].x + px[cd[n].d] * y - py[cd[n].d] * x;
	  yy = cd[n].y + py[cd[n].d] * y + px[cd[n].d] * x;

	  /* add for PhiWin aplication error 20000522 */
	  if (IS_CLIENT_PHIWIN(n)) {
		l = 0;
		for (i = 0; i < MAX_CHRS_INONE; i++) {
		  j = map[yy][xx].chr[i] - 1; /* bug fix 20000701 */
		  if (j>0 && j != n)
			 l++;
		  if (l >= 2)
			break;
		}
		if (l >= 2)
		  continue;
	  }

	  if ((obj = dobj[yy][xx]-1) < 0)
		 continue;

	  if (od[obj].gname == nullptr)
		 continue;

	  if ((od[obj].dir & OD_BOARD) && cd[n].cv < 5085910)
		 continue;

	  std::snprintf(buf, sizeof(buf),
		"#map C%02i:%01i %01i %c %-31s 00 %-15s %c %02x \n",
		k, x+2, 3-y,
		(od[obj].dir & OD_TRACE) ?
			kRdc[(((std::abs(y)>std::abs(x))?(y>=0?0:2):(x>0?1:3)) + od[obj].dir) & 0x3] : /* 観測者との位置関係による向き */
		((od[obj].dir & OD_CONSTDIR) ?
			kRdc[od[obj].dir & OD_DIRECTION] :						   /* 固定向き */
			kRdc[(4 + od[obj].dir - cd[n].d) & 3]),				   /* ノーマル */
		(od[obj].name != nullptr) ? od[obj].name : "",
		 od[obj].gname,
	   (od[obj].dir & OD_DEBU) ? '*' : '#',
		 ((od[obj].dir & 0x30) >> 4) + OT_STATUE);

	  if(need_check
		 || last_npc_num[n]<=k
		 || std::strcmp(last_npc[n][k]+6,buf+6)){
		need_check = 1;
		CPYSTR(last_npc[n][k],buf);
	  }
	  k++;
	}

#endif

  if(need_check || last_npc_num[n]!=k){
	need_check = 1;
	last_npc_num[n]=k;
  }

  if(reflesh_last_map[n]){
	reflesh_last_map[n] = 0;
  }

  /* 以上で判定終了 */

  if(!need_check){
	cd[n].m_wait_c = 0;
	return;
  }
#ifdef DEBUG_LAZY
  std::snprintf(debug_buf, sizeof(debug_buf),"print map(%d)(print_map)",n);
  log_print(debug_buf);
#endif

  std::snprintf(buf, sizeof(buf), "#map M %c %08lx:",kDc[cd[n].d],time(nullptr)+tslide*60*60);
  for (j = 0, y = 0; y < 5; y++)
	for (x = 0; x < 5; x++) {
	  dd_char(buf + 18 + 50 + y * 10 + x * 2, cd[n].view[4 - y][x], 0x2);
	  dd_char(buf + 18 + y * 10 + x * 2, cd[n].view[4 - y][x], 0x1);
	}
  *(buf + 118) = '\n';
  *(buf + 119) = 0;
  check_write(n, buf);

  for(k = 0; k<last_npc_num[n]; k++)
	check_write(n, last_npc[n][k]);

  if (map[cd[n].y][cd[n].x].tip & T_MESSAGE) {
	if (cd[n].modef & MD_BOARDSEND)
	  check_write(n,last_board_title[n]); /* changed by Qajin 20000102 */
	if (f & MD_BOARD) {
	  dmcheck_write(n, DMM_LIST_BOARD);
	  load_sd(n, cd[n].x, cd[n].y, 'T', 1);
	  list_board(n, 1);
	}
  }
  if (map[cd[n].y][cd[n].x].tip & (T_ITEM | T_TRAP)) {
	if (f & MD_FLOORITEM) {
	  dmcheck_write(n, DMM_LIST_ITEM);
	  list_item(n, 1);
	}
  }
  std::snprintf(buf, sizeof(buf), "#map .\n");
  check_write(n, buf);
  cd[n].m_wait_c = 0;
  return;
}

void   print_status(int n)
{
  std::array<char, 256> buf;
  /* last_status / last_cond は CPYSTR の宛先のため C 配列のまま */
  static char last_status[MAX_NTTYS][256],last_cond[MAX_NTTYS][256];

  if(n>=MAX_NTTYS || cd[n].fd==-1) return;

  std::snprintf(buf.data(), buf.size(), "#status %-31s:% 5li:% 5li:% 5li:% 5li:% 9i:% 5i:% 5i:% 5i:% 5i:% 5i \n"
  /* changed by Qajin 19991224,19991228,20000101 */
  , (cd[n].cv < 5085910 || !IS_CGCHGED(n) || (cd[n].modef & (MD_M57 | MD_M81))) ? cd[n].name : (cd[n].cv >= 5090152 ? CHRGRA(n) : "__")
	, cd[n].hp, cd[n].mhp, cd[n].mp, cd[n].mmp, cd[n].exp, cd[n].gp
	,cd[n].lv[LV_FIGHTER], cd[n].lv[LV_WIZARD]
	,cd[n].lv[LV_MERCHANT], cd[n].lv[LV_CREATER]);
  if(reflesh_last_status[n] || std::strcmp(buf.data(), last_status[n])){
	CPYSTR(last_status[n], buf.data());
	check_write(n, buf.data());
#ifdef DEBUG_LAZY
	std::snprintf(debug_buf, sizeof(debug_buf),"send status(%d)(print_status)",n);
	log_print(debug_buf);
#endif
  }
  std::snprintf(buf.data(), buf.size(), "#cond %c%c%c%c%c%c%c\n",
	(TCN_POISON(n) ? '*' : '-'),
	(TCN_PALSY(n) ? '*' : '-'),
	(TCN_PANIC(n) ? '*' : '-'),
	(TCN_CONFUSE(n) ? '*' : '-'),
	(TCN_BERSERK(n) ? '*' : '-'),
	(TCN_SILENCE(n) ? '*' : '-'),
	(TCN_BLIND(n) ? '*' : '-'));
  if(reflesh_last_status[n] || std::strcmp(buf.data(), last_cond[n])){
	CPYSTR(last_cond[n], buf.data());
	check_write(n, buf.data());
#ifdef DEBUG_LAZY
	std::snprintf(debug_buf, sizeof(debug_buf),"send cond(%d)(print_status)",n);
	log_print(debug_buf);
#endif
  }
  cd[n].s_wait_c = 0;
  reflesh_last_status[n] = 0;
  return;
}

void   print_lists(int n)
{
  std::array<char, 256> buf;
  int	 i;

  for (i = 1; i < MAX_NTTYS; i++) {
	if (cd[i].type) {
	  std::snprintf(buf.data(), buf.size(), "#user %04d %-31s\n", i, cd[i].name);
	  check_write(n, buf.data());
	}
  }
  return;
}

#if MAX_OBJECTS > 0
int  check_pushobj_in(int x,int y)
{
	int i;

	if (map[y][x].tip & T_CWALL)
		return 1;
	if (dobj[y][x]-1>=0)
		return 1;
	for (i = 0; i < MAX_CHRS_INONE; i++) {
	  if (map[y][x].chr[i] - 1 >= 0)
	  	return 1;
	}
	return 0;
}
#endif

void   move_chr(int n, int sx, int sy, int ex, int ey, int msg)
{
  int	 i, j, k;
  int	 cng;

  cng = 0;

  if (TCN_NOMOVE(n)
	  || ex < 1 || ex >= MAX_X - 1 || ey < 1 || ey >= MAX_Y - 1) {
	cng = 1;
	ex = sx;
	ey = sy;
  }
  if (cd[n].type == CT_NORMAL && !TCN_PASSWALL(n)) {
	if ((map[ey][ex].tip & TMASK) == T_LBOX || (map[ey][ex].tip & TMASK) == T_LGATE) {
	  load_sd(n, ex, ey, 'L', 1);
	  ex = sx;
	  ey = sy;
	} else if (map[ey][ex].tip & T_CWALL) {
	  cng = 1;
	  ex = sx;
	  ey = sy;
	}
  } else if (cd[n].type != CT_ASTRAL && !TCN_PASSWALL(n) && (map[ey][ex].tip & T_EWALL)) {
	ex = sx;
	ey = sy;
  }
  /* 2x2 の大きな敵の移動は、基準マス以外の3マスについても
     範囲・地形・占有を検証し、1つでも塞がっていれば進めない。
     体が重なるため、相手が通常のキャラでも共有はできない */
  if (chr_big(n) && (ex != sx || ey != sy)) {
	int bi, bj, bk, bx2, by2;
	for (bi = 1; bi < 4 && (ex != sx || ey != sy); bi++) {
	  bx2 = ex + BIG_OFF[bi][0];
	  by2 = ey + BIG_OFF[bi][1];
	  if (bx2 < 1 || bx2 >= MAX_X - 1 || by2 < 1 || by2 >= MAX_Y - 1) {
		cng = 1;
		ex = sx;
		ey = sy;
		break;
	  }
	  if (!TCN_PASSWALL(n)) {
		if (cd[n].type == CT_NORMAL) {
		  if (map[by2][bx2].tip & T_CWALL) {
			cng = 1;
			ex = sx;
			ey = sy;
			break;
		  }
		} else if (cd[n].type != CT_ASTRAL && (map[by2][bx2].tip & T_EWALL)) {
		  cng = 1;
		  ex = sx;
		  ey = sy;
		  break;
		}
	  }
	  for (bj = 0; bj < MAX_CHRS_INONE; bj++) {
		bk = map[by2][bx2].chr[bj] - 1;
		if (bk >= 0 && bk != n) {
		  cng = 1;
		  ex = sx;
		  ey = sy;
		  break;
		}
	  }
	}
  }
  /* add 'if' for lock message for gigant 20000621 */
  if (ex != sx || ey != sy) {
	k = 0;
	for (j = 0; j < MAX_CHRS_INONE; j++) {
	  i = map[ey][ex].chr[j] - 1;
	  if (i >= 0)
		k++;
	  if (sg_check(i) || chr_big(i)) {
		cng = 1;
		ex = sx;
		ey = sy;
	  }
	}
	if (k > 0) {
	  if (sg_check(n)
	|| k >= MAX_CHRS_INONE) {
		cng = 1;
		ex = sx;
		ey = sy;
	  }
	}
  }

#if MAX_OBJECTS > 0
  /* add for pushable object 20011203 */
  if (ex != sx || ey != sy) {
	if ((j=dobj[ey][ex]-1)>=0 && (od[j].dir & OD_PUSH)) {
		if (check_pushobj_in(ex+fsign(ex-sx),ey+fsign(ey-sy))) {
			cng = 1;
			ex = sx;
			ey = sy;
		} else if (od[j].id != nullptr) {
			CPYSTR(cd[n].str,od[j].id);
			load_switch(n, ex, ey, 31);
		}
	}
  }
#endif

  if ((cng) && (msg))
	dmcheck_write(n, DMM_GO_NO);
  if (!trnf && sx == ex && sy == ey && map[sy][sx].tip & T_SWITCH)
	load_switch(n, sx, sy, 2);
  clear_chr(n);
  /**** 97/Jun/9 b-Yak-38 ADD ****/
  if (cd[n].x != ex || cd[n].y != ey) {
	cd[n].bx = cd[n].x;
	cd[n].by = cd[n].y;
  }
  /*******************************/
  cd[n].x = ex;
  cd[n].y = ey;
  set_chr(n);
  if (sx != ex || sy != ey) {
#if MAX_OBJECTS > 0
	/* add for pushable object 20011203 */
	if ((i=dobj[ey][ex]-1)>=0 && (od[i].dir & OD_PUSH)) {
		move_obj(i,(j=ex+fsign(ex-sx)),(k=ey+fsign(ey-sy)),-1);
		if(od[i].id != nullptr && (map[k][j].tip & T_SWITCH)) {
			CPYSTR(cd[n].str,od[i].id);
	  		load_switch(n, j, k, 30);
	  	}
	}
#endif
	print_map(n, cd[n].modef);
  }
  switch (map[ey][ex].tip & TMASK) {
  case T_TGATE:
	if (msg) {
	  check_write(n, "#list\n");
	  list_gate(n);
	  check_write(n, "#end-list\n");
	  dmcheck_write(n, DMM_TRANS_SELECT);
	  cd[n].st = ST_TRANSSELECT;
	  check_write(n, "#s-edit\n");
	  CN_GASIFY(n) = 0x10000;
	  break;
	} else {
	  break;
	}
  case T_FLOWER:
	/* HP0のアンデッドが踏むとダミープレートが出現していた bugfix by NEO 20120212
	if (cd[n].mhp > 0) {
	*/
	if ( cd[n].mhp > 0 || ( n >= MAX_NTTYS && cd[n].type == CT_UNDEAD ) ) /* Undead対策 */
	{
		map[ey][ex].tip &= ~T_FLOWER;	/* 花を消して */
		map[ey][ex].tip |= T_PLANT;		/* 草にする */
	}
  case T_PLANT:
	if (n >= MAX_NTTYS && cd[n].type == CT_UNDEAD)
	  map[ey][ex].tip &= ~T_PLANT;
	else if (PER_DICE < cd[n].mhp / 100)
	  map[ey][ex].tip &= ~T_PLANT;
	break;
  }
  if (sx == ex && sy == ey) {
	if (!trnf) {
		if((j = fld[map[ey][ex].area].fmove) >= 0) {
			change_flag(n,j,FLAG_ON,1);
			load_switch(n,cd[n].x,cd[n].y,10);
		}
	}
	if(map[ey][ex].tip & T_SWITCH)
	  load_switch(n, ex, ey, (!trnf ? 1 : 15));
  }
  if (map[ey][ex].tip & T_TRAP)
  	effect_trap(n);
  return;
}

static void embed_chr(int n, int x, int y, int *conflict)
/* 1マスへの埋め込み。既に埋め込み済みなら何もしない */
{
  MapData *d;
  int i;

  if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y)
    return;
  d = &(map[y][x]);
  for(i = 0; i < MAX_CHRS_INONE; i++) {
	if (d->chr[i] - 1 == n)
	  return;
	if (!(d->chr[i])) {
	  d->chr[i] = n + 1;
	  return;
	}
  }
  *conflict = 1;
}

void   set_chr(int n)
/* キャラクタのマップへの埋め込み */
/* これにより、他のキャラからの認識ができる */
{
  std::array<char, 256> buf;
  int i;
  int conflict;

  if (!cd[n].type)
	return;
  /* 2x2 の大きな敵は、基準マスと東・南・南東の4マスに埋め込む。
     これにより、どのマスへの攻撃・侵入も通常の chr 表で判定できる */
  conflict = 0;
  embed_chr(n, cd[n].x, cd[n].y, &conflict);
  if (chr_big(n)) {
	for (i = 1; i < 4; i++)
	  embed_chr(n, cd[n].x + BIG_OFF[i][0], cd[n].y + BIG_OFF[i][1], &conflict);
  }
  if (conflict)
	check_write(n,"#ex-chr-conflict"); /* new protocol added 20000227 change 0403 */

  /** psp6 by aleph **/
  /** 埋め込みはここまで。以降は変化による付随処理 **/

  /** snprintf(buf, sizeof(buf), "PSP6DEBUG : n = %i, cli = %i, bx = %i, by = %i, x = %i, y = %i"
   *	   , n, cd[n].cv, cd[n].bx, cd[n].by, cd[n].x, cd[n].y);
   * log_print(buf);
   **/

  if (n >= MAX_NTTYS)		/* MAX_NTTYSは普通64。nは1から始まる */
	return; 				/* NPCは関係ない */

/*	if (cd[n].cv < 5100006)
	return; delete */				 /* クライアントバージョンが古い */

  /***
   * snprintf(buf, sizeof(buf), "PSP6DEBUG: area = %i, floor = %i, mapset = %s, bgm = %s"
   *		 , fld[map[cd[n].y][cd[n].x].area].area, fld[map[cd[n].y][cd[n].x].area].floor
   *		 , fld[map[cd[n].y][cd[n].x].area].mapset, fld[map[cd[n].y][cd[n].x].area].bgm);
   * log_print(buf);
   ***/

	if (cd[n].by < 0 || cd[n].bx < 0 || /* bug fix 20000629 */
		map[cd[n].by][cd[n].bx].area != map[cd[n].y][cd[n].x].area) {

		ex_notice(n,3); /* add for SD51 */

	  /** mapsetの送信 **/
		if (fld[map[cd[n].y][cd[n].x].area].mapset != nullptr) {  /* 定義済みの場所へ */
		  std::snprintf(buf.data(), buf.size(), "#mapset %s\n", fld[map[cd[n].y][cd[n].x].area].mapset);
		  check_write(n, buf.data());
		} else {										   /* 未定義の場所へ */
		  if (cd[n].by < 0 || cd[n].bx < 0 ||				   /* 初期アクセス時 */
			  fld[map[cd[n].by][cd[n].bx].area].mapset != nullptr) {  /* 定義済みの場所から*/
			std::snprintf(buf.data(), buf.size(), "#mapset default\n");
			check_write(n, buf.data());
		  }
		}

	  /** bgm名の送信 **/
		if (fld[map[cd[n].y][cd[n].x].area].bgm[0] != ' ') {  /* 定義済みの場所へ */
		  std::snprintf(buf.data(), buf.size(), "#bgm %s\n", fld[map[cd[n].y][cd[n].x].area].bgm);
		  check_write(n, buf.data());
		} else {										   /* 未定義の場所へ */
		  if (cd[n].by < 0 || cd[n].bx < 0 ||				/* 初期アクセス時 */
			  fld[map[cd[n].by][cd[n].bx].area].bgm[0] != ' ') {  /* 定義済みの場所から*/
			std::snprintf(buf.data(), buf.size(), "#bgm default\n");
			check_write(n, buf.data());
		  }
		}

	}
  /** end **/

  return;
}

void   clear_chr(int n)
{
	int i;

	clear_chr_pos(n, cd[n].x, cd[n].y);
	if (chr_big(n)) {
	  for (i = 1; i < 4; i++)
		clear_chr_pos(n, cd[n].x + BIG_OFF[i][0], cd[n].y + BIG_OFF[i][1]);
	}
}

/* bug fix for remote put/get on trans switch 20050330 */
void   clear_chr_pos(int n, int x, int y)
{
  MapData *d;
  int i;

  if (x < 0 || x >= MAX_X || y < 0 || y >= MAX_Y)
    return;
  d = &(map[y][x]);
  for(i = 0; i < MAX_CHRS_INONE; i++) {
	if (d->chr[i] - 1 == n) {
	  d->chr[i] = 0;
	  break;
	}
  }
  return;
}

void   func_analyze(int n, char *rbuf)
{
  std::array<char, 255> wbuf;
  int	 i;

  if (!std::strcmp(rbuf, "#map"))
	print_map(n,0);
  else if (!std::strcmp(rbuf, "#status"))
	print_status(n);
  else if (!std::strcmp(rbuf, "#ulist"))
	print_lists(n);
  else if (!std::strcmp(rbuf, "#enter-win")) {
	cd[n].eye = 1;
	std::snprintf(wbuf.data(), wbuf.size(), "#enter-win %s\n", cd[n].name);
	write_companion(n, 0, wbuf.data(), 0);
  } else if (!std::strcmp(rbuf, "#leave-win")) {
	cd[n].eye = 0;
	std::snprintf(wbuf.data(), wbuf.size(), "#leave-win %s\n", cd[n].name);
	write_companion(n, 0, wbuf.data(), 0);
  } else if (!std::strcmp(rbuf, "#end-lag")) {
	if (n < MAX_NTTYS && CN_ALAG(n)) {
	  if (CN_ALAG(n) == 2) {
  CN_LAG(n) = -CN_LAG(n) - 2;
	  }
	  CN_ALAG(n)--;
	  /* add for lag attack 20020115 */
	  if (!CN_ALAG(n))
	  	CN_DLAG(n) = 0;
	} else {
	  CN_LAG(n) = -CN_LAG(n) - 2;
	}
  } else if (!std::strcmp(rbuf, "#code-sjis")) {
	check_write(n, "#code-sjis-ok\n");
  } else if (!std::strncmp(rbuf, "#version-cli ", 13)) {
	/* for character graphic added by Qajin 19991228 */
	i = std::atoi(rbuf + 13);

	if (i && cd[n].cv == 0 && IS_CGCHGED(n)) {
	  if (i >= 5090152) {
		std::snprintf(wbuf.data(), wbuf.size(), "#name %s\n", CHRGRA(n)); /* phiwin/Tphi 識別追加 added 20000101 */
		check_write(n, wbuf.data());
	  } else if (i >= 5085910)
		check_write(n, "#name __\n");
	}

	cd[n].cv = i;
	ex_version_dm(n);
	std::snprintf(wbuf.data(), wbuf.size(), "#version-srv %s\n", PHI_PROT_VERS);
	check_write(n, wbuf.data());

	/** psp6 **/
	if (cd[n].cv >= 5100006) {
	  if (fld[map[cd[n].y][cd[n].x].area].mapset != nullptr) {  /* 定義済みの場所 */
		std::snprintf(wbuf.data(), wbuf.size(), "#mapset %s\n", fld[map[cd[n].y][cd[n].x].area].mapset);
		check_write(n, wbuf.data());
	  } else {											 /* 未定義の場所 */
		std::snprintf(wbuf.data(), wbuf.size(), "#mapset default\n");
		check_write(n, wbuf.data());
	  }
	  if (fld[map[cd[n].y][cd[n].x].area].bgm[0] != ' ') {	/* 定義済みの場所へ */
		std::snprintf(wbuf.data(), wbuf.size(), "#bgm %s\n", fld[map[cd[n].y][cd[n].x].area].bgm);
		check_write(n, wbuf.data());
	  } else {											 /* 未定義の場所へ */
		std::snprintf(wbuf.data(), wbuf.size(), "#bgm default\n");
		check_write(n, wbuf.data());
	  }

	  std::snprintf(wbuf.data(), wbuf.size(), "#remap\n");
	  check_write(n, wbuf.data());
	}
	/** end **/

  } else if (!std::strncmp(rbuf, "#map-iv", 7))
	cd[n].m_wait = std::atoi(rbuf + 8);
  else if (!std::strncmp(rbuf, "#status-iv", 10))
	cd[n].s_wait = std::atoi(rbuf + 11);
  else if (!std::strncmp(rbuf, "#priv ", 6)) {
	strlcpy(wbuf.data(), rbuf + 6, 5);
/*	  wbuf[4] = '\0';*/  /* PHI:0055 Bug Fix by EL */
	i = std::atoi(wbuf.data());
	if (i < MAX_NTTYS) { /* bug fix for over range 20030915 */
		std::snprintf(wbuf.data(), wbuf.size(), "#priv %04d %s\n", n, rbuf + 11);
		check_write(i, wbuf.data());
	}
  } else if (!std::strncmp(rbuf, "#ex-",4)) {
	func_analyze_ex(n,rbuf);
  }
  return;
}

void   input_analyze(int n, char *rbuf)
{
  FILE	*fp;
  std::array<char, 256> wbuf;
  std::array<char, 256> wbuf2;
  char	*p;
  int	 in, tn, size, ic, wn, an, on;
  int	 i, j, k, l;
  int	 mbuf[7];
  int	 numbers[MAX_ANALYZE_INPUT_NUMBERS];
  int	 x, y; /* add for remote get & put 20000612 */
  std::array<char, 16> fid;

  k = 0;
  if (n < MAX_NTTYS && cd[n].parse != 0) {
	if (cd[n].parse > 0) {
	  for(i=0;i<10;i++)
		if (!std::strcmp(rbuf,kParsecom[i])) {
		std::snprintf(rbuf, 1023,"%1x",i); /* main()のbufサイズに依存 */
		cd[n].parse = -1;
		break;
	  }
	if (i == 10)
	  cd[n].parse = 0;
	} else {
	   if (*rbuf >= '0' && *rbuf <= '9'){ /* bug fix 20000324 */
	  i = (*rbuf - '0');
	  strlcpy(rbuf,kParsecom[i],1023); /* main()のbufサイズに依存 */
	   }
	cd[n].parse = 0;
	}
  }

  if (cd[n].st == ST_CAST) {
	dmcheck_write(n, DMM_CAST_STOP);
	cd[n].wt = 0;
	cd[n].st = ST_COMMAND;
  }
  if (cd[n].st == ST_JOIN_INVITE) {
	dmprintf(n,0,wbuf.data(),wbuf.size(),DMM_JOIN_GIVEUP);
	write_companion(n,0,wbuf.data(),0);
	cd[n].st = ST_COMMAND;
	for (i = 0; i < MAX_CHRS_INONE; i++) {
		j = map[cd[n].y+py[cd[n].d]][cd[n].x+px[cd[n].d]].chr[i] - 1;
		if (cd[j].st == ST_JOIN_ANSWER) {
			cd[j].st = ST_COMMAND;
			cd[j].vf = 0;
		}
	}
  }
  CN_IDLE(n) = 0; /* add 20020822 */
  x = y = -1; /* for remote put/get on trans switch 20050330 */
  switch (cd[n].st) {
  case ST_TRANSSELECT:
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	i=analyze_single_number(rbuf);
	if(i>=1)
	  load_gate(n, i);
	else
	  dmcheck_write(n, DMM_NO_NUMBER);
	break;
  /* add for remote get 20000612 */
  case ST_R_GET:
	x = cd[n].x;
	y = cd[n].y;
	cd[n].x = cd[n].ix;
	cd[n].y = cd[n].iy;
  case ST_GET:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	if(size >= 1) {
	  /* less list added 20000106 */
	  if (cd[n].vf2 > 1 && (!IS_CLIENT_RPHIWIN(n) ? numbers[0] == 0 : numbers[0] == cd[n].vf2 - 1)) { /* bug(?) fix 20000831 */
		numbers[0] = cd[n].vf = cd[n].vf2 - 8;
		if (numbers[0] < 1)
			numbers[0] = cd[n].vf = 1;
	  }

	  if (cd[n].vf && (numbers[0] == cd[n].vf || (!IS_CLIENT_RPHIWIN(n) && numbers[0] == 9))) {
		   numbers[0] = cd[n].vf;
  cd[n].vf = 0;
  check_write(n, "#list\n");
  if (!IS_CLIENT_RPHIWIN(n) && numbers[0] > 1) {
	std::snprintf(wbuf.data(), wbuf.size(), "0) [%2i] ... previous\n",numbers[0] - 1);
	check_write(n, wbuf.data());
  }
  cd[n].vf2 = numbers[0];
  list_item(n, numbers[0]);
  check_write(n, "#end-list\n");
  dmcheck_write(n, DMM_GET_SELECT);
  check_write(n, "#s-edit\n");
  /* add for remote get 20000612 */
  if (cd[n].st == ST_R_GET) {
	cd[n].x = x;
	cd[n].y = y;
  }
  return;
	  }
	  if (numbers[0] == 0)
		  size = 0;

	  for(i=0, tn=0; i<size;){
  if(numbers[i]==-1) k++;
  else k=numbers[i];
  if(!IS_CLIENT_RPHIWIN(n) && cd[n].vf2 > 8 && k < 10)
	k += cd[n].vf2 - 1;
  if(save_id(n, k-tn, 0)){
	if(numbers[i]!=-1)
	  dmcheck_write(n, DMM_GET_NO);
	break;
  } else {
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_GET , cd[n].str);
	write_companion(n, 0, wbuf.data(), 0);
	load_switch(n, cd[n].x, cd[n].y, 18); /* for take switch 20000803 */
	tn++;
	if(numbers[i]!=-1) i++;
  }
	  }
	}
  /* add for remote get 20000612 */
	if (x >= 0) {
		/* bug fix for remote put/get on trans switch 20050330 */
		if (cd[n].x == cd[n].ix && cd[n].y == cd[n].iy) {
		  cd[n].x = x;
		  cd[n].y = y;
		} else {
			clear_chr_pos(n, x, y);
		}
	}
	if (cd[n].st != ST_MSGW) /* bug fix for cancel no-move 20030709 */
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_READ:
	i = analyze_single_number(rbuf);
	if(i>=0+(cd[n].vf2 == 1)){
	  if (cd[n].vf2 > 1 && (!IS_CLIENT_RPHIWIN(n) ? i == 0 : i == cd[n].vf2 - 1)) { /* bug(?) fix 20000831 */
		i = cd[n].vf = cd[n].vf2 - 8;
		if (i < 1)
			i = cd[n].vf = 1;
	  }

	  if (cd[n].vf && (i == cd[n].vf || (!IS_CLIENT_RPHIWIN(n) && i == 9))) {
			i = cd[n].vf;
  cd[n].vf = 0;
  check_write(n, "#list\n");
  if (!IS_CLIENT_RPHIWIN(n) && i > 1) {
	std::snprintf(wbuf.data(), wbuf.size(), "0) [%2i] ... previous\n",i - 1);
	check_write(n, wbuf.data());
  }
  cd[n].vf2 = i;
  list_board(n, i);
  check_write(n, "#end-list\n");
  dmcheck_write(n, DMM_READ_SELECT);
  check_write(n, "#s-edit\n");
  return;
	  } else {
		if(!IS_CLIENT_RPHIWIN(n) && cd[n].vf2 > 8 && i < 10)
		  i += cd[n].vf2 - 1;
  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_READ);
  write_companion(n, 0, wbuf.data(), 0);
  cd[n].vf = i;
  j = load_sd(n, cd[n].x, cd[n].y, 'B', i);
  if (i == 1) {
	dmcheck_write(n, DMM_READ_FINISH);
  } else if (j == 0) {
	dmcheck_write(n, DMM_READ_NO);
  } else {
	dmcheck_write(n, DMM_READ_MORE);
	  cd[n].vf = i - 1;
	  cd[n].st = ST_MORE;
	  return;
  }
	  }
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;

  case ST_TITLE:
	i = analyze_single_number(rbuf);
	if(i>=0+(cd[n].vf2 == 1)){
	  if (cd[n].vf2 > 1 && (!IS_CLIENT_RPHIWIN(n) ? i == 0 : i == cd[n].vf2 - 1)) {
		i = cd[n].vf = cd[n].vf2 - 8;
		if (i < 1)
			i = cd[n].vf = 1;
	  }

	  if (cd[n].vf && (i == cd[n].vf || (!IS_CLIENT_RPHIWIN(n) && i == 9))) {
			i = cd[n].vf;
  cd[n].vf = 0;
  check_write(n, "#list\n");
  if (!IS_CLIENT_RPHIWIN(n) && i > 1) {
	std::snprintf(wbuf.data(), wbuf.size(), "0) [%2i] ... previous\n",i - 1);
	check_write(n, wbuf.data());
  }
  cd[n].vf2 = i;
  list_board(n, i);
  check_write(n, "#end-list\n");
  dmcheck_write(n, DMM_TITLE_SELECT);
  check_write(n, "#s-edit\n");
  return;
	  } else {
		if(!IS_CLIENT_RPHIWIN(n) && cd[n].vf2 > 8 && i < 10)
		  i += cd[n].vf2 - 1;
		j = set_title(n, cd[n].x, cd[n].y, i);
		if (!j) {
		  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_TITLE);
		  write_companion(n, 0, wbuf.data(), 0);
		} else if (j == -1) {
		  dmcheck_write(n, DMM_NO_OWNER);
		} else if (j == -2) {
		  dmcheck_write(n, DMM_TITLE_NO);
		}
	  }
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_ERASE:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	if(size>=1)
	{
	  if (cd[n].vf2 > 1 && (!IS_CLIENT_RPHIWIN(n) ? numbers[0] == 0 : numbers[0] == cd[n].vf2 - 1)) { /* bug(?) fix 20000831 */
		numbers[0] = cd[n].vf = cd[n].vf2 - 8;
		if (numbers[0] < 1)
			numbers[0] = cd[n].vf = 1;
	  }

	  if (cd[n].vf && (numbers[0] == cd[n].vf || (!IS_CLIENT_RPHIWIN(n) && numbers[0] == 9)))
	  {
		   numbers[0] = cd[n].vf;
		cd[n].vf = 0;
		check_write(n, "#list\n");

		if (!IS_CLIENT_RPHIWIN(n) && numbers[0] > 1) {
		  std::snprintf(wbuf.data(), wbuf.size(), "0) [%2i] ... previous\n",numbers[0] - 1);
		  check_write(n, wbuf.data());
		}
		cd[n].vf2 = numbers[0];
		list_board(n, numbers[0]);
		check_write(n, "#end-list\n");
		dmcheck_write(n, DMM_ERASE_SELECT);
		check_write(n, "#s-edit\n");
		return;
	  }

	  if (numbers[0]==0)
		 size = 0;

	  for(i=0, tn=0; i<size;)
	  {
		if (numbers[i]==-1)
		  k++;
		else
		  k = numbers[i];

		if(!IS_CLIENT_RPHIWIN(n) && cd[n].vf2 > 8 && k < 10)
		  k += cd[n].vf2 - 1;

		if ((j=save_sd(n,cd[n].x,cd[n].y,'B',nullptr,k-tn))<0)
		{
		  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_ERASE);
		  write_companion(n, 0, wbuf.data(), 0);
		  tn++;
		  if (numbers[i] != -1)
			i++;
		}
		else
		{
		  if (numbers[i] != -1)
		  {
			dmcheck_write(n, DMM_ERASE_NO);
			i++;
		  }
		  else if (j<k-tn)
			i++;
		}
	  }
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_MORE:
	if (*rbuf == '.')
	  dmcheck_write(n, DMM_READ_STOP);
	else {
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_READ);
	  write_companion(n, 0, wbuf.data(), 0);
	  load_sd(n, cd[n].x, cd[n].y, 'B', cd[n].vf);
	  if (--(cd[n].vf) > 0) {
  dmcheck_write(n, DMM_READ_MORE);
  return;
	  } else if (cd[n].vf == 0) /* bug? fix 20000324 */
		  dmcheck_write(n, DMM_READ_FINISH);
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  /* add for remote put 20000612 */
  case ST_R_PUT:
	  x = cd[n].x;
	  y = cd[n].y;
	  cd[n].x = cd[n].ix;
	  cd[n].y = cd[n].iy;
  case ST_PUT:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0, tn=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i];
	  if((ic=item_count(n, k-tn))< 0) break;
	  if(save_id(n, ic ,1)){
  if(numbers[i]!=-1)
	dmcheck_write(n, DMM_PUT_NO);
  break;
	  } else {
  item_gc(n);
  set_fn(n);
  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_PUT
	, cd[n].str);
  write_companion(n, 0, wbuf.data(), 0);
  if (map[cd[n].y][cd[n].x].tip & T_SWITCH)
	load_switch(n, cd[n].x, cd[n].y, 4);
  tn++;
	  }
	  if(numbers[i]!=-1) i++;
	}
  /* add for remote put 20000612 */
	if (x >= 0) {
		/* bug fix for remote put/get on trans switch 20050330 */
		if (cd[n].x == cd[n].ix && cd[n].y == cd[n].iy) {
		  cd[n].x = x;
		  cd[n].y = y;
		} else {
			clear_chr_pos(n, x, y);
		}
	}
	if (cd[n].st != ST_MSGW) /* bug fix for cancel no-move 20030709 */
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_USE:
	/* bug fix for charge sphere 20030709 */
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	if(is_single_use){							/* 複数同時使用禁止 */
	  numbers[0]=analyze_single_number(rbuf);
	  size = ((numbers[0]<=0)? 0 : 1);
	} else
	  size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0, tn=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i];
	  if((ic=item_count(n, k-tn))< 0) break;
	  if ((cd[n].id[ic].type & IT_MASK) == IT_CHARGE
	&& (fld[map[cd[n].y][cd[n].x].area].flag & FF_CHARGE)){
  if(numbers[i]!=-1)
	dmcheck_write(n, DMM_USE_NO_CHARGE);
	  } else {
  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_USE_ITEM, cd[n].id[ic].name);
  write_companion(n, 0, wbuf.data(), 0);
  if(use_item(n, ic)) tn++;
	  }
	  if(numbers[i]!=-1) i++;
	}
	break;
  case ST_DETECT:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i++];
	  if((ic=item_count(n, k))< 0) break;
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DETECT_ITEM
		, cd[n].id[ic].name);
	  write_companion(n, 0, wbuf.data(), 0);
	  detect_item(n, ic);
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_ANALYZE:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i++];
	  if (k > MAX_MAGICS || k <= 0)
  break;
	  if (cd[n].md[k - 1].effect == EF_NONE)
  break;
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_ANALYZE_SPELL,
		 cd[n].md[k - 1].spell);
	  write_companion(n, 0, wbuf.data(), 0);
	  detect_magic(n, k - 1);
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_WINTALK:
	for (i=0; i<MAX_TALKS; i++)
	  if(cd[n].td[i].type == TT_NONE) break;
	if (i<MAX_TALKS){
	  cd[n].td[i].type = TT_KILL;
	  cd[n].td[i].string = findstr(rbuf);
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_WINTALK_SET, cd[n].td[i].string);
	  check_write(n, wbuf.data());
	} else
	  dmcheck_write(n, DMM_WINTALK_NO);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  /* add for lose talk 20001212 */
  case ST_LOSETALK:
	for (i=0; i<MAX_TALKS; i++)
	  if(cd[n].td[i].type == TT_NONE) break;
	if (i<MAX_TALKS){
	  cd[n].td[i].type = TT_DYING;
	  cd[n].td[i].string = findstr(rbuf);
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_LOSETALK_SET, cd[n].td[i].string);
	  check_write(n, wbuf.data());
	} else
	  dmcheck_write(n, DMM_LOSETALK_NO);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_FORGET:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0,tn=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i++];
	  if (k > MAX_MAGICS || k <= 0)
  break;
	  if (cd[n].md[k - 1 - tn].effect == EF_NONE)
  break;
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_FORGET_SPELL
		, cd[n].md[k - 1 - tn].spell);
	  write_companion(n, 0, wbuf.data(), 0);
	  forget_magic(n, k - 1 - tn);
	  tn++;
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_CREATE:
  case ST_CREATE_:
	for (p = rbuf, tn = 0; tn < 2; tn++) {
	  for (in = 0; *p; p++) {
  if (*p < '0' || *p > '9')
	continue;
  for (i = 0; *p >= '0' && *p <= '9'; p++)
	i = i * 10 + (*p - '0');
  in = i;
  break;
	  }
	  if (cd[n].st == ST_CREATE && tn && !in) {
  for (i = 0; i < MAX_ITEMS; i++)
	if (cd[n].weq != &(cd[n].id[i])
		&& cd[n].id[i].type != DT_NONE)
	  break;
  if (i >= MAX_ITEMS) {
	dmcheck_write(n, DMM_CREATE_ONLY_ONE);
	break;
  }
  check_write(n, "#list\n");
  for (i = 0, j = 0; i < MAX_ITEMS; i++) {
	if (cd[n].weq == &(cd[n].id[i])
		|| cd[n].id[i].type == DT_NONE)
	  continue;
	j++;
	std::snprintf(wbuf.data(), wbuf.size(), "[%2i] %s \n", j, cd[n].id[i].name);
	check_write(n, wbuf.data());
  }
  j++;
  std::snprintf(wbuf.data(), wbuf.size(), "[%2i] CANCEL \n", j);
  check_write(n, wbuf.data());
  check_write(n, "#end-list\n");
  dmcheck_write(n, DMM_CREATE_SELECT2);
  cd[n].st = ST_CREATE_;
  CN_GASIFY(n) = 0x10000;
  check_write(n, "#s-edit\n");
  break;
	  }
	  for (i = 0; i < MAX_ITEMS; i++)
  if (cd[n].id[i].type != DT_NONE) {
	if (cd[n].st == ST_CREATE_ && cd[n].weq == &(cd[n].id[i]))
	  continue;
	in--;
	if (!in)
	  break;
  }
	  if (i >= MAX_ITEMS) {
  dmcheck_write(n, DMM_CREATE_STOP);
  cd[n].st = ST_COMMAND;
  CN_GASIFY(n) = 0;
  break;
	  }
	  if (cd[n].st == ST_CREATE_ || tn) {
  create_item(n, i);
  cd[n].st = ST_COMMAND;
  CN_GASIFY(n) = 0;
  break;
	  }
	  if (cd[n].oeq == &(cd[n].id[i])) {  /* niju soubi kinsi */
  cd[n].oeq = nullptr;
  cd[n].osep = 0;
  cd[n].at = AT_NONE;
	  }
	  if (cd[n].aeq == &(cd[n].id[i])) {
  cd[n].aeq = nullptr;
  cd[n].asep = 0;
	  }
	  cd[n].weq = &(cd[n].id[i]);
	  cd[n].wsep = sep_calc(n, cd[n].weq);
	  item_gc(n);
	  set_fn(n);
	}
	break;
  case ST_CHARGE_SPELL:
	i = analyze_single_number(rbuf);
	if (i<=0)
	{
	  dmcheck_write(n, DMM_NO_NUMBER);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	  break;
	}
	else if ((ic=itemtype_count(n, i, IT_JEWEL)) < 0)
	{
	  dmcheck_write(n, DMM_CHARGESPL_STOP);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	  break;
	}
	if (cd[n].oeq == &(cd[n].id[ic])) { /* niju soubi kinsi */
	  cd[n].oeq = nullptr;
	  cd[n].osep = 0;
	  cd[n].at = AT_NONE;
	}
	if (cd[n].aeq == &(cd[n].id[ic])) {
	  cd[n].aeq = nullptr;
	  cd[n].asep = 0;
	}
	cd[n].weq = &(cd[n].id[ic]);
	cd[n].wsep = sep_calc(n, cd[n].weq);
	item_gc(n);
	set_fn(n);
	if(cd[n].md[0].spell==nullptr){
	  dmcheck_write(n, DMM_CHARGESPL_NOSPL);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	  break;
	}
	check_write(n, "#list\n");
	for (i = 0, j = 0; i < MAX_MAGICS; i++) {
	  if (cd[n].md[i].spell == nullptr)
  break;
	  j++;
	  std::snprintf(wbuf.data(), wbuf.size(), "[%2i] %s \n", j, cd[n].md[i].spell);
	  check_write(n, wbuf.data());
	}
	j++;
	std::snprintf(wbuf.data(), wbuf.size(), "[%2i] CANCEL \n", j);
	check_write(n, wbuf.data());
	check_write(n, "#end-list\n");
	dmcheck_write(n, DMM_CHARGESPL_SELECTSPL);
	cd[n].st = ST_CHARGE_SPELL_;
	CN_GASIFY(n) = 0x10000;
	check_write(n, "#s-edit\n");
	break;
  case ST_CHARGE_SPELL_:
	in = analyze_single_number(rbuf);
	if(in<=0 || 9<=in || cd[n].md[in-1].spell==nullptr){
	  dmcheck_write(n, DMM_CHARGESPL_STOP);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	  break;
	}
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_CHARGESPL,
	  cd[n].md[in-1].spell);
	write_companion(n, 0, wbuf.data(), 0);
	charge_spell(n, in-1);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_DUPLICATE:	/* アイテム複製 */
	i = analyze_single_number(rbuf);
	if((ic=item_count(n, i))<0)
	  dmcheck_write(n, DMM_DUPLICATE_STOP);
	else {
	  for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
	break;
	  }
	  std::memcpy(&(cd[n].id[i]), &(cd[n].id[ic]), sizeof (ItemData));
	  dmcheck_write(n, DMM_DUPLICATE);
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_DESTROY:	  /* アイテム消去 */
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	for(i=0, tn=0, l=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i++];
	  if((ic=item_count(n, k-tn))< 0) break;
	  if((cd[n].id[ic].type & IT_MASK) != IT_FOREIGN) {
  if(is_destroy_companion){
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DESTROY,
	  cd[n].id[ic].name);
	write_companion(n, 0, wbuf.data(), 0);
  }
  delete_item(&(cd[n].id[ic]));
  tn++;
	  } else {
  dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DESTROY_NOBREAK, cd[n].id[ic].name);
  check_write(n, wbuf.data());
	  }
	}
	if (tn == 0) {
	  dmcheck_write(n, DMM_DESTROY_STOP);
	} else {
	  item_gc(n);
	  set_fn(n);
	  if(!is_destroy_companion)
  dmcheck_write(n, DMM_DESTROY_HIDDEN);
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_METAMORPHOSIS:
	for(p = rbuf, i=0; *p && i<7; p++){
	  if(*p<'0' || '9'< *p) continue;
	  mbuf[i]=0;
	  for(;*p; p++){
  if(*p<'0' || '9'<*p) break;
  mbuf[i] = mbuf[i]*10 + (*p-'0');
	  }
	  if(mbuf[i]<0 || 9999<mbuf[i]) break;
	  i++;
	}
	if(i==7 && mbuf[6] <= 99){
	  cd[n].lv[LV_FIGHTER] = mbuf[0];
	  cd[n].lv[LV_WIZARD]  = mbuf[1];
	  cd[n].lv[LV_MERCHANT]= mbuf[2];
	  cd[n].lv[LV_CREATER] = mbuf[3];
	  cd[n].mhp = cd[n].hp = mbuf[4];
	  cd[n].mmp = cd[n].mp = mbuf[5];
	  cd[n].align = mbuf[6];
	  dmcheck_write(n, DMM_METAMORPHOSYS);
	} else
	  dmcheck_write(n, DMM_METAMORPHOSYS_NO);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_MALOR:
	i = cd[n].x;
	j = cd[n].y;
	for (p = rbuf, tn = 0; tn < 2; tn++) {
	  for (; *rbuf; rbuf++) {
  if (*rbuf == '+' || *rbuf == '-')
	p = rbuf;
  if (*rbuf < '0' || *rbuf > '9')
	continue;
  for (in = 0; *rbuf >= '0' && *rbuf <= '9'; rbuf++)
	in = in * 10 + (*rbuf - '0');
  if (*p == '+' || *p == '-') {
	if (*p == '-')
	  in = in * (-1);
	switch (*rbuf) {
	case 'E':
	  i = i + in;
	  break;
	case 'W':
	  i = i - in;
	  break;
	case 'S':
	  j = j + in;
	  break;
	case 'N':
	  j = j - in;
	  break;
	default:
	  if (tn == 0)
		i = i + in;
	  else
		j = j + in;
	  break;
	}
	break;
  } else {
	if (tn == 0)
	  i = in;
	else
	  j = in;
	break;
  }
	  }
	}
	if (i == cd[n].x && j == cd[n].y) {
	  dmcheck_write(n, DMM_MALOR_STOP);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	  return;
	}
	CN_PASSWALL(n) = 0x10000;
	move_chr(n, cd[n].x, cd[n].y, i, j, 1);
	CN_PASSWALL(n) = 0;
	if (cd[n].x == i && cd[n].y == j)
	  dmcheck_write(n, DMM_MALOR);
	else
	  dmcheck_write(n, DMM_MALOR_NO);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
/* added by Qajin 19991021 */
  case ST_BAN:
	  for(i=0;i<BANNEDIPMAX -1;i++)
		if (ip_ban[i].ip == nullptr || ip_ban[i].time < time(nullptr))
		   break;
	  p = rbuf;
	  SKIP_SPACE(p);
	  loadstr(wbuf.data(), &p, ' ',wbuf.size());
	  if(*(p-1) == ' ') {
		ip_ban[i].ip = findstr(wbuf.data());
		SKIP_SPACE(p);
		ip_ban[i].time = time(nullptr) + 60 * std::atoi(p);
		dmprintf(n, 0, wbuf.data(),wbuf.size(),DMM_BAN_TIME, ip_ban[i].ip,std::atoi(p));
	  } else {
		p = rbuf;
		SKIP_SPACE(p);
		loadstr(wbuf.data(), &p, '\n',wbuf.size());
		ip_ban[i].ip = findstr(wbuf.data());
		ip_ban[i].time = ULONG_MAX;
		dmprintf(n, 0, wbuf.data(),wbuf.size(),DMM_BAN, ip_ban[i].ip);
	  }
	  if(wildcmp(ip_ban[i].ip ,ip_address_check(n)) || wildcmp(ip_ban[i].ip ,ip_domain_check(n))){
		dmcheck_write(n,DMM_BAN_SELF);
	  }
	  check_write(n, wbuf.data());
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	break;
  case ST_BAN_:
	  p = rbuf;
	  SKIP_SPACE(p);
	  loadstr(wbuf.data(), &p, '\n', wbuf.size());
	  for(i=0;i<BANNEDIPMAX;i++)
		if (ip_ban[i].ip != nullptr && wildcmp(wbuf.data(),ip_ban[i].ip)) {
			ip_ban[i].ip = nullptr;
			ip_ban[i].time = 0;
		}
	  /* rbuf は main() の buf[1024]。以前は sizeof(rbuf) と書かれて
	     おり、ポインタの大きさ (8) になって返信が先頭 8 バイトに
	     切り詰められていた（2026-11-16 修正）。 */
	  dmprintf(n, 0, rbuf,1024,DMM_BAN_ALLOW,wbuf.data());
	  check_write(n, rbuf);
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	break;
/* ここまで */
  case ST_WRITE:
	if (*rbuf == '.' && (*(rbuf + 1) == '!' || *(rbuf + 1) == 0)) {
	  check_write(n, "#.\n");
  id2f(n,fid.data());
  std::snprintf(wbuf.data(), wbuf.size(), "BoardTMP.%s", fid.data());
	  switch (*(rbuf + 1)) {
	  case '!':
  dmcheck_write(n, DMM_WRITE_STOP);
  if ((fp = fopen(wbuf.data(), "rb")) != nullptr) {
	fclose(fp);
	unlink(wbuf.data());
  }
  break;
	  case 0:
  if ((fp = fopen(wbuf.data(), "rb")) != nullptr) {
	fclose(fp);
	abstime = time(nullptr);
	std::snprintf(wbuf.data(), wbuf.size(), "BoardTMP.%s", fid.data());
	std::snprintf(wbuf2.data(),wbuf2.size(), "phi_docs/B%08lx.%s", abstime, fid.data());
	if (!rename(wbuf.data(),wbuf2.data())) {
/*		unlink(wbuf); delete for MINGW32 20050404 */
	  dmcheck_write(n, DMM_WRITE);
	  std::snprintf(wbuf.data(), wbuf.size(), "%08lx", abstime);
	  save_sd(n, cd[n].x, cd[n].y
		,(map[cd[n].y][cd[n].x].tip & T_MESSAGE ? 'B' : 'T')
		,wbuf.data(), 0);
	} else {
	  dmcheck_write(n, DMM_WRITE_FAIL);
	  dmcheck_write(n, DMM_WRITE_MISS);
	}
  } else
	dmcheck_write(n, DMM_WRITE_NULL);
  break;
	  }
	  cd[n].st = ST_COMMAND;
	  CN_GASIFY(n) = 0;
	} else
	  append_file(n, "BoardTMP", rbuf);
	break;
  case ST_SEARCH:
	p = rbuf + std::strlen(rbuf) - 1;
	if (*p == '\n')
	  *p = 0;
	if (!search_board(n, rbuf))
	  dmcheck_write(n, DMM_SEARCH_NO);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_MAIL:
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_SPELL:
	*(cd[n].com) = 'C';
	CPYSTR(cd[n].str, rbuf);
	cd[n].str[0] = ftoupper(cd[n].str[0]);
	if(n<MAX_NTTYS){
	  if (*rbuf > '0' && *rbuf <= '9') {
  sscanf(rbuf, "%i", &i);
  if (1 <= i && i <= MAX_MAGICS && cd[n].md[i - 1].spell != nullptr) {
	CPYSTR(cd[n].str, cd[n].md[i - 1].spell);
	cd[n].wt = mgstrlen(cd[n].str) * 2 - TCN_HASTE(n);
	cd[n].wt = cd[n].wt * (10000 - cd[n].lv[LV_WIZARD]) / 10000;
	if (cd[n].wt < 5)
	  cd[n].wt = 5;
	cd[n].st = ST_CAST;
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_CAST_NOW, cd[n].str);
	check_write(n, wbuf.data());
	break;
  }
	  }
	  if(is_need_cast_time) {
  for(i=0; i<MAX_MAGICS; i++){
	if(cd[n].md[i].spell == nullptr) continue;
	if(!std::strcmp(cd[n].md[i].spell, cd[n].str)) break;
  }
  if(i<MAX_MAGICS){
	cd[n].wt = std::strlen(cd[n].str) * 2 - TCN_HASTE(n);
	cd[n].wt = cd[n].wt * (10000 - cd[n].lv[LV_WIZARD]) / 10000;
	if (cd[n].wt < 5)
	  cd[n].wt = 5;
	cd[n].st = ST_CAST;
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_CAST_NOW, cd[n].str);
	check_write(n, wbuf.data());
	break;
  }
	  }
	}
	cd[n].st = ST_COMMAND;
	check_write(n, "#end-mg\n");
	if (TCN_SILENCE(n) && (n<MAX_NTTYS || !cd[n].md[0].is_inherence)) {
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),TLK_SPELL, TLK_SILENCE);
	  write_companion(n, 0, wbuf.data(), 0);
	} else {
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),TLK_SPELL, cd[n].str);
	  write_companion(n, 0, wbuf.data(), 0);
	  spell_analyze(n, cd[n].str);
	}
	break;
  case ST_CAST_COMPLETE:
	*(cd[n].com) = 'C';
	cd[n].st = ST_COMMAND;
	check_write(n, "#end-mg\n");
	if (TCN_SILENCE(n)) {
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),TLK_SPELL, TLK_SILENCE);
	  write_companion(n, 0, wbuf.data(), 0);
	} else {
	  dmprintf(n, 0, wbuf.data(), wbuf.size(),TLK_SPELL, cd[n].str);
	  write_companion(n, 0, wbuf.data(), 0);
	  spell_analyze(n, cd[n].str);
	}
	break;
  case ST_JOIN_ANSWER:
	i = cd[n].vf;
	if (*(rbuf+1)=='\0' && (*rbuf == 'Y' || *rbuf == 'y')) {
		dmprintf(i,n,wbuf.data(),wbuf.size(),DMM_JOIN_PARTY);
		if (!cd[i].party) {
			cd[i].party = i;
		}
		cd[n].party = cd[i].party;
		renumber_party(cd[i].party);
		write_companion(n,i,wbuf.data(),3);
	} else {
		dmprintf(i,n,wbuf.data(),wbuf.size(),DMM_JOIN_NO);
		write_companion(n,i,wbuf.data(),0);
	}
	cd[n].st = ST_COMMAND;
	cd[i].st = ST_COMMAND;
	CN_GASIFY(n) = CN_GASIFY(i) = 0;
	break;
  case ST_COMMAND:
  case ST_ATTACK:
  case ST_DEFEND: /* add 20010703 */
	if (TCN_CONFUSE(n) && n < MAX_NTTYS) {
	  for (i = 0, j = std::strlen(rbuf); i < j / 2; i++) {
  wbuf[0] = *(rbuf + i);
  *(rbuf + i) = *(rbuf + j - i - 1);
  *(rbuf + j - i - 1) = wbuf[0];
	  }
	  if (*rbuf > '0' && *rbuf <= '9')
  *rbuf = ('9' - *rbuf) + '0';
	}
	command_analyze(n, rbuf); /* change for lose talk & npc dead psychogram 20001212 */
	break;
  case ST_UNEQUIP:
	size = analyze_numbers(numbers, MAX_ANALYZE_INPUT_NUMBERS, rbuf);
	wn = an = on = 0;
	if(cd[n].weq !=nullptr) wn = an = on = 1;
	if(cd[n].aeq !=nullptr) an = on = wn + 1;
	if(cd[n].oeq !=nullptr) on = an + 1;
	for(i=0; i<size;){
	  if(numbers[i]==-1) k++;
	  else k=numbers[i++];
	  if (k >= 4 || k <= 0) break;
	  if (k == wn) {
  dmprintf(n, 0, wbuf.data(),wbuf.size(),DMM_UNEQUIP_WEAPON);
  cd[n].weq = nullptr;
  cd[n].wsep = 0;
  set_fn(n);
  write_companion(n, 0, wbuf.data(), 0);
	  } else if (k == an) {
  dmprintf(n, 0, wbuf.data(),wbuf.size(),DMM_UNEQUIP_ARMOR);
  cd[n].aeq = nullptr;
  cd[n].asep = 0;
  set_fn(n);
  write_companion(n, 0, wbuf.data(), 0);
	  } else if (k == on) {
  dmprintf(n, 0, wbuf.data(),wbuf.size(),DMM_UNEQUIP_ACCESSORY);
  cd[n].oeq = nullptr;
  cd[n].at = AT_NONE;
  cd[n].osep = 0;
  set_fn(n);
  write_companion(n, 0, wbuf.data(), 0);
	  }
	}
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_ADDRESS:
	for(i=0;i<48 && rbuf[i] != '\0' && rbuf[i] != '@';i++);
	if (rbuf[i] != '@') {
		dmcheck_write(n, DMM_ADDRESS_REENTER);
		break;
	}
	CPYSTR(cd[n].str, rbuf);
	dmcheck_write(n, DMM_ADDRESS_TRANS);
	cd[n].st = ST_ADDRESS_;
	break;
  case ST_ADDRESS_:
	if (*(rbuf+1)=='\0' && *rbuf != 'Y' && *rbuf != 'y') {
		CPYSTR(cd[n].adr,cd[n].str);
	} else {
		std::snprintf(wbuf.data(), wbuf.size(),"/*@NP:%s*/",cd[n].str);
		CPYSTR(cd[n].adr,wbuf.data());
	}
	dmcheck_write(n, DMM_ADDRESS);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
  case ST_MENU:
	select_menu(n, rbuf);
	break;
/* 一般公開するわけには行きません(^^;)
  case ST_RENAME:
	i = 0;
	while (fisspace(rbuf[i]))
	  i++;
	j = i;
	if (strlen(rbuf + j) <= 1) {
	  dmcheck_write(n, DMM_RENAME_NOLONGER);
	  break;
	}
	if (strlen(rbuf + j) >= 32) {
	  dmcheck_write(n, DMM_RENAME_NOSHORTER);
	  break;
	}
	CPYSTR(cd[n].str, rbuf+i);
	cd[n].str[0] = ftoupper(cd[n].str[0]);
	cd[n].name = findstr(cd[n].str);
	dmcheck_write(n, DMM_RENAME);
	cd[n].st = ST_COMMAND;
	CN_GASIFY(n) = 0;
	break;
*/
  default:
	break;
  }
  return;
}

void   auto_equip(int n)
{
  int	 i,j;
  std::array<char, 256> buf;

  cd[n].weq = nullptr;
  cd[n].aeq = nullptr;
  cd[n].oeq = nullptr;
  cd[n].wsep = 0;
  cd[n].asep = 0;
  cd[n].osep = 0;
  cd[n].at = AT_NONE;
  for (j = i = 0; i < MAX_ITEMS; i++) {
	if (cd[n].id[i].type == DT_NONE)
	  continue;
	if ((cd[n].id[i].type & IT_MASK) == IT_ACCESSORY) {
	  if (cd[n].oeq == nullptr) {
  cd[n].oeq = &(cd[n].id[i]);
  cd[n].at = cd[n].oeq->AT;
  cd[n].osep = sep_calc(n, cd[n].oeq);
  j |= 4;
	  }
	} else if ((cd[n].id[i].type & IT_MASK) == IT_ARMOR) {
	  if (cd[n].aeq == nullptr || cd[n].id[i].DFP + cd[n].id[i].MRP
	> cd[n].aeq->DFP + cd[n].aeq->MRP) {
  cd[n].aeq = &(cd[n].id[i]);
  cd[n].asep = sep_calc(n, cd[n].aeq);
  j |= 2;
	  }
	} else {
	  if (cd[n].weq == nullptr || cd[n].id[i].ATP > cd[n].weq->ATP) {
  cd[n].weq = &(cd[n].id[i]);
  cd[n].wsep = sep_calc(n, cd[n].weq);
  j |= 1;
	  }
	}
  }
  switch (j) {
	case 0: /* nothing */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_NUL);
	  break;
	case 1: /* weapon */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_WPN);
	  break;
	case 2: /* armor */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_NUL_ARM);
	  break;
	case 3: /* wpn & arm */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_WPN_ARM);
	  break;
	case 4: /* accessory */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_NUL_ACC);
	  break;
	case 5: /* wpn & acc */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_WPN_ACC);
	  break;
	case 6: /* arm & acc */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_NUL_ARM_ACC);
	  break;
	case 7: /* all */
	  dmprintf(n,0,buf.data(),buf.size(),DMM_EQUIP_WPN_ARM_ACC);
	  break;
  }
  set_fn(n);
  write_companion(n, 0, buf.data(), 0);
}

void   itemlist(char *buf, int n, ItemData * id, int j, int siz)
{
  int	 k, l, m;
  /* buf2 は CATSTR の宛先のため C 配列のまま */
  char	 buf2[70]; /* bug fix 20000802 */

  l = m = 0;
  for (k = 0; k < (int) std::strlen(id->name); k++) {
	if (id->name[k] == '/' &&
  id->name[k + 1] == '*') {
	  m = -1;
	}
	if (m)
	  l++;
	if (id->name[k] == '*' &&
  id->name[k + 1] == '/') {
	  l++;
	  m = 0;
	}
  }
  l += 31 - std::strlen(id->name);
  std::snprintf(buf, siz-1, "     [/*color=cyan*/%2i/*.*/] %s", j, id->name);
  m = std::strlen(buf);
  for (k = 0; k < l; k++) {
	buf[m++] = ' ';
  }
  buf[m] = '\0';
  std::snprintf(buf2, sizeof(buf2),"Wp  : %2d ", id->wp);
  if (id == cd[n].oeq)
	CATSTR(buf2, "[/*color=blue*/Acr/*.*/] ");
  else if (id == cd[n].aeq)
	CATSTR(buf2, "[/*color=+hp*/Arm/*.*/] ");
  else if (id == cd[n].weq)
	CATSTR(buf2, "[/*color=red*/Wpn/*.*/] ");
  else
	CATSTR(buf2, "      ");
  if (is_need_charge_time
	  && (id->type & IT_MASK) == IT_CHARGE
	  && id->active_time > time(nullptr))
	CATSTR(buf2, "(/*color=blue*/charging/*.*/) ");
  CATSTR(buf2, "\n");

  strlcat(buf, buf2, siz);
}

int    command_analyze(int n, char *com)
{
  /* buf は CATSTR の宛先のため C 配列のまま */
  char   buf[256];
  char   buf2[20];
  std::array<char, 32> bufa;
  std::array<char, 256> bufb; /* Add for メールシステム by NEO 2008/05/02 */
  std::array<char, 16> crgr; /* Add for キャラグラ設定 by NEO */
  int    x, y;
  int    i, j, k, l;
  int    tlv;
  std::array<char, 256> homeland; /* 出身地表示 by NEO 20111112 */

  i = 0;
  if (*com == 0)
	return (0);
  if (!std::strcmp(com, "exit") ||
	 !std::strcmp(com, "quit")) {
	for (i = 0; i <= NORMAL_CN; i++)
	  if (cd[n].cn[i])
  break;
	if (i <= NORMAL_CN) {
	  dmcheck_write(n, DMM_EXIT_BAD_CONDITION);
	  return (0);
	}
	if (is_need_charge_time){
	  for(i=0; i<MAX_ITEMS; i++)
  if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
	 && cd[n].id[i].active_time > time(nullptr))
	break;
	  if(i < MAX_ITEMS){
  dmcheck_write(n, DMM_EXIT_CHARGING);
  return 0;
	  }
	}
	std::snprintf(buf, sizeof(buf), "exit     : %s ", cd[n].name);
	log_print(buf);
	dmcheck_write(n, DMM_SAVEDATA);
	dmcheck_write(n, DMM_SEEYOU);
	check_write(n, "#close\n");
	dmprintf(n, 0, buf, sizeof(buf),DMM_EXIT); /* 先にcloseしちゃうとキャラ名が拾えない ^^; */
	nfflush(n, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
	close_connect(n);
	write_companion(n, 0, buf, access_dm);
	return (0);
  } else if (!std::strcmp(com, "see")) {
	reflesh_last_map[n] = 1;
	print_map(n, 0);
	return (0);
  } else if (!std::strcmp(com, "look")) {
	reflesh_last_map[n] = 1;
	print_map(n, MD_BOARD | MD_FLOORITEM);
	return (0);
  } else if (!std::strcmp(com, "check")) {
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_NAME);
    check_write(n, buf);
/*  位置合わせ追加 20000923  "gp"-"MP"(2)+null分(1)
    i = mgstrlen(cirunit)-2+1;  
    nstr(buf2,' ',i);  
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_HPMP, buf2);  delete by NEO 2008/04/02 */
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_HPMP);
    check_write(n, buf);
/*  cirunit[0] = ftoupper(buf2[0] = cirunit[0]); delete by NEO 2008/04/02 */
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_EXPGP);
/*  cirunit[0] = buf2[0]; delete by NEO 2008/04/02 */
    check_write(n, buf);
    
    readhomeland(n, homeland.data()); /*  出身地表示 by NEO 20111112 */
    
    /* 出身地表示 by NEO 20111112 */
    std::snprintf(buf, sizeof(buf),DMM_CHECK_HLAND,homeland.data());
    check_write(n, buf);
    /* ここまで */
    
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_LAND);
    check_write(n, buf);
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_AREA);
    check_write(n, buf);
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_LV);
    check_write(n, buf);
	/* トータルレベル表示 by NEO 20050302 */
    tlv = TOTAL_LEVEL / 2000;
        if (tlv > 999) {
      tlv = 999;
    }
    std::snprintf(buf, sizeof(buf),DMM_TOTAL_LV,tlv);
    check_write(n, buf);
    /* by normal */
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_FLVWLV);
    check_write(n, buf);
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_MLVCLV);
    check_write(n, buf);
    dmprintf(n, 0, buf, sizeof(buf),DMM_CHECK_ITEMS);
    check_write(n, buf);
    for (i = 0, j = 0; i < MAX_ITEMS; i++) {
      if (cd[n].id[i].type == DT_NONE)
  continue;
      j++;
      itemlist(buf, n, &(cd[n].id[i]), j, sizeof(buf));
      check_write(n, buf);
    }
    return (0);
  } else if (!std::strcmp(com, "level")) { /* level追加 by NEO 2008/04/05 */
	if (TOTAL_LEVEL > 9999) {  
	next_level_check(n);
	} else {
       dmprintf(n, 0, buf, sizeof(buf),DMM_ERR_LEVEL);
	   check_write(n, buf);
	}
    return (0);
  } else if (!std::strcmp(com, "wizlist")) { /* wizlist追加 by NEO 20050316 */
    user_lists(n);
    return (0);
  } else if (!std::strcmp(com, "wizeye")) { /* wizeye追加 by NEO 20050803 */
	wizard_eye_f(n, 0);
    return (0);
  } else if(!std::strncmp(com, "setcg ", 6)) { /* setcg追加 by NEO 20050316 */
    SKIP_TO_SPACE(com);
    SKIP_SPACE(com);
    sscanf(com, "%s", crgr.data());
	if (TOTAL_LEVEL > 1999) {  /* キャラグラ設定条件を変更 cd[n].exp→TOTAL_LEVEL 2008/04/05 */
		if (twobyte_check(crgr.data()) ==0 || std::strcmp(crgr.data(),"__") == 0 || std::strstr(crgr.data(),"\\") != nullptr || std::strstr(crgr.data(),":") != nullptr || std::strstr(crgr.data(),"%") != nullptr) {
       dmprintf(n, 0, buf, sizeof(buf),DMM_CG_ERR_NOUSE);
	   check_write(n, buf); }
	   else {
	   charagra_set(n,crgr.data()); }
	}
	else {
    dmprintf(n, 0, buf, sizeof(buf),DMM_ERR_LEVEL);
    check_write(n, buf);
	}
	return (0);
/* Add for メールシステム */
	} else if(!std::strncmp(com, "sendmail ", 9)) { /* メール書き込みコマンド */
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	sscanf(com, "%s", buf);

	j = std::strlen(buf);
	
	if ( j > 128 ) { /* メッセージの長さチェック */
		dmcheck_write(n, DMM_MAIL_LONG);
		return (0);
	}    

	i = strindex( buf, ":" );
	if (i == -1 || i > 31) {
		dmcheck_write(n, DMM_MAIL_WRONG);
		return(0);
	}
	
	std::strncpy(bufa.data(), buf, i ); /* 宛先を抽出 */
	if (i > 0)
    	bufa[i]= '\0';
	std::strncpy(bufb.data(), buf + i + 1 , j - i ); /* メッセージを抽出 */
	if ( save_mail( n, bufa.data(), bufb.data() ) == -1 ) /* メッセージ書き込み＆チェック */
		dmcheck_write(n, DMM_MAIL_NULL);

	return (0);
	} else if (!std::strcmp(com, "readmail")) { /* メール読み込みコマンド */
    read_mail(n);
    return (0);
	} else if (!std::strcmp(com, "delmail")) { /* メール消去コマンド */
    delete_mail(n);
    return (0);
/* ここまで　by NEO 2008/05/02 */
  } else if (!std::strcmp(com, "board send")) {
	cd[n].modef ^= MD_BOARDSEND;
	dmcheck_write(n,(char*)((cd[n].modef & MD_BOARDSEND)? DMM_SENDBOARD : DMM_SENDBOARD_NO ));
	return (0);
  } else if (!std::strcmp(com, "board")) {
	cd[n].modef ^= MD_BOARD;
	dmcheck_write(n,(char*)((cd[n].modef & MD_BOARD)? DMM_BOARD : DMM_BOARD_NO ));
	return (0);
  } else if (!std::strcmp(com, "floor item")) {
	cd[n].modef ^= MD_FLOORITEM;
	dmcheck_write(n,(char*)((cd[n].modef & MD_FLOORITEM)? DMM_FLOORITEM : DMM_FLOORITEM_NO));
	return 0;
  } else if (!std::strcmp(com, "chat")) {
	cd[n].modef ^= MD_CHAT;
	dmcheck_write(n,(char*)((cd[n].modef & MD_CHAT)? DMM_CHAT : DMM_CHAT_NO));
	return 0;
  } else if (!std::strncmp(com, "mail", 4) && CHKSP(com + 4)) {
	dmcheck_write(n, DMM_MAIL);
	return (0);
  } else if (!std::strcmp(com, "spells")) {
	dmprintf(n, 0, buf, sizeof(buf),DMM_SPELLS_SPELL);
	check_write(n, buf);
	for (i = 0, j = 0; i < MAX_MAGICS; i++) {
	  if (cd[n].md[i].effect == EF_NONE)
  break;
	  j++;
	  std::snprintf(buf, sizeof(buf), "     [/*color=cyan*/%2i/*.*/] %s \n", j, cd[n].md[i].spell);
	  check_write(n, buf);
	}
	return (0);
  } else if (!std::strcmp(com, "version")) {
	dmprintf(n, 0, buf, sizeof(buf),DMM_DISP_VERSION);
	check_write(n, buf);
	return (0);
  } else if (!std::strcmp(com, "address")) {
	if (!std::strncmp(cd[n].adr,"/*@NP:",6)) {
		CPYSTR(cd[n].str,cd[n].adr+6);
		cd[n].str[std::strlen(cd[n].str)-2] = '\0';
	} else {
		CPYSTR(cd[n].str,cd[n].adr);
	}
	dmprintf(n, 0, buf, sizeof(buf), DMM_ADDRESS_PRINT, cd[n].str);
	check_write(n,buf);
	dmcheck_write(n,DMM_ADDRESS_INPUT);
	cd[n].st = ST_ADDRESS;
	CN_GASIFY(n) = 0x10000;
	return (0);
  } else if (!std::strcmp(com, "dmsetupcheck") && (cd[n].exp == 99999999)) { /* DM設定チェックコマンド added by NEO 2011/12/6 */
        dmprintf(n, 0, buf, sizeof(buf), DMM_CHECK_DPB, dp_beginner); /* 初心者の加護設定表示 */
        check_write(n, buf);
        if (counter_effect == 0) { /* カウンター効果係数が0だとカウンター処理しない added by NEO 2011/12/10 */
        dmprintf(n, 0, buf, sizeof(buf), DMM_CHECK_CEN);
        check_write(n, buf);
        } else {
        dmprintf(n, 0, buf, sizeof(buf), DMM_CHECK_CE, counter_effect); /* カウンター効果設定表示 */
        check_write(n, buf);
        }
        dmprintf(n, 0, buf, sizeof(buf), DMM_CHECK_AE, attack_effect); /* 新攻撃効果設定表示 */
        check_write(n, buf);
        dmprintf(n, 0, buf, sizeof(buf), DMM_CHECK_ABS, axe_blade_setting); /* 斧＆ブレード補正設定表示 added by NEO 2011/12/8 */
        check_write(n, buf);
	return (0);
/* 一般公開するわけには行きません(^^;)
  } else if (!strcmp(com, "rename")) {
	dmcheck_write(n,DMM_RENAME_INPUT);
	cd[n].st = ST_RENAME;
	CN_GASIFY(n) = 0x10000;
	return (0);
*/
  }

  /* add for switchable command 20041216 */
  if (check_and_process_cmd(n,'C',com)) {
	return(0);
  }

  /* add for new commands 20010618 */
  if (!command_analyze_ex(n,com))
	return (0);

  CN_GASIFY(n) = 0;
  if (!std::strncmp(com, "turn", 4) && CHKSP(com + 4)) {
	if (TCN_PALSY(n) || cd[n].st == ST_DEFEND) {
	  dmcheck_write(n, DMM_MOVE_NO);
	  return (0);
	}
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	i = cd[n].d;
	switch (*com) {
	case 'l':
	  i--;
	  break;
	case 'r':
	  i++;
	  break;
	case 'b':
	  i += 2;
	  break;
	case 'n':
	  i = 0;
	  break;
	case 'e':
	  i = 1;
	  break;
	case 's':
	  i = 2;
	  break;
	case 'w':
	  i = 3;
	  break;
	default:
	  dmcheck_write(n, DMM_TURN_BAD);
	  return (0);
	}
	if (i < 0)
	  i += 4;
	else if (i > 3)
	  i -= 4;
	cd[n].d = i;
	print_map(n, 0);
	*(cd[n].com) = *com;
	/* add turn check switch 20001201 */
	if (map[cd[n].y][cd[n].x].tip & T_SWITCH)
	  load_switch(n, cd[n].x, cd[n].y, 15);
	return (0);
  } else if (!std::strncmp(com, "go", 2) && CHKSP(com + 2)) {
	if (TCN_PALSY(n) || cd[n].st == ST_DEFEND) {
	  dmcheck_write(n, DMM_MOVE_NO);
	  return (0);
	}
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	switch (*com) {
	default:
	  x = cd[n].x + px[cd[n].d];
	  y = cd[n].y + py[cd[n].d];
	  *(cd[n].com) = 'F';
	  break;
	case 'r':
	  x = cd[n].x - py[cd[n].d];
	  y = cd[n].y + px[cd[n].d];
	  *(cd[n].com) = 'R';
	  break;
	case 'b':
	  x = cd[n].x - px[cd[n].d];
	  y = cd[n].y - py[cd[n].d];
	  *(cd[n].com) = 'B';
	  break;
	case 'l':
	  x = cd[n].x + py[cd[n].d];
	  y = cd[n].y - px[cd[n].d];
	  *(cd[n].com) = 'L';
	  break;
	case 'N':
	  cd[n].d = 0;
	case 'n':
	  x = cd[n].x;
	  y = cd[n].y - 1;
	  *(cd[n].com) = 'N';
	  break;
	case 'E':
	  cd[n].d = 1;
	case 'e':
	  x = cd[n].x + 1;
	  y = cd[n].y;
	  *(cd[n].com) = 'E';
	  break;
	case 'W':
	  cd[n].d = 3;
	case 'w':
	  x = cd[n].x - 1;
	  y = cd[n].y;
	  *(cd[n].com) = 'W';
	  break;
	case 'S':
	  cd[n].d = 2;
	case 's':
	  x = cd[n].x;
	  y = cd[n].y + 1;
	  *(cd[n].com) = 'S';
	  break;
	}
	move_chr(n, cd[n].x, cd[n].y, x, y, 1);
	return (0);
  } else if (!std::strncmp(com, "pay", 3) && CHKSP(com + 3)) {
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if (*com >= '0' && *com <= '9') {
	  com[4] = '\0'; /* bug fix for over int 20031011 */
	  sscanf(com, "%i", &j);
	  if (cd[n].gp < j)
  dmcheck_write(n, DMM_PAY_NO);
	  else {
	/* change for variable multi characters in one space 20000721 */
		k = 0;
		for (x = 0; x < MAX_CHRS_INONE; x++) {
  l = map[cd[n].y][cd[n].x].chr[x] - 1;
  if (l >= 0 && l != n) {
	i = l;
	k++;
  }
		}
  if (k == 0) {
	if (map[cd[n].y][cd[n].x].tip & T_SWITCH) {
	  CPYSTR(cd[n].str, com);
	  load_switch(n, cd[n].x, cd[n].y, 5);
	} else
	  dmcheck_write(n, DMM_PAY_NOBODY);
	return (0);
  }
  if (k > 1) {
	dmcheck_write(n, DMM_PAY_MANY);
	return (0);
  }
  if (cd[i].type > CT_NORMAL) {
	dmcheck_write(n, DMM_PAY_NO);
	return (0);
  }
  /* add for gift tax 20001204 */
  if (gift_tax) {
	if (cd[n].gp < j * 10 / 9) {
	  dmcheck_write(n, DMM_PAY_NO_WITH_TAX);
	  return(0);
	}
	cd[n].gp -= j * 10 / 9;
	cd[i].gp += j;
	tax += (j * 10 / 9 - j);
  } else {
	cd[n].gp -= j;
	cd[i].gp += j * 9 / 10;
	tax += (j- (j * 9 / 10));
  }
  if (cd[i].gp > 9999){
	tax+= (cd[i].gp-9999);
	cd[i].gp = 9999;
  }
  if (!gift_tax && PER_DICE < j) /* change with gift tax :-P */
	lv_up(i, n, LV_MERCHANT, 0);
  if (i >= MAX_NTTYS && cd[i].align >= 50 && cd[n].align < 99)
	cd[n].align++;
  dmprintf(n, i, buf, sizeof(buf),DMM_PAY
	, j);
  write_companion(n, i, buf, 0);
	  }
	} else
	  dmcheck_write(n, DMM_PAY_BAD);
	return (0);
  } else if (!std::strcmp(com, "read")) {
	cd[n].vf = 0;
	cd[n].vf2 = 1;
	check_write(n, "#list\n");
	i = list_board(n, 1);
	check_write(n, "#end-list\n");
	if (map[cd[n].y][cd[n].x].tip & T_MESSAGE && i > 0) {
	  dmcheck_write(n, DMM_READ_SELECT);
	  cd[n].st = ST_READ;
	  CN_GASIFY(n) = 0x10000;
	  check_write(n, "#s-edit\n");
	} else
	  dmcheck_write(n, DMM_NO_BOARD);
	return (0);
  } else if (!std::strcmp(com, "write")) {
	  if (fld[map[cd[n].y][cd[n].x].area].flag & FF_WRITE) {
  dmcheck_write(n, DMM_WRITE_NO);
  return (0);
	  }
	dmprintf(n, 0, buf, sizeof(buf),DMM_WRITE_OTHER);
	write_companion(n, 0, buf, 0);
	dmcheck_write(n, DMM_WRITE_START);
	cd[n].st = ST_WRITE;
	CN_GASIFY(n) = 0x10000;
	check_write(n, "#m-edit\n");
	return (0);
  } else if (!std::strcmp(com, "erase")) {
	cd[n].vf = 0;
	cd[n].vf2 = 1;
	check_write(n, "#list\n");
	i = list_board(n, 1);
	check_write(n, "#end-list\n");
	if (map[cd[n].y][cd[n].x].tip & T_MESSAGE) {
	  if (i) {
  dmcheck_write(n, DMM_ERASE_SELECT);
  cd[n].st = ST_ERASE;
  CN_GASIFY(n) = 0x10000;
  check_write(n, "#s-edit\n");
	  } else if (save_sd(n, cd[n].x, cd[n].y, 'T', nullptr, 1)<0) {
  dmprintf(n, 0, buf, sizeof(buf),DMM_ERASE_ALL);
  write_companion(n, 0, buf, 0);
	  } else
  dmcheck_write(n, DMM_NO_OWNER);
	} else
	  dmcheck_write(n, DMM_NO_BOARD);
	return (0);
  } else if (!std::strcmp(com, "title")) {
	cd[n].vf = 0;
	cd[n].vf2 = 1;
	check_write(n, "#list\n");
	i = list_board(n, 1);
	check_write(n, "#end-list\n");
	if (map[cd[n].y][cd[n].x].tip & T_MESSAGE) {
	  if (i) {
  dmcheck_write(n, DMM_TITLE_SELECT);
  cd[n].st = ST_TITLE;
  CN_GASIFY(n) = 0x10000;
  check_write(n, "#s-edit\n");
	  } else
  dmcheck_write(n, DMM_NO_MESSAGE);
	} else
	  dmcheck_write(n, DMM_NO_BOARD);
	return (0);
  } else if (!std::strncmp(com, "get", 3) && CHKSP(com + 3)) {
	for (i = 0; i < MAX_ITEMS; i++)
	  if (cd[n].id[i].type == DT_NONE)
  break;

	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if (*com) {
	  *com = ftoupper(*com);
	  /* add for storehouse 20020823 */
	  if ((map[cd[n].y][cd[n].x].tip & TMASK) == T_STORE) {
		if (!save_store(n,0,0,com)) {
		  dmprintf(n, 0, buf, sizeof(buf),DMM_GET
			, cd[n].str);
		  write_companion(n, 0, buf, 0);
		  load_switch(n, cd[n].x, cd[n].y, 18);
		} else
		  dmcheck_write(n, DMM_GET_BAD);
	} else {
	  i = item_number_on_floor(n, com, -1);
	  if (i) {
  if (save_id(n, i, 0))
	dmcheck_write(n, DMM_GET_NO);
  else {
	dmprintf(n, 0, buf, sizeof(buf),DMM_GET
	  , cd[n].str);
	write_companion(n, 0, buf, 0);
	load_switch(n, cd[n].x, cd[n].y, 18); /* for take switch 20000803 */
  }
	  } else
  dmcheck_write(n, DMM_GET_BAD);
  }
	} else {
	  cd[n].vf = 0;
	  cd[n].vf2 = 1;
	  check_write(n, "#list\n");
	  i = list_item(n, 1);
	  check_write(n, "#end-list\n");
	  if (i) {
  dmcheck_write(n, DMM_GET_SELECT);
  cd[n].st = ST_GET;
  CN_GASIFY(n) = 0x10000;
  check_write(n, "#s-edit\n");
	  } else
  dmcheck_write(n, DMM_NO_ITEM_HERE);
	}
	return (0);
  } else if (!std::strncmp(com, "put", 3) && CHKSP(com + 3)) {
	for (i = 0; i < MAX_ITEMS; i++)
	  if (cd[n].id[i].type != DT_NONE)
  break;
	if (i >= MAX_ITEMS) {
	  dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
	  return (0);
	}
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if (*com) {
	  *com = ftoupper(*com);
	  for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
	continue;
  if (!std::strcmp(cd[n].id[i].name, com))
	break;
	  }
	  if (i >= MAX_ITEMS)
  dmcheck_write(n, DMM_PUT_BAD);
	  else {
  if (save_id(n, i, 1))
	dmcheck_write(n, DMM_PUT_NO);
  else {
	dmprintf(n, 0, buf, sizeof(buf),DMM_PUT
	  , cd[n].str);
	write_companion(n, 0, buf, 0);
	item_gc(n);
	set_fn(n);
	if (map[cd[n].y][cd[n].x].tip & T_SWITCH)
	  load_switch(n, cd[n].x, cd[n].y, 4);
  }
	  }
	} else {
	  list_inv(n);
	  cd[n].st = ST_PUT;
	  CN_GASIFY(n) = 0x10000;
	}
	return (0);
  } else if (!std::strncmp(com, "use", 3) && CHKSP(com + 3)) {
	for (i = 0; i < MAX_ITEMS; i++)
	  if (cd[n].id[i].type != DT_NONE)
  break;
	if (i >= MAX_ITEMS) {
	  dmcheck_write(n, DMM_NO_ITEM_INVESTORY);
	  return (0);
	}
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if (*com) {
	  *com = ftoupper(*com);
	  for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
	continue;
  if (!std::strcmp(cd[n].id[i].name, com))
	break;
	  }
	  if (i >= MAX_ITEMS)
  dmcheck_write(n, DMM_PUT_BAD);
	  else {
  if ((cd[n].id[i].type & IT_MASK) == IT_CHARGE
	  && (fld[map[cd[n].y][cd[n].x].area].flag & FF_CHARGE)) {
	dmcheck_write(n, DMM_USE_NO_CHARGE);
	return (0);
  }
  dmprintf(n, 0, buf, sizeof(buf),DMM_USE_ITEM
	, cd[n].id[i].name);
  write_companion(n, 0, buf, 0);
  (void) use_item(n, i);
	  }
	} else {
	  check_write(n, "#list\n");
	  for (i = 0, j = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
	continue;
  j++;
  std::snprintf(buf, sizeof(buf), "[%2i] %-31s ", j, cd[n].id[i].name);
  if(is_need_charge_time && cd[n].id[i].active_time > time(nullptr))
	CATSTR(buf, "(charging) ");
  CATSTR(buf,"\n");
  check_write(n, buf);
	  }
	  check_write(n, "#end-list\n");
	  dmcheck_write(n, DMM_USE_SELECT);
	  cd[n].st = ST_USE;
	  CN_GASIFY(n) = 0x10000;
	  check_write(n, "#s-edit\n");
	}
	return (0);
  } else if (!std::strncmp(com, "equip", 5) && CHKSP(com + 5)) {
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if (*com) {
	  *com = ftoupper(*com);
	  for (i = 0; i < MAX_ITEMS; i++) {
  if (cd[n].id[i].type == DT_NONE)
	continue;
  if (!std::strcmp(cd[n].id[i].name, com)) {
	if (cd[n].weq == &(cd[n].id[i])
		|| cd[n].aeq == &(cd[n].id[i])
		|| cd[n].oeq == &(cd[n].id[i]))
	  continue;
	if ((cd[n].id[i].type & IT_MASK) == IT_ACCESSORY) {
	  cd[n].oeq = &(cd[n].id[i]);
	  cd[n].at = cd[n].oeq->AT;
	  cd[n].osep = sep_calc(n, cd[n].oeq);
	} else if ((cd[n].id[i].type & IT_MASK) == IT_ARMOR) {
	  cd[n].aeq = &(cd[n].id[i]);
	  cd[n].asep = sep_calc(n, cd[n].aeq);
	} else {
	  cd[n].weq = &(cd[n].id[i]);
	  cd[n].wsep = sep_calc(n, cd[n].weq);
	}
	dmprintf(n, 0, buf, sizeof(buf),DMM_EQUIP
	  , cd[n].id[i].name);
	write_companion(n, 0, buf, 0);
	set_fn(n);
	return (0);
  }
	  }
	  dmcheck_write(n, DMM_EQUIP_NO);
	} else {
	  auto_equip(n);
	}
	return (0);
  } else if (!std::strcmp(com, "unequip")) {
	j = 0;
	if (cd[n].weq != nullptr)
	  j++;
	if (cd[n].aeq != nullptr)
	  j++;
	if (cd[n].oeq != nullptr)
	  j++;
	if (j == 0) {
	  dmcheck_write(n, DMM_UNEQUIP_NO);
	  return (0);
	}
	check_write(n, "#list\n");
	j = 0;
	if (cd[n].weq != nullptr) {
	  j++;
	  itemlist(buf, n, cd[n].weq, j, sizeof(buf));
	  check_write(n, buf);
	}
	if (cd[n].aeq != nullptr) {
	  j++;
	  itemlist(buf, n, cd[n].aeq, j, sizeof(buf));
	  check_write(n, buf);
	}
	if (cd[n].oeq != nullptr) {
	  j++;
	  itemlist(buf, n, cd[n].oeq, j, sizeof(buf));
	  check_write(n, buf);
	}
	check_write(n, "#end-list\n");
	dmcheck_write(n, DMM_UNEQUIP_SELECT);
	cd[n].st = ST_UNEQUIP;
	CN_GASIFY(n) = 0x10000;
	check_write(n, "#s-edit\n");
	return (0);
  } else if (!std::strcmp(com, "sort")) {
	item_gc(n);
	i = 0;
	if (cd[n].weq != nullptr) {
	  for (j = i + 1; j < MAX_ITEMS; j++) {
  if (cd[n].weq == &(cd[n].id[j])) {
	swap_item(&(cd[n].id[i]), &(cd[n].id[j]));
	if (cd[n].aeq == &(cd[n].id[i]))
	  cd[n].aeq = &(cd[n].id[j]);
	if (cd[n].oeq == &(cd[n].id[i]))
	  cd[n].oeq = &(cd[n].id[j]);
	cd[n].weq = &(cd[n].id[i]);
	break;
  }
	  }
	  i++;
	}
	if (cd[n].aeq != nullptr) {
	  for (j = i + 1; j < MAX_ITEMS; j++) {
  if (cd[n].aeq == &(cd[n].id[j])) {
	swap_item(&(cd[n].id[i]), &(cd[n].id[j]));
	if (cd[n].weq == &(cd[n].id[i]))
	  cd[n].weq = &(cd[n].id[j]);
	if (cd[n].oeq == &(cd[n].id[i]))
	  cd[n].oeq = &(cd[n].id[j]);
	cd[n].aeq = &(cd[n].id[i]);
	break;
  }
	  }
	  i++;
	}
	if (cd[n].oeq != nullptr) {
	  for (j = i; j < MAX_ITEMS; j++) {
  if (cd[n].oeq == &(cd[n].id[j])) {
	swap_item(&(cd[n].id[i]), &(cd[n].id[j]));
	if (cd[n].weq == &(cd[n].id[i]))
	  cd[n].weq = &(cd[n].id[j]);
	if (cd[n].aeq == &(cd[n].id[i]))
	  cd[n].aeq = &(cd[n].id[j]);
	cd[n].oeq = &(cd[n].id[i]);
	break;
  }
	  }
	  i++;
	}
	x = ++i;
	for (; i < MAX_ITEMS; i++) {
	  if (cd[n].id[i].type == DT_NONE)
  break;
	  j = i;
	  while (j >= x && ((cd[n].id[j - 1].type & IT_MASK) > (cd[n].id[j].type & IT_MASK))) {
  swap_item(&(cd[n].id[j - 1]), &(cd[n].id[j]));
  j--;
	  }
	}
	dmprintf(n, 0, buf, sizeof(buf),DMM_SORT);
	write_companion(n, 0, buf, 0);
	return (0);
  } else if (!std::strcmp(com, "cast")) {
	if (CN_REFLEX(n) & 0xffff) {
	  dmcheck_write(n, DMM_CAST_NO_REFREX);
	  return (0);
	}
	if(n>=MAX_NTTYS && cd[n].md[0].is_inherence)
	  dmprintf(n, 0, buf, sizeof(buf),DMM_CAST_SPECIAL);
	else
	  dmprintf(n, 0, buf, sizeof(buf),DMM_CAST);
	write_companion(n, 0, buf, 0);
	check_write(n, "#list\n");
	for (i = 0, j = 0; i < MAX_MAGICS; i++) {
	  if (cd[n].md[i].spell == nullptr)
  break;
	  j++;
	  std::snprintf(buf, sizeof(buf), "[%2i] %s \n", j, cd[n].md[i].spell);
	  check_write(n, buf);
	}
	check_write(n, "#end-list\n");
	if(i!=0)
	  dmcheck_write(n, DMM_CAST_SELECT);
	else
	  dmcheck_write(n, DMM_CAST_SPELL);
	cd[n].st = ST_SPELL;
	check_write(n, "#magic\n");
	check_write(n, "#s-edit\n");
	return (0);
  } else if (!std::strcmp(com, "hit")) {
	if (TCN_PALSY(n)) {
	  dmcheck_write(n, DMM_MOVE_NO);
	  return (0);
	}
	  if (fld[map[cd[n].y][cd[n].x].area].flag & FF_ATTACK) {
  dmcheck_write(n, DMM_ATTACK_NO);
  return (0);
	  }
	if (cd[n].weq != nullptr) {
	  if (((cd[n].weq->type & IT_MASK) == IT_CHARGE)
	&& (cd[n].weq->SE == SE_HITISUSE)) {

  for (i = 0; i < MAX_ITEMS; i++) {
	if (&(cd[n].id[i]) == cd[n].weq)
	  break;
  }
  if (i < MAX_ITEMS)  /* ありえないけど check */
	(void) use_item(n, i);
  return (0);
	  }
	}
	*(cd[n].com) = 'A';
	attack_other(n);
	return (0);
  } /* move to talk_analyze 20001212 */
  return (talk_analyze(n,com));
}

/* add for lose talk & npc dead psychogram 20001212 */
/* add area psychogram 20010614 */
int    talk_analyze(int n, char *com)
{
  std::array<char, 256> buf;
  int  i;

  if (!std::strncmp(com, "action", 6) && CHKSP(com + 6)) {  /* from psp12*/
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	dmprintf(n, 0, buf.data(), buf.size(),DMM_ACTION, com);
	write_companion(n, 0, buf.data(), 0);
	return(0);
  } else if (!std::strncmp(com, "fakeaction", 10) && CHKSP(com + 10)) {  /* fakeaction added by NEO 20060408*/
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	dmprintf(n, 0, buf.data(), buf.size(),DMM_FACTION, com);
	write_companion(n, 0, buf.data(), 0);
	return(0);
	} else if (!std::strncmp(com, "gfakeaction", 11) && CHKSP(com + 11) && (cd[n].exp == 99999999)) {  
	/* grobalfakeaction added by NEO 20060408*/
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	dmprintf(n, 0, buf.data(), buf.size(),DMM_FACTION, com);
	write_companion(n, 0, buf.data(), 1);
	return(0);
  } else if (!std::strncmp(com, "priv", 4) && CHKSP(com + 4)) {
	SKIP_TO_SPACE(com);
	SKIP_SPACE(com);
	if('0' <= *com && *com <= '9') {
	  for (i=0; '0' <= *com && *com <= '9' ; com++)
  i = i * 10 + (*com - '0');
	  if(*com !=' ')
  i= 0;
	} else {
	  /* change for name include space 20010813 */
	  for(i=1; i<MAX_NTTYS; i++)
  if(cd[i].type && !std::strncmp(com,cd[i].name, std::strlen(cd[i].name))) {
	  com += std::strlen(cd[i].name);
	  break;
	  }
	}
	while (*com == ' ') com++;
	if (*com) {
	  if(0<i && i<MAX_NTTYS && cd[i].type!=CT_NOEXIST) {
  dmprintf(n, 0, buf.data(), buf.size(),TLK_PRIV, com);
  check_write(i, buf.data());
  dmprintf(n, i, buf.data(), buf.size(),DMM_PRIV);
  check_write(n, buf.data());
	  } else {
  dmcheck_write(n, DMM_PRIV_BAD);
	  }
	} else {
	  dmcheck_write(n, DMM_PRIV_NO_MESSAGE);
	}
	return (0);
  } else {
	if (*com == '*')
	  dmprintf(n, 0, buf.data(), buf.size(),TLK_PSYCHO
		, (TCN_SILENCE(n) ? TLK_SILENCE : com + 1));
	else if (*com == '&')
	  dmprintf(n, 0, buf.data(), buf.size(),TLK_AREA
		, (TCN_SILENCE(n) ? TLK_SILENCE : com + 1));
	else if (*com == '+')
	  dmprintf(n, 0, buf.data(), buf.size(),TLK_PARTY
		, (TCN_SILENCE(n) ? TLK_SILENCE : com + 1));
	else
	  dmprintf(n, 0, buf.data(), buf.size(),TLK_NORMAL
		, (TCN_SILENCE(n) ? TLK_SILENCE : com));
	  write_companion(n, 0, buf.data(), (*com == '*' ? 1 : (*com == '&' ? 2 : (*com == '+' ? 3 : 0))));
	  CPYSTR(cd[n].str, com);
	  if (map[cd[n].y + py[cd[n].d]][cd[n].x + px[cd[n].d]].tip & T_SWITCH)
  load_switch(n, cd[n].x + px[cd[n].d], cd[n].y + py[cd[n].d], 3);
	  if ((n >= MAX_NTTYS) && (map[cd[n].y][cd[n].x].tip & T_SWITCH))
  load_switch(n, cd[n].x, cd[n].y, 7);
  }
  return(1);
}


void   adduser(int n, char *rbuf)
{
  int	 i, k;
  std::array<char, 256> wbuf;
  char *p;

  /* add for refuse co-addusers 20000706 */
  if (limit_addusers && !std::strcmp(cd[n].str,pre_adduser_ip)) {
	std::snprintf(wbuf.data(), wbuf.size(), "adduser refused: (%i:%i) = %s",
			n, fd_check(n), ip_domain_check(n));
	log_print(wbuf.data());
	write_line(n, "#x\n", 3, 0);
	nfflush(n, 100);
	nclose(n);
	return;
  }
  if (!std::strcmp(rbuf, "exit")) {
	dmcheck_write(n, DMM_SEEYOU);
	check_write(n, "#close\n");
	nfflush(n, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
	close_connect(n);
	return;
  }
  std::snprintf(wbuf.data(),wbuf.size(),"You > %s\n",rbuf);
  check_write(n, wbuf.data());
  /* add for reject unsurport character 20000627 */
  for(i = 0, p=rbuf; i < 256 && *p != '\0' && !(*p & 0x80); i++, p++);
  if (*p != '\0') {
	  dmcheck_write(n, DMM_NEWUSER_BAD_CODE);
	  check_write(n, "DM > Please re-enter.\n");
	  return;
  }
  while (fisspace(*rbuf))
	rbuf++;
  switch (cd[n].st) {
  case ST_USERID:
	p=rbuf;
	while (!fisspace(*p)) {
	  if (*p == '\0')
  break;
	  p++;
	}
	*p = '\0';
	if (std::strlen(rbuf) <= 2) {
	  check_write(n, "DM > Please use longer password.\n");
	  check_write(n, "DM > Please re-enter your password (within 6 letters):\n");
	  break;
	}
	if (std::strlen(rbuf) >= 7) {
	  check_write(n, "DM > Please use shorter password.\n");
	  check_write(n, "DM > Please re-enter your password (within 6 letters):\n");
	  break;
	}
	std::snprintf(cd[n].uid,sizeof(cd[n].uid),"%s%05ld%s",addw,s_id,rbuf);
	check_write(n, "DM > Plese enter your E-Mail address:\n");
	cd[n].st = ST_USERAD;
	break;
  case ST_USERNM:
	if (std::strlen(rbuf) <= 1) {
	  dmcheck_write(n, DMM_RENAME_NOLONGER);
	  check_write(n, "DM > Please re-enter your name (within 31 letters):\n");
	  break;
	}
	if (std::strlen(rbuf) >= 32) {
	  dmcheck_write(n, DMM_RENAME_NOSHORTER);
	  check_write(n, "DM > Please re-enter your name (within 31 letters):\n");
	  break;
	}
	/* change for deny bad name 20020207 */
	loadstr(cd[n].str, &rbuf, DSPACE, 32);
	cd[n].str[0] = ftoupper(cd[n].str[0]);
	check_write(n, "DM > Please enter your password (within 6 letters):\n");
	cd[n].st = ST_USERID;
	break;
  case ST_USERAD:
	p = rbuf;
	k = 0;
	while (!fisspace(*p)) {
	  if (*p == '@')
  k = -1;
	  if (*p == '\0')
  break;
	  p++;
	}
	*p = '\0';
	if (!k || (47 < (int) std::strlen(rbuf))) {
	  check_write(n, "DM > Plese enter your E-Mail address:\n");
	  break;
	}
	CPYSTR(cd[n].adr, rbuf);
	rbuf[0] = '\0'; /* add for 'y*@*' 20050615 */
/* add for selection of initial charactor graphic 20050412 */
	if (init_graph_n > 0) {
		check_write(n, "#list");
		for(i=0;i<init_graph_n;i++) {
			std::snprintf(wbuf.data(), wbuf.size(), "[%02d] %s\n", i, init_graphic[i]);
			check_write(n, wbuf.data());
		}
		check_write(n, "#end-list");
		check_write(n, "DM > Please select initial charactor graphic:\n");
		check_write(n, "#s-edit");
		cd[n].st = ST_USERGRP;
		break;
	}
  case ST_USERGRP:
	if (init_graph_n > 0) {
		if (*rbuf >= '0' && *rbuf <= '9') {
			i=0;
			sscanf(rbuf, "%i",&i);
			if (i < 0 || i >= init_graph_n) {
			  check_write(n, "DM > Please re-select initial charactor graphic:\n");
			  break;
			}
			cd[n].ocg = findstr(init_graphic[i]);
		} else
		if (*rbuf == 'y' || *rbuf == 'Y') {
			cd[n].ocg = findstr(init_graphic[0]);
		}
	}
	std::snprintf(wbuf.data(), wbuf.size(), "     ID    : %s\n", cd[n].uid);
	check_write(n, wbuf.data());
	std::snprintf(wbuf.data(), wbuf.size(), "     Name  : %s\n", cd[n].str);
	check_write(n, wbuf.data());
	std::snprintf(wbuf.data(), wbuf.size(), "     E-mail: %s\n", cd[n].adr);
	check_write(n, wbuf.data());
	if (init_graph_n > 0) {
		std::snprintf(wbuf.data(), wbuf.size(), "     figure: %s\n", cd[n].ocg);
		check_write(n, wbuf.data());
	}
	if (*rbuf != 'y' && *rbuf != 'Y') {
		check_write(n, "DM > OK ? [y/n]\n");
		cd[n].st = ST_USERCF;
		break;
	}
  case ST_USERCF:
	if (*rbuf == 'n' || *rbuf == 'N') {

	  check_write(n, "DM > Please re-enter your name (within 31 letters):\n");
	  cd[n].st = ST_USERNM;
	  break;
	} else
	if (*rbuf != 'y' && *rbuf != 'Y') {
		break;
	}

	new_user(n);

	if (cd[n].cv <= 0) {
	  dmcheck_write(n, DMM_NEWUSER_NOSTART1);
	  dmcheck_write(n, DMM_NEWUSER_NOSTART2);
	  dmcheck_write(n, DMM_NEWUSER_NOSTART3);
	  dmcheck_write(n, DMM_NEWUSER_NOSTART4);
	  dmcheck_write(n, DMM_SAVEDATA);
	  dmcheck_write(n, DMM_SEEYOU);
	  check_write(n, "#close\n");
	  nfflush(n, 1000);		/* 送信バッファーフラッシュ bugfix 2011/11/26 */
	  close_connect(n);
	  break;
	}
	ex_notice(n,1); /* add for SD51 */
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_ACCESS_NEW);
	write_companion(n, 0, wbuf.data(), 1);
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DISP_VERSION);
	check_write(n, wbuf.data());

	if (dp_limitter == 1) /* デスペナリミット設定 added by NEO 2011/12/04 */
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DISP_PENARTY_LMT, penalty_rate); /* デスペナリミット付きの場合 */
	else
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DISP_PENARTY, penalty_rate); /* デスペナ設定表示　added by NEO 2008/04/01 */
	check_write(n, wbuf.data());

	if (counter_effect == 0) /* カウンター効果設定 added by NEO 2011/12/10 */
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DISP_COUNTER_OFF); /* カウンター効果なしの場合 */
	else
	dmprintf(n, 0, wbuf.data(), wbuf.size(),DMM_DISP_COUNTER_ON); /* カウンター効果ありの場合 */
	check_write(n, wbuf.data());

	bulletin_board(n, "phi.bb");
	bulletin_board(n, "phi.inf");
	if (wizeyef) {
	  wizard_light(n);
	} else {
  wizard_eye_f(n,0);
	}
	print_map(n, 0);
	auto_equip(n);
	CN_GASIFY(n) = 0x10000; /* add 20020908 */

	break;
  }
}


/* add 20050414 */
int new_user(int n)
{
	int i;
  /* wbuf は CPYSTR の宛先のため C 配列のまま */
	char wbuf[256];

	cd[n].name = findstr(cd[n].str);
	cd[n].cg = nullptr /*findstr("human")*/;
	for (i = 0; i < MAX_LEVEL; i++)
	  cd[n].lv[i] = 0;
	cd[n].bx = cd[n].by = -1; /* bug? fix 20000629 */
	cd[n].wt = cd[n].exp = cd[n].align = cd[n].d = 0;
	cd[n].mhp = cd[n].mmp = cd[n].hp = cd[n].mp = 1000;
	cd[n].moral = 99;
	cd[n].ix = cd[n].x = addx;
	cd[n].iy = cd[n].y = addy;
	cd[n].last = abstime;
	cd[n].eye = 1;
	if (cd[n].auth != nullptr) {			/* bug fix 20000320 */
	  if (cd[n].auth->territory != nullptr)
		free(cd[n].auth->territory);
	  free(cd[n].auth);
	}
	cd[n].auth = nullptr;					/* ここまで */
	for (i = 0; i < MAX_ITEMS; i++)
	  delete_item(&(cd[n].id[i]));
	for (i = 0; i < MAX_MAGICS; i++){
	  cd[n].md[i].spell = nullptr;
	  cd[n].md[i].effect = EF_NONE;
	  cd[n].md[i].fn = FN_NONE;
	cd[n].md[i].extension = nullptr;	  /* bug fix 20000320 */
	}
	for (i = 0; i < MAX_TALKS; i++){
	  cd[n].td[i].type = TT_NONE;
	  cd[n].td[i].string = nullptr;
	}
	cd[n].gp = cirate * 10;
	/* tax -= 0; */
	if (cd[n].gp > 9999) {
  CPYSTR(wbuf, "10 Msp  0505 10 l 10 00 10 00");
  load_item(&cd[n].id[0], wbuf);
  tax+= cd[n].gp;
  cd[n].gp = 0;
  /* 初期所持金 1000gp はデフォルト。それを持っていないなら 		*/
  /* いずれ換金されることを考えて tax に 1000gp 分 増やしておくのが */
  /* 自然であろう。 												*/
  /* もしくはすべての金を合わさると ０ になるようにするならば、 	*/
  /* 1000 gp 所持者にも世界がお金を渡すべきではないだろうか。		*/
  /*												  LazyCat		*/
	}
	cd[n].st = ST_COMMAND;
	cd[n].type = CT_NORMAL;
	cd[n].md[0].spell = findstr("Cure");
	cd[n].md[0].effect = EF_HEAL;
	std::snprintf(wbuf, sizeof(wbuf), "new user : %s\n", cd[n].name);
	log_print(wbuf);
	CPYSTR(pre_adduser_ip,ip_address_check(n)); /* add for refuse co-addusers 20000706 */
	std::snprintf(wbuf, sizeof(wbuf), "#set-user-id %s\n", cd[n].uid);
	check_write(n, wbuf);
	s_id++;
	set_chr(n);
	return(0);
}


char *errnum_to_msg(int errnum){
  static std::array<char, 30> msg;

  switch(errnum){
  case CONNECT_TIMEOUT:    return "time out";
  case CONNECT_EXCEPT:	   return "receive exception";
  case CONNECT_DETECT_EOF: return "lost connection";
  case CONNECT_BUFFULL:    return "buffer full";
#ifdef ESPIPE
  case ESPIPE:	   return "broken pipe";
#endif
#ifdef ECONNRESET
  case ECONNRESET: return "connection reset by peer";
#endif
#ifdef EBADR
  case EBADR:	   return "invalid request descriptor";
#endif
#ifdef ETIME
  case ETIME:	   return "timer expired";
#endif
#ifdef ENONET
  case ENONET:	   return "machine is not on the network";
#endif
#ifdef ECOMM
  case ECOMM:	   return "communication error on send";
#endif
#ifdef ESTRPIPE
  case ESTRPIPE:   return "streams pipe error";
#endif
#ifdef ENETDOWN
  case ENETDOWN:   return "network is down";
#endif
#ifdef ENETUNREACH
  case ENETUNREACH: return "network is unreachable";
#endif
#ifdef ENETRESET
  case ENETRESET:  return "network dropped connection besouse of reset";
#endif
#ifdef ECONNABORTED
  case ECONNABORTED: return "software caused connection abort";
#endif
#ifdef ETIMEDOUT
  case ETIMEDOUT:  return "connection timed out";
#endif
#ifdef ECONNREFUSED
  case ECONNREFUSED: return "connection refused";
#endif
#ifdef EHOSTUNREACH
  case EHOSTUNREACH: return "no route to host";
#endif
  default:
	std::snprintf(msg.data(), msg.size(),"fault No.%i", errnum);
	return msg.data();
  }
}

void	connection_error_process(int n, int errnum)
{
  int i,cn;
  std::array<char, 256> buf;
  std::array<char, 16> fid;

  switch(cd[n].st){
  case ST_CONNECTSERVER:
	std::snprintf(buf.data(), buf.size(), "trs-err  : dead gate : %s (%s)",
		cd[n].name, errnum_to_msg(errnum));
	log_print(buf.data());
	if(!std::strcmp(cd[i=cd[n].vf].uid, cd[n].uid)){
	  cd[i].type = CT_NORMAL;
	  cd[i].st = ST_COMMAND;
	  cd[i].wt = 0;
	  cd[i].ix = cd[i].x;
	  cd[i].iy = cd[i].y;
	  set_chr(i);
	  print_map(i, 0);
	  dmcheck_write(i, DMM_TRANS_NO_MISS_DEST);
  /* add for dead gate delete 20000523 */
  if (gatedata[cn=cd[n].vf2] != nullptr && gatedata[cn][0].di == 'G') { /* 隠しゲートは閉じない bug fix 20000724 */
	 if (!(gatedata[cn][7].dl)) {	   /* エラー回数 -> 経過時間に変更 change 20020830 */
	 	gatedata[cn][7].dl = time(nullptr);
	 } else if (time(nullptr)-gatedata[cn][7].dl > 60*60) { /* ゲート削除経過時間を１時間に変更　by NEO 20061014 old 60*60*24*30 */
		/* cd[n].name には 世界名、.strにはIPとportがプロトコルと共に入っている */
		std::snprintf(buf.data(), buf.size()," %03i %03i %03i %03i %-31s  %s \n",
			  gatedata[cn][1].di,gatedata[cn][2].di,gatedata[cn][3].di,gatedata[cn][4].di,
			  cd[n].name,cd[n].str+8);
		/* get_gateはcd[].strにIPが入っている必要が有る */
		for(i=8;i<40 && cd[n].str[i] != ' ' && cd[n].str[i] != '\0' &&
				cd[n].str[i] != '\n'; cd[n].str[i-8] = cd[n].str[i],i++);
		cd[n].str[i-8] = '\0';
		get_gate(buf.data(), 1, n);			   /* phi.td からも消す */
		std::snprintf(buf.data(), buf.size(), "g-delete : %s", cd[n].name);
		log_print(buf.data());
		dmprintf(n, 0, buf.data(),buf.size(),DMM_GATE_DELETE,cd[n].name);
		write_companion(0, 0, buf.data(), 1);
		clear_gate_and_switch_on_map();
		remap_gate_and_switch();
	 }
  }
	}
	close_connect(n);
	break;
  case ST_CONNECTSERVER_:
  case ST_SENDCHR: /* add for checking abnomal process sequence 20000725 */
	std::snprintf(buf.data(), buf.size(), "trs-err  : %s (%s)", cd[n].name, errnum_to_msg(errnum));
	log_print(buf.data());
	/* クライアントが接続手続きに入ってしまっているため、*/
	/* ST_TRANSFER 状態から解放できない。クライアントから情報待ち */
	close_connect(n);
	break;
  case ST_RESERVE:
	std::snprintf(buf.data(), buf.size(), "rsv-err  : (%d:%d) (%s)",
		n, cd[n].fd, errnum_to_msg(errnum));
	log_print(buf.data());
	close_connect(n);
	break;
  case ST_TRANSFER:
  case ST_TRYRESERVE:
  case ST_TRANSFAILE:
	std::snprintf(buf.data(), buf.size(), "trs-err  : (%d:%d) %s (%s)",
		n, cd[n].fd, cd[n].name, errnum_to_msg(errnum));
	log_print(buf.data());
	dmprintf(n, 0, buf.data(), buf.size(),DMM_EXIT_WITH_ERR
		, errnum_to_msg(errnum));
	close_connect(n);
	write_companion(n, 0, buf.data(), access_dm);
	break;
  case ST_WRITE: /* ディスク圧迫攻撃対策 20010215 */
	id2f(n,fid.data());
	std::snprintf(buf.data(), buf.size(), "BoardTMP.%s", fid.data());
	unlink(buf.data());
  case ST_RECEIVECOMPLETE: /* 転送は完了しているので通常と同じ扱い */
  default:
	if(cd[n].type == CT_NOEXIST){
	  std::snprintf(buf.data(), buf.size(), "cli-err  : (%i:%i) (%s)",
		  n, cd[n].fd, errnum_to_msg(errnum));
	  log_print(buf.data());
	  close_connect(n);
	  break;
	} else {
	  std::snprintf(buf.data(), buf.size(), "cli-err  : (%d:%d) %s (%s)",
		  n, cd[n].fd, cd[n].name, errnum_to_msg(errnum));
	  log_print(buf.data());
	  dmprintf(n, 0, buf.data(), buf.size(),DMM_EXIT_WITH_ERR,
		  errnum_to_msg(errnum));
	  close_connect(n);
	  write_companion(n, 0, buf.data(), access_dm);
	  break;
	}
  }
}

void   charagra_set(int n, char *crgr) /*キャラグラセット用　by NEO 20050620 */
{
    std::array<char, 256> buf;

    if (cd[n].type == CT_NORMAL && n <= MAX_NTTYS) { 
	  if (std::strlen(crgr) < 17) {
        cd[n].cg = cd[n].ocg = findstr(crgr);
        if (!(cd[n].modef & (MD_M57 | MD_M81))) {
	        if (cd[n].cv >= 5090152) {
    	      std::snprintf(buf.data(),buf.size(),"#name %s\n",cd[n].cg);
			  check_write(n,buf.data());
	        } else if (cd[n].cv >= 5085910)
    	      check_write(n,"#name __\n");
            }
          dmprintf(n, 0, buf.data(), buf.size(),DMM_CG_CHANGE, cd[n].cg);
          check_write(n, buf.data());
	    }
	  else {
      dmprintf(n, 0, buf.data(), buf.size(),DMM_CG_ERR_LONG);
      check_write(n, buf.data());
	  }
	}

return;
}

int twobyte_check(char *crgr) /* 文字チェック by NEO 20050316 */
{
	/* m は 0 で初期化する。空文字列だとループが回らず、以前は
	   未初期化のまま返っていた（2026-11-16 修正）。0 は「有効な
	   文字が無い＝不合格」で、呼び出し側の判定とも矛盾しない。 */
	int   i,l,m = 0;
    char  c;

	l = std::strlen(crgr);
	for(i = 0;i < l ;i++) {
      c = crgr[i];
	  if (c >= 0x30 && c <= 0x39 || c >= 0x41 && c <= 0x5A || c >= 0x61 && c <= 0x7A || c == 0x21 || c == 0x2D || c == 0x5F)
		m = 1;
	  else {
        m = 0;
		break;
	  }
    }
    return m;
}

int   next_level_check(int n) /* レベルチェック表示用　by NEO 2008/04/05 */
{
	int  nlv;
    std::array<char, 256> buf;

  	nlv = ( TOTAL_LEVEL / 2000 + 1 ) * 20000
  	 - ((cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_WIZARD] + cd[n].lv[LV_MERCHANT] + cd[n].lv[LV_CREATER]) *10 + cd[n].exp) ;
	dmprintf(n, 0, buf.data(), buf.size(),DMM_NEXT_LV, cd[n].name , nlv / 10 , nlv );
  	check_write(n, buf.data());
	return 0;
}

int   level_up_print(int n) /* レベルアップ表示用　by NEO 2008/04/29 */
{
	int    i;
	std::array<char, 256> buf;

	for (i = 1; i < 999; i++) {
  		if ( TOTAL_LEVEL == ( 2000 * i ) ) {
		dmprintf(n, 0, buf.data(), buf.size(),DMM_LV_UP, cd[n].name , i );
  		write_companion(n, 0, buf.data(), 1);
			if ( i == 1 ) {  /* レベル１ Setcg 説明 by NEO 2008/04/05 */
			dmprintf(n, 0, buf.data(), buf.size(),DMM_SET_CG);
  			check_write(n, buf.data());
  			} else if ( i == 5 ) {  /* レベル５ level 説明 by NEO 2008/04/05 */
			dmprintf(n, 0, buf.data(), buf.size(),DMM_LEVEL_COM);
  			check_write(n, buf.data());
  			}
  		break;
  		}
	}
	return 0;
}

int save_mail(int n, char *mailto, char *msg) /* Add for メールシステム:メールの保存 by NEO 2008/05/02 */
{
	FILE *fp;
	std::array<char, 256> buf;
	std::array<char, 32> nbuf;
	int i,j;
	std::array<char, 32> sp;

	std::snprintf(buf.data(),buf.size(),"phi_mail/%s.md", mailto); /* ファイル準備 */
	if ((fp = efopen(buf.data(), "ab")) == nullptr) {
		log_print("DM > Can not write item data file [phi_mail/].\n");
		return (-1);
	}

	j = cmtstrcpy(nbuf.data(),nbuf.size(),cd[n].name,20);
	nstr(sp.data(),' ',20-j);
	std::snprintf(buf.data(),buf.size(),DMM_MAIL_WRITE, nbuf.data(), sp.data(), msg );
	fputs(buf.data(),fp); /* メール書き込み */

	efclose(fp);
	dmprintf(n, 0, buf.data(), buf.size(),DMM_MAIL_SEND, mailto);
	check_write(n, buf.data());
	return (0);
}


int read_mail(int n) /* Add for メールシステム:メールの読み込み by NEO 2008/05/02 */
{
    FILE *fp;
    std::array<char, 256> rbuf;
    std::array<char, 256> wbuf;
    int i;

    std::snprintf(rbuf.data(),rbuf.size(),"phi_mail/%s.md",cd[n].name); /* メール検索 */
    if ((fp = efopen(rbuf.data(), "rb")) == nullptr) {
		dmcheck_write(n, DMM_MAIL_NO);
		return (0);
	}
	dmcheck_write(n, DMM_MAIL_LIST);
	for(i = 0;;) { /* i はメッセージ番号 */
		if (efgets(rbuf.data(), rbuf.size(), fp) == nullptr) 
		break;
		i++;
		std::snprintf(wbuf.data(), wbuf.size(),"[%2i] %s", i , rbuf.data()); /* メール表示 */
		check_write(n, wbuf.data());
	}
	efclose(fp);
	return (0);
}


int delete_mail(int n) /* Add for メールシステム:メールの消去 by NEO 2008/05/02 */
{
    FILE *fp;
    std::array<char, 256> buf;
    int i;

    std::snprintf(buf.data(),buf.size(),"phi_mail/%s.md",cd[n].name); /* メール検索 */
    if ((fp = efopen(buf.data(), "rb")) == nullptr) {
		dmcheck_write(n, DMM_MAIL_NO);
		return (0);
	}
	efclose(fp);
	dmcheck_write(n, DMM_MAIL_DEL);
	unlink(buf.data()); /* メール全消去 */
	return (0);
}


int check_mail(int n) /* Add for メールシステム:メールのチェック by NEO 2008/05/02 */
{
    FILE *fp;
    std::array<char, 256> buf;
    int i;

    std::snprintf(buf.data(),buf.size(),"phi_mail/%s.md",cd[n].name); /* メール検索 */
    if ((fp = efopen(buf.data(), "rb")) == nullptr)
		return (0);

	for(i = 0;;) { /* i はメッセージ番号 */
		if (efgets(buf.data(), buf.size(), fp) == nullptr) 
		break;
		i++;
	}
	efclose(fp);
	std::snprintf(buf.data(), buf.size(),DMM_MAIL_CHECK, i ); /* メール件数表示 */
	check_write(n, buf.data());
	return (0);
}

int strindex( char *ln, char *s ) /* 文字列から特定の文字を探しその場所を返す */
{ 
	int i, j, k; 

	for ( i = 0; ln[i] != '\0'; i++ ) { 
		j = i; 
		for ( j = i, k = 0; 
			ln[j] == s[k] && s[k] != '\0'; j++, k++ ) ; 
			if ( s[k] == '\0' ) 
			return i; 
		} 
	return -1; 
}

/* 出身地表示 by NEO 20111112 */
void readhomeland(int n, char *homeland) {
  FILE *rfp;
  std::array<char, 256> buf;
  std::array<char, 8> wid;
  char *p;

  std::snprintf(homeland, 255, "Unknown");

  if ((rfp = efopen("worldname.txt", "rb")) == nullptr) return;

  while(1) {
    if (efgets(buf.data(), 255, rfp) == nullptr) break;

    p = buf.data();

    loadstr(wid.data(), &p, ' ', 5);
    if (!std::strncmp(wid.data(), cd[n].uid, 4)) {
      std::snprintf(homeland, 255, "%s", p);
      for (p=homeland;((*p != '\n')&&(*p != '\0'));p++) {
        if ((*p == '\r') || (*p == '\n')) {
          *p = '\0';
          break;
        }
      }
      efclose(rfp);
      return;
    }
  }
  efclose(rfp);
  return;
}
