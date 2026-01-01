set(CMAKE_SYSTEM_NAME Windows)

# Set the MSYS2 UCRT64 toolchain paths
# Replace <MY_USERNAME> with your actual username, or use environment variable
if(DEFINED ENV{MSYS2_ROOT})
    set(MSYS2_ROOT $ENV{MSYS2_ROOT})
else()
    # Default path - adjust if needed
    set(MSYS2_ROOT "C:/Users/$ENV{USERNAME}/tools/msys64")
endif()

set(UCRT64_PREFIX "${MSYS2_ROOT}/ucrt64")

# Specify the compilers
set(CMAKE_C_COMPILER "${UCRT64_PREFIX}/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${UCRT64_PREFIX}/bin/g++.exe")
set(CMAKE_RC_COMPILER "${UCRT64_PREFIX}/bin/windres.exe")

# Where to look for libraries and headers
set(CMAKE_FIND_ROOT_PATH "${UCRT64_PREFIX}")

# Adjust the default behavior of the FIND_XXX() commands:
# search for programs in both host and target environments
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
# search for libraries and headers only in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
