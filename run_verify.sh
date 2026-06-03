#!/bin/bash

# --- Configuration Paths ---
ROCM_PATH="/opt/rocm-6.3.1"
KOKKOS_PATH="${HOME}/kokkos-install-rocm"
BUILD_DIR="build_verify"

# Get the application name from the first argument, or default to 'verify_kokkos'
APP_NAME="${1:-verify_kokkos}"

echo "===================================================="
echo "Target Application: ${APP_NAME}"
echo "Using Compiler:     ${ROCM_PATH}/bin/hipcc"
echo "===================================================="

# 1. Dynamically update CMakeLists.txt target names
if [ -f "CMakeLists.txt" ]; then
    echo "Updating CMakeLists.txt for target '${APP_NAME}'..."
    # Update project() line
    sed -i "s/project(.*)/project(${APP_NAME} CXX)/g" CMakeLists.txt
    # Update add_executable() line assuming the source file matches the app name
    sed -i "s/add_executable(.*)/add_executable(${APP_NAME} ${APP_NAME}.cpp)/g" CMakeLists.txt
    # Update target_link_libraries() line
    sed -i "s/target_link_libraries(.*)/target_link_libraries(${APP_NAME} Kokkos::kokkos)/g" CMakeLists.txt
else
    echo "Error: CMakeLists.txt not found in current directory!"
    exit 1
fi

# 2. Automatically wipe and recreate the build directory
echo "Cleaning up old build artifacts..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 3. Quiet OpenMP warnings at runtime
export OMP_PROC_BIND=spread
export OMP_PLACES=threads

# 4. Configure with CMake
echo "Running CMake configuration..."
cmake .. \
    -DKokkos_ROOT="${KOKKOS_PATH}" \
    -DCMAKE_CXX_COMPILER="${ROCM_PATH}/bin/hipcc" \
    -DCMAKE_PREFIX_PATH="${ROCM_PATH}"

if [ $? -ne 0 ]; then
    echo "Error: CMake configuration failed!"
    exit 1
fi

# 5. Compile the binary
echo "Compiling application..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Error: Compilation failed!"
    exit 1
fi

# 6. Run the newly compiled binary automatically
echo "Running executable..."
echo "===================================================="
./"${APP_NAME}"
