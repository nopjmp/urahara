#include "http.hpp"
using namespace std;
using namespace urahara;

int main() {
  urahara::http server{"127.0.0.1", 5000};
  server.run(
      [](http_connection *http_conn, uvw::TcpHandle &client, http_data data) {
        // TODO: remove test data
        // for now just send back test data
        const char str[] = "HTTP/1.1 200 OK\r\n"
                           "Server: Test\r\n"
                           "Content-Length: 5\r\n\r\n"
                           "Test\n";

        client.write(const_cast<char *>(str), sizeof(str) - 1);
      });
  return 0;
}