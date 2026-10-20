\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -S'/(.)(.)(.)/\\3<-\\2->\\1/g' \\
                                 -s '#150' -n 15 t0011.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
