#define FUSE_USE_VERSION 31
#include "../common/io.h"
#include "../common/log.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

int init_response(int sock, int id, models::InitResponse init_response) {
  (void)sock;
  (void)id;
  (void)init_response;
  return 0;
}

int init_request(int sock, int id, models::InitRequest init_request) {
  (void)init_request;
  models::InitResponse init_response = models::InitResponse();
  init_response.set_error(0);
  int len =
      send_packet(sock, id, models::PacketType::INIT_RESPONSE, &init_response);
  if (len < 0) {
    log(ERROR, sock, "Error sending init response");
    return -1;
  }
  return 0;
}

recv_handlers handlers{.init_request = init_request,
                       .init_response = init_response};

void client_handler(int fd) {
  while (true) {
    int err = handle_recv(fd, handlers);
    if (err < -1) {
      log(ERROR, fd, "Error handling packet");
    }
    if (err < 0) {
      log(INFO, fd, "Closing connection");
      close(fd);
      return;
    }
  }
}

std::vector<std::thread> threads;
int sock;

void signal_handler(int signum) {
  log(INFO, 0, "Caught signal %d", signum);
  close(sock);
  google::protobuf::ShutdownProtobufLibrary();
  for (auto &t : threads) {
    t.detach();
  }
  exit(signum);
}

int main() {
  int port = 7400;
  sock = socket(AF_INET, SOCK_STREAM, 0);
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
    log(ERROR, sock, "Error setting socket options: %s", strerror(errno));
    return 1;
  }
  err = listen(sock, 10);
  if (err < 0) {
    log(ERROR, sock, "Error listening: %s", strerror(errno));
    return 1;
  }
  log(INFO, sock, "Listening on port %d", port);
  signal(SIGINT, signal_handler);
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  while (true) {
    const int client_sock =
        accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_sock < 0) {
      log(ERROR, sock, "Error accepting connection: %s", strerror(errno));
      return 1;
    }
    log(INFO, client_sock, "Accepted connection from %s:%d",
        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    std::thread t(client_handler, client_sock);
    threads.push_back(std::move(t));
  }
  for (auto &t : threads) {
    t.join();
  }
  google::protobuf::ShutdownProtobufLibrary();
  return 0;
};
