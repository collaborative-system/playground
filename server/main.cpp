#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include "../common/objects.h"

void client_handler(int fd) {
  int len;
  while (true) {
    Header header;
    len = get_header(fd, header);
    if (len < 0) {
      close(fd);
      return;
    }
    switch (header.type) {
      case models::PacketType::INIT_REQUEST: {
        models::InitRequest init_request;
        char *recv_buffer = new char[header.size];
        len = full_read(fd, recv_buffer, header.size);
        if (len < header.size) {
          close(fd);
          return;
        }
        bool err = init_request.ParseFromArray(recv_buffer, header.size);
        if (!err) {
          close(fd);
          return;
        }
        printf("Received: %s\n", init_request.DebugString().c_str());
        models::InitResponse init_response;
        init_response.set_error(0);
        len = send_packet(fd, header.id, models::PacketType::INIT_RESPONSE, &init_response);
        if (len < 0) {
          close(fd);
          return;
        }
        break;
      }
    }
  }
}

int main() {
  int port = 7400;
  const int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return 1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }
  constexpr int one = 1;
  int err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (err < 0) {
    perror("setsockopt");
    return 1;
  }
  err = listen(sock, 10);
  if (err < 0) {
    perror("listen");
    return 1;
  }
  printf("Listening on port %d\n", port);
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  std::vector<std::thread> threads;
  while (true) {
    const int client_sock =
        accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sock < 0) {
      perror("accept");
      return 1;
    }
    std::thread t(client_handler, client_sock);
    threads.push_back(std::move(t));
  }
  for (auto &t : threads) {
    t.join();
  }
  return 0;
};
