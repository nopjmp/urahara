cmake_minimum_required(VERSION 3.0) # setting this is required
project(urahara)            # this sets the project name

# TODO: remove this with manual file addtions
file(GLOB_RECURSE sources      src/*.cpp include/*.hpp)

add_executable(urahara ${sources})

# Just for example add some compiler flags.
target_compile_options(urahara PUBLIC -std=c++1z -Wall -pedantic -Wextra -fno-rtti)
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
