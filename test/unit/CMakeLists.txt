add_executable(rlist rlist.c test.c)
add_executable(queue queue.c)
add_executable(mhash mhash.c)
add_executable(rope_basic rope_basic.c ${CMAKE_SOURCE_DIR}/src/rope.c)
add_executable(rope_avl rope_avl.c ${CMAKE_SOURCE_DIR}/src/rope.c)
add_executable(rope_stress rope_stress.c ${CMAKE_SOURCE_DIR}/src/rope.c)
add_executable(rope rope.c ${CMAKE_SOURCE_DIR}/src/rope.c)

# Bitmap library
add_library(bitmap STATIC
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/bitmap.h
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/bitmap.c
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/iterator.h
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/iterator.c
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/index.h
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/index.c
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/bit.h
    ${CMAKE_SOURCE_DIR}/src/box/bitmap/bit.c
)
# Bitmap tests
add_executable(bitmap_basic bitmap_basic.c)
target_link_libraries(bitmap_basic bitmap)
add_executable(bitmap_iterator bitmap_iterator.c)
target_link_libraries(bitmap_iterator bitmap)
add_executable(bitmap_index bitmap_index.c)
target_link_libraries(bitmap_index bitmap)
set_target_properties(bitmap
		      bitmap_basic
		      bitmap_iterator
		      bitmap_index
    PROPERTIES COMPILE_FLAGS "-std=c99")

add_executable(objc_finally objc_finally.m)
add_executable(objc_catchcxx objc_catchcxx.m)
add_dependencies(objc_finally build_bundled_libs)
add_dependencies(objc_catchcxx build_bundled_libs)
set_target_properties(mhash PROPERTIES COMPILE_FLAGS "-std=c99")
target_link_libraries(objc_finally ${LIBOBJC_LIB} -lm -pthread)
target_link_libraries(objc_catchcxx ${LIBOBJC_LIB} ${LUAJIT_LIB} -lm -pthread)
if (TARGET_OS_LINUX OR TARGET_OS_DEBIAN_FREEBSD)
    target_link_libraries(objc_catchcxx dl)
endif()
