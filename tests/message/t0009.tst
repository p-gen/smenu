\S[300]\s[80]OUT=$(smenu -a m:7/5 -M \\
-m '=\\uefbcb4\\[0]\\uefbca5\\uefbcb3\\[!]\\[7/5]\\uefbcb4=\n=test=' t0009.in)||exit 1
\S[300]\s[300]\r
\S[300]\s[80]echo ":$\s[80]OUT:"
exit 0
