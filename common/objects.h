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

int handle_recv(int sock, recv_handlers &handlers);
