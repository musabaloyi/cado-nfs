
add_library (ant ant.cpp)
target_link_libraries (ant utils)

add_executable(badideals badideals.cpp)
target_link_libraries (badideals ant)
