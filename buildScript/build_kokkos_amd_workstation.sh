#!/bin/bash
set +e  # Do not exit automatically on error; let us catch it safely

# --- Configuration Variables ---
ROCM_VERSION="6.3.1"
ROCM_PATH="/opt/rocm-${ROCM_VERSION}"

# Point compilers to the ROCm hipcc wrapper
export CC=${ROCM_PATH}/bin/hipcc
export CXX=${ROCM_PATH}/bin/hipcc

# Force ROCm runtime architecture mapping to handle consumer cards cleanly
export HSA_OVERRIDE_GFX_VERSION=11.0.2
export HIP_VISIBLE_DEVICES=0
export OMP_PROC_BIND=spread
export OMP_PLACES=threads
# Kokkos configuration -> Upgraded to a version with mature RDNA3 support
KOKKOS_VERSION="5.4.0"
KOKKOS_HOME="${HOME}/kokkos-${KOKKOS_VERSION}"
BUILD_DIR="${KOKKOS_HOME}/build-rocm"
INSTALL_DIR="${HOME}/kokkos-install-rocm"
KOKKOS_CODE="${HOME}/kokkos"

echo "===================================================="
echo "Building Kokkos ${KOKKOS_VERSION} for RDNA3"
echo "Kokkos Home Directory: ${KOKKOS_HOME}"
echo "Using Compiler Wrapper: ${CXX}"
echo "ROCm Path: ${ROCM_PATH}"
echo "===================================================="

# 1. Clone Kokkos to KOKKOS_HOME if the folder doesn't exist
if [ ! -d "${KOKKOS_HOME}" ]; then
    echo "Cloning Kokkos repository..."
    git clone https://github.com/kokkos/kokkos.git "${KOKKOS_HOME}"
fi

# 2. Enter KOKKOS_HOME and checkout the target version
cd "${KOKKOS_HOME}"
git fetch --tags
git checkout ${KOKKOS_VERSION}

# 3. Setup Build Directory inside KOKKOS_HOME
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# 4. Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DCMAKE_PREFIX_PATH="${ROCM_PATH}" \
    -D_ROCM_PATH="${ROCM_PATH}" \
    -DKokkos_ENABLE_HIP=ON \
    -DKokkos_ARCH_AMD_GFX906=ON \
    -DCMAKE_CXX_COMPILER=${CXX} \
    -DCMAKE_CXX_FLAGS="--offload-arch=gfx1102" \
    -DKokkos_ENABLE_SERIAL=ON \
    -DKokkos_ENABLE_OPENMP=ON


# Catch configuration failure safely without closing the terminal window
if [ $? -ne 0 ]; then
    echo "===================================================="
    echo "ERROR: CMake configuration failed! Aborting build."
    echo "===================================================="
    return 1 2>/dev/null || exit 1
fi

# 5. Build and Install
echo "Compiling Kokkos..."
make -j$(nproc)

# Catch compilation failure safely
if [ $? -ne 0 ]; then
    echo "===================================================="
    echo "ERROR: Compilation failed!"
    echo "===================================================="
    return 1 2>/dev/null || exit 1
fi

echo "Installing Kokkos to ${INSTALL_DIR}..."
make install

echo "===================================================="
echo "Kokkos successfully installed at ${INSTALL_DIR}"
echo "===================================================="

cd ${KOKKOS_CODE}
