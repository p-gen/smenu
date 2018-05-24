\S[10]OUT=$(smenu -1 '1$' b -2 '2$' 7/1,br -3 '3$' 0/5 \\
            -4 4 r -5 5 rb -c -- t0004.in)
\S[100]\s[120]j\r
\s[0]echo ":$OUT:"
exit 0
