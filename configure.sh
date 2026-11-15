#!/bin/sh

cc="cc"
ccflag=""
cflags=""
libs="-lm"
chmodf=""
target="phi_dm"
bsddir="bsd"
bsdarc="bsd.o"

echo " "
echo " Select your system. "
echo "  1. BSD/Linux"
echo "  2. IRIX "
echo "  3. HP-UX "
echo "  4. AIX "
echo "  5. MIPS "
echo "  6. NEWS "
echo "  7. other SVR4 "
echo "  8. Win32 (Cygnus GNU-Win32) "
read ans
case "$ans" in
'1')
	cflags="-DBSD" ;;
'2')
	ccflag="-xansi -cckr" ;;
'3')
	ccflag="-Aa -D_HP_UX" ;;
'4')
	cflags="-DAIX" ;;
'5')
	cc="cc -systype bsd43"
	ccflag="-Olimit 2000"
	chmodf=""
	cflags="-DBSD -DMips" ;;
'8')
	cflags="-DBSD"
	libs="$libs -lwsock32"
	target="phi_dm.exe" ;;
esac

if [ ! $ans = '5' ]; then
echo " Search for gcc . "
cat > conf_test.c <<EOF
#include "stdio.h"
int main() { exit(0); }
EOF
compile="gcc conf_test.c -o conf_test > /dev/null 2>&1"
if eval $compile; then
	cc="gcc"
	ccflag="-O3"
	if [ $ans = '6' ]; then
		ccflag="-O2"
	fi
fi
rm conf_test*
fi

echo " Library checking for -lsocket . "
libs=$libs
chklibs="$libs -lsocket"
cat > conf_test.c <<EOF
#include "stdio.h"
int main() { exit(0); }
EOF
compile="$cc $ccflag conf_test.c -o conf_test $chklibs > /dev/null 2>&1"
if eval $compile; then
	libs=$chklibs
fi
rm conf_test*

echo " Library checking for -lnsl . "
libs=$libs
chklibs="$libs -lnsl"
cat > conf_test.c <<EOF
#include "stdio.h"
int main() { exit(0); }
EOF
compile="$cc $ccflag conf_test.c -o conf_test $chklibs > /dev/null 2>&1"
if eval $compile; then
	libs=$chklibs
fi
rm conf_test*

echo " "
echo " Input install directory for 'phi_dm'. Default is \"..\". "
read ans
destdir="$ans"
case "$destdir" in
'')
	destdir=".."
esac

if [ ! -d $destdir ]; then
	mkdir $destdir
	cp phi.bb phi.inf $destdir
fi

if [ ! -d $destdir/phi_docs ]; then
	mkdir $destdir/phi_docs
fi

if [ ! -d $destdir/backup ]; then
	mkdir $destdir/backup
fi

if [ ! -d $destdir/phi_shows ]; then
	mkdir $destdir/phi_shows
fi

if [ ! -d $destdir/phi_circle ]; then
	mkdir $destdir/phi_circle
fi

if [ ! -d $destdir/phi_circle_b ]; then
	mkdir $destdir/phi_circle_b
fi

if [ ! -d $destdir/phi_store ]; then
	mkdir $destdir/phi_store
fi

if [ ! -d $destdir/phi_mail ]; then
	mkdir $destdir/phi_mail
fi

if [ ! -f $destdir/phi.td ]; then
	echo " "
	echo " Input your area name. "
	read ans
	areaname="$ans"
	case "$areaname" in
	'')
		areaname="Default area name"
		echo " Set area name = $areaname "
	esac
	cat > $destdir/phi.td <<EOT
:N $areaname
:T 0
:O O 20017 A 001 001 MSGT
:H 001 001 - Metro Planetarium Gateway             59.106.178.128 20023
:G 001 001 032 032 Metro Planetarium Gateway       59.106.178.128 20023
:G 001 001 004 004 inside house                    -
.
EOT
fi

if [ ! -f $destdir/phi.swt ]; then
	cat > $destdir/phi.swt <<EOT
:S 002 005 01 00 0000 001 005 01 __
:S 002 005 02 001 005 01 @@
.
EOT
fi

if [ ! -f $destdir/phi_dm.cnf ]; then
	echo " "
	echo " Input width of this area. Default is 8. "
	read ans
	case "$ans" in
	'')
		ans="0"
	esac
	max_x="$ans"
	if test "$max_x" -lt 8 ; then
		echo " Map size should be greater than 8x8. So set width = 8. "
		max_x=8
	fi
	if test "$max_x" -gt 1000 ; then
		echo " Map size should be less than 1000x1000. So set width = 1000. "
		max_x=1000
	fi

	echo " "
	echo " Input height of this area. Default is 8. "
	read ans
	case "$ans" in
	'')
		ans="0"
	esac
	max_y="$ans"
	if test $max_y -lt 8 ; then
		echo " Map size should be greater than 8x8. So set height = 8. "
		max_y=8
	fi
	if test "$max_y" -gt 1000 ; then
		echo " Map size should be less than 1000x1000. So set height = 1000. "
		max_y=1000
	fi

	echo " "
	echo " Input cdi-format separation "
	echo "  0: No separate (default)"
	echo "  1: cdi-64"
	echo "  2: cdi-100"
	echo "  3: cdi-256 (NEV class)"
	echo "  4: cdi-1000 (chrdata > 5MB)"
	echo "  5: cdi-3906 (max)"
	echo "  6: custom"
	read ans
	case "$ans" in
	'')
		cdiopt="/* No CDI separation */" ;;
	'0')
		cdiopt="/* No CDI separation */" ;;
	'1')
		cdiopt="#define EPHI_FastBind_CD_NUM 64" ;;
	'2')
		cdiopt="#define EPHI_FastBind_CD_NUM 100" ;;
	'3')
		cdiopt="#define EPHI_FastBind_CD_NUM 256" ;;
	'4')
		cdiopt="#define EPHI_FastBind_CD_NUM 1000" ;;
	'5')
		cdiopt="#define EPHI_FastBind_CD_NUM 3906" ;;
	'6')
		echo " Input CDI number."
		read ans
		if test $ans -gt 1 ; then
			cdiopt="#define EPHI_FastBind_CD_NUM $ans"
		else
			cdiopt="/* No CDI separation */"
		fi ;;
	esac

	echo " "
	echo " Input number of character backup. Default is 16. "
	read ans
	case "$ans" in
	'')
		ans="0"
	esac
	c_backup="$ans"
	if test $c_backup -lt 16 ; then
		echo " Backup size should be greater than 16. So set backup size = 16. "
		c_backup=16
	fi

	echo " "
	echo " Input number of NPC. Default is 128. "
	read ans
	case "$ans" in
	'')
		ans="128"
	esac
	npc_num="$ans"
	if test $ans -gt 32704 ; then
		echo " Number of NPC should be less than 32704. So set Number of NPC = 32704. "
		npc_num=32704
	fi

	echo " "
	echo " Input number of Objects. Default is 64. "
	read ans
	case "$ans" in
	'')
		ans="64"
	esac
	obj_num="$ans"
	if test $obj_num -gt 65535 ; then
		echo " Number of Objects should be less than 65535. So set Number of Objects = 65535. "
		obj_num=65535
	fi

	echo " "
	echo " Input number of general flags. Default is 0x100. "
	read ans
	case "$ans" in
	'')
		ans="0x100"
	esac
	switch_num="$ans"

	echo " "
	echo " Input number of general variables. Default is 0x100. "
	read ans
	case "$ans" in
	'')
		ans="0x100"
	esac
	gvar_num="$ans"

	echo " "
	echo " Input number of private variables. Default is 0x10. "
	read ans
	case "$ans" in
	'')
		ans="0x10"
	esac
	pvar_num="$ans"

	echo " "
	echo " Input number of saved variables. Default is 0x10. "
	read ans
	case "$ans" in
	'')
		ans="0x10"
	esac
	svar_num="$ans"

	echo " "
	echo " Input number of characters in one space. Default is 2. "
	read ans
	case "$ans" in
	'')
		ans="2"
	esac
	onespace_num="$ans"

	echo " "
	echo " Use sdi format ? "
	echo "  0: No separate (default)"
	echo "  1: sdi (area hashed)"
	read ans
	case "$ans" in
	'')
		sdiopt="/* No SDI separation */" ;;
	'0')
		sdiopt="/* No SDI separation */" ;;
	'1')
		sdiopt="#define SDI_AREA"
		if test ! -d $destdir/phi_sdi ; then
			mkdir $destdir/phi_sdi
			chmod og-rwx $destdir/phi_sdi
			echo " Please use cnv_sd.pl "
		fi ;;
	esac

	echo " "
	echo " Use sub area code ? "
	echo "  0: No (default)"
	echo "  1: sub area sepalated"
	read ans
	case "$ans" in
	'')
		subarea="/* No SUB area */" ;;
	'0')
		subarea="/* No SUB area */" ;;
	'1')
		subarea="#define AREA_SUB" ;;
	esac

	echo " "
	echo " Select language of DM. Default is English. "
	echo "  1: English (Default)"
	echo "  2: Japanese"
	read ans
	lang="/* DM language English */"
	case "$ans" in
	'2')
		lang="#define DM_LANG_JP";;
	esac

	cat > phi_dm.cnf <<EOT

/*	Phantasmal Island ( Configuration )	1994-5(C)b-Yak-38	*/

#define	MAX_X		$max_x
#define	MAX_Y		$max_y

#define C_BACKUP	$c_backup

#define MAX_ENEMIES     $npc_num
#define MAX_OBJECTS     $obj_num
#define GFLAG_MAX       $switch_num
#define GVAR_MAX        $gvar_num
#define PVAR_MAX        $pvar_num
#define SVAR_MAX        $svar_num

#define MAX_CHRS_INONE  $onespace_num
$cdiopt
$sdiopt
$subarea
$lang
EOT
else
	cp $destdir/phi_dm.cnf .
	if [ -f $destdir/mt.h ]; then
 	  cp $destdir/mt.h .
	fi
	if [ -f $destdir/fn.h ]; then
 	  cp $destdir/fn.h .
	fi
	if [ -f $destdir/wt.h ]; then
 	  cp $destdir/wt.h .
	fi
	if [ -f $destdir/it.h ]; then
 	  cp $destdir/it.h .
	fi
	if [ -f $destdir/ae.h ]; then
 	  cp $destdir/ae.h .
	fi
	if [ -f $destdir/at.h ]; then
 	  cp $destdir/at.h .
	fi
	if [ -f $destdir/wta.h ]; then
 	  cp $destdir/wta.h .
	fi
	if [ -f $destdir/se.h ]; then
 	  cp $destdir/se.h .
	fi
	if [ -f $destdir/ci.h ]; then
 	  cp $destdir/ci.h .
	fi
	if [ -f $destdir/rs.h ]; then
 	  cp $destdir/rs.h .
	fi
	if [ -f $destdir/use.h ]; then
 	  cp $destdir/use.h .
	fi
	if [ -f $destdir/ae.h ]; then
 	  cp $destdir/foods.h .
	fi
fi

if [ ! -f $destdir/phi.cd ]; then
	echo " "
	echo " Setting default character data. "
	cat > $destdir/phi.cd <<EOT
:guest           Guest                           0000 0000 0000 0000 00 1000 1000 1000 1000 1000 00000000 002 002 0 00000000 nagai@ee.kogakuin.ac.jp
:M Cure                            09 s
.
:master          World Master                    9999 9999 9999 9999 00 4999 9999 4999 9999 9999 99999999 002 002 0 00000000 nagai@ee.kogakuin.ac.jp
:M Evil wind from Malaika          99 n *
:M Load td                         99 n *
:M Close gate                      99 n *
:M Dumapic                         99 n *
:M Pass wall                       99 n *
:M Cache                           99 n *
:M Metamorphosis                   99 n *
:M Restoration                     99 n *
:Swp:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgA:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgE1:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgE2:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgE3:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgF:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SmgS:9999:9999:9999:9999:9999:9999:9999
:SdtI:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SdtW:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:SdtM:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:ScrI:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:ScrW:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
:ScrM:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999:9999
.
.
EOT
fi

if [ ! -f $destdir/phi.bd ]; then
	echo " "
	echo " Setting default backup data. "
	cat > $destdir/phi.bd <<EOT
.
EOT
fi

if [ ! -f $destdir/phi.ed ]; then
	echo " "
	echo " Setting default enemy data. "
	cat > $destdir/phi.ed <<EOT
:Human                           02 00 00 50 40 1000 1000 1000 1000 0100 004 004 04 00 24
:T 99 01 Fight!
:T 99 02 Help!
:T 99 03 I won!
:T 99 04 I lost...
<select
:T 50 05 Hello.
:T 99 05 Hi.
>
:M 99 Cure                            09 s
:M 99 Harm                            01 s
:M 99 Insane                          05 s
:I 99 Cloth                           0001 05 n 01 05 00 00
:I 99 Knife                           0312 02 n 02 00 00 00
.
:Cat                             02 01 50 50 30 0500 0500 0050 0050 0000 004 004 04 00 24
:T 99 01 Hooooow!
:I 99 Fang                            0f82 20 n 04 00 00 00
:I 99 Claw                            0f02 05 n 08 00 00 00
:I 99 Fur                             0f01 00 n 01 05 00 00
:I 50 Cat meat                        0003 05 n 01 00 02 02
.
:Dog                             02 01 50 50 30 0500 0500 0050 0050 0000 004 004 04 00 24
:T 99 01 Bow bow!
:I 99 Fang                            0f82 20 n 04 00 00 00
:I 99 Claw                            0f02 05 n 08 00 00 00
:I 99 Fur                             0f01 00 n 01 05 00 00
:I 50 Dog meat                        0003 10 n 01 00 05 02
.
.
EOT
fi

if [ ! -f $destdir/phi.id ]; then
	echo " "
	echo " Setting default item data. "
	cat > $destdir/phi.id <<EOT
.
EOT
fi

if [ ! -f $destdir/phi.sd ]; then
	echo " "
	echo " Setting default system data. "
	cat > $destdir/phi.sd <<EOT
.
EOT
fi

if [ ! -f $destdir/phi.bb ]; then
	cat > $destdir/phi.bb <<EOT
 ###      Welcome to Phantasmal Island.
 
 ###      These are some basic commands to adventure.
 ###  exit   ... Save data and exit the world.
 ###  go     ... Go forward. If type 'go left', go left with same direction.
 ###  turn   ... If type 'turn left', turn to left direction.
 ###  look   ... Look around.        |  check   ... Check your status.
 ###  see    ... See around.         |  spells  ... Display spells.
 ###  read   ... Read messages.      |  write   ... Write messages.
 ###  erase  ... Erase messages.     |  title   ... Change board title.
 ###  get    ... Get items.          |  put     ... Put your items.
 ###  use    ... Use your items.     |  equip   ... equip your items.
 ###  sort   ... Sort your items.    |  unequip ... Unequip your items.
 ###  hit    ... Hit enemies.        |  cast    ... Cast the spell.
 ###  pay    ... Pay money.          |  priv    ... Send private message.
 ###  chat   ... Chat mode           |  action  ... Act something.
 ###  board      ... Read board automaticaly
 ###  floor item ... Check items on floor automaticaly
 ###  version    ... Display server version.
EOT
fi

if [ ! -f $destdir/phi.inf ]; then
	cat > $destdir/phi.inf <<EOT
 ###    Enjoy your virtual life.
EOT
fi

if [ ! -f $destdir/phi.dd ]; then
	echo " "
	echo " Setting default dungeon data. "
	cat > $destdir/phi.dd <<EOT
HHHHHHHHHHHHHHHH
HH        ____HH
HH            HH
HH    #~[~#~  HH
HHTT  |~x~|~  HH
HH@@  #~|~#~  HH
HHTT          HH
HHHHHHHHHHHHHHHH
EOT

	echo " "
	echo " Setting default area data. "
	cat > $destdir/phi.fd <<EOT
0000000000000000
0000000000010100
0000000000000000
0000000202020000
0000000202020000
0000000202020000
0000000000000000
0000000000000000
:E 00 00 Field
:E 01 00 Pond
:E 02 00 House
.
EOT
fi

if [ ! -f $destdir/phi.od ]; then
	echo " "
	echo " Setting sample objects data. "
	cat > $destdir/phi.od <<EOT
:O --- --- !ML              ND "" !MLine0
:O --- --- !ML              ND "" !MLine1
:O --- --- !ML              ND "" !MLine2
:O --- --- !M0              FD "" !MHit
:O --- --- !MSC             ND "" !MExplode0
:O --- --- !MSS             ND "" !MExplode1
:O --- --- !MSC             ED "" !MExplode2
:O --- --- !MSS             WD "" !MExplode3
:O --- --- !M0              LD "" !MExplode4
:O --- --- !MSS             ED "" !MExplode5
:O --- --- !MSC             WD "" !MExplode6
:O --- --- !MSS             SD "" !MExplode7
:O --- --- !MSC             SD "" !MExplode8
:O --- --- !M1              FD "" !MHeal
:O --- --- !M1              LD "" !MCoat
:O --- --- !M1              BD "" !MProtect
:O --- --- !M1              RD "" !MIllusion
:O --- --- !M0              BD "" !MDown
EOT
fi


echo " "
echo " Setting Makefile. "
cat > Makefile <<EOT

DISTDIR	= $destdir
MAKEOPT	=

SHELL	= /bin/sh
CC	= $cc
CCFLAG	= -c $cflags $ccflag \$(MAKEOPT) -Inames 
LIBS	= $libs

BSDDIR  = $bsddir
BSDARC  = $bsdarc

OBJS	= phi_dm.o phi_data.o phi_enemy.o \\
		phi_magic.o phi_attack.o phi_use.o phi_calc.o \\
		phi_str.o phi_m57.o new_proto.o ext_data.o new_command.o \\
		expression_op.o ext_skills.o ext_party.o\\
		\$(BSDARC) \\
		tcp.o
SRCS	= phi_dm.c phi_data.c phi_enemy.c \\
		phi_magic.c phi_attack.c phi_use.c phi_calc.c \\
		phi_str.c phi_m57.c new_proto.c ext_data.c new_command.c \\
		expression_op.c ext_skills.c ext_party.c\\
		tcp.c
PROGRAM	= $target
DOCS	= phi.dat README.sjis

all:	\$(PROGRAM)

.c.o:
	\$(CC) \$< \$(CCFLAG)

\$(PROGRAM): \$(OBJS)
	\$(CC) -o \$@ \$(OBJS) \$(LIBS)

\$(BSDARC):
	cd \$(BSDDIR);make

install: \$(PROGRAM)
	cp \$(PROGRAM) phi_dm.cnf \$(DOCS) \$(DISTDIR)
	rm \$(PROGRAM)
	chmod $chmodf og-rwx \$(DISTDIR) \$(DISTDIR)/phi_docs \$(DISTDIR)/backup
clean:
	rm -f *.o \$(PROGRAM)
	cd \$(BSDDIR);make clean

cdcnv: cdcnv.o \$(BSDARC)
	\$(CC) -o cdcnv cdcnv.o \$(BSDARC)
EOT

echo " "
echo " Setting Makefile(./$bsddir). "
cat > $bsddir/Makefile <<EOT

CC	= $cc
CCFLAG	= -c $cflags $ccflag
BSDOBJ  = strlcpy.o strlcat.o
BSDSRC 	= strlcpy.c strlcat.c
BSDARC  = $bsdarc

.c.o:
	\$(CC) \$< \$(CCFLAG)

\$(BSDARC): \$(BSDOBJ)
	ar -r ../\$@ \$(BSDOBJ)

clean:
	rm -f *.o

EOT

rm -f *.o

echo " "
echo " Please 'make', 'make install' and execute 'phi_dm' as background process. "
echo " Thank you. "
