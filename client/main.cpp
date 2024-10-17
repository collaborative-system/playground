#define FUSE_USE_VERSION 31
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "../common/io.h"
#include "../common/log.h"
#include "../proto/models.pb.h"
#include <absl/strings/str_format.h>
#include <condition_variable>
#include <cstdio>
#include <fuse3/fuse.h>
#include <fuse3/fuse_log.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>
#include <iostream>
#include <map>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

std::map<int, std::string> messages;
std::map<int, std::condition_variable> conditions;
std::map<int, std::mutex> mutexes;
int requst_id = 0;

template <typename T> int response_handler(int sock, int id, T message) {
  (void)sock;
  std::unique_lock<std::mutex> lock(mutexes[id]);
  messages[id] = message.SerializeAsString();
  conditions[id].notify_all();
  lock.unlock();
  return 0;
}

template <typename T> int request_handler(int sock, int id, T message) {
  (void)sock;
  (void)id;
  (void)message;
  return 0;
}

static recv_handlers handlers{
    .init_request = request_handler<models::InitRequest>,
    .init_response = response_handler<models::InitResponse>,
    .getattr_request = request_handler<models::GetattrRequest>,
    .getattr_response = response_handler<models::GetattrResponse>,
    .readdir_request = request_handler<models::ReaddirRequest>,
    .readdir_response = response_handler<models::ReaddirResponse>,
    .open_request = request_handler<models::OpenRequest>,
    .open_response = response_handler<models::OpenResponse>,
    .release_request = request_handler<models::ReleaseRequest>,
    .release_response = response_handler<models::ReleaseResponse>,
    .read_request = request_handler<models::ReadRequest>,
    .read_response = response_handler<models::ReadResponse>,
    .write_request = request_handler<models::WriteRequest>,
    .write_response = response_handler<models::WriteResponse>,
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
      exit(0);
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
  int id = requst_id++;
  int err =
      send_packet(sock, id, models::PacketType::INIT_REQUEST, &init_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return NULL;
  }
  std::unique_lock<std::mutex> lock(mutexes[id]);
  while (messages.find(id) == messages.end()) {
    conditions[id].wait(lock);
  }
  models::InitResponse *init_response = new models::InitResponse();
  bool error = init_response->ParseFromString(messages[id]);
  if (!error) {
    log(ERROR, sock, "Error parsing init response");
    return NULL;
  }
  log(INFO, sock, "Received init response: %s", init_response->error());
  return NULL;
}

static int getattr(const char *path, struct stat *stbuf,
                   struct fuse_file_info *fi) {
  log(INFO, sock, "Getting attributes for path %s", path);
  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    return 0;
  }
  models::GetattrRequest get_attr_request;
  get_attr_request.set_path(path);
  int id = requst_id++;
  int err = send_packet(sock, id, models::PacketType::GETATTR_REQUEST,
                        &get_attr_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return -EBUSY;
  }
  std::unique_lock<std::mutex> lock(mutexes[id]);
  while (messages.find(id) == messages.end()) {
    conditions[id].wait(lock);
  }
  models::GetattrResponse *get_attr_response = new models::GetattrResponse();
  bool error = get_attr_response->ParseFromString(messages[id]);
  if (!error) {
    log(ERROR, sock, "Error parsing getattr response");
    return -EBUSY;
  }
  lock.unlock();
  log(INFO, sock, "Received getattr response for path %s", path);
  int ret = get_attr_response->error();
  if (ret < 0) {
    return ret;
  }
  stbuf->st_mode = get_attr_response->stbuf().mode();
  stbuf->st_size = get_attr_response->stbuf().size();
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_mtime = get_attr_response->stbuf().mtime();
  messages.erase(id);
  mutexes.erase(id);
  conditions.erase(id);
  return ret;
}

static int readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                   off_t offset, struct fuse_file_info *fi,
                   enum fuse_readdir_flags flags) {
  log(INFO, sock, "Reading directory %s", path);
  models::ReaddirRequest readdir_request;
  readdir_request.set_path(path);
  int id = requst_id++;
  int err = send_packet(sock, id, models::PacketType::READDIR_REQUEST,
                        &readdir_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return -EBUSY;
  }
  std::unique_lock<std::mutex> lock(mutexes[id]);
  while (messages.find(id) == messages.end()) {
    conditions[id].wait(lock);
  }
  models::ReaddirResponse *readdir_response = new models::ReaddirResponse();
  bool error = readdir_response->ParseFromString(messages[id]);
  if (!error) {
    log(ERROR, sock, "Error parsing readdir response");
    return -EBUSY;
  }
  lock.unlock();
  log(INFO, sock, "Received readdir response for path %s", path);
  int ret = readdir_response->error();
  if (ret < 0) {
    return ret;
  }

  google::protobuf::RepeatedPtrField<std::string> *entries =
      readdir_response->mutable_entries();
  for (int i = 0; i < entries->size(); i++) {
    std::string name = entries->Get(i);
    std::cout << name << std::endl;
    log(INFO, sock, "Adding entry %s", name.c_str());
    filler(buf, name.c_str(), NULL, 0, FUSE_FILL_DIR_PLUS);
  }
  return 0;
}

static int write_(const char *path, const char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi) {
  log(INFO, sock, "Writing to path %d", path);
  models::WriteRequest write_request;
  write_request.set_path(path);
  write_request.set_offset(offset);
  write_request.set_data(buf);
  int id = requst_id++;
  int err =
      send_packet(sock, id, models::PacketType::WRITE_REQUEST, &write_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return -EBUSY;
  }
  std::unique_lock<std::mutex> lock(mutexes[id]);
  while (messages.find(id) == messages.end()) {
    conditions[id].wait(lock);
  }
  models::WriteResponse *write_response = new models::WriteResponse();
  bool error = write_response->ParseFromString(messages[id]);
  if (!error) {
    log(ERROR, sock, "Error parsing write response");
    return -EBUSY;
  }
  lock.unlock();
  log(INFO, sock, "Received write response for path %s", path);
  int ret = write_response->error();
  if (ret < 0) {
    return ret;
  }
  return size;
}

static int read_(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi) {
  log(INFO, sock, "Reading from path %s", path);
  models::ReadRequest read_request;
  read_request.set_path(path);
  log(DEBUG, sock, "Offset %d", offset);
  read_request.set_offset(offset);
  read_request.set_size(size);
  int id = requst_id++;
  log(DEBUG, sock, "Read request %s", read_request.DebugString().c_str());
  int err =
      send_packet(sock, id, models::PacketType::READ_REQUEST, &read_request);
  if (err < 0) {
    log(ERROR, sock, "Error sending packet");
    return -EBUSY;
  }
  std::unique_lock<std::mutex> lock(mutexes[id]);
  while (messages.find(id) == messages.end()) {
    conditions[id].wait(lock);
  }
  models::ReadResponse *read_response = new models::ReadResponse();
  bool error = read_response->ParseFromString(messages[id]);
  if (!error) {
    log(ERROR, sock, "Error parsing read response");
    return -EBUSY;
  }
  lock.unlock();
  log(INFO, sock, "Received read response for path %s", path);
  int ret = read_response->error();
  if (ret < 0) {
    return ret;
  }
  std::string data = read_response->data();
  memcpy(buf, data.c_str(), data.size());
  return data.size();
}

static void destroy(void *private_data) {
  (void)private_data;
  close(sock);
  recv_th.join();
  google::protobuf::ShutdownProtobufLibrary();
}

static struct fuse_operations fuse_opr = {
    .getattr = getattr,
    .read = read_,
    .write = write_,
    .readdir = readdir,
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
