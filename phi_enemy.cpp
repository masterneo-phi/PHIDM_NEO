
/*  Phantasmal Island ( Dungeon Master )  1994-5(C)b-Yak-38 */

#include  <cstdio>
#include  <cstdlib>
#include  <cstring>
#include  <cmath>
#include  <ctime>
#include  <climits>
#include  <array>

#include  "phi_dm.h"
#include  "phi_data.h"
#include  "phi_enemy.h"
#include  "phi_calc.h"
#include  "phi_use.h"
#include  "phi_magic.h"
#include  "phi_str.h"

namespace {

/* 歩行・回転のコマンド文字列。添字は STEP_* (phi_enemy.h)。
   input_analyze() の引数が char * (phi_dm.h のモジュール間
   インターフェース) のため、呼び出し側で const_cast する。 */
constexpr std::array<const char *, 7> kGoCom =
{"go", "go l",   "go r",   "go b",
 "turn l", "turn r", "turn b"};

} // namespace

#ifdef DEBUG
char  debug_buf[256];
#endif

extern int px[];
extern int py[];

namespace {

/* 視界 (5x5) の走査順。view[ kSy[sa] + 1 ][ kSx[sa] + 2 ] の形で使う。 */
constexpr std::array<int, 25> kSx =
{0, -1, 1, 0, 0, -1, 1, -1, 1, 0, -2, 2, 0, -1, 1, -2, 2, -2, 2, -1, 1, -2, 2, -2, 2};

constexpr std::array<int, 25> kSy =
{1, 0, 0, -1, 0, 1, 1, -1, -1, 2, 0, 0, 3, 2, 2, 1, 1, -1, -1, 3, 3, 2, 2, 3, 3};

} // namespace

extern int enmax; /* move for controlable sammon 20000614 */
extern CHROData *ed_o;

int    try_to_attackstep(int n, int m, int vx, int vy, int mc)
{
  /* vy^        */
  /*   |        */
  /*   +--> vx  */

  int calc[7];
  int wd;
  int max_calc;
  int max_num;
  int i,j;
  int d;

  for(i=0; i<7; i++)
    calc[i]=100;

  /* 移動不可は消去 */
  if(!(mc & MC_F)) calc[STEP_GO  ]=0;
  if(!(mc & MC_L)) calc[STEP_GO_L]=0;
  if(!(mc & MC_R)) calc[STEP_GO_R]=0;
  if(!(mc & MC_B)) calc[STEP_GO_B]=0;

  /* 視界外に出す動きは消去 */
  if(vy==0) calc[STEP_GO    ]=0;
  if(vy==3) calc[STEP_TURN_B]=0;
  if(vy==4){
    calc[STEP_GO_B  ]=0;
    calc[STEP_TURN_L]=0;
    calc[STEP_TURN_R]=0;
    calc[STEP_TURN_B]=0;
  }
  if(vx==0){
    calc[STEP_GO_R  ]=0;
    calc[STEP_TURN_R]=0;
  }
  if(vx==4){
    calc[STEP_GO_L  ]=0;
    calc[STEP_TURN_L]=0;
  }

  /* 隣接すると攻撃できる動きは倍の評価 */
  wd = cd[n].wd;
  if(vy==3 && vx==2)
    if(wd & WD_FORE) calc[STEP_GO]*=2;
  if(vy==2 && vx==1){
    if(wd & WD_LEFT ) calc[STEP_GO  ]*=2;
    if(wd & WD_FORE ) calc[STEP_GO_L]*=2;
  }
  if(vy==2 && vx==2){
    if(wd & WD_LEFT ) calc[STEP_TURN_R]*=2;
    if(wd & WD_RIGHT) calc[STEP_TURN_L]*=2;
    if(wd & WD_BACK ) calc[STEP_TURN_B]*=2;
  }
  if(vy==2 && vx==3){
    if(wd & WD_RIGHT) calc[STEP_GO  ]*=2;
    if(wd & WD_FORE ) calc[STEP_GO_R]*=2;
  }
  if(vy==1 && vx==0)
    if(wd & WD_LEFT ) calc[STEP_GO_L]*=2;
  if(vy==1 && vx==1){
    if(wd & WD_FORE ) calc[STEP_TURN_L]*=2;
    if(wd & WD_RIGHT) calc[STEP_TURN_B]*=2;
    if(wd & WD_BACK ) calc[STEP_TURN_R]*=2;
  }
  if(vy==1 && vx==2){
    if(wd & WD_FORE ) calc[STEP_GO_B]*=2;
    if(wd & WD_LEFT ) calc[STEP_GO_R]*=2;
    if(wd & WD_RIGHT) calc[STEP_GO_L]*=2;
    if(wd & WD_BACK ) calc[STEP_GO  ]*=2;
  }
  if(vy==1 && vx==3){
    if(wd & WD_FORE ) calc[STEP_TURN_R]*=2;
    if(wd & WD_LEFT ) calc[STEP_TURN_B]*=2;
    if(wd & WD_BACK ) calc[STEP_TURN_L]*=2;
  }
  if(vy==1 && vx==4)
    if(wd & WD_RIGHT) calc[STEP_GO_R]*=2;
  if(vy==0 && vx==1){
    if(wd & WD_LEFT ) calc[STEP_GO_B]*=2;
    if(wd & WD_BACK ) calc[STEP_GO_L]*=2;
  }
  if(vy==0 && vx==2){
    if(wd & WD_FORE ) calc[STEP_TURN_B]*=2;
    if(wd & WD_RIGHT) calc[STEP_TURN_R]*=2;
    if(wd & WD_LEFT ) calc[STEP_TURN_L]*=2;
  }
  if(vy==0 && vx==3){
    if(wd & WD_RIGHT) calc[STEP_GO_B]*=2;
    if(wd & WD_BACK ) calc[STEP_GO_R]*=2;
  }

  /* 自ら敵の武器攻撃範囲に入る行動は評価を半分 */
  if(!((cd[m].st & 0xf0) == ST_SPELL)){
    wd = cd[m].wd;
    d=(4 + cd[m].d - cd[n].d)%4;
    if(   (d==0 && (wd & WD_FORE))
       || (d==1 && (wd & WD_LEFT))
       || (d==2 && (wd & WD_BACK))
       || (d==3 && (wd & WD_RIGHT))){ /* 同方向 */
      if(vy==0 && vx==1) calc[STEP_GO_L]/=2;
      if(vy==0 && vx==2){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==3) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_LEFT))
       || (d==1 && (wd & WD_BACK))
       || (d==2 && (wd & WD_RIGHT))
       || (d==3 && (wd & WD_FORE))){  /* 左方向 */
      if(vy==2 && vx==3) calc[STEP_GO]/=2;
      if(vy==1 && vx==3){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==3) calc[STEP_GO_B]/=2;
      if(vy==1 && vx==4) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_BACK))
       || (d==1 && (wd & WD_RIGHT))
       || (d==2 && (wd & WD_FORE))
       || (d==3 && (wd & WD_LEFT))){ /* 下方向 */
      if(vy==3 && vx==2) calc[STEP_GO]/=2;
      if(vy==2 && vx==2){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==2 && vx==1) calc[STEP_GO_L]/=2;
      if(vy==2 && vx==3) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_RIGHT))
       || (d==1 && (wd & WD_FORE))
       || (d==2 && (wd & WD_LEFT))
       || (d==3 && (wd & WD_BACK))){ /* 右方向 */
      if(vy==2 && vx==1) calc[STEP_GO]/=2;
      if(vy==1 && vx==1){
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==1) calc[STEP_GO_B]/=2;
      if(vy==2 && vx==0) calc[STEP_GO_L]/=2;
    }
  }

  /* 敵から遠ざかる動きは評価を半分 */
  if(vy>=1) calc[STEP_GO_B]/=2;
  if(vy<=1) calc[STEP_GO  ]/=2;
  if(vx<=2) calc[STEP_GO_R]/=2;
  if(vx>=2) calc[STEP_GO_L]/=2;
  /* 敵への攻撃ポイントを近付けない回転は評価を半分 */
  wd = cd[n].wd;
  if(vy==4 || vy==3 || (vy==2 && 1<=vx && vx<=3)){
    if(!(wd & WD_LEFT )) calc[STEP_TURN_R]/=2;
    if(!(wd & WD_RIGHT)) calc[STEP_TURN_L]/=2;
    if(!(wd & WD_BACK )) calc[STEP_TURN_B]/=2;
  } else if(vy==0 && 1<=vx && vx<=3){
    if(!(wd & WD_FORE )) calc[STEP_TURN_B]/=2;
    if(!(wd & WD_LEFT )) calc[STEP_TURN_L]/=2;
    if(!(wd & WD_RIGHT)) calc[STEP_TURN_R]/=2;
  } else if(vx<=1){
    if(!(wd & WD_FORE )) calc[STEP_TURN_L]/=2;
    if(!(wd & WD_RIGHT)) calc[STEP_TURN_B]/=2;
    if(!(wd & WD_BACK )) calc[STEP_TURN_R]/=2;
  } else {
    if(!(wd & WD_FORE )) calc[STEP_TURN_R]/=2;
    if(!(wd & WD_LEFT )) calc[STEP_TURN_B]/=2;
    if(!(wd & WD_BACK )) calc[STEP_TURN_L]/=2;
  }

  /* 計算と行動 */
  for(i=0,max_calc=0,max_num=0; i<7; i++){
    if(max_calc < calc[i]){
      max_calc = calc[i];
      max_num = 1;
    } else if(max_calc == calc[i])
      max_num++;
  }
  if(max_calc==0) return 0;
  j=frand(max_num)+1;
  i=0;
  while(1){
    if(calc[i]==max_calc){
      if(j==1) break;
      j--;
    }
    i++;
  }
  input_analyze(n, const_cast<char *>(kGoCom[i]));
  return 1;
}

int   try_to_wizardstep(int n, int m, int vx, int vy, int mc)
{

  /* vy^        */
  /*   |        */
  /*   +--> vx  */

  int calc[7];
  int wd;
  int max_calc;
  int max_num;
  int i,j;
  int d;

  for(i=0; i<7; i++)
    calc[i]=100;

  /* 移動不可は消去 */
  if(!(mc & MC_F)) calc[STEP_GO  ]=0;
  if(!(mc & MC_L)) calc[STEP_GO_L]=0;
  if(!(mc & MC_R)) calc[STEP_GO_R]=0;
  if(!(mc & MC_B)) calc[STEP_GO_B]=0;

  /* 視界外に出す動きは消去 */
  if(vy==0) calc[STEP_GO    ]=0;
  if(vy==3) calc[STEP_TURN_B]=0;
  if(vy==4){
    calc[STEP_GO_B  ]=0;
    calc[STEP_TURN_L]=0;
    calc[STEP_TURN_R]=0;
    calc[STEP_TURN_B]=0;
  }
  if(vx==0){
    calc[STEP_GO_R  ]=0;
    calc[STEP_TURN_R]=0;
  }
  if(vx==4){
    calc[STEP_GO_L  ]=0;
    calc[STEP_TURN_L]=0;
  }

  /* 敵を攻撃可能範囲から外す行動は評価を１／２減 */
  /* 敵との位置関係を良くする評価を２倍 */
  /* 前後移動はここでは評価しない */
  clear_chr(n);
  wd=direction_check(cd[n].x,cd[n].y,cd[n].d,cd[m].x,cd[m].y);
  if(wd & ED_LINE){
    calc[STEP_GO_R]/=2;
    calc[STEP_GO_L]/=2;
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_B]/=2;
  } else if(wd & ED_FOREAROUND){
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_B]/=2;
    wd = direction_check(X_RIGHT(n), Y_RIGHT(n), cd[n].d, cd[m].x, cd[m].y);
    if(wd & ED_LINE)                calc[STEP_GO_R]*=2;
    else if (!(wd & ED_FOREAROUND)) calc[STEP_GO_R]/=2;
    wd = direction_check(X_LEFT(n), Y_LEFT(n), cd[n].d, cd[m].x, cd[m].y);
    if(wd & ED_LINE)                calc[STEP_GO_L]*=2;
    else if (!(wd & ED_FOREAROUND)) calc[STEP_GO_L]/=2;
  } else {
    wd = direction_check(X_RIGHT(n), Y_RIGHT(n), cd[n].d, cd[m].x, cd[m].y);
    if(wd & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO_R]*=2;
    wd = direction_check(X_LEFT(n), Y_LEFT(n), cd[n].d, cd[m].x, cd[m].y);
    if(wd & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO_L]*=2;
    wd = direction_check(cd[n].x, cd[n].y, D_RIGHT(n), cd[m].x, cd[m].y);
    if(wd & (ED_LINE | ED_FOREAROUND)) calc[STEP_TURN_R]*=2;
    wd = direction_check(cd[n].x, cd[n].y, D_BACK(n), cd[m].x, cd[m].y);
    if(wd & (ED_LINE | ED_FOREAROUND)) calc[STEP_TURN_B]*=2;
    wd = direction_check(cd[n].x, cd[n].y, D_LEFT(n), cd[m].x, cd[m].y);
    if(wd & (ED_LINE | ED_FOREAROUND)) calc[STEP_TURN_L]*=2;
  }
  set_chr(n);

  /* 自ら敵の武器攻撃範囲に入る行動は評価を半分 */
  if(!((cd[m].st & 0xf0) == ST_SPELL)){
    wd = cd[m].wd;
    d=(4 + cd[m].d - cd[n].d)%4;
    if(   (d==0 && (wd & WD_FORE))
       || (d==1 && (wd & WD_LEFT))
       || (d==2 && (wd & WD_BACK))
       || (d==3 && (wd & WD_RIGHT))){ /* 同方向 */
      if(vy==0 && vx==1) calc[STEP_GO_L]/=2;
      if(vy==0 && vx==2){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==3) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_LEFT))
       || (d==1 && (wd & WD_BACK))
       || (d==2 && (wd & WD_RIGHT))
       || (d==3 && (wd & WD_FORE))){  /* 左方向 */
      if(vy==2 && vx==3) calc[STEP_GO]/=2;
      if(vy==1 && vx==3){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==3) calc[STEP_GO_B]/=2;
      if(vy==1 && vx==4) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_BACK))
       || (d==1 && (wd & WD_RIGHT))
       || (d==2 && (wd & WD_FORE))
       || (d==3 && (wd & WD_LEFT))){ /* 下方向 */
      if(vy==3 && vx==2) calc[STEP_GO]/=2;
      if(vy==2 && vx==2){
  calc[STEP_TURN_L]/=2;
  calc[STEP_TURN_R]/=2;
  calc[STEP_TURN_B]/=2;
      }
      if(vy==2 && vx==1) calc[STEP_GO_L]/=2;
      if(vy==2 && vx==3) calc[STEP_GO_R]/=2;
    }
    if(   (d==0 && (wd & WD_RIGHT))
       || (d==1 && (wd & WD_FORE))
       || (d==2 && (wd & WD_LEFT))
       || (d==3 && (wd & WD_BACK))){ /* 右方向 */
      if(vy==2 && vx==1) calc[STEP_GO]/=2;
      if(vy==1 && vx==1){
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_B]/=2;
      }
      if(vy==0 && vx==1) calc[STEP_GO_B]/=2;
      if(vy==2 && vx==0) calc[STEP_GO_L]/=2;
    }
  }

  /* 敵に近付く動きは評価を半分 */
  if(vy==4 || vy==3 || (vy==2 && 1<=vx && vx<=3)) calc[STEP_GO]/=2;
  if(vy==0 && 1<=vx && vx<=3) calc[STEP_GO_B]/=2;
  if((vx==0 && vy<=3) || (vx==1 && vy<=2)) calc[STEP_GO_L]/=2;
  if((vx==4 && vy<=3) || (vx==3 && vy<=2)) calc[STEP_GO_R]/=2;

  /* 敵を正面に捕らえない回転は評価を半分 */
  if(vy==4 || (vy==3 && 1<=vx && vx<=3) || (vy==2 && vx==2)){
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_B]/=2;
  } else if(vy==0 && vx==2){
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
  } else if(vx<=1){
    calc[STEP_TURN_B]/=2;
    calc[STEP_TURN_R]/=2;
  } else {
    calc[STEP_TURN_B]/=2;
    calc[STEP_TURN_L]/=2;
  }

  /* 計算と行動 */
  for(i=0,max_calc=0,max_num=0; i<7; i++){
    if(max_calc < calc[i]){
      max_calc = calc[i];
      max_num = 1;
    } else if(max_calc == calc[i])
      max_num++;
  }
  if(max_calc==0) return 0;
  j=frand(max_num)+1;
  i=0;
  while(1){
    if(calc[i]==max_calc){
      if(j==1) break;
      j--;
    }
    i++;
  }
  input_analyze(n, const_cast<char *>(kGoCom[i]));
  if(i==0 && max_num == 1 && PER_DICE < 30)
    cd[n].think = TH_NONE;
  return 1;
}

int    try_to_tracestep(int n, int m, int vx, int vy, int mc)
{
  /* vy^        */
  /*   |        */
  /*   +--> vx  */

  int calc[7];
  int max_calc;
  int max_num;
  int i,j;

  for(i=0; i<7; i++)
    calc[i]=100;

  /* 移動不可は消去 */
  if(!(mc & MC_F)) calc[STEP_GO  ]=0;
  if(!(mc & MC_L)) calc[STEP_GO_L]=0;
  if(!(mc & MC_R)) calc[STEP_GO_R]=0;
  if(!(mc & MC_B)) calc[STEP_GO_B]=0;

  /* 視界外に出す動きは消去 */
  if(vy==0) calc[STEP_GO    ]=0;
  if(vy==3) calc[STEP_TURN_B]=0;
  if(vy==4){
    calc[STEP_GO_B  ]=0;
    calc[STEP_TURN_L]=0;
    calc[STEP_TURN_R]=0;
    calc[STEP_TURN_B]=0;
  }
  if(vx==0){
    calc[STEP_GO_R  ]=0;
    calc[STEP_TURN_R]=0;
  }
  if(vx==4){
    calc[STEP_GO_L  ]=0;
    calc[STEP_TURN_L]=0;
  }

  /* 目標から遠ざかる動きは評価を半分 */
  if(vy>=1) calc[STEP_GO_B]/=2;
  if(vy<=1) calc[STEP_GO  ]/=2;
  if(vx<=2) calc[STEP_GO_R]/=2;
  if(vx>=2) calc[STEP_GO_L]/=2;

  /* 目標を正面に捕らえない回転は評価を半分 */
  if(vy==4 || vy==3 || (vy==2 && 1<=vx && vx<=3)){
    calc[STEP_TURN_R]/=2;
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_B]/=2;
  } else if(vy==0 && 1<=vx && vx<=3){
    calc[STEP_TURN_L]/=2;
    calc[STEP_TURN_R]/=2;
  } else if(vx<=1){
    calc[STEP_TURN_B]/=2;
    calc[STEP_TURN_R]/=2;
  } else {
    calc[STEP_TURN_B]/=2;
    calc[STEP_TURN_L]/=2;
  }

  /* 計算と行動 */
  for(i=0,max_calc=0,max_num=0; i<7; i++){
    if(max_calc < calc[i]){
      max_calc = calc[i];
      max_num = 1;
    } else if(max_calc == calc[i])
      max_num++;
  }
  if(max_calc==0) return 0;
  j=frand(max_num)+1;
  i=0;
  while(1){
    if(calc[i]==max_calc){
      if(j==1) break;
      j--;
    }
    i++;
  }
  input_analyze(n, const_cast<char *>(kGoCom[i]));
  return 1;
}

int    try_to_runstep(int n, int m, int vx, int vy, int mc, int damaged)
{

  /* vy^        */
  /*   |        */
  /*   +--> vx  */

  int calc[7];
  int max_calc;
  int max_num;
  int i,j;
  int wd, wd2, d;
  int xx, yy;

  for(i=0; i<7; i++)
    calc[i]=100;

  /* 移動不可は消去 */
  if(!(mc & MC_F)) calc[STEP_GO  ]=0;
  if(!(mc & MC_L)) calc[STEP_GO_L]=0;
  if(!(mc & MC_R)) calc[STEP_GO_R]=0;
  if(!(mc & MC_B)) calc[STEP_GO_B]=0;

  if(vx!=-1 && vy!=-1){ /* 視界内 */

    /* 目標に近付く動きは評価を半分 */
    if(vy>=2) calc[STEP_GO  ]/=2;
    if(vy==0) calc[STEP_GO_B]/=2;
    if(vx<=1) calc[STEP_GO_L]/=2;
    if(vx>=3) calc[STEP_GO_R]/=2;

    /* 敵を正面に捉える回転は評価を半分 */
    if(vy==0 && 1<=vx && vx<=3)
      calc[STEP_TURN_B]/=2;
    if(vx == 0 || (vx == 1 && vy<=3))
      calc[STEP_TURN_L]/=2;
    if(vx == 4 || (vx == 3 && vy<=3))
      calc[STEP_TURN_R]/=2;

    if(m!=-1){
      /* 敵の直接武器攻撃範囲内に入る動きは評価を半分 */
      if(!((cd[m].st & 0xf0) == ST_SPELL)){
  wd = cd[m].wd;
  d=(4 + cd[m].d - cd[n].d)%4;
  if(   (d==0 && (wd & WD_FORE))
     || (d==1 && (wd & WD_LEFT))
     || (d==2 && (wd & WD_BACK))
     || (d==3 && (wd & WD_RIGHT))){ /* 同方向 */
    if(vy==0 && vx==1) calc[STEP_GO_L]/=2;
    if(vy==0 && vx==2){
      calc[STEP_TURN_L]/=2;
      calc[STEP_TURN_R]/=2;
      calc[STEP_TURN_B]/=2;
    }
    if(vy==0 && vx==3) calc[STEP_GO_R]/=2;
  }
  if(   (d==0 && (wd & WD_LEFT))
     || (d==1 && (wd & WD_BACK))
     || (d==2 && (wd & WD_RIGHT))
     || (d==3 && (wd & WD_FORE))){  /* 左方向 */
    if(vy==2 && vx==3) calc[STEP_GO]/=2;
    if(vy==1 && vx==3){
      calc[STEP_TURN_L]/=2;
      calc[STEP_TURN_R]/=2;
      calc[STEP_TURN_B]/=2;
    }
    if(vy==0 && vx==3) calc[STEP_GO_B]/=2;
    if(vy==1 && vx==4) calc[STEP_GO_R]/=2;
  }
  if(   (d==0 && (wd & WD_BACK))
     || (d==1 && (wd & WD_RIGHT))
     || (d==2 && (wd & WD_FORE))
     || (d==3 && (wd & WD_LEFT))){ /* 下方向 */
    if(vy==3 && vx==2) calc[STEP_GO]/=2;
    if(vy==2 && vx==2){
      calc[STEP_TURN_L]/=2;
      calc[STEP_TURN_R]/=2;
      calc[STEP_TURN_B]/=2;
    }
    if(vy==2 && vx==1) calc[STEP_GO_L]/=2;
    if(vy==2 && vx==3) calc[STEP_GO_R]/=2;
  }
  if(   (d==0 && (wd & WD_RIGHT))
     || (d==1 && (wd & WD_FORE))
     || (d==2 && (wd & WD_LEFT))
     || (d==3 && (wd & WD_BACK))){ /* 右方向 */
    if(vy==2 && vx==1) calc[STEP_GO]/=2;
    if(vy==1 && vx==1){
      calc[STEP_TURN_L]/=2;
      calc[STEP_TURN_R]/=2;
      calc[STEP_TURN_B]/=2;
    }
    if(vy==0 && vx==1) calc[STEP_GO_B]/=2;
    if(vy==2 && vx==0) calc[STEP_GO_L]/=2;
  }
      }
    }
  } else {  /* 視界外 */
    if(damaged>=0){
      /* ダメージ方向に近付く動きは評価を半分 */
      switch(damaged){
      case 0: calc[STEP_GO  ]/=2; break;
      case 1: calc[STEP_GO_R]/=2; break;
      case 2: calc[STEP_GO_B]/=2; break;
      case 3: calc[STEP_GO_L]/=2; break;
      }
      /* ダメージ方向を向く回転は評価を 1/2 */
      switch(damaged){
      case 1: calc[STEP_TURN_R]/=2; break;
      case 2: calc[STEP_TURN_B]/=2; break;
      case 3: calc[STEP_TURN_L]/=2; break;
      }
    } else { /* ダメージを受けてない */
      if(m!=-1){
  /* view 位置への変換 */
  xx= -py[cd[n].d]*(cd[m].x-cd[n].x)+px[cd[n].d]*(cd[m].y-cd[n].y);
  yy=  px[cd[n].d]*(cd[m].x-cd[n].x)+py[cd[n].d]*(cd[m].y-cd[n].y);
  if(std::abs(xx)<=std::abs(yy))
    if(yy>=0) calc[STEP_GO]/=2;
    else      calc[STEP_GO_B]/=2;
  else
    if(xx>=0) calc[STEP_GO_R]/=2;
    else      calc[STEP_GO_L]/=2;
      } else {
  calc[STEP_GO_B]/=2;
  calc[STEP_TURN_B]/=2;
      }
    }
  }

  if(m!=-1){
    /* 敵からの間接攻撃可能範囲から外す行動は評価を2倍 */
    /* 敵からの間接攻撃可能範囲に入る行動は評価を1/2倍 */
    clear_chr(n);
    wd=   direction_check(cd[m].x, cd[m].y, cd[m].d, cd[n].x, cd[n].y);
    wd2 = direction_check(cd[m].x, cd[m].y, cd[m].d, X_FORE(n), Y_FORE(n));
    if(wd & ED_LINE) {
      if(!(wd2 & ED_LINE)) calc[STEP_GO]*=2;
    } else if(wd & ED_FOREAROUND) {
      if(wd2 & ED_LINE) calc[STEP_GO]/=2;
      else if(!(wd2 & ED_FOREAROUND)) calc[STEP_GO]*=2;
    } else {
      if(wd2 & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO]/=2;
    }
    wd2 = direction_check(cd[m].x, cd[m].y, cd[m].d, X_RIGHT(n), Y_RIGHT(n));
    if(wd & ED_LINE) {
      if(!(wd2 & ED_LINE)) calc[STEP_GO_R]*=2;
    } else if(wd & ED_FOREAROUND) {
      if(wd2 & ED_LINE) calc[STEP_GO_R]/=2;
      else if(!(wd2 & ED_FOREAROUND)) calc[STEP_GO_R]*=2;
    } else {
      if(wd2 & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO_R]/=2;
    }
    wd2 = direction_check(cd[m].x, cd[m].y, cd[m].d, X_BACK(n), Y_BACK(n));
    if(wd & ED_LINE) {
      if(!(wd2 & ED_LINE)) calc[STEP_GO_B]*=2;
    } else if(wd & ED_FOREAROUND) {
      if(wd2 & ED_LINE) calc[STEP_GO_B]/=2;
      else if(!(wd2 & ED_FOREAROUND)) calc[STEP_GO_B]*=2;
    } else {
      if(wd2 & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO_B]/=2;
    }
    wd2 = direction_check(cd[m].x, cd[m].y, cd[m].d, X_LEFT(n), Y_LEFT(n));
    if(wd & ED_LINE) {
      if(!(wd2 & ED_LINE)) calc[STEP_GO_L]*=2;
    } else if(wd & ED_FOREAROUND) {
      if(wd2 & ED_LINE) calc[STEP_GO_L]/=2;
      else if(!(wd2 & ED_FOREAROUND)) calc[STEP_GO_L]*=2;
    } else {
      if(wd2 & (ED_LINE | ED_FOREAROUND)) calc[STEP_GO_L]/=2;
    }
    if(wd & (ED_LINE | ED_FOREAROUND)){
      calc[STEP_TURN_L]/=2;
      calc[STEP_TURN_R]/=2;
      calc[STEP_TURN_B]/=2;
    }
    set_chr(n);
  }

  /* 壁を向く回転は評価を半分 */
  if(!(mc & MC_L)) calc[STEP_TURN_L]/=2;
  if(!(mc & MC_R)) calc[STEP_TURN_R]/=2;
  if(!(mc & MC_B)) calc[STEP_TURN_B]/=2;

  /* 計算と行動 */
  for(i=0,max_calc=0,max_num=0; i<7; i++){
    if(max_calc < calc[i]){
      max_calc = calc[i];
      max_num = 1;
    } else if(max_calc == calc[i])
      max_num++;
  }
  if(max_calc==0) return 0;
  j=frand(max_num)+1;
  i=0;
  while(1){
    if(calc[i]==max_calc){
      if(j==1) break;
      j--;
    }
    i++;
  }
  input_analyze(n, const_cast<char *>(kGoCom[i]));
  return 1;
}

int auto_action(int n)
{
  /* buf は CPYSTR/CATSTR (sizeof(d) を使うマクロ) で使うため C 配列のまま */
  char   buf[256],*p;
  int    x, y, d;
  int    pdx, pdy;
  int    mc;
  int    i, j, k;
  ItemOData *flooritem;
  int    vx, vy;

  set_view(n);

  if ((cd[n].st & 0xf0)== ST_SPELL) {
    input_analyze(n, cd[n].md[0].spell);
    return (0);
  }
  if (cd[n].st == ST_TRANSSELECT){
    input_analyze(n, (char *)".");
    return 0;
  }
  if(cd[n].think == TH_WIN){
    cd[n].think = TH_NONE;
    if(cd[n].damaged <0 && npc_talk(n, TT_KILL)){
      return 0;
    }
  }
  if (TCN_CONFUSE(n)) {
    CPYSTR(buf, (PER_DICE / 50 ? "turn" : "go"));
    switch (PER_DICE / 25) {
    case 1:
      CATSTR(buf, " l");
      break;
    case 2:
      CATSTR(buf, " r");
      break;
    case 3:
      CATSTR(buf, " b");
      break;
    }
    input_analyze(n, buf);
    return (0);
  }
  if((cd[n].think == TH_TALK || cd[n].think == TH_WAIT) && cd[n].damaged >=0){
    if(view_location(n, cd[n].trace, nullptr, nullptr))
      input_analyze(n, (char *)NTK_LATE);
    else
      input_analyze(n, (char *)NTK_WHERE);
    cd[n].trace = -1;
    cd[n].vf = 0;
    cd[n].think = TH_NONE;
  }
  if (cd[n].type == CT_NORMAL
      && cd[n].mmp>0
      && (cd[n].mp < cd[n].mmp * (99 - cd[n].moral) / 100
    || cd[n].mp < cd[n].mmp/2)){
    for (i = 0; i < MAX_ITEMS; i++)
      if ((cd[n].id[i].type & IT_MASK) == IT_FOOD
    && cd[n].id[i].active_time !=ULONG_MAX
    &&(cd[n].id[i].wp >= cd[n].id[i].ENTROP
       || cd[n].at == AT_IRONSTOMACH))
  break;
    if (i < MAX_ITEMS) {
      for(j = 0; j< i; j++)
  if(cd[n].id[i].name==cd[n].id[j].name){
    swap_item(&(cd[n].id[i]), &(cd[n].id[j]));
    i=j;
    break;
  }
      snprintf(buf, sizeof(buf), "use %s", cd[n].id[i].name);
      input_analyze(n, buf);
      return (0);
    }
  }
  if (npc_use_charged_item
      && (!((fld[map[cd[n].y][cd[n].x].area].flag) & FF_CHARGE))
      && cd[n].mhp > 0
      && (cd[n].type == CT_NORMAL
    && ((cd[n].hp < cd[n].mhp / 2
        || cd[n].hp < cd[n].mhp * (99 - cd[n].moral) /100)
    || (cd[n].think != TH_TALK && PER_DICE > cd[n].hp * 100 / cd[n].mhp)))){
    for (i = 0; i < MAX_ITEMS; i++)
      if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
   && cd[n].id[i].active_time !=ULONG_MAX
   && cd[n].id[i].SE != SE_HITTOUSE
   && cd[n].id[i].SE != SE_HITISUSE
   && (cd[n].id[i].UE == EF_HEAL
       || cd[n].id[i].UE == EF_HEALS
       || (cd[n].id[i].UE == EF_REGENE && CN_REGENE(n)==0))
   && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr)))
  break;
    if(i<MAX_ITEMS){
      for(j = 0; j< i; j++)
  if(cd[n].id[i].name==cd[n].id[j].name){
    swap_item(&(cd[n].id[i]), &(cd[n].id[j]));
    i=j;
    break;
  }
      snprintf(buf, sizeof(buf),"use %s",cd[n].id[i].name);
      input_analyze(n,buf);
      return 0;
    }
  }
  if (cd[n].think == TH_RUN
      && cd[n].damaged < 0
      && (PER_DICE < 5
    || (cd[n].hp >= cd[n].mhp * (99 - cd[n].moral) / 100
     && cd[n].mp >= cd[n].mmp * (99 - cd[n].moral) / 100))){
    cd[n].think = TH_NONE;
    cd[n].angry = -1;
  }
  if (cd[n].think == TH_GET_AWAY) {
    if(cd[n].damaged >=0)
      cd[n].think = TH_NONE;
    else if(PER_DICE < 5){
      cd[n].think = TH_NONE;
      cd[n].angry = -1;
    }
  }

  x = cd[n].x;
  y = cd[n].y;
  d = cd[n].d;
  pdx = px[d];
  pdy = py[d];
  mc = 0;
  if (move_check(n, x + pdx, y + pdy))
    mc |= MC_F;
  if (move_check(n, x + pdy, y - pdx))
    mc |= MC_L;
  if (move_check(n, x - pdy, y + pdx))
    mc |= MC_R;
  if (move_check(n, x - pdx, y - pdy))
    mc |= MC_B;

  switch (cd[n].think) {
  case TH_TALK:
    if (cd[n].target == cd[n].trace){
      if (str_search(n, (char *)"bye")) {
  input_analyze(n, (char*)(50 > cd[n].align ? NTK_BYE_LAW : NTK_BYE_CHAOS));
  cd[n].trace = -1;
  cd[n].vf = 0;
  cd[n].think = TH_NONE;
  return (0);
      } else if (str_search(n, (char *)"help")) {
  if(!((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGIC)){
    for (i = 0; i < MAX_MAGICS; i++)
      if (cd[n].md[i].effect == EF_HEALS)
        break;
    if (i < MAX_MAGICS && 50 > cd[n].align
        && cd[n].mp > cd[n].mmp * 3 / 4
        && is_left_mp_moral(n, i)){
      select_magic(n, i);
      return 0;
    }
  }
  input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_HELP_NO_LAW : NTK_HELP_NO_CHAOS));
  return (0);
      } else if (!(cd[n].vf & VF_NOT_TRACE) && str_search(n, (char *)"come")) {
  if (PER_DICE < cd[n].align) {
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_COME_NO_LAW : NTK_COME_NO_CHAOS));
    cd[n].vf |= VF_NOT_TRACE;
  } else {
    input_analyze(n, (char *)NTK_COME_OK);
    cd[n].think = TH_TRACE;
  }
  return (0);
      } else if (!(cd[n].vf & VF_NOT_STAY) && str_search(n, (char *)"stay")) {
  if (PER_DICE < cd[n].align) {
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_STAY_NO_LAW : NTK_STAY_NO_CHAOS));
    cd[n].vf |= VF_NOT_STAY;
  } else {
    input_analyze(n, (char *)NTK_STAY_OK);
    cd[n].think = TH_WAIT;
  }
  return (0);
      } else if (str_search(n, (char *)"?")) {
  if (!(cd[n].vf & VF_FOOD) && str_search(n, (char *)"food")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if ((cd[n].id[i].type & IT_MASK) == IT_FOOD
    &&(cd[n].id[i].wp >= cd[n].id[i].ENTROP
       || cd[n].at == AT_IRONSTOMACH))
        break;
    if (i >= MAX_ITEMS) {
      for (i = 0; i < MAX_ITEMS; i++)
        if (cd[n].id[i].type == DT_NONE)
    break;
      if (i < MAX_ITEMS) {
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_FOOD_LAW : NTK_BUY_FOOD_CHAOS));
        cd[n].vf |= VF_FOOD;
        return (0);
      }
    }
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_BUY_FOOD_NO_LAW : NTK_BUY_FOOD_NO_CHAOS));
    return (0);
  } else if (!(cd[n].vf & VF_WEAPON) && str_search(n, (char *)"weapon")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type == DT_NONE)
        break;
    if (i < MAX_ITEMS) {
      input_analyze(n, (char*)(50 > cd[n].align
            ? NTK_BUY_WPN_LAW : NTK_BUY_WPN_CHAOS));
      cd[n].vf |= VF_WEAPON;
      return (0);
    }
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_BUY_WPN_NO_LAW : NTK_BUY_WPN_NO_CHAOS));
    return (0);
  } else if (!(cd[n].vf & VF_ARMOR) && str_search(n, (char *)"armor")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type == DT_NONE)
        break;
    if (i < MAX_ITEMS) {
      input_analyze(n, (char*)(50 > cd[n].align
            ? NTK_BUY_ARM_LAW : NTK_BUY_ARM_CHAOS));
      cd[n].vf |= VF_ARMOR;
      return (0);
    }
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_BUY_ARM_NO_LAW : NTK_BUY_ARM_NO_CHAOS));
    return (0);
  } else if (!(cd[n].vf & VF_ACCESSORY) && str_search(n, (char *)"accessory")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type == DT_NONE)
        break;
    if (i < MAX_ITEMS) {
      input_analyze(n, (char*)(50 > cd[n].align
            ? NTK_BUY_ACC_LAW : NTK_BUY_ACC_CHAOS));
      cd[n].vf |= VF_ACCESSORY;
      return (0);
    }
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_BUY_ACC_NO_LAW : NTK_BUY_ACC_NO_CHAOS));
    return (0);
  } else if (npc_use_charged_item
       &&!(cd[n].vf & VF_CHARGE)
       && str_search(n, (char *)"charged item")) {
    for (i = 0; i < MAX_ITEMS; i++)
      if (cd[n].id[i].type == DT_NONE)
        break;
    if (i < MAX_ITEMS) {
      input_analyze(n, (char*)(50 > cd[n].align
            ? NTK_BUY_CHG_LAW : NTK_BUY_CHG_CHAOS));
      cd[n].vf |= VF_CHARGE;
      return (0);
    }
    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_BUY_CHG_NO_LAW : NTK_BUY_CHG_NO_CHAOS));
    return (0);
  } else {
    for(i = 0; i < MAX_TALKS; i++)
      if(cd[n].td[i].type == TT_KEYWORD){
        CPYSTR(buf, cd[n].td[i].string);
        if((p=std::strchr(buf,':'))!=nullptr){
    *p='\0';
    if(str_search(n,buf)){
      input_analyze(n, p+1);
      return 0;
    }
        }
      }
  }
      } else if (cd[n].mhp > 0 && PER_DICE >= cd[n].hp * 100 / cd[n].mhp
     && !(cd[n].vf & VF_DYING)) {
  input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_HELP_LAW : NTK_HELP_CHAOS));
  cd[n].vf |= VF_DYING;
  return (0);
      } else if ((cd[n].vf & VF_DYING) && (cd[n].vf & VF_HEAL)) {
  input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_HELP_THX_LAW : NTK_HELP_THX_CHAOS));
  if (PER_DICE > cd[n].align + 50) {
    snprintf(buf, sizeof(buf), "pay %i", (99 - cd[n].align) / 10 + 1);
    input_analyze(n, buf);
  }
  cd[n].vf &= ~(VF_DYING | VF_HEAL);
  return (0);
      } else if ((map[y][x].tip & T_ITEM) && (map[y][x].tip & TMASK)!=T_LBOX) {
  for (i = 0; i < MAX_ITEMS; i++)
    if (cd[n].id[i].type == DT_NONE)
      break;
  if (i < MAX_ITEMS) {
    if((cd[n].vf & VF_FOOD)
       && (flooritem=investigate_item_on_floor(n,IT_FOOD))!=nullptr){
      cd[n].vf &= ~VF_FOOD;
      item_name_on_floor(cd[n].str,flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      if (PER_DICE < cd[n].align - 50) {
        cd[n].think = TH_GET_AWAY;
        cd[n].angry = cd[n].trace;
        cd[n].trace = -1;
        cd[n].vf = 0;
        return (0);
      }
      if (cd[n].id[i].wp >= cd[n].id[i].ENTROP)
        i = cd[n].id[i].VALUE * 2;
      else if(cd[n].at == AT_IRONSTOMACH)
        i = 1;
      else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str);
        input_analyze(n, buf);
        input_analyze(n
          ,(char*)(50 > cd[n].align
            ? NTK_BUY_FOOD_BAD_LAW
            : NTK_BUY_FOOD_BAD_CHAOS));
        return (0);
      }
      if (i > cd[n].gp / 2)
        i = cd[n].gp / 2;
      if (i < 10)
        i = 10;
      if (cd[n].gp >= i) {
        snprintf(buf, sizeof(buf), "pay %i", i);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_FOOD_THX_LAW : NTK_BUY_FOOD_THX_CHAOS));
      } else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str); /* bug fix by Qajin 20000103 */
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_FOOD_POOR_LAW
        : NTK_BUY_FOOD_POOR_CHAOS));
      }
      return (0);
    }
    if ((cd[n].vf & VF_WEAPON)
        && (flooritem=investigate_item_on_floor(n, 0))!=nullptr){
      cd[n].vf &= ~VF_WEAPON;
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      if (PER_DICE < cd[n].align - 50) {
        cd[n].think = TH_GET_AWAY;
        cd[n].angry = cd[n].trace;
        cd[n].trace = -1;
        cd[n].vf = 0;
        return (0);
      }
      for (j = 0; j < MAX_ITEMS; j++) {
        if (cd[n].id[j].type == DT_NONE || i == j)
    continue;
        if (cd[n].id[i].ATP <= cd[n].id[j].ATP) {
    snprintf(buf, sizeof(buf), "put %s", cd[n].id[i].name);
    input_analyze(n, buf);
    input_analyze(n
            ,(char*)(50 > cd[n].align
        ? NTK_BUY_WPN_BAD_LAW
        : NTK_BUY_WPN_BAD_CHAOS));
    return (0);
        }
      }
      i = cd[n].id[i].ATP * (100 - cd[n].align) / 10;
      if (i > cd[n].gp / 2)
        i = cd[n].gp / 2;
      if (i < 10)
        i = 10;
      if (cd[n].gp >= i) {
        snprintf(buf, sizeof(buf), "pay %i", i);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_WPN_THX_LAW : NTK_BUY_WPN_THX_CHAOS));
      } else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_WPN_POOR_LAW
        : NTK_BUY_WPN_POOR_CHAOS));
      }
      return (0);
    }
    if ((cd[n].vf & VF_ACCESSORY)
        &&(flooritem=investigate_item_on_floor(n, IT_ACCESSORY))!=nullptr){
      cd[n].vf &= ~VF_ACCESSORY;
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      if (PER_DICE < cd[n].align - 50) {
        cd[n].think = TH_GET_AWAY;
        cd[n].angry = cd[n].trace;
        cd[n].trace = -1;
        cd[n].vf = 0;
        select_accessory(n);
        return (0);
      }
      if ((cd[n].oeq != nullptr) || (curse_check(n, cd[n].id[i].SE) < 0)) {
        snprintf(buf, sizeof(buf), "put %s", cd[n].id[i].name);
        input_analyze(n, buf);
        input_analyze(n
          ,(char*)(50 > cd[n].align
            ? NTK_BUY_ACC_BAD_LAW
            : NTK_BUY_ACC_BAD_CHAOS));
        return (0);
      }
      i = cd[n].id[i].AE * (200 - cd[n].align) / 10;
      if (i > cd[n].gp / 2)
        i = cd[n].gp / 2;
      if (i < 10)
        i = 10;
      if (cd[n].gp >= i) {
        snprintf(buf, sizeof(buf), "pay %i", i);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_ACC_THX_LAW : NTK_BUY_ACC_THX_CHAOS));
        select_accessory(n);
      } else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_ACC_POOR_LAW
        : NTK_BUY_ACC_POOR_CHAOS));
      }
      return (0);
    }
    if ((cd[n].vf & VF_ARMOR)
        && (flooritem=investigate_item_on_floor(n, IT_ARMOR))!=nullptr){
      cd[n].vf &= ~VF_ARMOR;
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      if (PER_DICE < cd[n].align - 50) {
        cd[n].think = TH_GET_AWAY;
        cd[n].angry = cd[n].trace;
        cd[n].trace = -1;
        cd[n].vf = 0;
        select_armor(n);
        return (0);
      }
      for (j = 0; j < MAX_ITEMS; j++) {
        if ((cd[n].id[j].type & IT_MASK) != IT_ARMOR || i == j)
    continue;
        if (cd[n].id[i].DFP + cd[n].id[i].MRP
      <= cd[n].id[j].DFP + cd[n].id[j].MRP) {
    snprintf(buf, sizeof(buf), "put %s", cd[n].id[i].name);
    input_analyze(n, buf);
    input_analyze(n
            ,(char*)(50 > cd[n].align
        ? NTK_BUY_ARM_BAD_LAW
        : NTK_BUY_ARM_BAD_CHAOS));
    return (0);
        }
      }
      i = (cd[n].id[i].DFP + cd[n].id[i].MRP)
        * (100 - cd[n].align) / 10;
      if (i > cd[n].gp / 2)
        i = cd[n].gp / 2;
      if (i < 10)
        i = 10;
      if (cd[n].gp >= i) {
        snprintf(buf, sizeof(buf), "pay %i", i);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_ARM_THX_LAW : NTK_BUY_ARM_THX_CHAOS));
        select_armor(n);
      } else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_ARM_POOR_LAW
        : NTK_BUY_ARM_POOR_CHAOS));
      }
      return (0);
    }
    if ((cd[n].vf & VF_CHARGE)
        && (flooritem=investigate_item_on_floor(n,IT_CHARGE))!=nullptr){
      cd[n].vf &= ~VF_CHARGE;
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      if (PER_DICE < cd[n].align - 50) {
        cd[n].think = TH_GET_AWAY;
        cd[n].angry = cd[n].trace;
        cd[n].trace = -1;
        cd[n].vf = 0;
        return (0);
      }
      if(cd[n].id[i].CHARGE==0)
        i = mgstrlen(cd[n].id[i].name) * 99;
      else
        i = mgstrlen(cd[n].id[i].name) * cd[n].id[i].CHARGE;
      i = i * (100 - cd[n].align) / 10;
      if (i > cd[n].gp / 2)
        i = cd[n].gp / 2;
      if (i < 10)
        i = 10;
      if (cd[n].gp >= i) {
        snprintf(buf, sizeof(buf), "pay %i", i);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_CHG_THX_LAW : NTK_BUY_CHG_THX_CHAOS));
      } else {
        snprintf(buf, sizeof(buf), "put %s", cd[n].str);
        input_analyze(n, buf);
        input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BUY_CHG_POOR_LAW
        : NTK_BUY_CHG_POOR_CHAOS));
      }
      return (0);
    }
  }
      }
    }
    if(!view_location(n, cd[n].trace, &vx, &vy)){
      input_analyze(n, (char *)NTK_WHERE);
      cd[n].trace = -1;
      cd[n].vf = 0;
      cd[n].think = TH_NONE;
      break;
    } else {
      set_view(n);
      k = temptation_to_attack(n);
      if(k == 0 && vx == 2 && vy == 1) break;
      if(PER_DICE<= cd[n].align + k){
  if(k>0) input_analyze(n, (char *)NTK_LATE);
  else    input_analyze(n, (char*)(50 > cd[n].align
          ? NTK_TRACESTOP_LAW : NTK_TRACESTOP_CHAOS));
  cd[n].trace = -1;
  cd[n].think = TH_NONE;
  break;
      }
    }
    break;
  default:
    cd[n].think = TH_NONE;
  case TH_NONE:
  case TH_TRACE:
  case TH_WAIT:
  case TH_ATTACK:
  case TH_MAGIC:
  case TH_WIZARDSTEP:
    if (cd[n].mhp > 0
  && (cd[n].hp < cd[n].mhp * (99 - cd[n].moral) / 100
      || PER_DICE > cd[n].hp * 100 / cd[n].mhp)){
      if((!((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGIC))
   && (cd[n].mmp==0 || cd[n].mp >= cd[n].mmp /2))
  for (i = 0; i < MAX_MAGICS; i++)
    if ((cd[n].md[i].effect == EF_HEAL
         || cd[n].md[i].effect == EF_HEALS
         || (cd[n].md[i].effect == EF_REGENE && CN_REGENE(n)==0))
        && is_left_mp_moral(n, i)){
      select_magic(n, i);
      return 0;
    }
    }
    /* change for variable multi characters in one space 20000721 */
    for (j = 0; j < MAX_CHRS_INONE; j++) {
      i = map[y][x].chr[j] - 1;
      if (i >= 0 && i != n)
        break;
    }
    if (cd[n].type == CT_NORMAL
  && i >= 0 && i < MAX_NTTYS && cd[n].target == i) {
      if (str_search(n, (char *)"hi")
    && cd[n].damaged <0
    && PER_DICE > cd[i].align && PER_DICE > cd[n].align
    && cd[n].angry!=i) {
  if(!npc_talk(n, TT_HI))
    input_analyze(n, (char *)NTK_HI);
  cd[n].trace = i;
  cd[n].vf = 0;
  cd[n].think = TH_TALK;
  return (0);
      } else if (cd[n].trace == i && str_search(n, (char *)"bye")){
  input_analyze(n, (char*)(50 > cd[n].align
        ? NTK_BYE_LAW : NTK_BYE_CHAOS));
  cd[n].trace = -1;
  cd[n].vf = 0;
  cd[n].think = TH_NONE;
  return (0);
      }
    }

    /* PHI:0055 Bug Fix by EL */
    if((map[cd[n].y][cd[n].x].tip & T_ITEM) && (map[cd[n].y][cd[n].x].tip & TMASK)!=T_LBOX &&
       (PER_DICE > temptation_to_attack(n) || cd[n].type == CT_ERASER)) {
      for (i = 0; i < MAX_ITEMS; i++)
  if (cd[n].id[i].type == DT_NONE)
    break;
      if (i < MAX_ITEMS) {
  if((flooritem = investigate_item_on_floor(n,IT_FOOD))!=nullptr
     && (flooritem->id.wp >= flooritem->id.ENTROP
         || cd[n].at == AT_IRONSTOMACH)){
    item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
    snprintf(buf, sizeof(buf), "get %s", cd[n].str);
    input_analyze(n, buf);
    for (j = 0; j < MAX_ITEMS; j++)
      if (i != j
    && cd[n].id[j].active_time != ULONG_MAX
    && (cd[n].id[j].type & IT_MASK) == IT_FOOD
    && (cd[n].id[j].wp >= cd[n].id[j].ENTROP
        || cd[n].at == AT_IRONSTOMACH))
        break;
    if (j < MAX_ITEMS || cd[n].type > CT_NORMAL) {
      /* food を他に持たない知能的生物が food を拾った場合、*/
      /* 以下(すなわち、拾った food を即使うこと)は行わない */
      for(k=0; k<i; k++)
        if(cd[n].id[i].name==cd[n].id[k].name){
    swap_item(&(cd[n].id[i]), &(cd[n].id[k]));
    i=k;
    break;
        }
      snprintf(buf, sizeof(buf), "use %s", cd[n].id[i].name);
      input_analyze(n, buf);
    }
    return 0;
  }
  if(cd[n].type == CT_NORMAL
     && (flooritem=investigate_item_on_floor(n, IT_WEAPON))!=nullptr){
    for(j=0; j<MAX_ITEMS; j++)
      if((cd[n].id[j].type & IT_MASK) == IT_WEAPON
         && cd[n].id[j].active_time!=ULONG_MAX)
        break;
    if(j>=MAX_ITEMS){
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      return 0;
    }
  }
  if(npc_use_charged_item){
    if(cd[n].type == CT_NORMAL
       && (flooritem=investigate_item_on_floor(n, IT_CHARGE))!=nullptr){
      item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
      snprintf(buf, sizeof(buf), "get %s", cd[n].str);
      input_analyze(n, buf);
      return 0;
    }
  }
      }
      if(cd[n].type == CT_NORMAL
   && (flooritem=investigate_item_on_floor(n, IT_JEWEL))!=nullptr
   && (i< MAX_ITEMS || is_gp(flooritem))){
  item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
  snprintf(buf, sizeof(buf), "get %s", cd[n].str);
  input_analyze(n, buf);
  return 0;
      }
      if(i<MAX_ITEMS){
  if(cd[n].type == CT_ERASER
     && (flooritem=investigate_item_on_floor(n, 0))!=nullptr){
    item_name_on_floor(cd[n].str, flooritem,sizeof(cd[n].str));
    j = cd[n].gp;
    snprintf(buf, sizeof(buf), "get %s", cd[n].str);
    input_analyze(n, buf);
    if (cd[n].gp > j) {
      dmprintf(n,0,buf, sizeof(buf),DMM_DESTRUCT_MONEY
        , cd[n].gp-j);
      write_companion(n, 0, buf, 0);
      tax += (cd[n].gp-j);
      cd[n].gp = j;
    } else {
      dmprintf(n,0,buf, sizeof(buf),DMM_DESTRUCT
        , cd[n].id[i].name);
      write_companion(n, 0, buf, 0);
      delete_item(&(cd[n].id[i]));
    }
    return 0;
  }
      }
    }

    if(cd[n].think == TH_WAIT) {
      if(PER_DICE<= cd[n].align + temptation_to_attack(n)) {
  if(view_location(n, cd[n].trace, nullptr, nullptr)){
    input_analyze(n, (char *)NTK_LATE);
    cd[n].trace = -1;
    cd[n].think = TH_NONE;
  } else {
    input_analyze(n, (char *)NTK_WHERE);
    cd[n].trace = -1;
    cd[n].think = TH_NONE;
  }
      }
      break;
    }

    if (cd[n].think == TH_TRACE && PER_DICE <= cd[n].align - 50){
      if(view_location(n, cd[n].trace, nullptr, nullptr))
  input_analyze(n, (char *)NTK_TRACESTOP_CHAOS);
      else
  input_analyze(n, (char *)NTK_WHERE);
      cd[n].trace = -1;
      cd[n].think = TH_NONE;
      break;
    }

    if(attack_angry(n,mc)) return 0;
    if(attack_enemy(n,mc)) return 0;

    if(cd[n].think == TH_WIZARDSTEP)
      cd[n].think = TH_NONE;

    if (cd[n].trace != -1
  && !view_location(n, cd[n].trace, nullptr, nullptr)
  && PER_DICE <= cd[n].align) {
      /* trace してる PC を見失う */
      input_analyze(n, (char *)NTK_WHERE);
      cd[n].target = cd[n].trace = -1;
      cd[n].think = TH_NONE;
      break;
    }
    /* add for controlable sammon 20000614 */
    if (cd[n].trace == -1 && cd[n].vf2 >= 0
  && view_location(n, cd[n].vf2, nullptr, nullptr)
  && accept_master(n,cd[n].vf2)) {
      /* 見失った master を 再発見した */
      if(cd[n].type == CT_NORMAL)
          input_analyze(n,(char*)(50 > cd[n].align ? NTK_WAIT_MASTER_LAW : NTK_WAIT_MASTER_CHAOS));
      cd[n].trace = cd[n].target = cd[n].vf2;
      cd[n].vf = 0; /* bug fix 20000706 */
      cd[n].think = TH_TRACE;
      break;
    }
    random_walk(n, mc);
    break;
  case TH_RUN:
    if (cd[n].mhp > 0
  && (cd[n].hp < cd[n].mhp * (99 - cd[n].moral) / 100)
  && PER_DICE < 20)
      if((!((fld[map[cd[n].y][cd[n].x].area].flag) & FF_MAGIC))
   && (cd[n].mmp==0 || cd[n].mp >= cd[n].mmp /2))
  for (i = 0; i < MAX_MAGICS; i++)
    if ((cd[n].md[i].effect == EF_HEAL
         || cd[n].md[i].effect == EF_HEALS
         || (cd[n].md[i].effect == EF_REGENE && CN_REGENE(n)==0))
        && is_left_mp_moral(n, i)){
      select_magic(n, i);
      return 0;
    }
    if(PER_DICE<10) npc_talk(n,TT_HELP);
  case TH_GET_AWAY:
    if(run_walk(n, mc)) return 0;
    random_walk(n, mc);
    break;
  }
  return 0;
}

void      random_walk(int n, int mc)
{
  int xx, yy;
  int pdx, pdy;
  /* CPYSTR で使うため C 配列のまま */
  char buf[7];

  xx = cd[n].x;
  yy = cd[n].y;
  pdx = px[cd[n].d];
  pdy = py[cd[n].d];
  if (!(mc & MC_F)) {
    if (!(mc & MC_L) && !(mc & MC_R))
      if(mc & MC_B)
  CPYSTR(buf, "turn b");
      else
  switch(frand(4)){
  case 1: CPYSTR(buf, "turn r"); break;
  case 2: CPYSTR(buf, "turn l"); break;
  case 3: CPYSTR(buf, "turn b"); break;
  default: return;
  }
    else if ((mc & MC_L) && !(mc & MC_R))
      CPYSTR(buf, "turn l");
    else if (!(mc & MC_L) && (mc & MC_R))
      CPYSTR(buf, "turn r");
    else
      CPYSTR(buf, (PER_DICE < 50 ? "turn l" : "turn r"));
  } else {
    if (mc & MC_L) {
      if (mc & MC_R) {
  if (PER_DICE < 10)
    CPYSTR(buf, "turn l");
  else if (PER_DICE < 11)
    CPYSTR(buf, "turn r");
  else
    CPYSTR(buf, "go");
      } else {
  if (PER_DICE < 20)
    CPYSTR(buf, "turn l");
  else
    CPYSTR(buf, "go");
      }
    } else {
      if (mc & MC_R) {
  if (PER_DICE < 20)
    CPYSTR(buf, "turn r");
  else
    CPYSTR(buf, "go");
      } else
  CPYSTR(buf, "go");
    }
  }
  input_analyze(n, buf);
  if (*buf == 'g' && xx == cd[n].x && yy == cd[n].y)
    input_analyze(n, (char *)"turn b");
}

int    run_walk(int n, int mc)
{
  int i, j;
  int vx, vy;
  int sa;
  int damaged;

  damaged = cd[n].damaged;
  cd[n].damaged = -1;

  if((j=cd[n].angry) !=-1 && view_location(n, j, &vx, &vy)
     && try_to_runstep(n, j, vx, vy, mc, damaged))
    return 1;
  for (sa=0; sa<25; sa++)
    for (i=0;i<MAX_CHRS_INONE;i++){
      if ((j = cd[n].view[vy=kSy[sa]+1][vx=kSx[sa]+2].chr[i] - 1) < 0)
  continue;
      if (enemy_check(n,j) && try_to_runstep(n, j, vx, vy, mc, damaged))
  return 1;
    }
  if(try_to_runstep(n, cd[n].angry, -1, -1, mc, damaged))
    return 1;
  return 0;
}

int    move_check(int n, int x, int y)
{
  int    i, j, k;

  if (TCN_NOMOVE(n) || x < 1 || x >= MAX_X - 1 || y < 1 || y >= MAX_Y - 1)
    return (0);
  k=0;
  for (j = 0; j < MAX_CHRS_INONE; j++) {
    i = map[y][x].chr[j] - 1;
    if (i >= 0 && i != n) {
      if (sg_check(i))
        return(0);
      k++;
    }
  }
  if (k > 0)
    if (sg_check(n)
  || k >= MAX_CHRS_INONE)
      return (0);
  if (TCN_PASSWALL(n))
    return (1);
  switch (cd[n].type) {
  case CT_NORMAL:
    if (map[y][x].tip & T_CWALL)
      return (0);
    if ((map[y][x].tip & T_EWALL) && PER_DICE < cd[n].align)
      return (0);
    if (((map[y][x].tip & TMASK) == T_FLOWER) && PER_DICE >= cd[n].align)
      return (0);
    if ((map[y][x].tip & T_PLANT) && PER_DICE < 25)
      return (0);
    break;
  default:
    /* change for creature barrier 20000607 */
    if (map[y][x].tip & (wall_for_beast | T_EWALL))
      return (0);
    break;
  case CT_ASTRAL:
    break;
  }
  return (1);
}

/* n が m を敵として認めるかどうか */
int enemy_check(int n, int m)
{
  int    i;

  if (n == m)
    return (0);
  if (CN_GASIFY(m) >= 60)
    return (0);
  if(m==cd[n].angry) return(1);
  if(cd[n].trace == m) return 0;
  if(accept_master(n,m)) return 0; /* add for controlable sammon 20000615 */
  switch (cd[n].type) {
  case CT_NORMAL:
    if (cd[n].align < 50 && cd[m].align < 50)
      return (0);
    if (cd[m].mhp > cd[n].mhp * 2)
      return (0);
    if (cd[m].mhp == 0 && cd[m].mmp == 0)
      return 0;
  case CT_BEAST:
    for (i = 0; i < 16 && cd[n].name[i] > ' ' && cd[m].name[i] > ' '; i++)
      if (cd[n].name[i] != cd[m].name[i])
  return (1);
    return (0);
  case CT_BERSERKER:
    break;
  default:
    if (cd[m].type >= CT_MAGICAL)
      return (0);
    break;
  }
  return (1);
}

/* n が m へ攻撃しようという意志があるならば、その処理を行う */
/* 敵と認めた場合、敵が遠くにいても、適切な武器を選択しようとする。*/
/* 返却値は */
/*  0(相手が敵でない) */
/*  1(移動モード) */
/* -1(装備できる武器がない) */
/* 2(直接攻撃を行った) */
static int big_target_in_dir(int n, int m, int dd)
/* 2x2 の大きな敵 n から見て、対象 m が dd 方向の攻撃セル
   (体の縁の外側) にいるか。縁は2マスあるため、基準マスの
   正面だけを見る通常の判定では届かない位置も対象になる */
{
  int i, tx, ty;

  for (i = 0; i < 4; i++) {
    tx = cd[n].x + BIG_OFF[i][0] + px[dd];
    ty = cd[n].y + BIG_OFF[i][1] + py[dd];
    if (tx >= cd[n].x && tx <= cd[n].x + 1
        && ty >= cd[n].y && ty <= cd[n].y + 1)
      continue;   /* 体内に戻る方向は縁ではない */
    if (cd[m].x == tx && cd[m].y == ty)
      return (1);
  }
  return (0);
}

static int big_target_any(int n, int m, int hf)
/* 武器の攻撃方向 (hf) のいずれかで、対象 m が攻撃セルにいるか */
{
  if ((hf & WD_FORE) && big_target_in_dir(n, m, D_FORE(n)))
    return (1);
  if ((hf & WD_BACK) && big_target_in_dir(n, m, D_BACK(n)))
    return (1);
  if ((hf & WD_RIGHT) && big_target_in_dir(n, m, D_RIGHT(n)))
    return (1);
  if ((hf & WD_LEFT) && big_target_in_dir(n, m, D_LEFT(n)))
    return (1);
  return (0);
}

int attack_check(int n, int m)
{
  int    f;
  int    i;
  int    hf, hflag;
  int    spear; /* add for long range spear 20000614 */

  if (!enemy_check(n, m))
    return (0);
    if ((fld[map[cd[n].y][cd[n].x].area].flag) & FF_ATTACK) {
      return (1);
    }
  for (f = -1, hflag = 0, i = MAX_ITEMS - 1; i >= 0; i--) {
    if (cd[n].id[i].type == DT_NONE || cd[n].id[i].active_time ==ULONG_MAX) continue;
    if ((cd[n].id[i].type & IT_MASK) == IT_WEAPON){
      if (curse_check(n, cd[n].id[i].SE) < 0) continue;
      if (cd[n].id[i].AE == AE_MENTAL) {
  if (CN_REFLEX(m) || !cd[m].mmp)
    continue;
      } else if (cd[n].id[i].AE == AE_ENERGY) {
  if (CN_REFLEX(m) || !cd[m].mhp)
    continue;
      } else
  if (CN_FORCE(m) || !cd[m].mhp)
    continue;
    } else if(npc_use_charged_item
        && ((cd[n].id[i].type & IT_MASK) == IT_CHARGE
      && (cd[n].id[i].SE == SE_HITTOUSE
          || cd[n].id[i].SE == SE_HITISUSE))
        && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr))) {
      if(!is_effective_effect(n, m, cd[n].id[i].UE, cd[n].id[i].fn,ED_NORESTRICT))
  continue;
    } else continue;

    /* 攻撃方向の確認 */
    hf = (cd[n].id[i].type & WD_MASK) ^ 0x8000;
    spear = ((cd[n].id[i].type & WT_MASK) == WT_SPEAR);

    if (((hf & WD_FORE)  && ((cd[m].x == X_FORE(n) && cd[m].y == Y_FORE(n)) ||
                   (spear &&  cd[m].x == X_FORE2(n)&& cd[m].y == Y_FORE2(n)))) ||
  ((hf & WD_BACK)  && ((cd[m].x == X_BACK(n) && cd[m].y == Y_BACK(n)) ||
           (spear &&  cd[m].x == X_BACK2(n)&& cd[m].y == Y_BACK2(n)))) ||
  ((hf & WD_RIGHT) && ((cd[m].x == X_RIGHT(n) && cd[m].y == Y_RIGHT(n)) ||
           (spear &&  cd[m].x == X_RIGHT2(n)&& cd[m].y == Y_RIGHT2(n)))) ||
  ((hf & WD_LEFT)  && ((cd[m].x == X_LEFT(n) && cd[m].y == Y_LEFT(n)) ||
           (spear &&  cd[m].x == X_LEFT2(n)&& cd[m].y == Y_LEFT2(n)))) || (chr_big(n) && big_target_any(n, m, hf))) {
      if (hflag == 0) {
  f = i;
  hflag = 1;
      } else if (PER_DICE < 50) {
  /* 適切な武器が複数ある場合は上から 50%,25%,12.5% ....で装備 */
  f = i;
      }
    } else if (f == -1 || (hflag == 0 && PER_DICE < 50))
      f = i;
  }
  if (f == -1)
    return (-1);
  if (hflag == 0)
    return (1);
  cd[n].think = TH_ATTACK;
  if (cd[n].st == ST_ATTACK) /* 攻撃後動作回復中。(ありえない) */
    return (2);
  cd[n].weq = &cd[n].id[f];
  cd[n].wsep = sep_calc(n, cd[n].weq);
  set_fn(n);
  input_analyze(n, (char *)"hit");
  return (2);
}

/* 返却値 1:攻撃, -1:攻撃不可, 0:敵が攻撃レンジになし */
int magic_check(int n, int m)
{
  int    f, ff;
  int    i;

  if(npc_use_charged_item &&
     (!((fld[map[cd[n].y][cd[n].x].area].flag) & FF_CHARGE)))
    if(use_charged_item_check(n, m)==1)
      return 1;

  for (i = 0; i < MAX_MAGICS; i++)
    if (cd[n].md[i].effect != EF_NONE)
      break;
  if (i >= MAX_MAGICS)
    return (-1);

  if (CN_REFLEX(n) & 0xffff)
    return (-1);
  if (fld[map[cd[n].y][cd[n].x].area].flag & FF_MAGIC)
    return (-1);
  if (map[cd[n].y][cd[n].x].tip & T_ANTIMAGIC)
    return (-1);
  /* add for EFFECT BLOCK area 20000613 */
  if (fld[map[cd[m].y][cd[m].x].area].flag & FF_MAGICEFFECT)
    return (-1);

  f=direction_check(cd[n].x, cd[n].y, cd[n].d, cd[m].x, cd[m].y);

  if ((f & ED_FACE) && ((CN_FORCE(m) & 0xffff) || (CN_REFLEX(m) & 0xffff))) {
    for (i = 0; i < MAX_MAGICS; i++)
      if (cd[n].md[i].effect == EF_ERASE && is_left_mp_moral(n, i)){
  select_magic(n, i);
  return 1;
      }
  }
  for (i = 0, ff = -1; i < MAX_MAGICS; i++)
    if(is_effective_effect(n, m, cd[n].md[i].effect, cd[n].md[i].fn, f)
       && is_left_mp_moral(n, i)
       && (ff == -1 || PER_DICE < 50))
      ff = i;
  if (ff == -1)
    return (-1);
  select_magic(n, ff);
  return (1);
}

int line_search(int n)
{
  int    x, y;
  int    f;
  int    i, j, k;
  int    xd[3],yd[3];

  for (y = cd[n].y - 1; y <= cd[n].y + 1; y++)
    for (x = cd[n].x - 1; x <= cd[n].x + 1; x++) {
      for (i = 0; i < MAX_CHRS_INONE; i++) {
  if ((j = map[y][x].chr[i] - 1) < 0)
    continue;
  if (!enemy_check(n, j))
    continue;
  if (magic_check(n, j) > 0)
    return (1);
      }
    }
  x = cd[n].x;
  y = cd[n].y;

  xd[0] = 0;                 /* その場所 */
  yd[0] = 0;
  xd[1] = px[(cd[n].d+1)%4]; /* 右 */
  yd[1] = py[(cd[n].d+1)%4];
  xd[2] = px[(cd[n].d+3)%4]; /* 左 */
  yd[2] = py[(cd[n].d+3)%4];

  for (f = 0; f < 8; f++) {
    x += px[cd[n].d];
    y += py[cd[n].d];
    if(1<= x && x<=MAX_X-2 && 1<=y && y<=MAX_Y-2){
      for (k = 0; k<=2; k++)
  for (i = 0; i < MAX_CHRS_INONE; i++) {
    if ((j = map[y+yd[k]][x+xd[k]].chr[i] - 1) < 0)
      continue;
    if (!enemy_check(n, j))
      continue;
    if (magic_check(n, j) > 0)
      return (1);
  }
    }
  }
  return (0);
}

void select_magic(int n, int f)
{
  MagicData md;
  int    i;

  std::memcpy(&md, &(cd[n].md[f]), sizeof(MagicData));
  for (i = f; i > 0; i--) {
    if (cd[n].md[i].effect == EF_NONE)
      continue;
    std::memcpy(&(cd[n].md[i]), &(cd[n].md[i-1]), sizeof(MagicData));
  }
  std::memcpy(&(cd[n].md[0]),&md, sizeof(MagicData));

  if ((f=best_er_weapon(n)) == -1) {
    cd[n].weq = nullptr;
    cd[n].wsep = 0;
  } else {
    cd[n].weq = &cd[n].id[f];
    cd[n].wsep = sep_calc(n, cd[n].weq);
  }
  set_fn(n);
  input_analyze(n, (char *)"cast");
  cd[n].wt = std::strlen(md.spell)*2 - TCN_HASTE(n);
  cd[n].wt = cd[n].wt * (10000 - cd[n].lv[LV_WIZARD]) / 10000;
  if (cd[n].wt < 5)
    cd[n].wt = 5;
  if(cd[n].md[0].is_inherence)
    cd[n].wt /= 2;
  return;
}

void select_armor(int n)
{
  int    f;
  int    i;

  for (f = -1, i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type & IT_MASK) != IT_ARMOR || cd[n].id[i].active_time==ULONG_MAX)
      continue;
    if (curse_check(n, cd[n].id[i].SE) < 0)
      continue;
    if (f < 0 || cd[n].id[i].DFP + cd[n].id[i].MRP > cd[n].id[f].DFP + cd[n].id[f].MRP)
      f = i;
  }
  cd[n].aeq = (f < 0 ? nullptr : &cd[n].id[f]);
  cd[n].asep = (f < 0 ? 0 : sep_calc(n, cd[n].aeq));
  set_fn(n);
  return;
}

void select_accessory(int n)
{
  int    f;
  int    i;

  for (f = -1, i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type & IT_MASK) != IT_ACCESSORY || cd[n].id[i].active_time==ULONG_MAX)
      continue;
    if (curse_check(n, cd[n].id[i].SE) < 0)
      continue;
    if (f < 0)
      f = i;
  }
  cd[n].oeq = (f < 0 ? nullptr : &(cd[n].id[f]));
  cd[n].at = (f < 0 ? AT_NONE : cd[n].oeq->AT);
  cd[n].osep = (f < 0 ? 0 : sep_calc(n, cd[n].oeq));
  set_fn(n);
  return;
}

int curse_check(int n, int se_t)
{
  if (se_t <= SE_NONE || se_t > MAX_SE) /* S.E. janai */
    return (0);
  switch (se_t) {
  default:
  case SE_ATPUP:
  case SE_SPELLUP:
  case SE_DEFUP:
  case SE_MRPUP:
  case SE_HASTE:
  case SE_ILLUSION:
  case SE_KARMADOWN:
  case SE_CLAIR:
  case SE_RAISEDEAD:
  case SE_VANISH:
  case SE_GIGANT:
    return (1);     /* dare demo PLUS no kouka ari */
  case SE_REGENE:
    if (cd[n].mhp == 0)
      return (0);
    return (1);
  case SE_WILL:
    if (cd[n].mmp == 0)
      return (0);
    return (1);
  case SE_ATPDOWN:
  case SE_SPELLDOWN:
  case SE_DEFDOWN:
  case SE_MRPDOWN:
  case SE_SLOW:
  case SE_BLIND:
  case SE_CONTRAST: /* add 20010226 */
    break;      /* type ni yoru... */
  case SE_POISON:
    if (cd[n].mhp == 0
  || cd[n].at == AT_POISONPROOF || cd[n].at == AT_IMMORTAL)
      return (0);   /* kouka nasi */
    break;
  case SE_PANIC:
    if (cd[n].mmp == 0
  || cd[n].at == AT_MEPROOF || cd[n].at == AT_IMMORTAL)
      return (0);   /* kouka nasi */
    break;
  case SE_PALSY:
    if (cd[n].at == AT_PALSYPROOF || cd[n].at == AT_IMMORTAL)
      return (0);   /* kouka nasi */
    break;
  case SE_CONFUSE:
  case SE_BERSERK:
    if (cd[n].at == AT_MEPROOF || cd[n].at == AT_IMMORTAL)
      return (0);   /* kouka nasi */
    break;
  case SE_SILENCE:
    if (cd[n].at == AT_IMMORTAL)
      return (0);   /* kouka nasi */
    break;
  case SE_KARMAUP:
    if (cd[n].align > 50)
      return (1);
    return (-1);
  case SE_PASSWALL:
  case SE_MIRROR:
  case SE_NOMOVE:
    if (n < MAX_NTTYS && cd[n].exp < 99999999)
      return (0);   /* PC ha mukou dayo */
    else
      return (1);   /* NPC,Master nara kouka ari */
  }
  if (cd[n].type >= CT_MAGICAL) /* noroi nante kankei naize */
    return (0);
  return (-1);      /* MINUS no kouka ari */
}

int str_search(int n, char *str)
{
  char  *p;

  for (p = cd[n].str; *p != 0 && *p != '>'; p++);
  for (; *p != 0; p++)
    if (ftolower(*p) == ftolower(*str) && next_char(p, str))
      return (1);
  return (0);
}

int next_char(char *sp, char *dp)
{
  if (*dp == 0)
    return (1);
  if (ftolower(*sp) == ftolower(*dp) && next_char(sp + 1, dp + 1))
    return (1);
  return (0);
}

int    direction_check(int x, int y, int d, int xx, int yy)
/* (x,y) (向き d) から (xx,yy) への位置関係 を調べる。*/
/* 返却値 は以下のビット和 */
/* ED_FACE:目の前  ED_AROUND:周辺(目の前除く)  ED_LINE:前方(目の前除く) */
/* ED_FOREAROUND: 爆風範囲(前方を除く) (ED_RANGEOUT(==0):それ以外)*/
{
  int i, j;
  int xd, yd;
  int f;

  f = ED_RANGEOUT;
  xd = px[d];
  yd = py[d];

  if (xx == x + xd && yy == y + yd)
    f |= ED_FACE;
  if (x-1 <= xx && xx <= x+1 && y-1 <= yy && yy <= y+1)
    f |= ED_AROUND;
  if(map[y+yd][x+xd].tip & T_ANTIMAGIC)
    return f;
  for (x += xd, y += yd, i = 1;
       i < 8 && 1<=x && x<=MAX_X-2 && 1<=y && y<=MAX_Y-2;
       x += xd, y += yd, i++){
    if (x == xx && y == yy)
      f |= ED_LINE;
    if(map[y][x].tip & T_MIST)
      break;
    if (i!=1 && (map[y+yd][x+xd].tip & T_ANTIMAGIC))
      if(x-1 <= xx && xx <= x+1 && y-1 <= yy && yy <= y+1)
  f |= ED_FOREAROUND;
  /* change for variable multi characters in one space 20000721 */
    if ((map[y+yd][x+xd].tip & T_ANTIMAGIC))
      break;
    for (j = 0; j < MAX_CHRS_INONE; j++)
      if (map[y][x].chr[j] != 0)
        break;
    if (j < MAX_CHRS_INONE)
      break;
  }
  /* add for NPC use quake */
  if (fld[map[y][x].area].floor == fld[map[yy][xx].area].floor)
    f |= ED_SAMEFLOOR;
  if (map[yy][xx].tip & T_ROOF)
    f |= ED_ROOF;
  return f;
}

int    is_effective_effect(int n, int m, int effect, int fn, int f)
/* NPC cd[n] が cd[m] を敵としたときに、*/
/* effect,fn が効果のあるものとして認めるかどうか。*/
/* f は上記の direction_check() の値もしくは ED_NORESTRICT */
{
  switch(effect){
  default:
    return 0;

  case EF_EXPLODE:
  case EF_LINE:
  case EF_PIERCE:
  case EF_SPHERE:
  /* add for NPC use quake 20000613 */
  case EF_ROOF:
  case EF_UNROOF:
  case EF_AREA:
    if(cd[m].mhp==0) return 0;
    if(fn % 2){
      if(CN_FORCE(m) || TCN_PROTECT(m) > 99) return 0;
    } else {
      if(CN_REFLEX(m) || TCN_SHELL(m) > 99) return 0;
    }
    switch(effect){
    case EF_EXPLODE: return (f & ED_FOREAROUND);
    case EF_LINE:    return (f & ED_LINE);
    case EF_PIERCE:  return (f & ED_LINE);
    case EF_SPHERE:  return (f & ED_AROUND);
    /* add for NPC use quake 20000613 */
    case EF_ROOF:
        if (!(f & ED_SAMEFLOOR) ||
            !(f & ED_ROOF) ||
             (cd[m].innate & IA_STONEHEAD) ||
             (cd[m].at == AT_HELMET))
          return 0;
        if ((cd[n].innate & IA_STONEHEAD) ||
            (cd[n].at == AT_HELMET) ||
           !(map[cd[n].y][cd[n].x].tip & T_ROOF) ||
             cd[n].mhp==0)
          return 1;
        if(fn % 2) {
            if(CN_FORCE(n) || TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    case EF_UNROOF:
        if (!(f & ED_SAMEFLOOR) ||
             (f & ED_ROOF) ||
             (cd[m].innate & IA_UMBRELLA) ||
             (cd[m].at == AT_UMBRELLA))
          return 0;
        if ((cd[n].innate & IA_UMBRELLA) ||
            (cd[n].at == AT_UMBRELLA) ||
            (map[cd[n].y][cd[n].x].tip & T_ROOF) ||
             cd[n].mhp==0)
          return 1;
        if(fn % 2) {
            if(CN_FORCE(n) || TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    case EF_AREA:
        if (!(f & ED_SAMEFLOOR) ||
             (cd[m].innate & IA_FLYING) ||
             (cd[m].at == AT_FLY))
          return 0;
        if ((cd[n].innate & IA_FLYING) ||
            (cd[n].at == AT_FLY) ||
             cd[n].mhp==0)
          return 1;
        if(fn % 2) {
            if(CN_FORCE(n) || TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    }
  case EF_MEXPLODE:
  case EF_MLINE:
  case EF_MPIERCE:
  case EF_MSPHERE:
  /* add for NPC use quake 20000613 */
  case EF_MROOF:
  case EF_MUNROOF:
  case EF_MAREA:
    if(cd[m].mmp==0) return 0;
    if(fn % 2){
      if(TCN_PROTECT(m) > 99) return 0;
    } else {
      if(CN_REFLEX(m) || TCN_SHELL(m) > 99) return 0;
    }
    switch(effect){
    case EF_MEXPLODE: return (f & ED_FOREAROUND);
    case EF_MLINE:    return (f & ED_LINE);
    case EF_MPIERCE:  return (f & ED_LINE);
    case EF_MSPHERE:  return (f & ED_AROUND);
    /* add for NPC use quake 20000613 */
    case EF_MROOF:
        if (!(f & ED_SAMEFLOOR) ||
            !(f & ED_ROOF) ||
             (cd[m].innate & IA_STONEHEAD) ||
             (cd[m].at == AT_HELMET))
          return 0;
        if ((cd[n].innate & IA_STONEHEAD) ||
            (cd[n].at == AT_HELMET) ||
           !(map[cd[n].y][cd[n].x].tip & T_ROOF) ||
             cd[n].mmp==0)
          return 1;
        if(fn % 2) {
            if(TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    case EF_MUNROOF:
        if (!(f & ED_SAMEFLOOR) ||
             (f & ED_ROOF) ||
             (cd[m].innate & IA_UMBRELLA) ||
             (cd[m].at == AT_UMBRELLA))
          return 0;
        if ((cd[n].innate & IA_UMBRELLA) ||
            (cd[n].at == AT_UMBRELLA) ||
            (map[cd[n].y][cd[n].x].tip & T_ROOF) ||
             cd[n].mmp==0)
          return 1;
        if(fn % 2) {
            if(TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    case EF_MAREA:
        if (!(f & ED_SAMEFLOOR) ||
             (cd[m].innate & IA_FLYING) ||
             (cd[m].at == AT_FLY))
          return 0;
        if ((cd[n].innate & IA_FLYING) ||
            (cd[n].at == AT_FLY) ||
             cd[n].mmp==0)
          return 1;
        if(fn % 2) {
            if(TCN_PROTECT(n) > 99) return 1;
        } else {
            if(CN_REFLEX(n) || TCN_SHELL(n) > 99) return 1;
        }
        return 0;
    }
  case EF_HEALS:
  case EF_HEAL:
    if (cd[n].mhp > 0 && PER_DICE < cd[n].hp * 100 / cd[n].mhp)
      return 0;
    switch(effect){
    case EF_HEALS: return ((f & 0x40) || !(f & ED_AROUND));
    case EF_HEAL:  return 1;
    }
  case EF_REGENE:
    if(PER_DICE > CN_REGENE(n)/2 +75 -CN_SPELL(n)/2) return 1;
    return 0;
  case EF_COAT:
    if(PER_DICE > OCN_COAT(n)/2 +75) return 1;
    return 0;
  case EF_SPELL:
    if(PER_DICE > CN_SPELL(n)/2 +75) return 1;
    return 0;
  case EF_PROTECT:
    if(PER_DICE > OCN_PROTECT(n)/2 +75 -CN_SPELL(n)/2) return 1;
    return 0;
  case EF_SHELL:
    if(PER_DICE > OCN_SHELL(n)/2 +75 -CN_SPELL(n)/2) return 1;
    return 0;
  case EF_HASTE:
    if(PER_DICE > OCN_HASTE(n)/2 +75 -CN_SPELL(n)/2) return 1;
    return 0;
  case EF_ILLUSION:
    if(PER_DICE > OCN_ILLUSION(n)/2 +75 -CN_SPELL(n)/2) return 1;
    return 0;

  case EF_POISON:
  case EF_PALSY:
  case EF_PANIC:
  case EF_CONFUSE:
  case EF_BERSERK:
  case EF_SILENCE:
  case EF_BLIND:
    if (cd[m].type >= CT_MAGICAL)
      return 0;
  case EF_COAT_:
  case EF_SPELL_:
  case EF_PROTECT_:
  case EF_SHELL_:
  case EF_HASTE_:
  case EF_HPDIVIDE:
  case EF_MPDIVIDE:
    return ((f & ED_FACE) && PER_DICE <= 25 + TCN_SPELL(n)/2);

  case EF_ANTIDOTE:
    if (PER_DICE < CN_POISON(n) * 2)
      return 1;
    return 0;
  case EF_REMOVE:
    if (PER_DICE < CN_PALSY(n) * 2)
      return 1;
    return 0;
  case EF_AWAKEN:
    if (PER_DICE <
  (CN_PANIC(n) + CN_CONFUSE(n) + CN_BERSERK(n) + CN_SILENCE(n)) * 2)
      return 1;
    return 0;
  case EF_FORCEFIELD:
  case EF_REFLEXFIELD:
  case EF_MIRROR:
  case EF_VANISH:
    if (PER_DICE <= 75)
      return 1;
    return 0;
  case EF_ERASE:
    return ((f & ED_FACE)
      && (((CN_FORCE(m) & 0xffff) || (CN_REFLEX(m) & 0xffff))
    || PER_DICE < (OCN_COAT(m)+OCN_PROTECT(m)+CN_SPELL(m)
             +OCN_HASTE(m)+OCN_ILLUSION(m)) * 2));
  }
}

int    use_charged_item_check(int n, int m)
{
  int    f, ff;
  int    i, j;
  std::array<char, 40> buf;

  if(fld[map[cd[n].y][cd[n].x].area].flag & FF_CHARGE)
    return -1;

  for (i = 0; i < MAX_ITEMS; i++)
    if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
       && cd[n].id[i].active_time !=ULONG_MAX
       && cd[n].id[i].SE != SE_HITTOUSE
       && cd[n].id[i].SE != SE_HITISUSE
       && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr))) break;
  if (i >= MAX_ITEMS) return -1;

  f=direction_check(cd[n].x, cd[n].y, cd[n].d, cd[m].x, cd[m].y);

  if((f & ED_FACE) && ((CN_FORCE(m) & 0xffff) || (CN_REFLEX(m) & 0xffff))) {
    for (i = 0; i < MAX_ITEMS; i++)
      if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
   && cd[n].id[i].active_time !=ULONG_MAX
   && cd[n].id[i].SE != SE_HITTOUSE
   && cd[n].id[i].SE != SE_HITISUSE
   && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr))
   && cd[n].id[i].UE == EF_ERASE)
  break;
    if(i < MAX_ITEMS) {
      for(j=0;j<i;j++)
  if(cd[n].id[i].name==cd[n].id[j].name){
    swap_item(&(cd[n].id[i]),&(cd[n].id[j]));
    i=j;
    break;
  }
      snprintf(buf.data(), buf.size(), "use %s", cd[n].id[i].name);
      input_analyze(n, buf.data());
      return 1;
    }
  }
  /* 未設定の判定は !ff (magic_check の ff == -1 と非対称で、添字 0 の
     候補も未設定扱いになる。元のまま) */
  for(i = MAX_ITEMS-1, ff = -1; i >= 0; i--)/*上の方ほど使われやすくするため*/
    if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
       && cd[n].id[i].active_time !=ULONG_MAX
       && cd[n].id[i].SE != SE_HITTOUSE
       && cd[n].id[i].SE != SE_HITISUSE
       && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr))
       && is_effective_effect(n, m, cd[n].id[i].UE, cd[n].id[i].fn, f))
      if(!ff || PER_DICE <50)
  ff = i;
  if(ff < 0)
    return -1;
  for(j=0;j<ff;j++)
    if(cd[n].id[ff].name==cd[n].id[j].name){
      swap_item(&(cd[n].id[ff]), &(cd[n].id[j]));
      ff = j;
      break;
    }
  snprintf(buf.data(), buf.size(), "use %s", cd[n].id[ff].name);
  input_analyze(n, buf.data());
  return 1;
}

int    view_location(int n, int j, int *vx, int *vy)
/* 視界内かどうか。視界内なら、view 位置 (vx,vy) を与える */
/* 先んじて set_view(n) の実行が必要 */
{
  int xx, yy;

  /* view 位置への変換 */
  xx= -py[cd[n].d] * (cd[j].x-cd[n].x) + px[cd[n].d] * (cd[j].y-cd[n].y);
  yy=  px[cd[n].d] * (cd[j].x-cd[n].x) + py[cd[n].d] * (cd[j].y-cd[n].y);

  if(xx<-2 || xx>2 || yy>3 || yy<-1) return 0;
  if(cd[n].view[yy+1][xx+2].tip== T_UNKNOWN) return 0;
  if(vx!=nullptr && vy!=nullptr){
    *vx = xx+2;
    *vy = yy+1;
  }
  return 1;
}

int    attack_angry(int n, int mc)
/* angry な(すなわち自分を攻撃してきた)敵に対して行動 */
/* 返却値 1:行動済み(cd[n].think も変化) */
/*        0:目標未発見により未行動 */
{
  int vx, vy;
  int j, k;
  int damaged;

  damaged = cd[n].damaged;
  cd[n].damaged = -1;

  if(damaged >=0 &&
     (cd[n].mhp > 0 && (cd[n].hp < cd[n].mhp * (99 - cd[n].moral) / 100
      || (cd[n].mmp > 0 && cd[n].mp < cd[n].mmp * (99 - cd[n].moral) / 100)))){
    npc_talk(n, TT_HELP);
    cd[n].think = TH_RUN;

    if((j=cd[n].angry) != -1 && view_location(n, j, &vx, &vy))
      if(try_to_runstep(n, j, vx, vy, mc, damaged))
  return 1;
    if(try_to_runstep(n, cd[n].angry, -1, -1, mc, damaged))
      return 1;
    return 0;
  }

  if((j=cd[n].angry)==-1 || CN_GASIFY(j)>=60) return 0;
  if(!view_location(n, j, &vx, &vy)){ /* 視界外 */
    if((cd[n].wd == WD_NONE
  || PER_DICE < cd[n].lv[LV_WIZARD]*40 / (cd[n].lv[LV_FIGHTER]+1))
       && magic_check(n,j)>0){ /* 視界外魔法攻撃 */
      cd[n].think = TH_MAGIC;
      return 1;
    }
    cd[n].think = TH_NONE;
    switch(damaged){
    case 0:
      if(mc & MC_F){
  input_analyze(n, (char *)"go");
  return 1;
      }
      return 0;
    case 1:
      input_analyze(n, (char *)"turn r");
      return 1;
    case 2:
      input_analyze(n, (char *)"turn b");
      return 1;
    case 3:
      input_analyze(n, (char *)"turn l");
      return 1;
    }
  }

  /* 視界内 */

  if(cd[n].think != TH_WIZARDSTEP
     && (cd[n].wd == WD_NONE
   || PER_DICE < cd[n].lv[LV_WIZARD]*20 / (cd[n].lv[LV_FIGHTER]+1))) {
    if((k=useable_long_range_magic(n, j))>0 && PER_DICE < k)
      cd[n].think = TH_WIZARDSTEP;
    else if(magic_check(n, j)>0){
      cd[n].think = TH_MAGIC;
      return 1;
    }
  }

  if(cd[n].think == TH_WIZARDSTEP){
    if (1<=vx && vx<=3
  && (((vy == 4 || vy == 3) && (!(mc & MC_B) || PER_DICE < 50))
      || (vy<=2 && (!(mc & MC_B) || PER_DICE < 30)))
  && magic_check(n, j)>0){
      cd[n].think = TH_MAGIC;
      return 1;
    }
    if(try_to_wizardstep(n, j, vx, vy, mc)){
      if(PER_DICE<10) npc_talk(n,TT_ATTACK);
      return 1;
    }
  }

  if((k=attack_check(n,j))==2)
    return 1; /* think として TH_ATTACK か TH_WIN が入る */

  if(try_to_attackstep(n, j, vx, vy, mc)){
    if(PER_DICE<10) npc_talk(n, TT_ATTACK);
    cd[n].think = TH_NONE;
    return 1;
  } else
    return 0;
}

int   attack_enemy(int n, int mc)
/* 攻撃目標に対して行動もしくはトレース目標にトレース行動 */
/* 返却値 1:行動済み(cd[n].think も変化) */
/*        0:目標がなく未行動 */
{
  int sa;
  int i, j, k;
  int vx, vy;

  k=temptation_to_attack(n);

  if(cd[n].trace!=-1
     && view_location(n, cd[n].trace, &vx, &vy)
     && PER_DICE > cd[n].align + k
     && try_to_tracestep(n, cd[n].trace, vx, vy, mc)){
    cd[n].think = TH_TRACE;
    return 1;
  }

  if(k>0){
    /* wd と比べる定数は TH_NONE (WD_NONE と同じ 0。同値なので動く。元のまま) */
    if(cd[n].think == TH_WIZARDSTEP
       || cd[n].wd == TH_NONE
       || PER_DICE < cd[n].lv[LV_WIZARD]*20/(cd[n].lv[LV_FIGHTER]+1))
      /* 魔法攻撃を選択 */
      for(sa=0; sa<25; sa++)
  for(i=0; i<MAX_CHRS_INONE; i++) {
    if ((j = cd[n].view[vy=kSy[sa]+1][vx=kSx[sa]+2].chr[i] - 1) < 0)
      continue;
    if (enemy_check(n, j)){
      k=useable_long_range_magic(n,j);
      if(cd[n].think != TH_WIZARDSTEP){
        if(k>0 && PER_DICE < k)
    cd[n].think = TH_WIZARDSTEP;
        else if(magic_check(n, j)>0){
    cd[n].think = TH_MAGIC;
    return 1;
        }
      } else if(k==0) continue; /* WIZARDSTEP の対象じゃない */

      if(cd[n].think == TH_WIZARDSTEP){
        if (1<=vx && vx<=3
      && (((vy==4 || vy==3) && (!(mc & MC_B) || PER_DICE < 50))
          || (vy<=2 && (!(mc & MC_B) || PER_DICE < 30)))
      && magic_check(n, j)>0){
    cd[n].think = TH_MAGIC;
    return 1;
        }
        if(try_to_wizardstep(n, j, vx, vy, mc)){
    if(PER_DICE<10) npc_talk(n,TT_ATTACK);
    return 1;
        }
      }
    }
  }

    /* 武器攻撃を選んだ もしくは 魔法攻撃ができなかった */
    /* 隣接地点攻撃チェック */
    for(sa=0; sa<=3; sa++)
      for(i=0; i<MAX_CHRS_INONE; i++)
  if ((j = cd[n].view[vy=kSy[sa]+1][vx=kSx[sa]+2].chr[i] - 1) >=0 && attack_check(n, j)==2)
    return 1; /* think として TH_ATTACK か TH_WIN が入る */

    /* 武器攻撃移動チェック */
    for(sa=0; sa<25; sa++)
      for(i=0; i<2; i++) {
  if ((j = cd[n].view[vy=kSy[sa]+1][vx=kSx[sa]+2].chr[i] - 1) < 0)
    continue;
  if (enemy_check(n, j))
    if(try_to_attackstep(n, j, vx, vy, mc)){
      if(PER_DICE<10) npc_talk(n,TT_ATTACK);
      cd[n].think = TH_NONE;
      return 1;
    }
      }
  }

  if(cd[n].think == TH_WIZARDSTEP)
    cd[n].think = TH_NONE;       /* 目標失う */
  if((cd[n].wd == WD_NONE
      || PER_DICE < cd[n].lv[LV_WIZARD]*40 / (cd[n].lv[LV_FIGHTER]+1))
     && line_search(n)>0){
    cd[n].think = TH_MAGIC;
    return 1;
  }
  return 0;
}


int    best_er_weapon(int n)
{
  int f, i;

  for (f = -1, i = 0; i < MAX_ITEMS; i++) {
    if ((cd[n].id[i].type & IT_MASK) != IT_WEAPON
  || cd[n].id[i].active_time ==ULONG_MAX)
      continue;
    if (curse_check(n, cd[n].id[i].SE) < 0)
      continue;
    if (f == -1 || cd[n].id[i].ER > cd[n].id[f].ER)
      f = i;
  }
  return f;
}

int    is_left_mp_moral(int n, int mn) {
  int mp, mmp;
  int moral;
  int mdp;
  int er;
  int i, j;

  mp = cd[n].mp;
  mmp = cd[n].mmp;
  moral = cd[n].moral;
  mdp = (j=mgstrlen(cd[n].md[mn].spell)) + 4;
  er = ((i=best_er_weapon(n))==-1)? 0 : cd[n].id[i].ER;

  if(mmp == 0) return 1;

  /* 使用すると */
  mp -= mdp * (100 - er)/100;
  /* add for NPC use quake 20000614 */
  if((i=cd[n].md[mn].effect) == EF_AREA || i == EF_MAREA ||
      i == EF_ROOF || i == EF_MROOF ||
      i == EF_UNROOF || i == EF_MUNROOF)
    mp -= (j + 4) * 29;
  if(mp < 1) return 0;
  if(mp < mmp * (99 - moral) / 100) return 0;
  return 1;
}

int    useable_long_range_magic(int n, int m)
{
  int i, j, powl, pows;

  powl = 0;
  pows = 0;
  for(i=0; i<MAX_MAGICS; i++){
    if((cd[n].md[i].effect == EF_EXPLODE
  || cd[n].md[i].effect == EF_LINE
  || cd[n].md[i].effect == EF_PIERCE
  || cd[n].md[i].effect == EF_MEXPLODE
  || cd[n].md[i].effect == EF_MLINE
  || cd[n].md[i].effect == EF_MPIERCE
  /* add for NPC use quake */
  || cd[n].md[i].effect == EF_AREA
  || cd[n].md[i].effect == EF_ROOF
  || cd[n].md[i].effect == EF_UNROOF
  || cd[n].md[i].effect == EF_MAREA
  || cd[n].md[i].effect == EF_MROOF
  || cd[n].md[i].effect == EF_MUNROOF)
       && is_effective_effect(n, m, cd[n].md[i].effect, cd[n].md[i].fn, ED_NORESTRICT)
       && is_left_mp_moral(n, i)
       && powl < (j=mgstrlen(cd[n].md[i].spell)))
      powl = j;
    else if((cd[n].md[i].effect == EF_SPHERE
       || cd[n].md[i].effect == EF_MSPHERE)
      && is_effective_effect(n, m, cd[n].md[i].effect, cd[n].md[i].fn, ED_NORESTRICT)
      && is_left_mp_moral(n, i)
      && pows < (j=mgstrlen(cd[n].md[i].spell)))
      pows = j;
  }

  if(npc_use_charged_item)
    for(i=0; i<MAX_ITEMS; i++){
      if((cd[n].id[i].type & IT_MASK) == IT_CHARGE
   && cd[n].id[i].active_time !=ULONG_MAX
   && cd[n].id[i].SE != SE_HITISUSE
   && cd[n].id[i].SE != SE_HITTOUSE
   && (!is_need_charge_time || cd[n].id[i].active_time <= time(nullptr))) {
  if((cd[n].id[i].UE == EF_EXPLODE
      || cd[n].id[i].UE == EF_LINE
      || cd[n].id[i].UE == EF_PIERCE
      || cd[n].id[i].UE == EF_MEXPLODE
      || cd[n].id[i].UE == EF_MLINE
      || cd[n].id[i].UE == EF_MPIERCE
  /* add for NPC use quake */
      || cd[n].id[i].UE == EF_AREA
      || cd[n].id[i].UE == EF_ROOF
      || cd[n].id[i].UE == EF_UNROOF
      || cd[n].id[i].UE == EF_MAREA
      || cd[n].id[i].UE == EF_MROOF
      || cd[n].id[i].UE == EF_MUNROOF)
     && is_effective_effect(n, m, cd[n].id[i].UE, cd[n].id[i].fn, ED_NORESTRICT)
     && powl < (j=mgstrlen(cd[n].id[i].name)))
    powl = j;
  else if((cd[n].id[i].UE == EF_SPHERE
     || cd[n].id[i].UE == EF_MSPHERE)
    && is_effective_effect(n, m, cd[n].id[i].UE, cd[n].id[i].fn, ED_NORESTRICT)
    && pows < (j=mgstrlen(cd[n].id[i].name)))
    pows = j;
      }
    }

  if(powl==0) return 0;
  if(pows==0) return 100;
  return (powl*100/(pows+powl));
}

int    npc_talk(int n, int tt)
{
  int  k;
  /* CPYSTR で使うため C 配列のまま */
  char buf[64];

  for(k=0; k<MAX_TALKS; k++)
    if(cd[n].td[k].type==tt)
      break;
  if(k<MAX_TALKS){
    CPYSTR(buf, cd[n].td[k].string);
    input_analyze(n, buf);
    return 1;
  }
  return 0;
}

int    temptation_to_attack(int n)
{
  int sa, k, i, j, vx, vy;

  for (sa=0; sa<25; sa++)
    for (i=0;i<MAX_CHRS_INONE;i++){
      if ((j =cd[n].view[vy=kSy[sa]+1][vx=kSx[sa]+2].chr[i] - 1) < 0)
  continue;
      if (enemy_check(n,j)){
  k=std::abs(vx-2)+std::abs(vy-1);
  if(k==0) return 100;
  return 100/k;
      }
    }
  return 0;
}

void  angry_check(int as, int ds)
{
  int xx, yy;

  if(as == ds) return; /* 自爆はパス */
  if(ds < MAX_NTTYS) {
    /* add for controlable sammon 20000614 */
    for(xx=MAX_NTTYS;xx<enmax;xx++) {
        if(accept_master(xx,ds))
            angry_check(as,xx);
    }
    return;
  }
  if(as < MAX_NTTYS) {
    for(xx=MAX_NTTYS;xx<enmax;xx++) {
        if(accept_master(xx,as))
            angry_check(ds,xx);
    }
  }
  if(cd[ds].type >= CT_MAGICAL && cd[as].type >= CT_MAGICAL) return;
  if(cd[ds].type == CT_BERSERKER) return; /* 狂戦士はひたすら目の前の敵を攻撃 */
  /* add for controlable sammon 20000614 */
  if(ed_o[cd[ds].et].apc == 3 && cd[ds].vf2 == as &&
     PER_DICE < cd[ds].moral && PER_DICE > cd[ds].align &&
     PER_DICE < cd[ds].cooldown) return;
  cd[ds].angry=as;
#ifdef DEBUG_ANGRY
  snprintf(debug_buf, sizeof(debug_buf), "angry    : %s(%d) <- %s(%d) ", cd[as].name,as,cd[ds].name,ds);
  log_print(debug_buf);
#endif

  /* view 位置への変換 */
  xx= -py[cd[ds].d] * (cd[as].x-cd[ds].x) + px[cd[ds].d] * (cd[as].y-cd[ds].y);
  yy=  px[cd[ds].d] * (cd[as].x-cd[ds].x) + py[cd[ds].d] * (cd[as].y-cd[ds].y);

  /* yy^       */
  /*   |       */
  /*   |       */
  /*   +--->xx */

  if(std::abs(xx)<=std::abs(yy))
    if(yy>=0) cd[ds].damaged = 0;   /* 前方から攻撃を受けた(以下同様) */
    else      cd[ds].damaged = 2;
  else
    if(xx>=0) cd[ds].damaged = 1;
    else      cd[ds].damaged = 3;
}

/* add for traitor summonee 20010221 */
/* n が m を 召喚主と認めるか */
int accept_master(int n, int m)
{

    if (ed_o[cd[n].et].apc == 3 &&
        cd[n].vf2 == m &&
        (cd[n].align < 50 ?
           (cd[m].align >= 50 && PER_DICE < cd[m].align) :
           ((std::log(cd[m].lv[LV_FIGHTER]+cd[m].lv[LV_WIZARD]+1)
           - std::log(cd[n].lv[LV_FIGHTER]+cd[n].lv[LV_WIZARD]+1))/std::log(2.0)+1)*50 > PER_DICE )
        )
        return 1;

   	return 0;
}

void  cooldown_check()
{
  int n;

  for (n = MAX_NTTYS; n < enmax; n++) {
    if(cd[n].type!=CT_NOEXIST
       && cd[n].angry!= -1
       && cd[n].think != TH_GET_AWAY
       && cd[n].think != TH_RUN) {
      if(cd[n].angry < MAX_NTTYS
   || cd[cd[n].angry].angry != n
   || cd[cd[n].angry].think == TH_RUN
   || cd[cd[n].angry].think == TH_GET_AWAY){
  if(PER_DICE <= cd[n].cooldown){
#ifdef DEBUG_ANGRY
    snprintf(debug_buf, sizeof(debug_buf),"cool down: %s(%d) <- %s(%d) (%d %%)",
      cd[cd[n].angry].name,cd[n].angry,
      cd[n].name,n,cd[n].cooldown);
    log_print(debug_buf);
#endif
    cd[n].angry = -1;
  }
      } else {
  if(n<cd[n].angry
     && PER_DICE <= (cd[n].cooldown+cd[cd[n].angry].cooldown)/2){
#ifdef DEBUG_ANGRY
    snprintf(debug_buf, sizeof(debug_buf),"cool down: %s(%d) <-> %s(%d) (%d %%)",
      cd[cd[n].angry].name,cd[n].angry,
      cd[n].name, n,
      (cd[n].cooldown+cd[cd[n].angry].cooldown)/2);
    log_print(debug_buf);
#endif
    cd[cd[n].angry].angry = -1;
    cd[n].angry = -1;
  }
      }
    }
  }
}
