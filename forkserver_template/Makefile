all:
	# compiling forkserver_template ...
	$(CC) -o forkserver_template forkserver_template.c

clean:
	rm forkserver_template CRASH_* stream.out testcase

test: all
	# testing forkserver_template ...
	for number in 0 1 2 3 4 5 6 7 8 9; do \
        echo $$number >corpus/$$number; \
    done
	./forkserver_template