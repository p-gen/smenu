\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -S'/(.)(.)(.)/\\3\\2\\1/g' \\
                                 -s '#150' -n 15 t0010.in)
\S[10]\s[0]\r
\s[10]echo ":$OUT:"
exit 0
