#include <Kokkos_Core.hpp>
#include <hip/hip_runtime.h> // Include native HIP runtime API
#include <iostream>
#include <vector>

void print_physical_gpus() {
    int device_count = 0;
    hipError_t err = hipGetDeviceCount(&device_count);
    
    std::cout << "=========================================\n";
    std::cout << "Detected Physical GPUs (via HIP Runtime):\n";
    std::cout << "=========================================\n";

    if (err != hipSuccess || device_count == 0) {
        std::cout << " No GPUs detected or HIP runtime error code: " << err << "\n";
        std::cout << "=========================================\n";
        return;
    }

    std::cout << "Total Visible GPUs: " << device_count << "\n\n";

    for (int i = 0; i < device_count; ++i) {
        hipDeviceProp_t props;
        hipGetDeviceProperties(&props, i);
        
        std::cout << "Device ID " << i << ":\n";
        std::cout << "  Name:       " << props.name << "\n";
        std::cout << "  GCN/RDNA: " << props.gcnArchName << "\n";
        std::cout << "  Total Mem:  " << (props.totalGlobalMem / (1024 * 1024)) << " MB\n";
        std::cout << "=========================================\n";
    }
}

int main(int argc, char* argv[]) {
    // Print hardware specs before initializing Kokkos ecosystem
    print_physical_gpus();

    Kokkos::initialize(argc, argv);
    {
        std::cout << "Kokkos Version: " << KOKKOS_VERSION << "\n";
        std::cout << "=========================================\n";
        
        std::cout << "Running a quick parallel test kernel... " << std::flush;
        
        int N = 10;
        Kokkos::View<int*> test_view("test_view", N);
        
        Kokkos::parallel_for("TestKernel", N, KOKKOS_LAMBDA(const int i) {
            test_view(i) = i * 2;
        });
        
        Kokkos::fence();
        std::cout << "Success!\n";
        std::cout << "=========================================\n";
    }
    Kokkos::finalize();
    return 0;
}
