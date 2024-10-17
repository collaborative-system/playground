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
  int (*getattr_request)(int sock, int id, models::GetattrRequest getattr_request);
  int (*getattr_response)(int sock, int id, models::GetattrResponse getattr_response);
  int (*readdir_request)(int sock, int id, models::ReaddirRequest readdir_request);
  int (*readdir_response)(int sock, int id, models::ReaddirResponse readdir_response);
  int (*open_request)(int sock, int id, models::OpenRequest open_request);
  int (*open_response)(int sock, int id, models::OpenResponse open_response);
  int (*release_request)(int sock, int id, models::ReleaseRequest release_request);
  int (*release_response)(int sock, int id, models::ReleaseResponse release_response);
  int (*read_request)(int sock, int id, models::ReadRequest read_request);
  int (*read_response)(int sock, int id, models::ReadResponse read_response);
  int (*write_request)(int sock, int id, models::WriteRequest write_request);
  int (*write_response)(int sock, int id, models::WriteResponse write_response);
};

int handle_recv(int sock, recv_handlers &handlers);

int full_read(int fd, char *buf, int size);
