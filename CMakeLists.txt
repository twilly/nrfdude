cmake_minimum_required (VERSION 2.6)
project (nrfdude)

add_executable (nrfdude nrfdude.c ihex.c)
set(CMAKE_C_FLAGS "-g -Wall -Werror -O2")

find_package(PkgConfig)
pkg_check_modules(USB libusb-1.0)

if(NOT USB_FOUND)
	MESSAGE(FATAL_ERROR "libusb-1.0 is required")
endif(NOT USB_FOUND)

target_link_libraries (nrfdude ${USB_LIBRARIES})
include_directories (${USB_INCLUDE_DIRS})
