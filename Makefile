CFLAGS=-O3 --std=gnu99 -Wall
.PHONY: target grind push

target: grizzly suds

grizzly: grizzly.c dlx.c
	$(CC) $(CFLAGS) -o $@ $^ -I ../blt ../blt/blt.c

suds: suds.c dlx.c
	$(CC) $(CFLAGS) -o $@ $^ -I ../blt ../blt/blt.c

dlx_test: dlx_test.c dlx.c
	cc -g --std=gnu99 -Wall -o $@ $^

grind: dlx_test
	valgrind ./dlx_test

push:
	git push git@github.com:blynn/dlx.git master
	git push https://code.google.com/p/blynn-dlx/ master
