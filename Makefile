CC := g++
CFLAGS := -g -lfuse3 -Wall -Wextra -pedantic -std=c++20 `pkg-config --cflags --libs protobuf` -pthread
COMMON := common/io.cpp common/header.cpp common/log.cpp proto/models.pb.cc

all: build

.PHONY: build
build: client server

.PHONY: clean
clean: clean-client clean-server

.PHONY: clean-all
clean-all: clean clean-proto

.PHONY: client-run
client-run: client
	./client/client -f test/

.PHONY: client
client: client/client

client/client: proto/models.pb.cc proto/models.pb.h
	$(CC) $(CFLAGS) -o $@ client/main.cpp $(COMMON)

.PHONY: clean-client
clean-client:
	rm -f client/client

.PHONY: server-run
server-run: server
	./server/server ~/name

.PHONY: server
server: server/server

server/server: proto/models.pb.cc proto/models.pb.h
	$(CC) $(CFLAGS) -o $@ server/main.cpp $(COMMON)

.PHONY: clean-server
clean-server:
	rm -f server/server

.PHONY: proto
proto: proto/models.pb.cc proto/models.pb.h

proto/models.pb.cc proto/models.pb.h:
	protoc -I=proto/ --cpp_out=proto/ proto/models.proto
	$(CC) $(CFLAGS) -c proto/models.pb.cc -o proto/models.pb.o

.PHONY: clean-proto
clean-proto:
	rm -f proto/models.pb.cc proto/models.pb.h proto/models.pb.o

