OBJECTS = pingpong.o nested.o test_poll.o coroutine.o switch.o

all: pingpong nested test_poll
	
pingpong: pingpong.o coroutine.o switch.o
	gcc -o pingpong pingpong.o coroutine.o switch.o
nested: nested.o coroutine.o switch.o
	gcc -o nested nested.o coroutine.o switch.o
test_poll: test_poll.o coroutine.o switch.o
	gcc -o test_poll test_poll.o coroutine.o switch.o
coroutine.o: coroutine.c coroutine.h
	gcc -c -ggdb -std=c99 -o coroutine.o coroutine.c
switch.o: switch-amd64.S
	as -ggdb -o switch.o switch-amd64.S
pingpong.o: pingpong.c coroutine.h
	gcc -c -ggdb -std=c99 -o pingpong.o pingpong.c
nested.o: nested.c coroutine.h
	gcc -c -ggdb -std=c99 -o nested.o nested.c
test_poll.o: test_poll.c coroutine.h
	gcc -c -ggdb -std=c99 -o test_poll.o test_poll.c
	
clean:
	rm -f pingpong nested test_poll $(OBJECTS)
