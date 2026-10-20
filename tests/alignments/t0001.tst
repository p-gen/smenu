\S[300]\s[80]OUT=$(smenu -c -g: -Cr1,3-4 -Rl/'c d'/ -- t0001.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
