#pragma once
#include <cstddef>
#include <cstdint>
#include <arpa/inet.h>
#include <cstring>
#include <google/protobuf/message.h>
#include "../proto/models.pb.h"

struct Header{
  int32_t size;
  int32_t id;
  int16_t type;
};

char* serialize(Header* header);

Header* deserialize(char* buffer);

int full_read(int fd, char *buf, int size);

int send_packet(int sock, int id, models::PacketType type, google::protobuf::Message* message);

int get_header(int sock, Header &header);
