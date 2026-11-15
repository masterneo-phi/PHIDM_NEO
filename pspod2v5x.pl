#!/usr/bin/perl

print "1. psp11 -> v5x\n";
print "2. v5x   -> psp11\n";

until($sel == 1 || $sel ==2) {
    $sel = getc;
}

system("cp phi.od phi.od.bak");

open(IN,"< phi.od.bak");
open(OUT,"> phi.od");

if ($sel == 1) {

while(<IN>){
    chop;
    if (/^:O ([^ ]*) +(\d{3}) +(\d{3}) +(\d) *(.*)$/) {
        ($obj,$x,$y,$dir,$nm) = ($1,$2,$3,$4,$5);
        #向き固定 -> 定義が必要
        if ($dir > 3) {
            #未定義
            if ($def{$obj,$dir} != 1) {
                $def{$obj,$dir} = 1;
                print OUT ":C $obj$dir \"$obj\" ";
                print OUT substr("BLFR",$dir-4,1);
                print OUT "DH \"\"\n";
            }
            printf OUT ":O %3.3d %3.3d %s DH",$x,$y,$obj.$dir;
        } else {
            printf OUT ":O %3.3d %3.3d %s %sDH",$x,$y,$obj,substr("NESW",$dir,1);
        }
        print OUT " \"$nm\"" if $nm ne "";
        print OUT "\n";
    }
}

} elsif ($sel == 2) {

#定義読込
while(<IN>){
    chop;
    if (/^:C +([^ ]*) +"?([^"]*)"? *([FLRB]?)/i) {
        $cg{$1} = $2;
        $dir{$1} = $3;
    }
}

seek(IN,0,0);

#座標読込＆書出
while(<IN>){
    chop;
    if (/^:O +(\d{3}) +(\d{3}) +([^ ]*) +([NEWS]?) *D? *H? *"?([^"]*)"?/i) {
        ($x,$y,$def,$dir,$nm) = ($1,$2,$3,"\u$4",$5);
        printf OUT ":O %s %3.3d %3.3d ",$cg{$def},$x,$y;
        if ($dir{$def} eq "") {
            printf OUT "%1.1d",index("NESW",$dir);
        } else {
            printf OUT "%1.1d",(index("BLFR",$dir{$def})+4);
        }
        printf OUT " %s",$nm if $nm ne "";
        print OUT "\n";
    }
}

print OUT ".\n";

}

close (OUT);
close (IN);
