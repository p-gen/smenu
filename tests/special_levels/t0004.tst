\S[300]\s[80]OUT=$(smenu -1 '1$' b -2 '2$' 7/1,br -3 '3$' 0/5 \\
            -4 4 r -5 5 rb -c -- t0004.in)||exit 1
\S[300]\s[300]j\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
