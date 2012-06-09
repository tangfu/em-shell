ALL:
	@-astyle --style=linux --mode=c --pad-oper --pad-paren-in --unpad-paren --break-blocks --delete-empty-lines --min-conditional-indent=0 --max-instatement-indent=80 --indent-col1-comments --indent-switches --lineend=linux *.c >/dev/null
	@gcc -o test -g3 -Wall test.c -I.. -I/usr/include/google -L.. -lcmockery -lshell -lpthread
