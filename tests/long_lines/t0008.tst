\S[300]\s[80]OUT=$(TERM=vt100 LC_ALL=C smenu -c -n 4 t0007.in)||exit 1
\S[300]\s[300]hhhhhljjhhjjhhkkhhkkhhjjllkklljjhhhhhjjhhjjllkkhhkkhhjjhhhhhkkhhhhjjjhhhhhh\
\W[75x24]\S[2000]\W[45x24]\S[2000]\W[55x24]\S[2000]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
