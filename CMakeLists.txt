cmake_minimum_required(VERSION 3.0.0)
project(PortableConnection VERSION 0.1.0)

file(COPY res DESTINATION .)

file(GLOB allFiles
     "src/*.hpp"
     "src/*.cpp"
     "src/*.h"
     "src/*.c"
)

add_executable(PortableConnection ${allFiles})
target_include_directories(PortableConnection PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(PortableConnection -pthread)