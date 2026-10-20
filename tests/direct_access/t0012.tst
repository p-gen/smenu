\S[300]\s[80]OUT=$(smenu -c -F -N -D i:1 n:2 s:100 f:n w:3 -- t0012.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
