#!/bin/bash
set -e

# --- Configuration Variables ---
ROCM_VERSION="6.2.4"
ROCM_PATH="/opt/rocm-${ROCM_VERSION}"
LLVM_PATH="/usr" # Standard apt install path for llvm-18

# Ensure the script uses LLVM-18 compilers
export CC=${LLVM_PATH}/bin/clang-18
export CXX=${LLVM_PATH}/bin/clang++-18

sudo apt-get install -y libomp-18-dev

# Kokkos configuration
KOKKOS_VERSION="5.1.1" # You can adjust this to your preferred version
BUILD_DIR="build-kokkos-rocm"
make -p "kokkos-install-rocm"
INSTALL_DIR="${HOME}/kokkos-install-rocm"

echo "===================================================="
echo "Building Kokkos ${KOKKOS_VERSION} for gfx1102"
echo "Using Compiler: ${CXX}"
echo "ROCm Path: ${ROCM_PATH}"
echo "===================================================="

# 1. Clone Kokkos if it doesn't exist
if [ ! -d "kokkos" ]; then
    git clone https://github.com/kokkos/kokkos.git
fi

cd kokkos
git checkout ${KOKKOS_VERSION}
cd ..

# 2. Setup Build Directory
rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# 3. Configure with CMake
# We use the HIP backend and explicitly target gfx1102
cmake ../kokkos \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} \
    -DKokkos_ENABLE_HIP=ON \
    -DKokkos_ARCH_AMD_GFX1102=ON \
    -DCMAKE_CXX_COMPILER=${CXX} \
    -DCMAKE_HIP_COMPILER=${CXX} \
    -D_ROCM_PATH=${ROCM_PATH} \
    -DKokkos_ENABLE_SERIAL=ON \
    -DKokkos_ENABLE_OPENMP=ON

# 4. Build and Install
echo "Compiling Kokkos..."
make -j$(nproc)

echo "Installing Kokkos to ${INSTALL_DIR}..."
make install

echo "===================================================="
echo "Kokkos successfully installed at ${INSTALL_DIR}"
echo "===================================================="
