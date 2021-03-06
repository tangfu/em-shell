
ALL:	
	@-astyle -n --style=linux --mode=c --pad-oper --pad-paren-in --unpad-paren --break-blocks --delete-empty-lines --min-conditional-indent=0 --max-instatement-indent=80 --indent-col1-comments --indent-switches --lineend=linux *.{c,h} >/dev/null
	@gcc -g3 -Wall -Wextra -Wunused -Wunused-parameter -c em-shell.c
	@ar rc libemshell.a em-shell.o
	@-rm *.o
#	@gcc shell.c -fPIC -shared -o libshell.so
	@make -C test
	@make -C example
#produce document
	@doxygen

release:	em-shell.c em-shell.h
	@gcc -Wall -Wextra -Wunused -Wunused-parameter -c em-shell.c
	@ar rc libemshell.a em-shell.o
	@-rm *.o

clean:
	@if [ -f libshell.a ];then \
		rm libemshell.a test/test example/example 2>/dev/null; \
		rm -rf doc/html/* 2>/dev/null; \
	fi
