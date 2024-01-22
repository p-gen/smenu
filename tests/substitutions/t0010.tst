\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -S'/(.)(.)(.)/\\3\\2\\1/g' \\
                                 -s '#150' -n 15 t0010.in)
\S[300]\s[200]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
