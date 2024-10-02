#include "../common/objects.h"
#include "../proto/models.pb.h"
#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
  int port = 7400;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return 1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    return 1;
  }
  printf("Connected to port %d\n", port);

  models::InitRequest init_request;
  init_request.set_project_name("Project");
  int err =
      send_packet(sock, 1, models::PacketType::INIT_REQUEST, &init_request);
  if (err < 0) {
    printf("Error sending packet\n");
    return 1;
  }
  Header header;
  int recived = get_header(sock, header);
  if (recived < 0) {
    printf("Error reading header\n");
    close(sock);
    return 1;
  }
  printf("Received: %d %d %hd\n", header.id, header.size, header.type);
  models::InitResponse init_response;
  char *recv_buffer = new char[header.size];
  recived = full_read(sock, recv_buffer, header.size);
  if (recived < 0) {
    printf("Error reading from socket\n");
    return 1;
  }
  init_response.ParseFromArray(recv_buffer, header.size);
  printf("Received: %d\n", init_response.error());
  delete[] recv_buffer;
  close(sock);
  return 0;
}
