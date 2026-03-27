# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

get_filename_component(_vdso_arch_dir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
get_filename_component(_vdso_arch ${_vdso_arch_dir} NAME)

set(VDSO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(VDSO_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(VDSO_LDS ${VDSO_BUILD_DIR}/vdso.lds)
set(VDSO_NOTE_OBJ ${VDSO_BUILD_DIR}/vdso-note.o)
set(VDSO_TIME_OBJ ${VDSO_BUILD_DIR}/vclock_gettime.o)
set(VDSO_SO ${VDSO_BUILD_DIR}/vdso.so)
set(VDSO_IMAGE_OBJ ${VDSO_BUILD_DIR}/vdso-image.o)
set(VDSO_INCLUDE_FLAGS
    -I${CMAKE_SOURCE_DIR}/include
    -I${CMAKE_SOURCE_DIR}/include/arch/${_vdso_arch}
)

function(os_vdso_build)
    set(_vdso_cflags ${VDSO_CFLAGS})
    set(_vdso_link_flags ${VDSO_LINK_FLAGS})
    set(_vdso_asflags ${VDSO_ASFLAGS} -I${CMAKE_SOURCE_DIR}/include/vdso)
    set(_vdso_extra_libs ${VDSO_EXTRA_LIBS})
    set(_vdso_image_target ${VDSO_IMAGE_TARGET})

    if(NOT _vdso_image_target)
        set(_vdso_image_target vdso_image_${CPU_ARCH})
    endif()

    if(CONFIG_BUILD_DEBUG_INFO)
        list(APPEND _vdso_cflags -g)
    endif()

    if(CONFIG_TOOLCHAIN_CLANG)
        list(APPEND _vdso_cflags -target ${CC_TARGET})
        list(APPEND _vdso_link_flags -target ${CC_TARGET})
        list(APPEND _vdso_asflags -target ${CC_TARGET})
    endif()

    add_custom_command(
        OUTPUT ${VDSO_IMAGE_OBJ}
        BYPRODUCTS ${VDSO_LDS} ${VDSO_NOTE_OBJ} ${VDSO_TIME_OBJ} ${VDSO_SO}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${VDSO_BUILD_DIR}
        COMMAND ${CMAKE_C_COMPILER} -E -P -x c -I${CMAKE_SOURCE_DIR}/include/vdso ${VDSO_SRC_DIR}/vdso.lds.S -o ${VDSO_LDS}
        COMMAND ${CMAKE_C_COMPILER} ${_vdso_cflags} -I${CMAKE_SOURCE_DIR}/include/vdso -c ${VDSO_SRC_DIR}/vdso-note.S -o ${VDSO_NOTE_OBJ}
        COMMAND ${CMAKE_C_COMPILER} ${_vdso_cflags} ${VDSO_INCLUDE_FLAGS} -c ${VDSO_SRC_DIR}/vclock_gettime.c -o ${VDSO_TIME_OBJ}
        COMMAND ${CMAKE_C_COMPILER} ${_vdso_link_flags} -o ${VDSO_SO} ${VDSO_NOTE_OBJ} ${VDSO_TIME_OBJ} ${_vdso_extra_libs}
        COMMAND ${CMAKE_C_COMPILER} ${_vdso_asflags} -c ${VDSO_SRC_DIR}/vdso-image.S -o ${VDSO_IMAGE_OBJ}
        WORKING_DIRECTORY ${VDSO_BUILD_DIR}
        DEPENDS
            ${VDSO_SRC_DIR}/vdso.lds.S
            ${VDSO_SRC_DIR}/vdso-note.S
            ${VDSO_SRC_DIR}/vclock_gettime.c
            ${VDSO_SRC_DIR}/vdso-image.S
            ${CMAKE_SOURCE_DIR}/include/vdso/vdso.lds.inc.S
            ${CMAKE_SOURCE_DIR}/include/vdso/vdso-note.inc.S
            ${CMAKE_SOURCE_DIR}/include/vdso/vdso-image.inc.S
            ${CMAKE_SOURCE_DIR}/include/vdso_datapage.h
            ${CMAKE_SOURCE_DIR}/include/vdso/vclock_gettime_common.h
    )

    add_custom_target(${_vdso_image_target} DEPENDS ${VDSO_IMAGE_OBJ})
    add_dependencies(${LIB_NAME} ${_vdso_image_target})

    set_source_files_properties(
        ${VDSO_IMAGE_OBJ}
        TARGET_DIRECTORY ${LIB_NAME}
        PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE
    )
    target_sources(${LIB_NAME} PRIVATE ${VDSO_IMAGE_OBJ})

    foreach(_vdso_file ${VDSO_LDS} ${VDSO_NOTE_OBJ} ${VDSO_TIME_OBJ} ${VDSO_SO} ${VDSO_IMAGE_OBJ})
        set_property(TARGET ${_vdso_image_target} APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${_vdso_file})
    endforeach()
endfunction()
