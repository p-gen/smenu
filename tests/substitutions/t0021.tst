\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -S'/(1)/\\[1 \\1 \\1\\]1/g' \\
-n 20 t0021.in)
\S[10]\s[120]\r
\S[10]\s[10]echo ":$OUT:"
exit 0
