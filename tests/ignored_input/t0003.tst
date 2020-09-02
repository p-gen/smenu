\S[150]\s[10]OUT=$(smenu -zap 02 t0003.in|od -c|tr '\\\\' '/'|tr -d ' ')
\S[150]\s[150]lll\r
\S[150]\s[10]echo ":$\s[10]OUT:"
exit 0
