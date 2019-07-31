
all:
	$(CC) -o fork_harness fork_harness.c
	$(CC) -lasan -O -g -fsanitize=address -fno-omit-frame-pointer -o oob oob.c
	$(CC) -o hog hog.c
	$(CC) -o crash crash.c

clean:
	rm fork_harness crash hog oob child_err_out_redir

test: all
	# "TEST 1: crash segment fault"
	./fork_harness ./crash && cat ./child_err_out_redir
	# "TEST 2: crash hog 
	./fork_harness ./hog &
	sleep 2 
	killall hog 
	cat ./child_err_out_redir
	# "TEST 3: crash oob asan
	./fork_harness ./oob && cat ./child_err_out_redir