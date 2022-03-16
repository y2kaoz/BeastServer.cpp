cmake_minimum_required(VERSION 3.22)
project(BeastServer LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(${PROJECT_NAME} 
  "src/main.cpp"
)

find_package(Boost REQUIRED COMPONENTS log program_options)
target_link_libraries(${PROJECT_NAME} PRIVATE pthread ${Boost_LIBRARIES})
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
target_compile_definitions(${PROJECT_NAME} PRIVATE "BOOST_BEAST_USE_STD_STRING_VIEW")
