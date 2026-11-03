cmake_minimum_required(VERSION 3.10)
project(WeatherPoller)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(weather_poller main.cpp)

# Link SQLite3, pthread, and standard system libraries
target_link_libraries(weather_poller PRIVATE sqlite3 pthread)
