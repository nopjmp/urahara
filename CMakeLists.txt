cmake_minimum_required(VERSION 3.0) # setting this is required
set(CMAKE_CXX_STANDARD 17)

project(urahara)            # this sets the project name

# TODO: remove this with manual file addtions
file(GLOB_RECURSE sources      src/*.cpp include/*.hpp)

add_executable(urahara ${sources})

# Just for example add some compiler flags.
target_compile_options(urahara PUBLIC -pedantic -Wextra -fno-rtti)
target_include_directories(urahara PUBLIC src include)

###############################################################################
## dependencies ###############################################################
###############################################################################

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(urahara PUBLIC
  Threads::Threads
  uv
)
###########################
# Testing
###########################
enable_testing()

SET(HTTPTEST_SRC test/http.cpp src/http.cpp include/catch.hpp include/http.hpp include/uvw.hpp)

add_executable(httptest ${HTTPTEST_SRC})
target_compile_options(httptest PUBLIC -fno-rtti)
target_include_directories(httptest PUBLIC test src include)
target_link_libraries(httptest PUBLIC 
  Threads::Threads
  uv
)
add_test(NAME HttpTest COMMAND httptest)