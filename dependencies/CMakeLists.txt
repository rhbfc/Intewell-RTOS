set(CMAKE_ENV_PATH "${CMAKE_SOURCE_DIR}/build-env/cmake")

include(${CMAKE_ENV_PATH}/dependencies.cmake)

# ============================================
# Dependencies 主库（最终聚合所有子模块）
# ============================================
set(DEPENDENCIES_MODULES "" CACHE INTERNAL "Dependencies module list")


# ============================================
# 获取 dependencies 下面所有一级子模块目录
# ============================================
file(GLOB DEPENDENCIES_SUBDIRS RELATIVE 
     ${CMAKE_CURRENT_LIST_DIR} 
     ${CMAKE_CURRENT_LIST_DIR}/*)

foreach(sub ${DEPENDENCIES_SUBDIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${sub}" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/${sub}/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${sub})  # 每个子模块 CMakeLists.txt 内再调用 dependencies_add_subdir
    endif()
endforeach()

dependencies_create_aggregate()

