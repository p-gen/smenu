\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -c -e 2 -E'/(1+)/(\\1)/g' -n 15 \\
t0015.in)
\S[300]\s[200]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
