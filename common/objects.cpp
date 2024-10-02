#include "objects.h"
#include "../proto/models.pb.h"
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <google/protobuf/message.h>

char *serialize(Header *header) {
  char *buf = new char[sizeof(Header)];
  uint32_t nsize = htonl(header->size);
  uint32_t nid = htonl(header->id);
  uint16_t ntype = htons(header->type);
  memcpy(buf, &nsize, sizeof(int32_t));
  memcpy(buf + sizeof(int32_t), &nid, sizeof(int32_t));
  memcpy(buf + sizeof(int32_t) + sizeof(int32_t), &ntype, sizeof(int16_t));
  return buf;
}

Header *deserialize(char *buffer) {
  Header *header = new Header;
  uint32_t nsize;
  uint32_t nid;
  uint16_t ntype;
  memcpy(&nsize, buffer, sizeof(int32_t));
  memcpy(&nid, buffer + sizeof(int32_t), sizeof(int32_t));
  memcpy(&ntype, buffer + sizeof(int32_t) + sizeof(int32_t), sizeof(int16_t));
  header->size = ntohl(nsize);
  header->id = ntohl(nid);
  header->type = ntohs(ntype);
  return header;
}

int full_read(int fd, char *buf, int size) {
  int recived = 0;
  while (recived < size) {
    int len = recv(fd, buf + recived, size - recived, 0);
    if (len == 0) {
      return 0;
    }
    if (len < 0) {
      perror("recv");
      return -1;
    }
    recived += len;
  }
  return recived;
}

int send_packet(int sock, int id, models::PacketType type,
                google::protobuf::Message *message) {
  Header header;
  header.size = message->ByteSizeLong();
  header.id = id;
  header.type = type;
  char *buffer = serialize(&header);
  int len = send(sock, buffer, sizeof(Header), 0);
  delete[] buffer;
  if (len < 0) {
    perror("send");
    return -1;
  }
  buffer = new char[message->ByteSizeLong()];
  bool err = message->SerializeToArray(buffer, message->ByteSizeLong());
  if (!err) {
    printf("Error serializing message\n");
    return -1;
  }
  printf("Sending: %s\n", message->DebugString().c_str());
  len = send(sock, buffer, message->ByteSizeLong(), 0);
  delete[] buffer;
  if (len < 0) {
    perror("send");
    return -1;
  }
  printf("Sent: %d\n", len);
  return len;
}

int get_header(int sock, Header &header) {
  char buffer[sizeof(Header)];
  int recived = full_read(sock, buffer, sizeof(buffer));
  if (recived <= 0) {
    return recived;
  }
  header = *deserialize(buffer);
  return recived;
}
