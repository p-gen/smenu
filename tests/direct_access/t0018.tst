\S[300]\s[80]OUT=$(smenu -l -F -D o:0+ i:1 n:2 -- t0018.in)||exit 1
\S[300]\s[300]jj\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
