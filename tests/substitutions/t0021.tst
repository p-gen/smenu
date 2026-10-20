\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -S'/(1)/\\[1 \\1 \\1\\]1/g' \\
-n 20 t0021.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
