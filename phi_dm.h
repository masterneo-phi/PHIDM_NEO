
/*  Phantasmal Island ( Data Definition ) 1994-8(C)b-Yak-38 */

#ifndef __PHI_DM_H__
#define __PHI_DM_H__

/* moved for configure language 20001208 */
#include "phi_dm.cnf"

#include "dm_msg.h"

#ifdef NO_SKILL
#define SKILL_VER " (No Skill)"
#else
#define SKILL_VER ""
#endif

/*	   v509ewl21+fix2 -> v509ewl21+fix2+psp6+QSCP -> v5x-010  */
/*	   v5x-010 release 13 test8 -> v5y rel.0 test1	*/
#define PHI_VERSION "v5z release v2.3.3 /*color=teal*/PHIDM NEO/*.*/ /*color=red*/'Nymphicus hollandicus'/*.*/" SKILL_VER	/* クライアント表示：任意長 */

#define PHI_DM_VERS "v5x:011:0005:38"		 /* DM種別通知：任意長 */
#define PHI_PROT_VERS "05115000" /* プロトコルバージョン：８桁 */
/*     05097500 = v5.09(0509) + ewl21(76) + fix(1) + none(0) */

/* 05097626 (serv) 05100006 (cli) */

#define VERS_SPELL_JEWEL  "05097540"

#ifdef SPLITLOG
#define TIMESTRMAX 25
#endif

/* added by Qajin 20000228 */
#define IS_CLIENT_PHIWIN(n) (cd[n].cv >= 5090152 && cd[n].cv <= 5090154)
/* 20000627 */
#define IS_CLIENT_RPHI(n) (cd[n].cv >= 5097777)
#define IS_CLIENT_RPHIWIN(n) (cd[n].cv >=5090152)
#define CHRGRA(n)   (cd[n].cg != NULL ? cd[n].cg : (cd[n].ocg != NULL ? cd[n].ocg : (n < MAX_NTTYS ? "human" : "")))
#define IS_CGCHGED(n)   (cd[n].cg != NULL || cd[n].ocg != NULL)
#define IS_CLIENT_TPHI(n) (cd[n].cv >= 5080000 && cd[n].cv < 5090000)

/*
   PHI_PROT_VERS

   ex. 05085910 = v5.08(0508) + u5c4(5) + e15(9) + k1(1) + none(0)
       05092000 = v5.09(0509) + e2(2) + none(000)
*/

#include "tcp.h"

#define SERV_TCP_PORT     20017
#define MAX_NTTYS   MAX_CLIENT
#ifndef MAX_ENEMIES
#define MAX_ENEMIES   128
#endif
#define MAX_ITEMS   8
#define MAX_MAGICS    8
#define MAX_TALKS   16
#define MAX_APPS    8
#define ITEM_FULL   8
#define BOARD_FULL    8
#define MAX_POWER_SPELL   16
#define MAX_MESSAGE_SIZE        2048
#define MAX_ANALYZE_INPUT_NUMBERS 16

#define CHKSP(p)    (fisspace(*(p)) || !(*(p)))
#define SKIP_TO_SPACE(p)  for(;*(p) && !fisspace(*(p));(p)++)
#define SKIP_SPACE(p)   for(;*(p) && fisspace(*(p));(p)++)
#define RAND  (rand())
#ifdef BSD
#define frand(x) ((((rand() >> 8) & 0xffff) * (x)) >> 16)
#else
#define frand(x) (((rand() & 0x3fff) * (x)) >> 14)
#endif
#define PER_DICE    (frand(100))
#define PT_APPC     90

#define is_gp(flooritem)        ((flooritem)->gp>0)
/* add for trap 20001215 */
#define is_trap(flooritem)        ((flooritem)->gp==-1)
#define is_item(flooritem)        ((flooritem)->gp==0)

#define D_FORE(n)                  (cd[(n)].d)
#define D_RIGHT(n)                 ((cd[(n)].d+1)%4)
#define D_BACK(n)                  ((cd[(n)].d+2)%4)
#define D_LEFT(n)                  ((cd[(n)].d+3)%4)
#define X_FORE(n)                 (cd[(n)].x+px[D_FORE(n)])
#define Y_FORE(n)                 (cd[(n)].y+py[D_FORE(n)])
#define X_RIGHT(n)                (cd[(n)].x+px[D_RIGHT(n)])
#define Y_RIGHT(n)                (cd[(n)].y+py[D_RIGHT(n)])
#define X_BACK(n)                 (cd[(n)].x+px[D_BACK(n)])
#define Y_BACK(n)                 (cd[(n)].y+py[D_BACK(n)])
#define X_LEFT(n)                 (cd[(n)].x+px[D_LEFT(n)])
#define Y_LEFT(n)                 (cd[(n)].y+py[D_LEFT(n)])

/* add for long range spear 20000518 */
#define X_FORE2(n)                 (cd[(n)].x+px[D_FORE(n)]*2)
#define Y_FORE2(n)                 (cd[(n)].y+py[D_FORE(n)]*2)
#define X_RIGHT2(n)                (cd[(n)].x+px[D_RIGHT(n)]*2)
#define Y_RIGHT2(n)                (cd[(n)].y+py[D_RIGHT(n)]*2)
#define X_BACK2(n)                 (cd[(n)].x+px[D_BACK(n)]*2)
#define Y_BACK2(n)                 (cd[(n)].y+py[D_BACK(n)]*2)
#define X_LEFT2(n)                 (cd[(n)].x+px[D_LEFT(n)]*2)
#define Y_LEFT2(n)                 (cd[(n)].y+py[D_LEFT(n)]*2)

/* Terrain */

#define T_SPACE     0x00  /*    (  ) */
#define T_PPLATE    0x01  /* o  (o ) */
#define T_SPEARBARRIER 0x04
#define T_ANTIMAGIC 0x08  /* ** (  ) */
#define T_PLANT     0x10  /* :: (::) */
#define T_FLOWER    0x11  /* +: (+:) */
#define T_WATER     0x12  /* __ (__) */
#define T_BOX       0x13  /* xx (xx) */
#define T_MIST      0x20  /* // (//) */
#define T_EWALL     0x40  /* .. (  ) */
#define T_TGATE     0x41  /* .. (><) */
#define T_STORE     0x42  /* ss (ss) */
#define T_MSGATE    0x6c  /* ZZ (HH) */
#define T_GATE      0x7c  /* [] ([]) */
#define T_SGATE     0x79  /* $$ (##) */
#define T_CWALL     0x80  /* !! (  ) */
#define T_JAIL      0xc0  /* II (II) */
#define T_LBOX      0xc1  /*    (%%) */
#define T_AWALL     0xc2  /* a  (  ) */
#define T_WINDOW    0xd0  /* || (||) */
#define T_BWATER    0xd1  /* ^^ (__) */
#define T_TREE      0xdc  /* TT (TT) */
#define T_GLASS     0xdd  /* == (==) */
#define T_MWALL     0xec  /* HH (HH) */
#define T_LGATE     0xed  /*    ({}) */
#define T_LSGATE    0xee  /*    (##) */
#define T_LMSGATE   0xef  /*    (HH) */
#define T_WALL      0xfc  /* ## (##) */
#define T_ROCK      0xfd  /* @@ (@@) */
#define T_UNKNOWN   0xff  /* ?? (??) */

#define TMASK   0xff
#define TCLEAR    0xff00

#define T_MESSAGE 0x1000  /* ( -) */
#define T_ITEM    0x0100  /* (. ) */
#define T_ITEM7   0x0700  /* (. ) */ /* for m57 */
#define T_SWITCH  0x2000  /* ( !) */
#define T_ROOF    0x4000  /* ( ~) */
#define T_TRAP    0x0800  /* ( ") */ /* add for trap 20001215 */
#define T_SURFACE 0x8000  /* ( ") */ /* add for surface 20010228 */

/* Input state */

#define ST_COMMAND  0x00
#define ST_ATTACK 0x01
#define ST_DEFEND 0x02
#define ST_TRANSSELECT  0x10
#define ST_TRANSFER 0x11
#define ST_RESERVE  0x12
#define ST_CONNECTSERVER  0x13
#define ST_CONNECTSERVER_ 0x14
/* vv add for checking abnomal process sequence 20000725 */
#define ST_TRYRESERVE 0x15
#define ST_TRANSFAILE 0x16
#define ST_SENDCHR 0x17
#define ST_RECEIVECOMPLETE 0x18
/* ^^ */
#define ST_SPELL  0x20
#define ST_CAST   0x21
#define ST_CAST_COMPLETE 0x22
#define ST_GET    0x30
#define ST_PUT    0x31
#define ST_USE    0x32
#define ST_EQUIP  0x33
#define ST_UNEQUIP  0x34
#define ST_R_GET    0x35 /* add for remote get 20000612 */
#define ST_R_PUT    0x36 /* add for remote put 20000612 */
#define ST_READ   0x40
#define ST_MORE   0x41
#define ST_WRITE  0x42
#define ST_ERASE  0x43
#define ST_TITLE  0x46 /* add for change board title 20000904 */
#define ST_MAIL   0x50
#define ST_ADDRESS  0x51 /* add for change mail address 20000830 */
#define ST_ADDRESS_ 0x52
#define ST_RENAME 0x53 /* add for rename :-P 20000905 */
#define ST_SEARCH 0x60
#define ST_DETECT 0x61
#define ST_CREATE 0x62
#define ST_CREATE_  0x63
#define ST_DUPLICATE  0x64
#define ST_DESTROY  0x65
#define ST_MALOR  0x66
#define ST_ANALYZE  0x67
#define ST_FORGET       0x68
#define ST_WINTALK      0x69
#define ST_GASIFY 0x70
#define ST_LOSETALK      0x71 /* add for lose talk 20001212 */
#define ST_CHARGE_SPELL  0x75
#define ST_CHARGE_SPELL_ 0x76
#define ST_METAMORPHOSIS 0x77
#define ST_BAN  0x78  /* added by Qajin 19991021 */
#define ST_BAN_ 0x79
#define ST_USERID 0x7a /* change with lose talk 20001212 */
#define ST_USERNM 0x7b
#define ST_USERAD 0x7c
#define ST_USERCF 0x7d
#define ST_USERGRP 0x7e /* add for selection of init graphic 20050412 */
#define ST_USERREGISTER 0x7f /* add for register user protocol 20050414 */
#define ST_MSGW 0x80  /* added by Qajin 20001122 */
#define ST_MENU 0x81  /* added by Qajin 20041212 */
#define ST_SELECT 0x82  /* :A 拡張 y/s の選択待ち (PHIDM NEO extension, from MRDM) */
#define ST_PETNAME 0x83  /* ペット命名の入力待ち (PHIDM NEO extension) */
#define ST_PETREL 0x84  /* ペット解放の確認待ち (PHIDM NEO extension) */
#define ST_JOIN_INVITE 0x90  /* add 20020823 */
#define ST_JOIN_ANSWER 0x91  /* add 20020823 */
#define ST_REMOTE_LOCK 0x6a /* add for Remote lock */
#define ST_REMOTE_UNLOCK 0x6b /* add for Remote unlock */
#define ST_REMOTE_BOARDS 0x6c /* add for Remote boards */
#define ST_REMOTE_ERASE 0x6d /* add for Remote erase */
#define ST_THUNDER 0x6e /* add for Thunder of Zeus */
#define ST_HAMMER 0x6f /* add for Hammer of Ares */

/* 注意: a-f の数を使うと今のところ phiwin でエラーが発生します。*/

/* Fundamental */

#define FN_NONE   0
#define FN_EARTH  1
#define FN_LIGHT  2
#define FN_AIR    3
#define FN_SPIRIT 4
#define FN_WATER  5
#define FN_FIRE   6
#define FN_NORMAL 6 /* normalと変化系の境目 add by Qajin 20000324 :-p */
#define FN_MIRROR 7
#define FN_PRISM  8
#define FN_RAINBOW 9

#define FN_MAX 10

/* Skill level */

#define LV_FIGHTER  0
#define LV_WIZARD 1
#define LV_MERCHANT 2
#define LV_CREATER  3

#define MAX_LEVEL 4

/* Item type */

#define MT_FOOD   0x000
#define MT_WOOD   0x100
#define MT_STONE  0x200
#define MT_STEEL  0x300
#define MT_SILVER 0x400
#define MT_MYTHRIL  0x500
#define MT_AURIC  0x600
#define MT_ADAMANT  0x700
#define MT_MAGICAL  0x800
#define MT_NOBREAK  0xf00

#define MT_MASK   0xf00
#define MT_PLACE  0x100

#define WT_FIST   0x00
#define WT_SWORD  0x10
#define WT_BLADE  0x20
#define WT_STINGER  0x30
#define WT_SPEAR  0x40
#define WT_AXE    0x50
#define WT_STRIKER  0x60
#define WT_WHIP   0x70
#define WT_FANG   0x80
#define WT_WIDE   0x90
#define WT_AROUND 0xa0

#define WT_MASK   0xf0
#define WT_PLACE  0x10

#define WD_NONE   0x0000
#define WD_FORE   0x8000
#define WD_BACK   0x4000
#define WD_LEFT   0x2000
#define WD_RIGHT  0x1000

#define WD_MASK   0xf000
#define WD_PLACE  0x1000

#define IT_NOUSE  0x0
#define IT_ARMOR  0x1
#define IT_WEAPON 0x2
#define IT_FOOD   0x3
#define IT_RESOURCE 0x4
#define IT_JEWEL  0x5
#define IT_CHARGE 0x6
#define IT_ACCESSORY  0x7
#define IT_SPELL_JEWEL  0x8
#define IT_EVENTLY  0xc
#define IT_SCROLL   0xd
#define IT_FOREIGN  0xe
#define IT_ALL    0xf

#define IT_MASK   0xf
#define IT_PLACE  0x1

#define DT_NONE   (MT_NOBREAK|WT_FIST|IT_NOUSE)
#define DT_GP   (MT_SILVER|WT_FIST|IT_JEWEL)

#define ATP op[0]   /* Attack point */

/* Armor parameter (IT_ARMOR -> op[1]/op[2] */

#define DFP op[1]   /* Defence point */
#define MRP op[2]   /* Magic resistance point */

/* Weapon parameter (IT_WEAPON -> op[1]/op[2]) */

#define ER  op[1]   /* Enhance rate */

#define ER_NONE   0
#define ER_SILVER 10
#define ER_MYTHRIL  25
#define ER_AURIC  50

#define AE  op[2]   /* Additional effect */

#define AE_NONE   0
#define AE_POISON 1
#define AE_PALSY  2
#define AE_PANIC  3
#define AE_CONFUSE  4
#define AE_BERSERK  5
#define AE_SILENCE  6
#define AE_DRAIN  7
#define AE_THIEF  8
#define AE_MENTAL 9
#define AE_ENERGY 10
#define AE_LAW    11
#define AE_CHAOS  12
#define AE_BLIND  13
#define AE_KNOCKBACK  14
#define AE_RANDOM 15
#define AE_ALL    16
#define AE_RICK   17
#define AE_GURD3  18  /* Gurd3/4はRickを兼ねるので先 */
#define AE_GURD4  19
#define AE_SAGA1  20  /* SaGaはRickを兼ねるので先 */
#define AE_SAGA2  21
#define AE_SAGA3  22
#define AE_SAGA4  23
#define AE_DRAGON 24  /* GurdはRickを兼ねないので後 */
#define AE_GOD    25
#define AE_ELEMENT  26
#define AE_DEMON  27
#define AE_GURD1  28
#define AE_GURD2  29

/* Value (IT_JEWEL/IT_FOOD -> op[2])  */

#define VALUE op[2]
#define VALUE2  op[1]

/* ENTROP (IT_FOOD -> op[3]) */

#define ENTROP  op[3]

/* Resource type (IT_RESOURCE -> op[2]) */

#define RS_RESOLVER 0
#define RS_PLANT  1
#define RS_TREE   2
#define RS_WATER  3
#define RS_WALL   4
#define RS_WINDOW 5
#define RS_DOOR   6
#define RS_SDOOR  7
#define RS_SAFE   8
#define RS_GLASS  9
#define RS_ROOF   10
#define RS_DUMMY  99 /* add for object resource 20011128 */

/* Charged item parameter (IT_CHARGE -> op[1]/op[2]) */

#define CHARGE  op[1]   /* Charge */
#define UE  op[2]   /* Use effect */

/* Spell jewel parameter (IT_SPELL_JEWEL -> op[2]) */
/* #define UE   op[2] */

/* Accessory parameter (IT_ACCESSORY -> op[2]) */

#define AT  op[2]   /* Accessory type */

#define AT_NONE   0
#define AT_POISONPROOF  1
#define AT_PALSYPROOF 2
#define AT_DRAINPROOF 3
#define AT_THEFTGUARD 4
#define AT_MEPROOF  5
#define AT_VISIBLE    6
#define AT_IMMORTAL 7
#define AT_IRONSTOMACH  8
#define AT_ABSORB 9
#define AT_STEALTH  10
#define AT_FLY    11
#define AT_HELMET 12
#define AT_UMBRELLA 13
#define AT_COUNTER 90

#define MAX_AT    13

/* Special Effect (IT_ALL -> op[3]) */

#define SE  op[3]   /* Special Effect (op[3]) */

#define SE_NONE    0
#define SE_ATPUP   1
#define SE_ATPDOWN   2
#define SE_DEFUP   3
#define SE_DEFDOWN   4
#define SE_MRPUP   5
#define SE_MRPDOWN   6
#define SE_HASTE   7
#define SE_SLOW    8
#define SE_ILLUSION  9
#define SE_CONTRAST  34
#define SE_REGENE 10
#define SE_WILL   11
#define SE_POISON 12
#define SE_PALSY  13
#define SE_PANIC  14
#define SE_CONFUSE  15
#define SE_BERSERK  16
#define SE_SILENCE  17
#define SE_KARMAUP  18
#define SE_KARMADOWN  19
#define SE_CLAIR        20
#define SE_BLIND  21
#define SE_RAISEDEAD  22
#define SE_PASSWALL 23
#define SE_GIGANT 24
#define SE_VANISH 25
#define SE_MIRROR 26
#define SE_SPELLUP  27
#define SE_SPELLDOWN  28
#define SE_STEALTH  29
#define SE_HITISUSE 30
#define SE_HITTOUSE 31
#define SE_LITTLE 32
#define SE_NOMOVE       33

#define MAX_SE    34
#define CREATE_SE 21  /* createで負化されるS.E.の限界 */

#define SE_APPEAR 99

/* Magic effect */

#define EF_NONE    0
#define EF_LINE    1
#define EF_PIERCE  2
#define EF_EXPLODE   3
#define EF_SPHERE  4
#define EF_MLINE   5
#define EF_MPIERCE   6
#define EF_MEXPLODE  7
#define EF_MSPHERE   8
#define EF_HEAL    9
#define EF_HEALS  10
#define EF_REGENE 11
#define EF_COAT   12
#define EF_COAT_  13
#define EF_SPELL  14  /* 38 ここから、phi.edやphi.cdと番号食い違う */
#define EF_SPELL_ 15  /* 39 */
#define EF_PROTECT  16  /* 14 ここから 連番 */
#define EF_PROTECT_ 17
#define EF_SHELL  18
#define EF_SHELL_ 19
#define EF_HASTE  20
#define EF_HASTE_ 21
#define EF_ILLUSION 22  /* 20 */
#define EF_POISON 23
#define EF_PALSY  24
#define EF_PANIC  25
#define EF_CONFUSE  26
#define EF_BERSERK  27
#define EF_SILENCE  28
#define EF_ANTIDOTE 29
#define EF_REMOVE 30
#define EF_AWAKEN 31
#define EF_FORCEFIELD 32 /* 30 */
#define EF_REFLEXFIELD 33
#define EF_ERASE  34    /* 32  ここまで */
#define EF_ENCOURAGE 35 /* 61 食い違い ５つまでRESERVEしてある */
#define EF_ILLUSION_ 36  /* 62 連番で拡張する場合は下へ */
#define EF_RESERVE3 37  /* 単独で拡張する場合はここを */
#define EF_RESERVE4 38
#define EF_RESERVE5 39  /* 65 食い違いここまで */
#define EF_ROOF   40
#define EF_UNROOF 41
#define EF_AREA   42
#define EF_MROOF  43
#define EF_MUNROOF  44
#define EF_MAREA  45
 /* #define EF_????      46-60 *//* ＰＣと共通魔法を追加はここ 46-60  */

#define NORMAL_EF 60  /* これを越える EF は PC は覚えられない */

#define EF_MIRROR 61  /* 33 ここからも番号食い違う 連番号 */
#define EF_VANISH 62
#define EF_BLIND  63
#define EF_HPDIVIDE 64
#define EF_MPDIVIDE 65  /* 37 ここまで */
#define EF_R_GET  66 /* add for remote get 20000612 */
#define EF_R_PUT  67 /* add for remote put 20000612 */
#define EF_SAVELIFE 68 /* add for save life  20000612 */
 /* #define EF_??????    69-93 *//* NPC&MASTER MAGIC追加 */
/* vv add traps 20001215 */
#define EF_TRAPHP  94
#define EF_TRAPMP  95
#define EF_MINEHP  96
#define EF_MINEMP  97
/* ^^ */
#define EF_DISAPPEAR  98
#define EF_APPEAR 99

/* Condition Number */

#define CNN_COAT  0
#define CNN_PROTECT 1
#define CNN_SHELL 2
#define CNN_HASTE 3
#define CNN_ILLUSION  4
#define CNN_SPELL 5
#define CNN_REGENE  6
#define CNN_FORCE 7
#define CNN_REFLEX  8
#define CNN_MIRROR  9
#define CNN_VANISH  10
#define CNN_POISON  11
#define CNN_PALSY 12
#define CNN_PANIC 13
#define CNN_CONFUSE 14
#define CNN_BERSERK 15
#define CNN_SILENCE 16
#define CNN_BLIND 17
#define CNN_PASSWALL  18
#define CNN_RAISEDEAD  19
#define CNN_IDLE  20 /* change 20020822 */
#define CNN_LAG 21
#define CNN_AEGIS 22 /* add for Aegis of Athena */
#define CNN_HAMMER 23 /* add for Hammer of Ares */

/* Condition */

#define CN_COAT(n)  (cd[n].cn[CNN_COAT])
#define CN_PROTECT(n) (cd[n].cn[CNN_PROTECT])
#define CN_SHELL(n) (cd[n].cn[CNN_SHELL])
#define CN_HASTE(n) (cd[n].cn[CNN_HASTE])
#define CN_ILLUSION(n)  (cd[n].cn[CNN_ILLUSION])
#define CN_SPELL(n) (cd[n].cn[CNN_SPELL]) /* xN用に順番変 20000725 */
#define CN_REGENE(n)  (cd[n].cn[CNN_REGENE])
#define CN_FORCE(n) (cd[n].cn[CNN_FORCE])
#define CN_REFLEX(n)  (cd[n].cn[CNN_REFLEX])
#define CN_MIRROR(n)  (cd[n].cn[CNN_MIRROR])
#define CN_VANISH(n)  (cd[n].cn[CNN_VANISH])
#define ERASE_CN  10  /* これ以下の CN は ERASE 魔法で効果減衰 */
#define CN_POISON(n)  (cd[n].cn[CNN_POISON])
#define CN_PALSY(n) (cd[n].cn[CNN_PALSY])
#define CN_PANIC(n) (cd[n].cn[CNN_PANIC])
#define CN_CONFUSE(n) (cd[n].cn[CNN_CONFUSE])
#define CN_BERSERK(n) (cd[n].cn[CNN_BERSERK])
#define CN_SILENCE(n) (cd[n].cn[CNN_SILENCE])
#define CN_BLIND(n) (cd[n].cn[CNN_BLIND])
#define CN_PASSWALL(n)  (cd[n].cn[CNN_PASSWALL])
#define CN_RAISEDEAD(n)  (cd[n].cn[CNN_RAISEDEAD]) /* add for save life 20000612 */
#define NORMAL_CN 19  /* これ以下の CN は 1秒ごとに効果減衰 */
#define CN_IDLE(n)  (cd[n].cn[CNN_IDLE]) /* add 20020822 */
#define CN_GASIFY(n)  (cd[n].gasify) /* change 20020822 */
#define CN_LAG(n) (cd[n].cn[CNN_LAG])
#define CN_AEGIS(n) (cd[n].cn[CNN_AEGIS]) /* add for Aegis of Athena */
#define TCN_AEGIS(n) ( CN_AEGIS(n) )
#define CN_HAMMER(n) (cd[n].cn[CNN_HAMMER]) /* add for Hammer of Ares */
#define TCN_HAMMER(n) ( CN_HAMMER(n) ) /* add for Hammer of Ares */
#define CN_ALAG(n)  (cd[n].et)
#define CN_DLAG(n)  (cd[n].ec)  /* add for lag attack 20020115 */

#define MAX_CN    24  /* cn の個数 */

/* add 20000707 */
/* xN Condition */

#define NCN_COAT(n)  (cd[n].ncn[CNN_COAT])
#define NCN_PROTECT(n) (cd[n].ncn[CNN_PROTECT])
#define NCN_SHELL(n) (cd[n].ncn[CNN_SHELL])
#define NCN_HASTE(n) (cd[n].ncn[CNN_HASTE])
#define NCN_ILLUSION(n)  (cd[n].ncn[CNN_ILLUSION])
#define NORMAL_NCN 4

#define MAX_NCN    5  /* ncn の個数 */

/* Original Condition */

#define OCN_COAT(n)  (int)(CN_COAT(n)/(NCN_COAT(n)>0 && CN_COAT(n) ? NCN_COAT(n):1))
#define OCN_PROTECT(n) (int)(CN_PROTECT(n)/(NCN_PROTECT(n)>0 && CN_PROTECT(n) ? NCN_PROTECT(n):1))
#define OCN_SHELL(n) (int)(CN_SHELL(n)/(NCN_SHELL(n)>0 && CN_SHELL(n) ? NCN_SHELL(n):1))
#define OCN_HASTE(n) (int)(CN_HASTE(n)/(NCN_HASTE(n)>0 && CN_HASTE(n) ? NCN_HASTE(n):1))
#define OCN_ILLUSION(n)  (int)(CN_ILLUSION(n)/(NCN_ILLUSION(n)>0 && CN_ILLUSION(n) ? NCN_ILLUSION(n):1))

/* Total Condition */

#define TCN_REGENE(n) ( CN_REGENE(n) + ecn(n,SE_REGENE) )
#define TCN_WILL(n) ( ecn(n,SE_WILL) )
#define TCN_COAT(n) ( OCN_COAT(n) + ecn(n,SE_ATPUP) + ecn(n,SE_ATPDOWN) )
#define TCN_SPELL(n)  ( CN_SPELL(n) + ecn(n, SE_SPELLUP) + ecn(n, SE_SPELLDOWN) )
#define TCN_PROTECT(n)  ( OCN_PROTECT(n) + ecn(n,SE_DEFUP) + ecn(n,SE_DEFDOWN) )
#define TCN_SHELL(n)  ( OCN_SHELL(n) + ecn(n,SE_MRPUP) + ecn(n,SE_MRPDOWN) )
#define TCN_HASTE(n)  ( OCN_HASTE(n) + ecn(n,SE_HASTE) + ecn(n,SE_SLOW) )
#define TCN_ILLUSION(n) ( OCN_ILLUSION(n) + ecn(n,SE_ILLUSION) + ecn(n,SE_CONTRAST) )
#define TCN_POISON(n) ( CN_POISON(n) + ecn(n,SE_POISON) )
#define TCN_PALSY(n)  ( CN_PALSY(n) + ecn(n,SE_PALSY) )
#define TCN_PANIC(n)  ( CN_PANIC(n) + ecn(n,SE_PANIC) )
#define TCN_CONFUSE(n)  ( CN_CONFUSE(n) + ecn(n,SE_CONFUSE) )
#define TCN_BERSERK(n)  ( CN_BERSERK(n) + ecn(n,SE_BERSERK) )
#define TCN_SILENCE(n)  ( CN_SILENCE(n) + ecn(n,SE_SILENCE) )
#define TCN_KARMA(n)  ( ecn(n,SE_KARMAUP) + ecn(n,SE_KARMADOWN) )
#define TCN_VANISH(n) ( CN_VANISH(n) + ecn(n,SE_VANISH) )
#define TCN_BLIND(n)  ( CN_BLIND(n) + ecn(n,SE_BLIND) + ecn(n,SE_CLAIR) )
#define TCN_MIRROR(n) ( CN_MIRROR(n) + ecn(n,SE_MIRROR) )
#define TCN_RAISEDEAD(n)  ( CN_RAISEDEAD(n) + ecn(n,SE_RAISEDEAD) ) /* change for save life 20000612 */
#define TCN_PASSWALL(n) ( CN_PASSWALL(n) + ecn(n,SE_PASSWALL) )
#define TCN_GIGANT(n) ( ecn(n,SE_GIGANT) > ecn(n, SE_LITTLE) )
#define TCN_LITTLE(n) ( ecn(n,SE_GIGANT) < ecn(n, SE_LITTLE) )
#define TCN_NOMOVE(n)   ( ecn(n,SE_NOMOVE) )

/* Character type */

#define CT_NOEXIST  0
#define CT_NORMAL 1
#define CT_BEAST  2
#define CT_BERSERKER  3
#define CT_MAGICAL  4
#define CT_UNDEAD 5
#define CT_ASTRAL 6
#define CT_ERASER 7

/* Enemy thinking state */

#define TH_NONE              0
#define TH_ATTACK      1
#define TH_MAGIC       2
#define TH_WIZARDSTEP      3
#define TH_WIN         4
#define TH_RUN         6
#define TH_TALK        7
#define TH_TRACE       8
#define TH_WAIT        9
#define TH_GET_AWAY         10

/* Enemy various flag */

#define VF_DYING  0x01
#define VF_HEAL   0x02
#define VF_FOOD   0x04
#define VF_WEAPON 0x08
#define VF_ARMOR  0x10
#define VF_ACCESSORY    0x20
#define VF_CHARGE       0x40
#define VF_NOT_TRACE    0x80
#define VF_NOT_STAY     0x100

/* Enemy talking type */

#define TT_NONE   0
#define TT_ATTACK 1
#define TT_HELP   2
#define TT_KILL   3
#define TT_DYING  4
#define TT_HI   5
#define TT_KEYWORD      6

/* Floor permission flag */

#define FF_RESOURCE     0x0001
#define FF_MAGIC        0x0002
#define FF_EAGLEEYE     0x0004
#define FF_RETURN       0x0008
#define FF_GATE         0x0010
#define FF_WRITE        0x0020
#define FF_LOCK         0x0040
#define FF_GASIFY       0x0080
#define FF_ATTACK       0x0100
#define FF_AREAMAGIC    0x0200
#define FF_CALL         0x0400
#define FF_EXTENSION    0x0800
#define FF_CHARGE       0x1000
#define FF_MAGICEFFECT  0x2000
#define FF_REMOTEGET    0x4000
#define FF_REMOTEPUT    0x8000
#define FF_M57          0x40000000

/* Indentify detective */

#define ID_NONE   0x0000
#define ID_KARMA  0x0001  /* k */
#define ID_AFN    0x0002  /* a */
#define ID_DFN    0x0004  /* d */
#define ID_HP   0x0008  /* H */
#define ID_MP   0x0010  /* M */
#define ID_STATUS 0x0020  /* S */
#define ID_SPEED  0x0040  /* s */
#define ID_DEFMRP 0x0080  /* D */
#define ID_ATP    0x0100  /* A */
#define ID_LEVEL  0x0200  /* L */

#define ID_SIMPLE 0x0067  /* n (KARMA AFN DFN STATUS SPEED)    */
#define ID_ALL    0x03ff  /* * (上に存在するもの全てのfilter   */
 /* 存在しないものを含んでは駄目) */

/* Identify No. */

#define IN_KARMA  0 /* 小さい値ほど簡単にみえる */
#define IN_AFN    500 /* FigLv + WizLv の差を指定 */
#define IN_DFN    1000  /* Ident Lv.の都合で        */
#define IN_HPMP   2000  /* 100の倍数にすること     */
#define IN_STATUS 5000
#define IN_SPEED  3000
#define IN_DEFMRP 6500
#define IN_ATP    6500
#define IN_LEVEL  10000

/* Innately attributes */
#define IA_NONE   0x0000  /* なし */
#define IA_STONEHEAD  0x0001  /* 石頭 */
#define IA_UMBRELLA 0x0002  /* 傘さし */
#define IA_FLYING 0x0004  /* 飛行 */
#define IA_RICK   0x0008  /* ライカンスロープ 銀・ミスリル・マジックのみ通じる */
#define IA_DRAGON 0x0010  /* 特定の武器でないとダメージ半分 */
#define IA_GOD    0x0020
#define IA_ELEMENT  0x0040
#define IA_DEMON  0x0080
#define IA_GURD1  0x0100
#define IA_GURD2  0x0200
#define IA_GURD3  0x0400
#define IA_GURD4  0x0800
#define IA_GURDIAN  0x0ff0
#define IA_SAGA1  0x1000  /* 伝説の武器しか効かない */
#define IA_SAGA2  0x2000
#define IA_SAGA3  0x4000
#define IA_SAGA4  0x8000
#define IA_SAGA   0xf000

/* NPC 魔法攻撃範囲識別用 */
#define ED_NORESTRICT   0x7f
#define ED_RANGEOUT     0x0
#define ED_FACE         0x1
#define ED_AROUND       0x2
#define ED_LINE         0x4
#define ED_FOREAROUND   0x8
#define ED_SAMEFLOOR    0x10 /* add for NPC using quake :-P */
#define ED_ROOF         0x20

/* add for some modes 20000629 */
#define MD_BOARD        0x1
#define MD_FLOORITEM    0x2
#define MD_CHAT         0x4
#define MD_BOARDSEND    0x8000
#define MD_M57          0x8
#define MD_M57SOLID     0x10
#define MD_FEAGLE       0x20
#define MD_M121         0x80
#define MD_FX           0x40  /* DM演出 (FX) 対応 (PHIDM NEO extension) */
#define MD_HEIGHT       0x100  /* 高さ印 (height) 対応 (PHIDM NEO extension) */
#define MD_FASTMOVE     0x200  /* 高速移動 (fastmove) 対応 (PHIDM NEO extension) */
#define MD_PET          0x400  /* ペット (#ex-pet) 対応 (PHIDM NEO extension) */
#define MD_JOB          0x800  /* 職業・性別 (#ex-job) 対応 (PHIDM NEO extension) */

typedef struct md_t {
  char  *spell;     /* 呪文 */
  signed char    fn;      /* 属性 */
  char    effect;    /* 効果 */
  int    is_inherence;          /* 固有の魔法か */
  char  *extension;   /* 拡張データ用 */
}      MagicData;

typedef struct item_data_t {
  char  *name;
  int    type;
  short    wp;
  signed char    fn;
  short    op[4];
  unsigned long   active_time;
  char  *extension;   /* 拡張データ用 */
}      ItemData;

typedef struct crtdata_t {
  int      level;
  ItemData *product;
  ItemData *fail_product;
  ItemData *staff1;
  ItemData *staff2;
  struct crtdata_t *next;
} CrtData;

extern CrtData *first_create_data;

typedef struct ido_t {
  short    gp;      /* gp>0 の時はお金 -1:罠(20001215 拡張)*/
  ItemData id;
  struct ido_t *next;   /* 次の要素へのポインタ */
}      ItemOData;

/* 座標ハッシュ */
/* add 20020806 */
typedef struct idp_t {
  short int    x, y;      /* 位置 */
  ItemOData  *fid;
  int     notouch;  /* file 追出し用 */
  struct idp_t *next;
}   ItemPData;

typedef struct td_t {
  int    type;      /* 会話タイプ */
  char  *string;    /* メッセージ */
}      TalkData;

typedef struct ad_t {
  int    type;
  char  *string;
}      AppData;

#define HASHMASK 0xff
#define HASHMAX (HASHMASK + 1)

typedef struct str_t {
  struct str_t *next;
  int    c;  /* str_gc()(必要なくなった文字列の解放)に使われるチェック */
  char   *string;
}      StringData;

/* ItemOData *ihash[HASHMAX]; */  /* ハッシュ配列 */
extern ItemPData *iphash[HASHMAX];
extern StringData *shash[HASHMAX];

/* マスター権限管理 added by Qajin 19991023 */
typedef struct authority_t {
   short main;
   short  sub;
   int  *territory;         /* 領土 */
}     MasterAuth;

/* マスター権限フラグ                */
/* サブは自領土内でのみ 有効 */
#define MS_ITEM     0x0010
#define MS_KICK     0x0020
#define MS_BAN      0x0040
#define MS_LOAD     0x0080
#define MS_SHUTDOWN 0x0100
#define MS_TRANS    0x0200
#define MS_QUEST    0x0400
#define MS_UNLOCK   0x0001
#define MS_RESOURCE 0x0002
#define MS_DELETE   0x0004
#define MS_ALL      0xffff

/* add for multi character stay in one 20000721 */
#ifndef MAX_CHRS_INONE
#define MAX_CHRS_INONE 2
#endif

/* MAP info added 20000705 */
typedef struct maps {
  unsigned short int tip;    /* 地形データ */
  unsigned char  surface;
  unsigned char  area;
  signed char    height;  /* 高さ -1..3 (0=通常) PHIDM NEO v1.13 */
#ifdef AREA_SUB
  unsigned char  area_sub;
#endif
#if MAX_ENEMIES > 191
  short int      chr[MAX_CHRS_INONE]; /* キャラ番号 */
#else
  unsigned char  chr[MAX_CHRS_INONE]; /* キャラ番号 */
#endif
}       MapData;
#define UNSEE(n,y,x)    {memset(&(cd[(n)].view[(y)][(x)]),'\0',sizeof(MapData)); cd[(n)].view[(y)][(x)].tip = T_UNKNOWN;}


/* for variables 20010704 */
#ifndef GVAR_MAX
#define GVAR_MAX 0x100
#endif
#ifndef PVAR_MAX
#define PVAR_MAX 16
#endif
#ifndef SVAR_MAX
#define SVAR_MAX 16
#endif

extern int    generic_variable[GVAR_MAX]; /* 変数 */

/* Character Extension data added 20000929 */
typedef struct cd_e {
    char *ext_data;
    struct cd_e *next;
}       CHRExData;

#include "ext_skills.h"

/* add for initial charactor graphic 20050412 */
#define MAX_INIT_GRAPHIC    32

extern char init_graphic[MAX_INIT_GRAPHIC][32];
extern int init_graph_n;


typedef struct cd_t {
  int    fd;
  char   uid[16];
  char   adr[48];
  char  *name;
  char  *cg;
  char  *ocg; /* for PSP gra 20030728 */
  int    sp;
  int    lv[MAX_LEVEL];
  int  **skill; /* for skills 20020307 */
  long   mhp, mmp;
  long   hp, mp;
  int    gp;
  int    afn, dfn, sfn;
  int    exp;
  int    cn[MAX_CN];
  double ncn[MAX_NCN];
  int    ix, iy;   /* ビーコンの設置場所 */
  int    x, y, d;
  int    bx, by;   /* 直前にいた場所。スイッチの作動に関係 */
  int    st;
  long   wt;
  ItemData *weq;
  ItemData *aeq;
  ItemData *oeq;
  int    type;
  int    align;
  int    moral, think;
  int    angry;
  int    cooldown;   /* angry の解除確率 */
  int    gasify;  /* add 20020822 */
  int    eye;    /* クライアントにフォーカスがあっているか */
  long   last;
  MapData view[5][5];
  ItemData id[MAX_ITEMS];
  MagicData md[MAX_MAGICS];
  TalkData td[MAX_TALKS];
  AppData ad[MAX_APPS];
  char   com[10];
  int    trace;
  int    target;
  int    party;  /* add 20020822 */
  char   str[256];
  int    damaged;
  long   vf;
  long   vf2;     /* テンキー入力リスト選択用 added by Qajin 19991203 */
  unsigned long pflag;   /* キャラ固有フラグ add 20000804 change 20010622 */
  int    pvar[PVAR_MAX]; /* 固有変数 add 20010704 */
  int    svar[SVAR_MAX]; /* 保存変数 add 20010704 */
  int    et;      /* 発生種 */
  int    ec;      /* 発生許可 */
  int    cv;      /* プロトコルバージョン */
                  /* version-cliのフォーマット：8桁 (int7桁)
                     05090001 上4桁＝対応するDM 下4桁＝クライアント独自 */
  int    wsep, asep, osep;  /* Special Effect no Point */
  int    at;      /* Accessory type */
  int    anmf;      /* メッセージ番号 */
  int    anmt;      /* メッセージ待ち時間 add 20001122 */
  int    m_wait, m_wait_c;  /* map送信間隔とそのカウンター */
  int    s_wait, s_wait_c;  /* status送信間隔とそのカウンター */
  int    modef;    /* 各種モード(旧 boardf) */
  int    undetect;    /* Identifyがかからないステータス */
  int    innate;    /* 生来の能力 */
  int    wd;      /* 総合攻撃可能方向 (NPC) 攻撃方向 (PC)*/
  MasterAuth *auth;  /* マスター権限 */
  int    parse;      /* コマンドパース on/off */
  char   sg;         /* 強制 Shrink/Gigant */
  char  *formatA;    /* format-A対応 */
  CHRExData *extension; /* 拡張データ */
  int    big;       /* 2x2 の大きな敵 (CHROData.big の引き継ぎ) */
}      CHRData;  /* PC,NPC 共有キャラクターデータ */

typedef struct csd_t {
  int  sf;         /* sf!=0 qの時は、これより sf 個が<select...>の範疇 */
  char flg;                     /* 記述している物の種類 */
  int  nm;                      /* NPC の所持確率 */
  union {
    ItemData  id;
    MagicData md;
    TalkData  td;
    AppData   ad;
  } data;
}      CHRSData;   /* NPC の持ち物の候補 */

typedef union atom_t {
  int    di;
  char   dc;
  char  *dp;
  ItemData *dt;
  unsigned long dl; /* add 20020830 */
}      ATOM;

typedef struct msg_t {
  short int  orgn;  /* add for used boid number bug 20041117 */
  short int  next;
  short int  wait;
  char  type;
  union data_t {
    struct pos_t {
      short int x,y;
    } pos;
    unsigned long flag;
    char  *mesg;
  } data;
}      MSGData;

typedef struct cdo_t {
  char   name[32];    /* 名前 */
  char   cg[16];    /* 表示グラフィック名 */
  int    nm;      /* 最大発生数(定数)／発生確率(確率) */
  int    nr;      /* 発生数 */
  int    type;      /* タイプ */
  int    align;     /* カルマ */
  int    moral;     /* モラル */
  int    sp;      /* スピード */
  int    flv, wlv;    /* レベル */
  int    mhp, mmp;    /* HP/MP */
  int    nhp, nmp;    /* Hp/Mp xN */
  int    gp;      /* 所持金 */
  int    ix, iy, d;   /* 発生位置と範囲 */
  int    st, et;    /* 発生時刻 */
  int    apc;     /* 再発生タイプ */
  int    anm;     /* 再発生が始まる数(定数)／最大発生数(確率) */
  int    apt;     /* 再発生までの時間 */
  int    apcode;    /* 召喚コード */
  int    aplvl;     /* 召喚推奨レベル */
  char  *apnm;      /* 召喚アイテム名 */
  char  *apdm;      /* 消滅アイテム名 */
  int    undetect;    /* Identifyがかからないステータス */
  int    innate;    /* 生来の能力 */
  int    cooldown;              /* 怒りが収まる割合 */
  CHRSData *sd;     /* 所持データ */
  int    fdn;       /* フラグON数(消滅時) add by Qajin 20000522 */
  int    fdead;     /* フラグNO(消滅時)   */
  int    fbn;       /* フラグON数(発生時) */
  int    fborn;     /* フラグNO(発生時)   */
  int    threaten;  /* 必要最低棲息時間 */
  int    big;       /* 2x2 の大きな敵 (phi.ed の :O B) */
}      CHROData;   /* NPC の元データ */

typedef struct fd_t {
  int    area;      /* PSP6 */
  int    floor;     /* フロア */
#ifndef AREA_SUB
  char   name[32];    /* 名称 */
#else
  char  *name[16];    /* 名称 */
#endif
  int    flag;
  int    teleport;    /* 固定位置 */
  /** psp6 by aleph **/
  char   *mapset;  /* マップパーツセット */
  char   bgm[32];     /* bgm名 */
  /** end **/
  int    fin;       /* 入った時フラグ */
  int    fout;      /* 出た時 */
  int    fexist;    /* 居る時 */
  int    fmove;     /* 動いた時 */
}      FLRData;

typedef struct ft_t {
  int    x, y;
}      FLRTData;

typedef struct ps_t {
  MagicData pwmg;
  int    multiplier;
}      PowerSpellData;   /* パワースペルの構造体 */

typedef struct exgd_t {
  int   x;
  int   y;
  char  name[32];
  char  ip[16];
}       ExGateData;         /* アイテムの輸出規制に使うゲートデータ */

typedef struct ex_t {
  char   type;
  union {
    ExGateData *gd;
    ItemData *id;
    MagicData *md;
  } data;
  struct ex_t *next;
}        ExportItemData;  /* アイテムの輸出規制用のデータ */

extern ExportItemData  *first_export_data;

typedef struct banned {
  char  *ip;
  unsigned long  time;             /* 期限切れ時間 */
}       BannedIP;         /* IPアドレスアクセス制限用データ added by Qajin 19991020 */

#define BANNEDIPMAX 16    /* 個数は取り敢えずの数 */

#define BANNEDIDMAX 16
extern char id_ban[BANNEDIDMAX][16];


#if MAX_OBJECTS > 0

typedef struct objects {  /* オブジェクトデータ */
  char  *gname;           /* グラフィック名     */
  unsigned char dir;      /* 向き               */
  char  *name;            /* 表示名             */
  int    x,y;             /* 位置               */
  char  *id;              /* ID(識別名)         */
}       ObjData;

extern ObjData od[MAX_OBJECTS];
#if MAX_OBJECTS > 255
extern unsigned short dobj[MAX_Y][MAX_X];
#else
extern unsigned char dobj[MAX_Y][MAX_X];
#endif

extern int    is_see_magic_effects;    /* 魔法視覚化 on/off */

#endif

extern FILE  *sfp;
extern char   tcp_addr[16];
extern char   land_name[32];
extern MapData map[MAX_Y][MAX_X];
extern long   tax;
extern int    tslide;  /* 時差 */
extern int yslide;  /* 西暦との差 */
extern char   yname[32];  /* 暦号 */
extern int    max_enemy;
extern CHRData cd[MAX_NTTYS + MAX_ENEMIES];
extern int    eflag;

extern int    log_op;

extern int    adding_power_spell_flag; /* Power spell, 乗算／加算のいずれか */
extern int    npc_use_charged_item;    /* NPC が charged item を使うか */
extern int    ciflag;      /* 通貨単位フラグ */
extern int    jgflag;      /* JEWELのGP化フラグ */
extern int    nciflag;           /* 税金の大きさを GP 単位にする */
extern int    ggflag;
       /* NPC の MHP に対する乱数の揺れによる巨大化境界値前後の揺れを許す */
extern char   cirunit[20];   /* 通貨単位 */
extern int    cirate;      /* 換算レート (100 gp がいくらか？)*/
extern int    trnf;      /* 連続移動フラグ */
extern int    addf, addx, addy;  /* 自動登録フラグ */
extern char   addw[5];     /* 自動登録用世界識別子 */
extern int    wizeyef;     /* アクセス時 wiz. light/eye */
extern int    autoeqf;     /* 転送時自動装備フラグ */
extern char   roofoff[23];   /* Eagle eye で見た時、屋根を消すもの */
extern int    sport;     /* サーバのポート */
extern long    s_id;      /* ID番号 */
extern FLRData *fld;     /* フロアデータ */
extern FLRTData fldt[26];
extern ATOM **swtd;      /* スイッチデータ */
extern int    swtn;      /* スイッチデータ数 */
extern MSGData *msgd;    /* メッセージデータ add 20030501 */
extern int      msgn;    /* メッセージデータ数 */
extern struct tm *sttime;  /* 現在時刻 (phi_dm.cpp で毎分更新) */
extern ATOM **gatedata;                /* ゲートデータ */
extern int    gatemax;                 /* ゲートデータ数 */
extern PowerSpellData power_spell[MAX_POWER_SPELL];  /* 倍加呪文のパターン */
extern int    is_see_burst;            /* バーストを他の PC に知らせるかどうか */
extern int    is_need_charge_time;     /* charged item に charge time を授けるか */
/*char  *server_ip_address;*/       /* サーバの IP アドレス */
extern int    is_need_cast_time;   /* 魔法名を直接書いても唱える時間を必要とするか */
extern int    is_single_use;      /* use で一度に複数のものを指定不可にするか */
extern int    is_destroy_companion;    /* 何を destroy したかを回りに知らせるか */
extern int    is_kill_master;          /* マスターを特別視しない */
extern int    is_speed_degrade;        /* 劣化を頻繁に起こるようにする */
extern int    is_degrade_charge;       /* チャージアイテムの使用で劣化するか */
extern int    is_limit_mrp;            /* create で MRP の上昇を確率で制限するか */
extern BannedIP ip_ban[BANNEDIPMAX];   /* アクセス禁止アドレス */
extern unsigned char wall_for_beast;          /* クリーチャーが知的生物バリアを壁とみなすか */
extern int    magic_anti_degrade;      /* アイテムの劣化に対して魔法強化を有効にするか */
extern int    spellup_only_time;       /* SpellUpの効果を時間延長のみに限定するか */
extern int    limit_addusers;          /* …(謎 って解るって^^; */
extern int    fighter_wizard;          /* Coat/SpellUp 併用 */
extern int    multi_access;            /* アクセス可能数 */
extern int    save_condition;          /* コンディション保存 */
extern int    lv_up_between_pc;        /* PC間 level up */
extern int    gift_tax;                /* 所得税/贈与税 */
extern int    offline_exp;             /* 切断中経験値 */
extern int    flagsave;                /* snフラグ保存 */
extern int    deny_pk;                 /* PK禁止 */
extern int    death_penalty;           /* 死亡ペナルティ */
extern int    penalty_rate;            /* 死亡ペナルティ割合  add by NEO 2008/03/27 */
extern int    dp_limitter;             /* デスペナリミッタ設定  add by NEO 2011/12/4 */
extern int    dp_beginner;             /* 初心者の加護設定  add by NEO 2011/12/4 */
extern int    rebirth_life;            /* 復活時HP */
extern int    access_dm;               /* アクセスDMメッセージ */
extern int    counter_effect;          /* カウンター効果係数  add by NEO 2011/12/6 */
extern int    attack_effect;           /* 新攻撃効果設定  add by NEO 2011/12/6 */
extern int    axe_blade_setting;       /* 斧＆ブレード補正  add by NEO 2011/12/8 */
extern int    sys_magic_no_degrade;    /* システム魔法で武器が欠けないようにするか  add by NEO 2012/2/11 */
extern int    mhp_up_for_attack;       /* 通常攻撃により抽選で最大HPを増加させるか add by NEO 2012/2/12 */
extern int    mmp_up_for_attack;       /* 魔法攻撃により抽選で最大MPを増加させるか add by NEO 2012/2/12 */
extern int    spell_learning;          /* 魔法ラーニング設定 added by NEO 2012/2/12 */
extern int    degrade_by_magic;        /* 魔法ダメージでも鎧・アクセが欠けるか（デフォルトなし） add by NEO 2012/2/12 */

#define FLAG_OFF  0
#define FLAG_ON   1
#define FLAG_REV  2
#ifndef GFLAG_MAX
#define GFLAG_MAX 0x100
#endif
#define PFLAG_MAX 32
#define SFLAG_MAX 16

extern int    generic_flag[GFLAG_MAX]; /* フラグ */

#define DSPACE  -1

#define TOTAL_LEVEL  (cd[n].lv[LV_FIGHTER] + cd[n].lv[LV_WIZARD] + cd[n].lv[LV_MERCHANT] + cd[n].lv[LV_CREATER] + cd[n].exp / 10 )  /* Add for トータルレベル表示 by NEO 2008/04/04 */


void   signal_trap(int);
void   log_print(char *);
int    init_connected_cd(int);
void   close_connect(int);
void   check_write(int, char *);
void   write_companion(int, int, char *, int);
void   set_view(int);
void   dd_char(char *, MapData, int);
void   print_map(int, int);
void   print_status(int);
void   move_chr(int, int, int, int, int, int);
void   set_chr(int);
void   clear_chr(int);
void   clear_chr_pos(int n, int x, int y);
void   input_analyze(int, char *);
void   auto_equip(int);
void   adduser(int, char *);
int new_user(int);
int    command_analyze(int, char *);
int    talk_analyze(int, char *);
void   func_analyze(int, char *);
void   socket_broken_error_message(char *,char *, int);
void   adduser(int, char *);
char   *errnum_to_msg(int);
void   connection_error_process(int, int);
void   charagra_set(int n, char *crgr);   /* add by NEO */
int    twobyte_check(char *crgr);         /* add by NEO */
int    next_level_check(int n);  /* add by NEO for レベルチェック表示 2008/04/05 */
int   level_up_print(int n); /* レベルアップ表示用　by NEO 2008/04/29 */
int   save_mail(int n, char *mailto, char *msg); /* Add for メールシステム:メールの保存 by NEO 2008/05/02 */
int   read_mail(int n); /* Add for メールシステム:メールの読み込み by NEO 2008/05/02 */
int   delete_mail(int n); /* Add for メールシステム:メールの消去 by NEO 2008/05/02 */
int   check_mail(int n); /* Add for メールシステム:メールのチェック by NEO 2008/05/02 */
int   strindex( char *ln, char *s ); /* 文字列から特定の文字を探しその位置を返す */
void  readhomeland(int n, char *homeland); /* 出身地表示 by NEO 20111112 */

#endif
