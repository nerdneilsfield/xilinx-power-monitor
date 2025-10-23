pybind11_add_module(xlnpwmon_py MODULE
    xlnpwmon_pybind.cpp
)

target_link_libraries(xlnpwmon_py PRIVATE xlnpwmon_static pybind11::headers)

# 设置输出名称
set_target_properties(xlnpwmon_py PROPERTIES
    PREFIX ""
    OUTPUT_NAME "xlnpwmon"
)

# 安装 Python 模块
if(SKBUILD)
    install(TARGETS xlnpwmon_py DESTINATION ${PYTHON_INSTALL_DIR})
else()
    install(TARGETS xlnpwmon_py DESTINATION ${Python_SITEARCH})
endif()
