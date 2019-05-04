LEX = lex
YACC = yacc
CC = gcc -g

bwsh: obj/main.o obj/y.tab.o obj/lex.yy.o
	$(CC) obj/main.o obj/y.tab.o obj/lex.yy.o -o bwsh -lreadline

obj/y.tab.o: src/y.tab.c src/y.tab.h
	$(YACC) -d src/shell.y -v -o src/y.tab.c
	$(LEX) -o src/lex.yy.c src/shell.l 
	$(CC) -c src/y.tab.c -o obj/y.tab.o 

obj/lex.yy.o: src/lex.yy.c
	$(CC) -c src/lex.yy.c -o obj/lex.yy.o 

obj/main.o: src/main.c
	$(CC) -c src/main.c -o obj/main.o

clean:
	rm src/y.output obj/main.o obj/y.tab.o obj/lex.yy.o