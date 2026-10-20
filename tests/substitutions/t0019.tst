\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -l -S'/1.*//' -n 4 t0019.in)||exit 1
\S[300]\s[300]lllllllllljjjlllll\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
