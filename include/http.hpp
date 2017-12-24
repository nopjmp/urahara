#ifndef HTTP_HPP
#define HTTP_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "uvw.hpp"

namespace urahara {
using namespace std;
constexpr size_t MAX_METHOD_LEN = 255;
constexpr size_t MAX_PATH_LEN = 4096;
constexpr size_t MAX_REQUEST_LEN = 10 * 1024 * 1024; // 10MB

using http_headers = unordered_map<string, string>;

struct http_data {
  string method;
  string path;
  string version;
  http_headers headers;
  string body;
  long long body_length;
};

struct http_response {
  int status_code;
  string status_msg;
  string content;
  http_headers headers;

  http_response() : status_code(200), status_msg("OK") {}

  void set_status(int code, string msg) {
    status_code = code;
    status_msg = msg;
  }

  void set_content_type(const char *type) {
    headers.insert_or_assign("Content-Type", type);
  }

  void set_content(const char *new_content) {
    content.assign(new_content);
    headers.insert_or_assign("Content-Length", to_string(content.size()));
  }

  void set_header(const char *name, const char *value) {
    headers.insert_or_assign(name, value);
  }

  string build();
};

struct http_exception : public exception {
  int status;
  const char *error_msg;
  http_exception() : status(500), error_msg("Internal Server Error") {}
  http_exception(int status, const char *error_msg)
      : status(status), error_msg(error_msg) {}

  http_response as_http();
};

struct http_connection {
  enum internal_state { HEADER, BODY, CHUNKED_BODY };

  using callback_fn =
      std::function<void(http_connection *, uvw::TcpHandle &, http_data)>;

  vector<char> buffer;
  size_t last_pos = 0;

  internal_state state = HEADER;
  http_data result;

  callback_fn callback;

  http_connection(callback_fn callback) : callback(callback) {
    buffer.reserve(4096);
  }

  ~http_connection();

  void reset() {
    result = {};
    state = HEADER;
    buffer.clear();
  }

  string_view parse();

  // events
  void on_data(const uvw::DataEvent &, uvw::TcpHandle &client);
  void on_start(uvw::TcpHandle &client);
  void on_body(uvw::TcpHandle &client);
  void on_chunked(uvw::TcpHandle &client);
  void on_close();

  // helper functions
  bool is_complete();
};

struct http {
  string addr;
  unsigned int port;
  shared_ptr<uvw::TcpHandle> tcp_handle;
  shared_ptr<uvw::Loop> loop;

  http(string addr, unsigned int port);
  ~http();

  void run(http_connection::callback_fn func);
};

}; // namespace urahara

#endif