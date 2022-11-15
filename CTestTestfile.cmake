# CMake generated Testfile for 
# Source directory: /home/demo/Desktop/v4l2rtspserver
# Build directory: /home/demo/Desktop/v4l2rtspserver
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(help "./v4l2rtspserver" "-h")
subdirs("libv4l2cpp")
