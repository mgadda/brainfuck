all: run

bfc: build_dir format
	c++ ./bf/src/bfc/bfc.c \
		-I/opt/twitter/opt/llvm/include \
		-L/opt/twitter/opt/llvm/lib \
		`/opt/twitter/opt/llvm/bin/llvm-config --ldflags --libs core executionengine jit interpreter analysis native bitwriter --system-libs` \
		`/opt/twitter/opt/llvm/bin/llvm-config --cxxflags` \
		-lLLVMCore \
		-g -Wall -o ./build/bfc

bfi: build_dir format
	gcc ./bf/src/bfi/bfi.c -g -Wall -o ./build/bfi

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
