\S[300]\s[80]OUT=$(smenu -! "Interrupted by CTRL-C" t0001.in)||exit 1
\S[300]\s[300]l\CC
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
