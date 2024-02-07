\S[300]\s[80]OUT=$(TERM=vt100 LANG=C smenu -c -n 4 t0007.in)
\S[300]\s[200]hhhhhljjhhjjhhkkhhkkhhjjllkklljjhhhhhjjhhjjllkkhhkkhhjjhhhhhkkhhhhjjjhhhhhh\
\W[75x24]\S[2000]\W[45x24]\S[2000]\W[55x24]\S[2000]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
