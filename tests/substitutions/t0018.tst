\S[10]OUT=$(LC_ALL=en_US.UTF-8 smenu -c -i 2 -E/1/A/g -I/1/B/g \\
                                         -S/1/C/g -n 6 t0018.in)
\S[10]\s[120]\r
\S[10]\s[10]echo ":$OUT:"
exit 0
