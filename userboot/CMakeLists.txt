enable_language(CXX ASM)

# This is the library used for interacting with the kernel. It mainly contains
# wrappers for syscalls.
add_library(system_lib STATIC syscalls.cpp)
target_compile_options(system_lib PRIVATE ${USER_CXX_FLAGS})
target_include_directories(system_lib
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE include/)

# This is an empty source file that can be used for libraries that are linked
# together from other libs and contain no sources themselves.
file(TOUCH ${CMAKE_BINARY_DIR}/null.cpp)

# Build a libc for userspace. This implicitly includes the system library, so
# targets that depend on this statically do not need to depend on the system
# lib statically. Ideally, this should be the only dependency any static
# executable would need for building a standalone ELF binary.
add_library(user_libc STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(user_libc
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(user_libc PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(user_libc
  PRIVATE system_lib
  PRIVATE user_libc_srcs)

add_library(user_libcxx STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(user_libcxx
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(user_libcxx PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(user_libcxx
  PRIVATE system_lib
  PRIVATE common_libcxx_srcs)

# This is a special userboot libc that *only* the stage 1 userboot uses. The
# only reason this is here is so __libc_start_main doesn't need to dynamically
# detect if it should unpack `argc` and `argv` from it's sole argument. Stage 1
# is special in that the kernel doesn't pass any meaninful information for its
# argument, so userboot stage 1 shouldn't do anything with it.
add_library(userboot_stage1_libc STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(userboot_stage1_libc
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(userboot_stage1_libc PRIVATE ${USER_CXX_FLAGS})
target_compile_definitions(userboot_stage1_libc PRIVATE __USERBOOT_STAGE1__)
target_link_libraries(userboot_stage1_libc
  PRIVATE system_libc
  PRIVATE user_libc_srcs
  PRIVATE common_libc_srcs)

add_library(userboot_stage1_libcxx STATIC ${CMAKE_BINARY_DIR}/null.cpp)
target_include_directories(userboot_stage1_libcxx
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_compile_options(userboot_stage1_libcxx PRIVATE ${USER_CXX_FLAGS})
target_compile_definitions(userboot_stage1_libcxx PRIVATE __USERBOOT_STAGE1__)
target_link_libraries(userboot_stage1_libcxx
  PRIVATE system_lib
  PRIVATE common_libcxx_srcs)

# This is the actual userboot binary. We construct it as a static library because
# we link it manyally with a linker script.
add_library(userboot_lib STATIC userboot.cpp userstart.S)
target_compile_options(userboot_lib
  PRIVATE ${USER_CXX_FLAGS})
target_include_directories(userboot_lib
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include/
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include/)
target_link_libraries(userboot_lib PRIVATE system_lib)
target_compile_definitions(userboot_lib PRIVATE __USERBOOT_STAGE1__)

function(add_userboot_binary name extra_link_flags extra_deps)
  add_custom_target(${name} ALL
    COMMAND ${CMAKE_LINKER}
            -T ${CMAKE_CURRENT_SOURCE_DIR}/link.ld
            -pie
            -m elf_i386
            -o ${name}

            ${extra_link_flags}

            --start-group

            --whole-archive

            --no-whole-archive
            $<TARGET_FILE:userboot_stage1_libc>
            $<TARGET_FILE:userboot_stage1_libcxx>
            $<TARGET_FILE:userboot_lib>
            $<TARGET_FILE:system_lib>

            --end-group
    # TODO: We should add some command here for asserting the file is not empty.
    COMMENT "Linking ${name}"
    DEPENDS system_lib userboot_lib user_libc ${extra_deps}
    VERBATIM)
endfunction()

# This binary is the regular ELF version of userboot. This is really only
# here so we can debug userboot much easier with ELF tools.
add_userboot_binary(userboottest1.elf "" "")
add_userboot_binary(userboottest1.bin "--oformat=binary" userboottest1.elf)

set(USER_PROGRAMS "" CACHE INTERNAL "")
set(USER_PROGRAM_DSTS "" CACHE INTERNAL "")

function(add_to_initrd binary dst)
  get_filename_component(path ${binary} REALPATH)
  set(USER_PROGRAMS "${USER_PROGRAMS} ${path}" CACHE INTERNAL "")
  set(USER_PROGRAM_DSTS "${USER_PROGRAM_DSTS} ${dst}" CACHE INTERNAL "")
  message(STATUS "Added ${binary} to USER_PROGRAMS")
endfunction()

add_executable(test-hello hello.cpp)
target_include_directories(test-hello
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include
  PRIVATE include)
target_compile_options(test-hello
  PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(test-hello
  PRIVATE user_libc
  PRIVATE user_libcxx
)
target_link_options(test-hello
  PRIVATE -nostdlib
  PRIVATE -Wl,--fatal-warnings
)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/test-hello
              "test-hello")

add_subdirectory(tests)

add_executable(userboot-stage2 userboot.cpp)
target_include_directories(userboot-stage2
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include)
target_compile_options(userboot-stage2 PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(userboot-stage2
  PRIVATE user_libc
  PRIVATE user_libcxx)
target_link_options(userboot-stage2 PRIVATE -nostdlib)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/userboot-stage2
              "userboot-stage2")

add_executable(shell shell.cpp)
target_include_directories(shell
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include)
target_compile_options(shell PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(shell
  PRIVATE user_libc
  PRIVATE user_libcxx)
target_link_options(shell PRIVATE -nostdlib)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/shell
              "bin/shell")

add_executable(ls ls.cpp)
target_include_directories(ls
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include)
target_compile_options(ls PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(ls
  PRIVATE user_libc
  PRIVATE user_libcxx)
target_link_options(ls PRIVATE -nostdlib)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/ls
              "bin/ls")

# TODO: This is commented out because the kernel has reached a point where it
# hit 4MB limit of the initial allocation we gave it for malloc. This means
# channels cannot resize any larger when streaming vfs data. This is commented
# out to save some space in the vfs data transfered over channels. Once malloc
# (for the kernel) is able to dynamically resize, this can be re-added.
#add_executable(page-fault page_fault.cpp)
#target_include_directories(page-fault
#  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
#  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
#  PRIVATE include)
#target_compile_options(page-fault PRIVATE ${USER_CXX_FLAGS})
#target_link_libraries(page-fault
#  PRIVATE user_libc
#  PRIVATE user_libcxx)
#target_link_options(page-fault PRIVATE -nostdlib)
#
#add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/page-fault
#              "bin/page-fault")

add_executable(printenv printenv.cpp)
target_include_directories(printenv
  PRIVATE ${CMAKE_SOURCE_DIR}/libc/include
  PRIVATE ${CMAKE_SOURCE_DIR}/libcxx/include/
  PRIVATE include)
target_compile_options(printenv PRIVATE ${USER_CXX_FLAGS})
target_link_libraries(printenv
  PRIVATE user_libc
  PRIVATE user_libcxx)
target_link_options(printenv PRIVATE -nostdlib)

add_to_initrd(${CMAKE_CURRENT_BINARY_DIR}/printenv
              "bin/printenv")

set(USER_PROGRAMS_LIST ${USER_PROGRAMS})
separate_arguments(USER_PROGRAMS_LIST)

set(INITRD_DIR ${INITRD_NAME}.dir)
add_custom_target(${INITRD_NAME}
  COMMAND rm -rf ${INITRD_DIR}
  COMMAND mkdir -p ${INITRD_DIR}

  # Copy user programs + data
  COMMAND python3 ${CMAKE_SOURCE_DIR}/userboot/make_initrd.py
          --files ${USER_PROGRAMS} --dests ${USER_PROGRAM_DSTS}
          --initrd-dir ${INITRD_DIR}
  COMMAND tar --format=ustar -cf ${INITRD_NAME}.tar -C ${INITRD_DIR} .

  # The initrd will consist of the concatenation of the entry program at the
  # start of the file and the tarball for the ustar filesystem.
  COMMAND /bin/sh -c "cat ${CMAKE_CURRENT_BINARY_DIR}/userboottest1.bin ${INITRD_NAME}.tar > ${INITRD_NAME}"

  #COMMAND /bin/sh -c "if ${SHOULD_COPY_INITRD}; then cp ${INITRD_NAME} ${INITRD_DST_DIR}/; fi"

  COMMENT "Create the initial ramdisk with '${USER_PROGRAMS}'"
  DEPENDS userboottest1.bin ${USER_PROGRAMS_LIST}
  VERBATIM)
