# Toolchain file for ZynqMP (PetaLinux 2022.2) cross-compilation

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compilation toolchain
set(CMAKE_C_COMPILER /usr/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /usr/bin/aarch64-linux-gnu-g++)
set(CMAKE_AR /usr/bin/aarch64-linux-gnu-ar)
set(CMAKE_RANLIB /usr/bin/aarch64-linux-gnu-ranlib)

# Sysroot with ncurses 6.2 for ZynqMP
if(DEFINED ENV{ZYNQMP_SYSROOT})
  set(CMAKE_SYSROOT $ENV{ZYNQMP_SYSROOT})
  set(CMAKE_FIND_ROOT_PATH $ENV{ZYNQMP_SYSROOT})

  # Add sysroot paths for finding libraries
  list(PREPEND CMAKE_INCLUDE_PATH "$ENV{ZYNQMP_SYSROOT}/usr/include")
  list(PREPEND CMAKE_LIBRARY_PATH "$ENV{ZYNQMP_SYSROOT}/usr/lib")
endif()

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories only
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
