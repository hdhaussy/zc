LDLIBS=-lzmq
EXE=zc

all: $(EXE)

clean:
	rm -f $(EXE)