#include "http.hpp"
using namespace std;
using namespace urahara;

int main() {
  urahara::http server{"127.0.0.1", 5000};
  server.run([](http_connection *, uvw::TcpHandle &client, http_data) {
    // TODO: remove test data
    // for now just send back test data
    http_response resp{};
    resp.set_content("Test\n");

    string rstr = resp.build();
    client.write(const_cast<char *>(rstr.c_str()), rstr.length());
  });
  return 0;
}