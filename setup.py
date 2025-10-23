from setuptools import setup, find_packages
import pybind11.setup_helpers as setup_helpers

ext_modules = [
    setup_helpers.Pybind11Extension(
        "xlnpwmon._core",
        ["bindings/python/xlnpwmon_pybind.cpp", "src/xlnpwmon.c"],
        # 示例，添加需要的编译器和链接器选项
        cxx_std=14,  # 使用C++14
        include_dirs=['include'],
        define_macros=[('VERSION_INFO', '0.0.1')],
        libraries=['pthread'],  # 如果需要
    ),
]

setup(
    name="xlnpwmon",
    version="0.0.1",
    author="Qi Deng",
    author_email="dengqi935@gmail.com",
    description="A simple power monitor library for Xilinx",
    long_description="A comprehensive power monitoring library for NVIDIA Xilinx devices, available in multiple programming languages.",
    package_dir={"": "python"},
    packages=find_packages(where='python'),
    ext_modules=ext_modules,
    zip_safe=False,
    python_requires=">=3.7",
)