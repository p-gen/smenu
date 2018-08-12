\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -c -i 2 -I'/(1+)/(\\1)/g' -n 15 \\
t0013.in)
\S[10]\s[0]\r
\s[10]echo ":$OUT:"
exit 0
