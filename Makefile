all: run

bfc: build_dir format runtime
	c++ ./bf/src/bfc/bfc.c \
		-I/usr/local/opt/llvm/include \
		-L/usr/local/opt/llvm/lib \
		`/usr/local/opt/llvm/bin/llvm-config --ldflags --libs core executionengine interpreter analysis native bitwriter --system-libs` \
		`/usr/local/opt/llvm/bin/llvm-config --cxxflags` \
		-g -Wall -o ./build/bfc

bfi: build_dir format
	clang ./bf/src/bfi/bfi.c -g -Wall -o ./build/bfi

runtime:
	clang ./bf/src/runtime/runtime.c -c -o ./build/runtime.o
	ar rcs ./build/libbfr.a ./build/runtime.o

run: bfc
	./build/bfc ./samples/helloworld.bf

runi: bfi
	./build/bfi ./samples/helloworld.bf

build_dir:
	mkdir -p ./build

format:
	./cleanup-format -i *.c

clean:
	rm -rf ./bfi ./bfc *.o *.dSYM build/*
