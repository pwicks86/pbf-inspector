# CMake simple example

## [main]

# Almost all CMake files should start with this
# You should always specify a range with the newest
# and oldest tested versions of CMake. This will ensure
# you pick up the best policies.
cmake_minimum_required(VERSION 3.14...3.20)

# This is your project statement. You should always list languages;
# Listing the version is nice here since it sets lots of useful variables
project(
  pbf-inspector
  VERSION 1.0
  LANGUAGES CXX C)

include(FetchContent)

# linenoise
FetchContent_Declare(
  linenoise
  GIT_REPOSITORY git@github.com:antirez/linenoise.git
  GIT_TAG 97d2850af13c339369093b78abe5265845d78220
  BUILD_COMMAND ""
  CONFIGURE_COMMAND ""
)
FetchContent_MakeAvailable(linenoise)

add_library(linenoise ${linenoise_SOURCE_DIR}/linenoise.c)
set_target_properties(linenoise PROPERTIES LINKER_LANGUAGE C)
target_include_directories(linenoise PUBLIC ${linenoise_SOURCE_DIR})

# add_executable(example example.cc)
# target_link_libraries(example asio)



# If you set any CMAKE_ variables, that can go here.
# (But usually don't do this, except maybe for C++ standard)
set (CMAKE_CXX_STANDARD 17)

# Find packages go here.

add_subdirectory(src)