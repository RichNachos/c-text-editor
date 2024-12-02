warm: warm.c
	mkdir -p build
	$(CC) warm.c -o ./build/warm -Wall -Wextra -pedantic -std=c99
