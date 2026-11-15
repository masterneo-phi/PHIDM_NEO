#!/bin/sh

if [ ! -f Makefile ]
then
	echo " "
	echo " Makefile not exists. Please configure at first."
	exit
fi

destdir=`awk -F= '/DISTDIR/ {print $2;}' Makefile`

echo " "
echo " Use old weapon property ? (y/n=any) "
read ans
case "$ans" in
'y'|'Y')
   makeopts=$makeopts" -DOLDWEAPON"
esac

echo " "
echo " Use split logs ? (y/n=any) "
read ans
case "$ans" in
'y'|'Y')
   makeopts=$makeopts" -DSPLITLOG"
   if test ! -d $destdir/phi_logs ; then
	   mkdir -p $destdir/phi_logs
	   chmod og-rwx $destdir/phi_logs
   fi
   ;;
esac

echo " "
echo " Display client domain name on log ? (y/n=any) "
read ans
case "$ans" in
'y'|'Y')
   makeopts=$makeopts" -DDOMAIN_DISPLAY"
   if test ! -d $destdir/phi_logs ; then
	   mkdir $destdir/phi_logs
	   chmod og-rwx $destdir/phi_logs
   fi ;;
esac

echo " "
echo " Need not skills ? (y/n=any) "
read ans
case "$ans" in
'y'|'Y')
   makeopts=$makeopts" -DNO_SKILL"
esac

cp Makefile Makefile.bak
sed -e "/^MAKEOPT	=/ s/^.*$/MAKEOPT	=$makeopts/" < Makefile.bak > Makefile
rm Makefile.bak

echo " "
echo " Remake Makefile. Please make clean & make."

