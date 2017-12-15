#include "http.hpp"

#include <array>
#include <functional>
#include <iostream>
#include <sstream>
#include <string_view>
#include <tuple>

using namespace std;
using namespace urahara;

http::http(string addr, unsigned int port)
    : addr(addr), port(port), loop(uvw::Loop::getDefault()) {
  tcp_handle = loop->resource<uvw::TcpHandle>();
}
http::~http() {}

void http::run(http_connection::callback_fn func) {
  tcp_handle->on<uvw::ListenEvent>(
      [func](const uvw::ListenEvent &, uvw::TcpHandle &srv) {
        auto client = srv.loop().resource<uvw::TcpHandle>();
        client->data(make_shared<http_connection>(func));

        client->on<uvw::EndEvent>(
            [](const uvw::EndEvent &, uvw::TcpHandle &client) {
              auto http_conn = client.data<http_connection>();
              http_conn->on_close();

              client.close();
            });

        client->on<uvw::DataEvent>(
            [](const uvw::DataEvent &event, uvw::TcpHandle &client) {
              auto http_conn = client.data<http_connection>();
              try {
                http_conn->on_data(event, client);
              } catch (http_exception &e) {
                // TODO: print these?
                client.close();
              }
            });
        srv.accept(*client);
        client->read();
      });

  tcp_handle->bind(addr, port);
  tcp_handle->listen();

  cout << "Listening on " << addr << ":" << port << endl;
  loop->run();
}

// TODO: move these helpers into an internal namespace
static void check_valid(string str, bool is_ctl, bool is_special) {
  for (char &c : str) {
    if (c > 127) {
      throw http_exception{};
    }

    if (is_ctl && (c <= 31 || c == 127)) {
      throw http_exception{};
    }

    if (is_special) {
      switch (c) {
      case '(':
      case ')':
      case '<':
      case '>':
      case '@':
      case ',':
      case ';':
      case ':':
      case '\\':
      case '"':
      case '/':
      case '[':
      case ']':
      case '?':
      case '=':
      case '{':
      case '}':
      case ' ':
      case '\t':
        throw http_exception{};
      }
    }
  }
}

static int check_eol(string_view data, bool expect = true) {
  char c = data.at(0);
  if (c == '\r') {
    c = data.at(1);
    if (c == '\n') {
      return 2;
    }
  } else if (c == '\n') {
    return 1;
  }

  if (expect) {
    throw http_exception{};
  } else {
    return 0;
  }
}

// (eol index, size of end of line)
static tuple<size_t, int> find_eol(string_view data) {
  // this is fairly hot path and could use some help from SSE4.2 instructions
  size_t i;
  for (i = 0; i < data.length(); ++i) {
    char c = data[i];
    // allow HT
    if ((c < '\040' && c != '\011') || c == '\177') {
      break;
    }
  }

  if (data[i] == '\r') {
    ++i;
    if (data[i] == '\n') {
      return make_tuple(++i, 2);
    } else {
      throw http_exception{};
    }
  } else if (data[i] == '\n') {
    return make_tuple(++i, 1);
  } else { // throw exception on other control characters
    throw http_exception{};
  }
}

// TODO: seperate out parts into their own functions
void http_connection::on_data(const uvw::DataEvent &event,
                              uvw::TcpHandle &client) {
  if (buffer.size() + event.length > MAX_REQUEST_LEN) {
    throw http_exception{};
  }

  std::copy_n(&(event.data[0]), event.length,
              back_insert_iterator<vector<char>>(buffer));

  switch (state) {
  case HEADER:
    on_start(client);
    break;
  case BODY:
    on_body(client);
    break;
  case CHUNKED_BODY:
    on_chunked(client);
    break;
  default:
    throw http_exception{};
  }
}

void http_connection::on_start(uvw::TcpHandle &client) {
  if (!is_complete()) {
    // we need more data
    return;
  }

  if (buffer.size() < 9) {
    client.close();
    return;
  }

  string_view data{buffer.data(), buffer.size()};

  // skip first newline
  // XXX: some clients send a newline first then their request
  if (data.at(0) == '\r') {
    if (data.at(1) != '\n') {
      throw http_exception{};
    }
    data.remove_prefix(2);
  } else if (data.at(0) == '\n') {
    data.remove_prefix(1);
  }

  // parse out the method, path, and version
  size_t index = data.find_first_of(' ');
  result.method = data.substr(0, index);
  check_valid(result.method, true, true);
  data.remove_prefix(index + 1);

  if (result.method.length() > MAX_METHOD_LEN) {
    throw http_exception{};
  }

  index = data.find_first_of(' ');
  result.path = data.substr(0, index);
  check_valid(result.path, true, false);
  data.remove_prefix(index + 1);

  if (result.path.length() > MAX_PATH_LEN) {
    throw http_exception{};
  }

  // check for HTTP/1.xx
  if (data.compare(0, 7, "HTTP/1.", 7) != 0) {
    throw http_exception{};
  }

  result.version = data.substr(0, 8);
  data.remove_prefix(8);
  // TODO: check version number

  data.remove_prefix(check_eol(data));

  // TODO: parse headers
  for (;;) {
    // check if we start with a newline, if so break out of loop
    auto eolcheck = check_eol(data, false);
    if (eolcheck > 0) {
      data.remove_prefix(eolcheck);
      break;
    }

    // find next eol
    auto [eol, eollen] = find_eol(data);
    auto header_block = data.substr(0, eol - eollen);
    data.remove_prefix(eol);

    // for name, but do not discard SP before colon, see
    // http://www.mozilla.org/security/announce/2006/mfsa2006-33.html
    size_t colon = header_block.find(':');
    auto name = header_block.substr(0, colon);
    header_block.remove_prefix(colon + 1);

    for (;;) {
      char c = header_block.at(0);

      // skip tabs and spaces after colon
      if (!(c == ' ' || c == '\t')) {
        break;
      }
      header_block.remove_prefix(1);
    }

    result.headers.emplace(name, header_block);
  }

  if (auto itr = result.headers.find("Content-Length");
      itr != result.headers.end()) {
    try {
      result.body_length = stoll(itr->second);
    } catch (logic_error &) {
      throw http_exception{};
    }

    state = BODY;
    if (data.length() > 0) {
      if ((long long)data.size() >= result.body_length) {
        std::copy_n(data.begin(), result.body_length, buffer.begin());
        buffer.resize(result.body_length);
        on_body(client);
      } else {
        std::copy(data.begin(), data.end(), buffer.begin());
        buffer.resize(data.length());
      }
    } else {
      buffer.clear();
    }
  } else if (auto itr = result.headers.find("Transfer-Encoding");
             itr != result.headers.end()) {

    if (itr->second.compare("chunked") != 0) {
      throw http_exception{};
    }

    state = CHUNKED_BODY;
    if (data.length() > 0) {
      std::copy(data.begin(), data.end(), buffer.begin());
      buffer.resize(data.length());
      on_chunked(client);
    } else {
      buffer.clear();
    }
  } else { // finished
    callback(this, client, result);
    reset();
  }
}

void http_connection::on_body(uvw::TcpHandle &client) {
  if (buffer.size() > MAX_REQUEST_LEN) {
    throw http_exception{};
  }

  if ((long long)buffer.size() < result.body_length) {
    // continue waiting for more data
    return;
  }

  result.body.assign(buffer.begin(), buffer.begin() + result.body_length);
  callback(this, client, result);
  reset();
}

void http_connection::on_chunked(uvw::TcpHandle &) { throw http_exception{}; }

void http_connection::on_close() {}

http_connection::~http_connection() {}

bool http_connection::is_complete() {
  constexpr array<char, 4> needle = {'\r', '\n', '\r', '\n'};
  size_t pos = last_pos < 3 ? 0 : last_pos - 3;

  if (auto itr = std::search(buffer.begin() + pos, buffer.end(), needle.begin(),
                             needle.end());
      itr != buffer.end()) {
    return true;
  }

  last_pos = buffer.size();
  return false;
}