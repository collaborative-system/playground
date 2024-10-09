#pragma once
#include "../proto/models.pb.h"
#include "header.h"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <google/protobuf/message.h>

int send_packet(int sock, int id, models::PacketType type,
                google::protobuf::Message *message);

struct recv_handlers {
  int (*init_request)(int sock, int id, models::InitRequest init_request);
  int (*init_response)(int sock, int id, models::InitResponse init_response);
};

struct reqs_handlers {
  int (*init_request)(int sock, int id, models::InitRequest init_request);
  int (response_handler)(int sock, int id, google::protobuf::Message &message);
};

int handle_recv(int sock, reqs_handlers &handlers);

int handle_recv(int sock, recv_handlers &handlers);

int full_read(int fd, char *buf, int size);
