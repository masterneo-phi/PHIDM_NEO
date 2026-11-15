#! /bin/sh

if [ -f phi.td.old ] ; then
  echo "Caution !! phi.td.old is exist already."
  exit 0
fi

mv phi.td phi.td.old

sed -e "/^:[SA]/ d" < phi.td.old > phi.td
sed -e "/^:[NTtOUMRPHGg]/ d" < phi.td.old > phi.swt
