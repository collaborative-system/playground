#define FUSE_USE_VERSION 31
#include "../common/io.h"
#include "../common/log.h"
#include "../proto/models.pb.h"
#include <condition_variable>
#include <cstdio>
#include <fuse3/fuse.h>
#include <fuse3/fuse_log.h>
#include <google/protobuf/message.h>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

std::map<int, google::protobuf::Message *> messages;
std::map<int, std::condition_variable> conditions;
std::map<int, std::mutex> mutexes;

template <typename T> int response_handler(int sock, int id, T message) {
  (void)sock;
  std::unique_lock<std::mutex> lock(mutexes[id]);
  messages[id] = &message;
  conditions[id].notify_all();
  lock.unlock();
  return 0;
}

int init_request(int sock, int id, models::InitRequest init_request) {
  (void)sock;
  (void)id;
  (void)init_request;
  return 0;
}

static recv_handlers handlers{
    .init_request = init_request,
    .init_response = response_handler<models::InitResponse>,
};

int recv_thread(int sock) {
  while (true) {
    int err = handle_recv(sock, handlers);
    if (err < -1) {
      log(ERROR, sock, "Error handling packet");
    }
    if (err < 0) {
      close(sock);
      log(INFO, sock, "Closing connection");
      return -1;
    }
  }
  return 0;
}

int sock;
std::thread recv_th;

static void *init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  (void)conn;
  cfg->kernel_cache = 1;
  models::InitRequest init_request;
  init_request.set_project_name("Project");
  int err =
      send_packet(sock, 1, models::PacketType::INIT_REQUEST, &init_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return NULL;
  }
  std::unique_lock<std::mutex> lock(mutexes[1]);
  while (messages.find(1) == messages.end()) {
    conditions[1].wait(lock);
  }
  models::InitResponse *init_response =
      static_cast<models::InitResponse *>(messages[1]);
  log(INFO, sock, "Received init response: %s", init_response->error());
  return NULL;
}

static void destroy(void *private_data) {
  (void)private_data;
  close(sock);
  recv_th.join();
  google::protobuf::ShutdownProtobufLibrary();
}

static struct fuse_operations fuse_opr = {
  .init = init,
  .destroy = destroy,
};

int main(int argc, char *argv[]) {
  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  int port = 7400;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    log(ERROR, sock, "Error creating socket: %s", strerror(errno));
    return 1;
  }
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    log(ERROR, sock, "Error connecting to port %d: %s", port, strerror(errno));
    return 1;
  }
  log(INFO, sock, "Connected to port %d", port);

  std::thread tmp(recv_thread, sock);
  recv_th.swap(tmp);
  ret = fuse_main(args.argc, args.argv, &fuse_opr, NULL);
  fuse_opt_free_args(&args);
  recv_th.detach();
  close(sock);
  google::protobuf::ShutdownProtobufLibrary();
  return ret;
}
