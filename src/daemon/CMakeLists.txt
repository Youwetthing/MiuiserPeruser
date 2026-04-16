include_directories(${CMAKE_SOURCE_DIR}/src/core/include)
include_directories(${CMAKE_SOURCE_DIR}/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/daemon)

# We ONLY build channel6_news.c because it now includes the others
add_executable(channel6_news channel6_news.c)

# Link the raw system libraries
target_link_libraries(channel6_news pthread m sqlite3)
