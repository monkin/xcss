add_executable(xcss main.c syntree.c parser.c)
add_dependencies(xcss maylib)
target_link_libraries(xcss maylib)
