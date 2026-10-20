\S[300]\s[80]OUT=$(smenu -P -N 0 -ai:6 s:5,b -c -C a-2:1,4-6:2,8-:3 \\
-R a/20/:4+rb -- t0008.in)||exit 1
\S[300]\s[300]llll>\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
