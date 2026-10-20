\S[300]\s[80]OUT=$(smenu -a e:5,br b:0/6 -c -R2 -- t0004.in)||exit 1
\S[300]\s[300]l\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
