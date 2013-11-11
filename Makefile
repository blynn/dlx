CFLAGS=-O3 --std=gnu99 -Wall

grizzly: grizzly.c dlx.c
	$(CC) $(CFLAGS) -o $@ $^ -I ../blt ../blt/blt.c

suds: suds.c

dlx_test: dlx_test.c dlx.c
	cc -g --std=gnu99 -Wall -o $@ $^

grind: dlx_test
	valgrind ./dlx_test
