# CMake generated Testfile for 
# Source directory: /home/scjy/v4l2rtspserver
# Build directory: /home/scjy/v4l2rtspserver
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(help "./v4l2rtspserver" "-h")
set_tests_properties(help PROPERTIES  _BACKTRACE_TRIPLES "/home/scjy/v4l2rtspserver/CMakeLists.txt;159;add_test;/home/scjy/v4l2rtspserver/CMakeLists.txt;0;")
subdirs("libv4l2cpp")
