/* for Phi m57     */
/* written by Qajin */

#ifndef __PHI_M57_H__
#define __PHI_M57_H__

#define UNSEE7(y,x)	{memset(&(view7[(y)][(x)]),'\0',sizeof(MapData)); view7[(y)][(x)].tip = T_UNKNOWN;}

int is_visible(int,int);
int flooritemtype(int,int);
void   set_view7(int);
void   dd_m57(char *, MapData *, int);
void   print_m57(int, int);
void   eagleeye_m57(int);

extern int chr_pos_psp_compati; /* キャラ位置 PSP互換 */
extern int turn_permission;     /* turn/solid 許可モード */
extern int mapsize_only55;

#endif
