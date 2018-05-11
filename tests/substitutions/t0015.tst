\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -c -e 2 -E'/(1+)/(\\1)/g' -n 15 \\
t0015.in)
\S[100]\s[120]\r
\s[0]echo ":$OUT:"
exit 0
