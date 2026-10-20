\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -c -e 2 -I'/(1+)/(\\1)/g' -n 15 \\
t0012.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
