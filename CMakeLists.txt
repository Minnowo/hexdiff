cmake_minimum_required(VERSION 2.8)
project(hexdiff C)
add_executable(hexdiff main.c)
set_property(TARGET hexdiff PROPERTY C_STANDARD 11)

# Install
include(GNUInstallDirs)
install(TARGETS hexdiff
	DESTINATION "${CMAKE_INSTALL_BINDIR}")

# Show Warnings
if (CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
endif()
