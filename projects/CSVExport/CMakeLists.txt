cmake_minimum_required (VERSION 3.14)

project (CSVExport VERSION 1.0.0 DESCRIPTION "Firebird csv export utility" LANGUAGES CXX)

# set variable for the C preprocessor to detect the operation system
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_compile_definitions(LINUX)
	set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/linux-x64)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(_WINDOWS)
	if (CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
	    set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/windows-x64)
	endif()
	if (CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "X86")
	    set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build/windows-x86)
	endif()	
endif()

###############################################################################
# Require and enable C++ 17
############
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "${PROJECT_NAME} C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "${PROJECT_NAME} C++ compiler: ${CMAKE_CXX_COMPILER_ID}")

add_compile_definitions(HAVE_CONFIG_H)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_COMPILER_IS_GNUCXX)
    add_compile_definitions(LINUX)
	# -fsanitize=address
  	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Werror")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
	include(CheckCXXCompilerFlag)
	check_cxx_compiler_flag("-stdlib=libc++" CXX_SUPPORTS_STDLIB)
	if(CXX_SUPPORTS_STDLIB)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
		set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -stdlib=libc++")
	endif()	
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
	string(REGEX REPLACE "[/-]W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	if (NOT (CMAKE_VERSION VERSION_LESS 3.6.0)) # Compiler features for Intel in CMake 3.6+
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Qstd=c++17")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /QaxCORE-AVX2")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
	set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   /Od")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O3")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Qipo")
elseif(MSVC)
	string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	if(MSVC_VERSION LESS 1900)
		message(FATAL_ERROR "${PROJECT_NAME} build: Build requires C++17-compliant compiler")
	endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
	message(STATUS "${PROJECT_NAME} compiler flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
elseif(CMAKE_BUILD_TYPE MATCHES Release)
	message(STATUS "${PROJECT_NAME} compiler flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
endif()

message(STATUS "${PROJECT_NAME} Binary directory: ${CMAKE_BINARY_DIR}")

####################################
# src
####################################
file(GLOB_RECURSE PROJECT_SOURCES 
    "../../src/*"
)

include_directories(${FIREBIRD_INCLUDE_DIR})
link_directories(${FIREBIRD_LIB_DIR})

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_link_libraries(${PROJECT_NAME} fbclient_ms)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} fbclient)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} ${CMAKE_DL_LIBS})
    target_link_libraries(${PROJECT_NAME} -lstdc++fs)
endif()

# Install the binary program
install(TARGETS ${PROJECT_NAME} DESTINATION ${FIREBIRD_BIN_DIR}/)
