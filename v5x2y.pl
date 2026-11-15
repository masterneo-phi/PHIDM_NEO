#!/usr/bin/perl

print STDERR "v5x -> v5y\n";

print STDERR "back up files ...\n";
if (-e "phi.od.bak" || -e "phi.dd.bak" || -e "phi.swt.bak") {
	print STDERR "backup files exist.\n";
	print STDERR "stop process.\n";
	exit;
}
system("cp phi.od phi.od.bak");
system("cp phi.dd phi.dd.bak");
system("cp phi.swt phi.swt.bak");

print STDERR "phase 1\n";
print STDERR "load phi.od ...\n";
open(IN,"< phi.od.bak");
#定義読込
while(<IN>){
	chop;
	if (/^:C +([^ ]*) +"?([^"]*)"? *([FLRB]?T?D?H?) *"?(..)?"? *"?(..)?"?/i) {
		$cg{$1} = $2;
		$dir{$1} = $3;
		$sight{$1} = $5;
	}
}
print STDERR "complete.\n";
seek(IN,0,0);

#座標読込＆書出
#od -> dd
#od -> od(new)
print STDERR "phase 2\n";
print STDERR "editing phi.dd ...\n";
open(DD,">> phi.dd");
open(OD,"> tmp.od");
while(<IN>) {
	if (/^:O\s+(\d{3})\s+(\d{3})\s+(.+)/) {
        	($x,$y) = ($1,$2);
		($df,$o,$s,$d) = &objstr2new($3);
	        if ($s ne "") {
		        printf DD ":T %3.3d %3.3d %s\n",$x,$y,$s;
		}
		if ($o ne "") {
		        printf OD ":O %3.3d %3.3d %s\n",$x,$y,$o;
		}
		$od{$x,$y} = $df;
	}
}
close (DD);
close (IN);
print STDERR "complete.\n";

#動作修正1
print STDERR "phase 3\n";
print STDERR "load & editing phi.swt 1...\n";
open(IN,"< phi.swt.bak");
open(SW,"> tmp.swt");
while(<IN>){
	if (/^(:#?S\s+\d{3}\s+\d{3}\s+([01][16]\s+\d\d\s+\d{4,8}|02|(03|07|13|14|04|08|18|10|12)\s+"[^"]*"|0[59]\s+\d{4}|15\s+.))\s+(\d{3})\s+(\d{3})\s+21\s*(.+)/) {
		($if,$x,$y) = ($1,$4,$5);
		($df,$o,$s,$d) = &objstr2new($6);
		if ($o ne "") {
			printf SW "%s %3.3d %3.3d 21 %3.3d%3.3d %s\n",$if,$x,$y,$x,$y,$d;
		    printf OD ":O --- --- %s %3.3d%3.3d\n",$o,$x,$y;
			$cntobj{$x,$y} = 1;
		} else {
			printf SW "%s %3.3d %3.3d f2 %3.3d%3.3d\n",$if,$x,$y,$x,$y;
		}
		if ($s ne "") {
			printf SW "%s %3.3d %3.3d 23 %s\n",$if,$x,$y,$s;
		}
		$od{$x,$y} = $df;
	} else {
		print SW;
	}
}
close (IN);
close (SW);
close (OD);

#動作修正2
print STDERR "editing phi.swt 2...\n";
open(IN,"< tmp.swt");
open(SW,"> phi.swt");
while(<IN>){
	if (/^(:#?S\s+\d{3}\s+\d{3}\s+([01][16]\s+\d\d\s+\d{4,8}|02|(03|07|13|14|04|08|18|10|12)\s+"[^"]*"|0[59]\s+\d{4}|15\s+.))\s+(\d{3})\s+(\d{3})\s+22/) {
		($if,$x,$y) = ($1,$4,$5);
		if ($cg{$od{$x,$y}} ne "") {
			printf SW "%s %3.3d %3.3d 22 %3.3d%3.3d\n",$if,$x,$y,$x,$y;
			$cntobj{$x,$y} = 1;
		}
		if ($sight{$od{$x,$y}} ne "") {
			printf SW "%s %3.3d %3.3d 24\n",$if,$x,$y;
		}
	} elsif (/^:#?S\s+\d{3}\s+\d{3}\s+([01][16]\s+\d\d\s+\d{4,8}|02|(03|07|13|14|04|08|18|10|12)\s+"[^"]*"|0[59]\s+\d{4}|15\s+.)\s+\d{3}\s+\d{3}\s+f2/) {
		($p,$f) = ($&,$');
		substr($p,-2,1) = "2";
		print SW $p.$f;
	} else {
		print SW;
	}
}
close (IN);
close (SW);
unlink ("tmp.swt");
print STDERR "complete.\n";

print STDERR "phase 4\n";
print STDERR "numbering objectIDs ...\n";
open(IN,"< tmp.od");
open(OUT,"> phi.od");
while(<IN>) {
	if (/^:O (\d{3}) (\d{3})/) {
		($x,$y) = ($1,$2);
		if ($cntobj{$x,$y} == 1) {
			s/$/sprintf(" %3.3d%3.3d",$x,$y)/e;
			$outar{$x,$y} = 1;
		}
		print OUT;
	}
	if (/^:O --- --- / && /([^"]{6})$/) {
		$id = $1;
		($x,$y) = (substr($id,0,3),substr($id,3,3));
		print OUT if ($outar{$x,$y} != 1);
	}
}

print OUT <<EOT;
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

close (OUT);
unlink("tmp.od");
print STDERR "complete.\n";

print STDERR "all converted. thx.\n";

sub objstr2new {
	local($_) = @_;
	local($def,$obj,$surface,$direction);
	if (/^([^ ]*)\s+([NEWSFLRB]?\s*D?\s*H?)\s*"?([^"]*)"?/i) {
        	($def,$dt,$nm) = ($1,$2,$3);
		if ($cg{$def} ne "") {
		        $obj = $cg{$def};
			if ($dir{$def} =~ /^([FLRB])/) {
				$obj .= "  " . $1;
				if ($dir{$def} =~ /^.T/) {
					$obj .= "T";
				}
				$direction = "";
			} elsif ($dt =~ /^([NEWS])/) {
				$obj .= "  " . $1;
				$direction = $1;
			} else {
				$obj .= "  N";
				$direction = substr($dt,0,1);
			}
			if ($dir{$def} =~ /D/ || $dt =~ /D/) {
				$obj .= "D";
			}
			if ($dir{$def} !~ /H/ && $dt !~ /H/) {
				$obj .= "K";
			}
			if ($nm ne "") {
				$obj .= " \"$nm\"";
			}
		}
	    if ($sight{$def} ne "") {
			$surface = $sight{$def};
		}
	}
	return (($def,$obj,$surface,$direction));
}
