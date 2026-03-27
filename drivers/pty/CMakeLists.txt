# driver CMakeLists.txt

set(KMODULE_SRC_FILES "")

# 设置宏控制是否启用
driver_add_subdir(. CONFIG_PSEUDOTERM)

file(GLOB KMODULE_SRC_FILES *.c *.S)

# 引入统一模板
os_driver_submodule_scan()
