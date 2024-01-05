#!/bin/execlineb -P
importas HOME HOME
find ${HOME}/.local/s6-rc/src -type d
	-exec
		cd {}
		forx -E src { contents dependencies }
			if { eltest -f $src }
			foreground { printf "Adjusting %s of %s...\n" $src {} }
			if { mkdir -p -- ${src}.d }
			if { redirfd -r 0 $src forstdin -E c touch -- ${src}.d/${c} }
			rm -v -- $src
	;
