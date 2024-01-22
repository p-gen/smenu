\S[300]\s[80]OUT=$(TERM=vt100 LANG=C smenu -c -e x -e '[0-9]' t0008.in)
\S[300]\s[200]JJJKKK\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
