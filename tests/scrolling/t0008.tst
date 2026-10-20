\S[300]\s[80]OUT=$(TERM=vt100 LC_ALL=C smenu -c -e x -e '[0-9]' t0008.in)||exit 1
\S[300]\s[300]JJJKKK\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
