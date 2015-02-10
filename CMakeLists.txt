cmake_minimum_required(VERSION 2.8.9)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BUILD_DIR}" IS_IN_SOURCE_BUILD)
if(IS_IN_SOURCE_BUILD)
	message(AUTHOR_WARNING "You are building MultiMC in-source. This is NOT recommended!")
endif()

if(WIN32)
	# In Qt 5.1+ we have our own main() function, don't autolink to qtmain on Windows
	cmake_policy(SET CMP0020 OLD)
endif()

project(Megatron)
enable_testing()

######## Set CMake options ########
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

######## Set module path ########
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
set(MMC_SRC "${PROJECT_SOURCE_DIR}")
set(MMC_BIN "${PROJECT_BINARY_DIR}")

# Output all executables and shared libs in the main build folder, not in subfolders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

if(UNIX)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
endif()

set(CMAKE_JAVA_TARGET_OUTPUT_DIR ${PROJECT_BINARY_DIR}/jars)

######## Set compiler flags ########
include(UseCXX11)
include(Coverage)
set(CMAKE_CXX_FLAGS " -Wall ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Werror=return-type")

################################ 3rd Party Libs ################################

# Find the required Qt parts
find_package(Qt5Core REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Test REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5WebKitWidgets REQUIRED)

include_directories(
	${Qt5Core_INCLUDE_DIRS}
	${Qt5Widgets_INCLUDE_DIRS}
	${Qt5Concurrent_INCLUDE_DIRS}
	${Qt5Test_INCLUDE_DIRS}
	${Qt5Network_INCLUDE_DIRS}
	${Qt5Xml_INCLUDE_DIRS}
)

# The Qt5 cmake files don't provide its install paths, so ask qmake.
include(QMakeQuery)
query_qmake(QT_INSTALL_PLUGINS QT_PLUGINS_DIR)
query_qmake(QT_INSTALL_IMPORTS QT_IMPORTS_DIR)
query_qmake(QT_INSTALL_LIBS QT_LIBS_DIR)
query_qmake(QT_HOST_DATA QT_DATA_DIR)
set(QT_MKSPECS_DIR ${QT_DATA_DIR}/mkspecs)

################################ Included Libs ################################

# Add quazip
add_definitions(-DQUAZIP_STATIC)
add_subdirectory(depends/quazip)
include_directories(depends/quazip)

# Add the java launcher and checker
add_subdirectory(depends/launcher)
add_subdirectory(depends/javacheck)

######## MultiMC Libs ########

# Add the util library.
add_definitions(-DLIBUTIL_STATIC)
add_subdirectory(depends/util)
include_directories(${LIBUTIL_INCLUDE_DIR})

# Add the updater
add_subdirectory(mmc_updater)

# Add the GUI -> Logic connection header
add_subdirectory(depends/LogicalGui)
include_directories(${LOGICALGUI_INCLUDE_DIR})

include(Coverity)

# Tests
add_subdirectory(tests)

add_subdirectory(logic)

add_subdirectory(application)
