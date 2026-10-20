\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -S'/^1+$/(&)/' -n 15 t0004.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
