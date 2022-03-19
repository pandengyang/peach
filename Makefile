peach: main.c module/peach.h
	gcc -o peach main.c -I./module

.PHONY: clean

clean:
	rm -rf peach
