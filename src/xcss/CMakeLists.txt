add_executable(xcss main.c syntree.c)
add_dependencies(xcss maylib)
target_link_libraries(xcss maylib)