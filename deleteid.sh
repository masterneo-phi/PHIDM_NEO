#!/bin/sh

echo " "
echo " Input phi data directory. Default is \"..\". "
read ans
destdir="$ans"
case "$destdir" in
'')
	destdir=".."
esac

echo " "
echo " Input target charactor ID. Only enter key to search cd/cdi/bd. "
read ans
cid="$ans"
case "$ans" in
'')
	echo " "
	echo " Input charactor name to search on cd/cdi/bd. "
	read ans
	name="$ans"

	cid=`cat $destdir/phi.cd $destdir/phi_cdi/cd.* | sed -n -e "/^:............... $name/ p" | awk '{print substr($1,2,15);}'`
	if [ a$cid = 'a' ]; then
		cid=`sed -n -e "/^:............... $name/ p" < $destdir/phi.bd | awk '{print substr($1,2,15);}'`
	fi
	if [ a$cid = 'a' ]; then
		echo " not found."
		exit
	fi

	echo " Found id for $name is $cid."
esac

if [ ! a$cid = 'a' ]; then

	echo " "
	echo " Which data delete? l)ock or b)oard "
	read ans
	sd=''
	case "$ans" in
	'l')
		sd="L";;
	'b')
		sd="TB";;
	esac

	if [ ! a$sd = 'a' ]; then
		for f in $destdir/phi.sd $destdir/phi_sdi/sd.??
		do
			cp $f $f.bak
			sed -e "/^:[$sd] [0-9][0-9][0-9] [0-9][0-9][0-9] $cid/ d" < $f.bak > $f
		done

		if [ $sd = 'TB' ]; then
			rm -f $destdir/phi_docs/B????????.$cid
		fi

		echo " "
		echo " Please cast a spell 'Load sd' on phi or reboot dm. "
		echo " Thank you."

	fi

fi
