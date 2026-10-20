\S[300]\s[80]OUT=$(smenu -n 20 t0020.in)||exit 1
\S[300]\s[300]^ab\CZ\r\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
