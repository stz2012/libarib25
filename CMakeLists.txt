cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "limit build types" FORCE)
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "default build type")

set(CMAKE_BUILD_WITH_INSTALL_RPATH True)

project(arib_std_b25 C)

set(ARIB25_LIB_NAME "arib25")
set(ARIB25_CMD_NAME "b25")

set(ARIB25_URL "https://github.com/stz2012/libarib25")
set(ARIB25_DESCRIPTION "Reference implementation of ARIB STD-B25")

set(ARIB25_MAJOR_VERSION "0")
set(ARIB25_MINOR_VERSION "2")
set(ARIB25_PATCH_VERSION "5")
set(ARIB25_VERSION stz-${ARIB25_MAJOR_VERSION}.${ARIB25_MINOR_VERSION}.${ARIB25_PATCH_VERSION})

if(CMAKE_C_COMPILER_ID MATCHES "(GNU|CLang)")
	set(CMAKE_C_FLAGS "-fPIC -Wall")
	set(CMAKE_C_FLAGS_DEBUG "-O2 -g")
	set(CMAKE_C_FLAGS_RELEASE "-O2")
	set(CMAKE_SHARED_LINKER_FLAGS "-fvisibility=hidden")
endif()

find_package(WINSCARD REQUIRED)

add_definitions("-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${WINSCARD_INCLUDE_DIRS})


add_library(arib25-objlib OBJECT src/arib_std_b25.c src/b_cas_card.c src/multi2.c src/ts_section_parser.c)

add_library(arib25-static STATIC $<TARGET_OBJECTS:arib25-objlib>)
set_target_properties(arib25-static PROPERTIES OUTPUT_NAME ${ARIB25_LIB_NAME})
target_link_libraries(arib25-static PRIVATE ${WINSCARD_LIBRARIES})

add_library(arib25-shared SHARED $<TARGET_OBJECTS:arib25-objlib>)
set_target_properties(arib25-shared PROPERTIES OUTPUT_NAME ${ARIB25_LIB_NAME})
set_target_properties(arib25-shared PROPERTIES SOVERSION ${ARIB25_MAJOR_VERSION})
set_target_properties(arib25-shared PROPERTIES VERSION ${ARIB25_VERSION})
target_link_libraries(arib25-shared PRIVATE ${WINSCARD_LIBRARIES})


add_executable(b25 src/td.c)
set_target_properties(b25 PROPERTIES OUTPUT_NAME ${ARIB25_CMD_NAME})
target_link_libraries(b25 PRIVATE ${WINSCARD_LIBRARIES})
target_link_libraries(b25 PRIVATE arib25-shared)

configure_file(src/libarib25.pc.in ${CMAKE_SHARED_LIBRARY_PREFIX}${ARIB25_LIB_NAME}.pc @ONLY)

install(TARGETS b25 RUNTIME DESTINATION bin)
install(TARGETS arib25-static arib25-shared ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES src/arib_std_b25.h src/b_cas_card.h src/portable.h DESTINATION include/arib25)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}${ARIB25_LIB_NAME}.pc DESTINATION lib/pkgconfig)

add_custom_target(uninstall ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Uninstall.cmake)
