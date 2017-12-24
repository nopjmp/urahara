#define CATCH_CONFIG_MAIN
#include "catch.hpp"
using Catch::Matchers::Equals;

#include "http.hpp"
using namespace std;
using namespace urahara;

#if 0
SECTION("partial") {
    http_data data;
    http_connection c{[&](http_connection *, uvw::TcpHandle &, http_data) {}};

    const char testdata[] = "GET / HTTP/1.0\r\n\r\n";
    c.buffer.assign(testdata, testdata + sizeof(testdata) - 1);
    REQUIRE_NOTHROW(c.parse());
  }
#endif

TEST_CASE("REQUEST", "[http]") {
  SECTION("simple") {
    http_data data;
    http_connection c{[&](http_connection *, uvw::TcpHandle &, http_data) {}};

    const char testdata[] = "GET / HTTP/1.0\r\n\r\n";
    c.buffer.assign(testdata, testdata + sizeof(testdata) - 1);
    REQUIRE_NOTHROW(c.parse());

    REQUIRE(c.result.headers.size() == 0);
    REQUIRE_THAT(c.result.method, Equals("GET"));
    REQUIRE_THAT(c.result.path, Equals("/"));
    REQUIRE_THAT(c.result.version, Equals("HTTP/1.0"));
  }

  SECTION("partial") {
    http_data data;
    http_connection c{[&](http_connection *, uvw::TcpHandle &, http_data) {}};

    const char testdata[] = "GET / HTTP/1.0\r\n\r";
    c.buffer.assign(testdata, testdata + sizeof(testdata) - 1);
    REQUIRE_THROWS(c.parse());
  }

  SECTION("parse headers") {
    http_data data;
    http_connection c{[&](http_connection *, uvw::TcpHandle &, http_data) {}};

    const char testdata[] =
        "GET /test HTTP/1.1\r\nHost: example.com\r\nCookie: \r\n\r\n";
    c.buffer.assign(testdata, testdata + sizeof(testdata) - 1);
    REQUIRE_NOTHROW(c.parse());

    REQUIRE(c.result.headers.size() == 2);
    REQUIRE_THAT(c.result.method, Equals("GET"));
    REQUIRE_THAT(c.result.path, Equals("/test"));
    REQUIRE_THAT(c.result.version, Equals("HTTP/1.1"));
    REQUIRE_THAT(c.result.headers["Host"], Equals("example.com"));
    REQUIRE_THAT(c.result.headers["Cookie"], Equals(""));
  }
}