\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -S'/(.)(.)(.)/\\3\\2\\1/g' \\
                                 -s '#150' -n 15 t0010.in)
\S[100]\s[120]\r
\s[0]echo ":$OUT:"
exit 0
