all:
	g++ -o bin/1_example src/example_1.c lib/libcolib.a  -I./include -lpthread -g
