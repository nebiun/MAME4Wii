#!/c/Tcl/bin/tclsh.exe

set fd [open "rotti.txt" r]
set lista_rotti ""
while {[gets $fd line] >= 0} {
	append lista_rotti "[lindex $line 2] "
}
close $fd
#puts $lista_rotti

set fd [open "mini.c" r]
while {[gets $fd line] >= 0} {
	if { [regexp -- {^[ \t]+DRIVER\( ([^\)]+) \).*} $line all rom] == 1 } {
#		puts ">>$line"
		if { [lsearch -exact $lista_rotti $rom] == -1 } {
			puts $line
		} else {
			puts "//$line"
		}
	} else {
		puts $line
	}
}
close $fd
