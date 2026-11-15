#!/usr/bin/perl

print " 1. phi.sd -> SDI(Area hashed)\n";
print " 2. SDI -> phi.sd\n";
print "\n";
print " please select\n";

$n = <STDIN>;

if ($n == 1) {

	open(CNF,"< phi_dm.cnf");
	while(<CNF>) {
		@f = split;
		$mx = $f[2] if (/MAX_X/);
		$my = $f[2] if (/MAX_Y/);
	}
	close(CNF);

	open(FD,"< phi.fd");
	@f = <FD>;
	@fd = @f[0 .. ($my-1)];
	close(FD);
	$mode = length($fd[0]) / $mx;

	if (! -d "phi_sdi") {
		mkdir("phi_sdi",0755);
	}

	system("cp phi.sd phi.sd.bak");
	unlink("phi.sd");

	open(SD,"< phi.sd.bak");
	while(<SD>) {
		if (/^:[TBL] (\d\d\d) (\d\d\d)/) {
			($x,$y) = ($1,$2);
			open(SDI,">> phi_sdi/sd.".substr($fd[$y],$x*$mode,2));
			print SDI $_;
			close(SDI);
		} elsif (/^:[GI]/) {
			open(SDI,">> phi.sd");
			print SDI $_;
			close(SDI);
		}
	}
	close(SD);

} else {

	system("cp phi.sd phi.sd.bak");
	system('sed -e "/^\./ d" phi.sd.bak > phi.sd');
	for($i=0;$i<0x100;$i++) {
		$f = "phi_sdi/sd.".sprintf("%02x",$i);
		if (-f $f) {
			system('sed -e "/^\./ d" '.$f.' >> phi.sd');
		}
	}

}

print " done.\n";

