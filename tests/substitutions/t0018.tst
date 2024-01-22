\S[300]\s[80]OUT=$(LANG=en_US.UTF-8 smenu -c -i 2 -E/1/A/g -I/1/B/g \\
                                         -S/1/C/g -n 6 t0018.in)
\S[300]\s[200]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
