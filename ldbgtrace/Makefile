all:
	g++ ldbgtrace.cc sym_reader.cc trace_writer.cc -o ldbgtrace

debug:
	g++ -ggdb ldbgtrace.cc sym_reader.cc trace_writer.cc -o ldbgtrace

asan:
	g++ -lasan -O -g -fsanitize=address -fno-omit-frame-pointer ldbgtrace.cc sym_reader.cc trace_writer.cc -o ldbgtrace

clean:
	rm ldbgtrace