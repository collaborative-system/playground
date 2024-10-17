#include <google/protobuf/timestamp.pb.h>
#include <list>
#include <string>
#include <sys/stat.h>
#define FUSE_USE_VERSION 31
#include "../common/io.h"
#include "../common/log.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <vector>

std::string working_dir;
std::map<std::string, int> path_to_fd;

template <typename T> int response_handler(int sock, int id, T message) {
  (void)sock;
  (void)id;
  (void)message;
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

int getattr_request(int sock, int id, models::GetattrRequest request) {
  struct stat st;
  int err = stat((working_dir + request.path()).c_str(), &st);
  models::GetattrResponse response;
  if (err >= 0) {
    response.mutable_stbuf()->set_mtime(st.st_mtime);
    response.mutable_stbuf()->set_size(st.st_size);
    response.mutable_stbuf()->set_mode(st.st_mode);
  }
  response.set_error(err);
  err = send_packet(sock, id, models::GETATTR_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending getattr response");
    return err;
  }
  return 0;
}

int readdir_request(int sock, int id, models::ReaddirRequest request) {
  DIR *dir = opendir((working_dir + request.path()).c_str());
  log(DEBUG, sock, "Readdir request for %s",
      (working_dir + request.path()).c_str());
  struct dirent *entry;
  std::list<std::string> entries;
  models::ReaddirResponse response;
  if (dir != nullptr) {
    while ((entry = readdir(dir)) != nullptr) {
      response.add_entries(entry->d_name);
    }
    response.set_error(1);
  } else {
    response.set_error(0);
  }
  int err = send_packet(sock, id, models::READDIR_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending readdir response");
    return err;
  }
  return 0;
}

int open_request(int sock, int id, models::OpenRequest request) {
  int fd = open((working_dir + request.path()).c_str(), O_RDWR);
  models::OpenResponse response;
  if (fd >= 0) {
    path_to_fd[request.path()] = fd;
    response.set_error(0);
  } else {
    response.set_error(-1);
  }
  int err = send_packet(sock, id, models::OPEN_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending open response");
    return err;
  }
  return 0;
}

int release_request(int sock, int id, models::ReleaseRequest request) {
  int fd = path_to_fd[request.path()];
  int err = close(fd);
  models::ReleaseResponse response;
  if (err >= 0) {
    response.set_error(0);
  } else {
    response.set_error(-1);
  }
  err = send_packet(sock, id, models::RELEASE_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending release response");
    return err;
  }
  return 0;
}

int read_request(int sock, int id, models::ReadRequest request) {
  int fd = path_to_fd[request.path()];
  char *buf = new char[request.size()];
  log(DEBUG, sock, "Read request for %s", request.offset());
  int offset = request.offset();
  printf("offset: %d\n", offset);
  int err = pread(fd, buf, request.size(), request.offset());
  models::ReadResponse response;
  if (err >= 0) {
    response.set_data(buf, err);
    response.set_error(0);
  } else {
    perror("pread");
    response.set_error(-1);
  }
  log(DEBUG, sock, "Read request for %s", request.path().c_str());
  err = send_packet(sock, id, models::READ_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending read response");
    return err;
  }
  return 0;
}

int write_request(int sock, int id, models::WriteRequest request) {
  int fd = path_to_fd[request.path()];
  int err = pwrite(fd, request.data().c_str(), request.data().size(),
                   request.offset());
  models::WriteResponse response;
  if (err >= 0) {
    response.set_error(0);
  } else {
    response.set_error(-1);
  }
  err = send_packet(sock, id, models::WRITE_RESPONSE, &response);
  if (err < 0) {
    log(ERROR, sock, "Error sending write response");
    return err;
  }
  return 0;
}

recv_handlers handlers{
    .init_request = init_request,
    .init_response = response_handler<models::InitResponse>,
    .getattr_request = getattr_request,
    .getattr_response = response_handler<models::GetattrResponse>,
    .readdir_request = readdir_request,
    .readdir_response = response_handler<models::ReaddirResponse>,
    .open_request = open_request,
    .open_response = response_handler<models::OpenResponse>,
    .release_request = release_request,
    .release_response = response_handler<models::ReleaseResponse>,
    .read_request = read_request,
    .read_response = response_handler<models::ReadResponse>,
    .write_request = write_request,
    .write_response = response_handler<models::WriteResponse>};

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

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <working_dir>\n", argv[0]);
    return 1;
  }
  working_dir = argv[1];
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
    log(ERROR, sock, "Error bind port: %s", strerror(errno));
    return 1;
  }
  constexpr int one = 1;
  int err = listen(sock, 10);
  if (err < 0) {
    log(ERROR, sock, "Error listening: %s", strerror(errno));
    return 1;
  }
  log(INFO, sock, "Listening on port %d", port);
  signal(SIGINT, signal_handler);
  err = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  if (err < 0) {
    log(ERROR, sock, "Error setting socket options: %s", strerror(errno));
    return 1;
  }
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
