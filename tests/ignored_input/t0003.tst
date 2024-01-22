\S[300]\s[80]OUT=$(smenu -zap 02 t0003.in|od -c|tr '\\\\' '/'|tr -d ' ')
\S[300]\s[200]lll\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
