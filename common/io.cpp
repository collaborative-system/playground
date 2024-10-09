#include "io.h"
#include "../proto/models.pb.h"
#include "log.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <google/protobuf/message.h>

int send_packet(int sock, int id, models::PacketType type,
                google::protobuf::Message *message) {
  Header header;
  header.size = message->ByteSizeLong();
  header.id = id;
  header.type = type;
  char *buffer = serialize(&header);
  int len = send(sock, buffer, sizeof(Header), 0);
  log(DEBUG, sock, "(%d) Send header success: %d bytes", id, len);
  delete[] buffer;
  if (len < 0) {
    log(DEBUG, sock, "(%d) Send header failed: %s", id, strerror(errno));
    return -1;
  }
  buffer = new char[message->ByteSizeLong()];
  bool err = message->SerializeToArray(buffer, message->ByteSizeLong());
  if (!err) {
    log(DEBUG, sock, "(%d) Serialize message failed", id);
    return -1;
  }
  len = send(sock, buffer, message->ByteSizeLong(), 0);
  delete[] buffer;
  if (len < 0) {
    log(DEBUG, sock, "(%d) Send message failed: %s", id, strerror(errno));
    return -1;
  }
  std::string debug = message->DebugString();
  debug.pop_back();
  log(DEBUG, sock, "(%d) Send message success: %s - %d bytes", id,
      debug.c_str(), len);
  return len;
}

int full_read(int fd, char *buf, int size) {
  int recived = 0;
  while (recived < size) {
    int len = recv(fd, buf + recived, size - recived, 0);
    if (len == 0) {
      log(DEBUG, fd, "EOF: %s", strerror(errno));
      return 0;
    }
    if (len < 0) {
      log(DEBUG, fd, "Full read failed: %s", strerror(errno));
      return -1;
    }
    recived += len;
  }
  return recived;
}

int handle_recv(int sock, recv_handlers &handlers) {
  Header *header;
  char buffer[sizeof(Header)];
  int recived = full_read(sock, buffer, sizeof(buffer));
  if (recived == 0) {
    log(DEBUG, sock, "EOF: %s", strerror(errno));
    return -1;
  }
  if (recived < (int)sizeof(buffer)) {
    log(DEBUG, sock, "Full header read failed");
    return -2;
  }
  header = deserialize(buffer);
  log(DEBUG, sock, "Received header: size %d id %d type %d %d bytes",
      header->size, header->id, header->type, recived);
  char *recv_buffer = new char[header->size];
  recived = full_read(sock, recv_buffer, header->size);
  if (recived == 0 && header->size != 0) {
    return -1;
  }
  if (recived < header->size) {
    return -2;
  }
  int ret = -2;
  switch (header->type) {
  case models::PacketType::INIT_REQUEST: {
    models::InitRequest request;
    bool err = request.ParseFromArray(recv_buffer, header->size);
    if (!err) {
      ret = -2;
    } else {
      std::string debug = request.DebugString();
      debug.pop_back();
      log(DEBUG, sock, "(%d) Received InitRequest: %s", header->id,
          debug.c_str());
      ret = handlers.init_request(sock, header->id, request);
    }
    break;
  }
  case models::PacketType::INIT_RESPONSE: {
    models::InitResponse response;
    bool err = response.ParseFromArray(recv_buffer, header->size);
    if (!err) {
      return -2;
    } else {
      std::string debug = response.DebugString();
      debug.pop_back();
      log(DEBUG, sock, "(%d) Received InitResponse: %s", header->id,
          debug.c_str());
      ret = handlers.init_response(sock, header->id, response);
    }
    break;
  }
  default:
    log(DEBUG, sock, "(%d) Unknown packet type: %d", header->id, header->type);
    break;
  }

  if (ret < 0) {
    log(DEBUG, sock, "Handler failed: %d", ret);
  } else {
    log(DEBUG, sock, "Handler success: %d", ret);
  }
  delete header;
  delete[] recv_buffer;
  return ret;
};

