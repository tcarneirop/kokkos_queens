#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <vector>
#include <cmath>
#include <Kokkos_Core.hpp>

#define _EMPTY_      -1

double rtclock() {
    struct timezone Tzp;
    struct timeval Tp;
    int stat = gettimeofday(&Tp, &Tzp);
    if (stat != 0) printf("Error return from gettimeofday: %d", stat);
    return (Tp.tv_sec + Tp.tv_usec * 1.0e-6);
}

// Struct layout preserved for host/device compatibility
struct QueenRoot {
    unsigned int control;
    int8_t board[12]; 
};

inline void prefixesHandleSol(QueenRoot *root_prefixes, unsigned int flag,
                             const char *board, int initialDepth, int num_sol) {
    root_prefixes[num_sol].control = flag;
    for(int i = 0; i < initialDepth; ++i)
        root_prefixes[num_sol].board[i] = board[i];
}

inline bool MCstillLegal(const char *board, const int r) {
    int i;
    int ld;
    int rd;
    for (i = 0; i < r; ++i)
        if (board[i] == board[r]) return false;
    ld = board[r];  
    rd = board[r];  
    for (i = r - 1; i >= 0; --i) {
        --ld; ++rd;
        if (board[i] == ld || board[i] == rd) return false;
    }
    return true;
}

// Device-side legality check marked with KOKKOS_INLINE_FUNCTION
KOKKOS_INLINE_FUNCTION bool GPU_queens_stillLegal(const char *board, const int r) {
    bool safe = true;
    int i, rev_i, offset;
    const char base = board[r];
    for (i = 0, rev_i = r - 1, offset = 1; i < r; ++i, --rev_i, offset++)
        safe &= !((board[i] == base) | ((board[rev_i] == base - offset) | (board[rev_i] == base + offset)));
    return safe;
}

unsigned long long int BP_queens_prefixes(int size, int initialDepth,
    unsigned long long *tree_size, QueenRoot *root_prefixes) {

    unsigned int flag = 0;
    int bit_test = 0;
    char board[32]; 
    int i, depth; 
    unsigned long long int local_tree = 0ULL;
    unsigned long long int num_sol = 0;

    #ifdef IMPROVED
    uint break_cond = (size / 2) + (size & 1);
    #endif 

    for (i = 0; i < size; ++i) { 
        board[i] = -1;
    }

    depth = 0;

    do {
        board[depth]++;
        bit_test = 0;
        bit_test |= (1 << board[depth]);

        if (board[depth] == size) {
            board[depth] = _EMPTY_;
               
        } else if (MCstillLegal(board, depth) && !(flag & bit_test)) { 

            #ifdef IMPROVED
            if (depth == 1) {
                if (size & 1) {
                    if (board[0] == break_cond - 1 && board[1] > board[0]) 
                        break;
                }
                else {
                    if (board[0] == break_cond)
                        break;
                }
            }
            #endif 

            flag |= (1ULL << board[depth]);
            depth++;
            ++local_tree;
            
            if (depth == initialDepth) { 
                prefixesHandleSol(root_prefixes, flag, board, initialDepth, num_sol);
                num_sol++;
            } else continue;
        } else continue;

        depth--;
        flag &= ~(1ULL<<board[depth]);

    } while(depth >= 0);

    *tree_size = local_tree;
    return num_sol;
}

int main(int argc, char *argv[]) {
    // Initialize the Kokkos execution environment
    Kokkos::initialize(argc, argv);
    {
        #ifdef IMPROVED
        printf("### IMPROVED SEARCH - Avoiding mirrored solutions\n");
        #endif

        if (argc != 4) {
            printf("Usage: %s <size> <initial depth> <block size>\n", argv[0]);
            Kokkos::finalize();
            return 1;
        }

        const int size = atoi(argv[1]);  
        const int initialDepth = atoi(argv[2]); 
        const int block_size = atoi(argv[3]); 
        
        unsigned long long initial_tree_size = 0ULL;
        unsigned long long qtd_sols_global = 0ULL;
        unsigned long long gpu_tree_size = 0ULL;

        unsigned int nMaxPrefixes = 75580635;

        printf("\n### Queens size: %d, Initial depth: %d, Block size: %d", size, initialDepth, block_size);

        // Host allocations for sequential generation phase
        QueenRoot* root_prefixes_h = (QueenRoot*)malloc(sizeof(QueenRoot) * nMaxPrefixes);

        double initial_time = rtclock();

        unsigned long long n_explorers = BP_queens_prefixes(size, initialDepth, &initial_tree_size, root_prefixes_h);

        // Define Kokkos Views mapping to Device memory spaces
        Kokkos::View<QueenRoot*> root_prefixes_d("root_prefixes_d", n_explorers);
        Kokkos::View<unsigned long long*> vector_of_tree_size_d("vector_of_tree_size_d", n_explorers);
        Kokkos::View<unsigned long long*> sols_d("sols_d", n_explorers);

        // Copy host data into the device View allocation space via Unmanaged Host Mirror
        auto host_mirror = Kokkos::View<QueenRoot*, Kokkos::HostSpace>(root_prefixes_h, n_explorers);
        Kokkos::deep_copy(root_prefixes_d, host_mirror);

        // Configure parallel range and block sizing policies 
        Kokkos::RangePolicy<> policy(0, n_explorers);

        // Parallel execution implementation replacement for the HIP Kernel Launch
        Kokkos::parallel_for("BP_queens_root_dfs", policy, KOKKOS_LAMBDA(const int idx) {
            unsigned int flag = 0;
            char board[32];
            int N_l = size;
            int i, depth;
            unsigned long long qtd_sols_thread = 0ULL;
            int depthGlobal = initialDepth;
            unsigned long long int tree_size = 0ULL;

            for (i = 0; i < N_l; ++i) {
                board[i] = _EMPTY_;
            }

            flag = root_prefixes_d(idx).control;

            for (i = 0; i < depthGlobal; ++i)
                board[i] = root_prefixes_d(idx).board[i];

            depth = depthGlobal;

            do {
                board[depth]++;
                const int mask = 1 << board[depth];

                if (board[depth] == N_l) {
                    board[depth] = _EMPTY_;
                    depth--;
                    flag &= ~(1 << board[depth]);
                } else if (!(flag & mask) && GPU_queens_stillLegal(board, depth)) {

                    ++tree_size;
                    flag |= mask;
                    depth++;

                    if (depth == N_l) { 
                        ++qtd_sols_thread;
                        depth--;
                        flag &= ~mask;
                    }
                }
            } while(depth >= depthGlobal);

            sols_d(idx) = qtd_sols_thread;
            vector_of_tree_size_d(idx) = tree_size;
        });

        // Fence to guarantee execution stream drains before processing reads on host space
        Kokkos::fence();

        // Create host extraction mirrors for data collection
        auto vector_of_tree_size_h = Kokkos::create_mirror_view(vector_of_tree_size_d);
        auto solutions_h = Kokkos::create_mirror_view(sols_d);

        Kokkos::deep_copy(vector_of_tree_size_h, vector_of_tree_size_d);
        Kokkos::deep_copy(solutions_h, sols_d);

        double final_time = rtclock();

        for (int i = 0; i < n_explorers; ++i) {
            qtd_sols_global += solutions_h(i);
            gpu_tree_size   += vector_of_tree_size_h(i);
        }

        #ifdef IMPROVED
        qtd_sols_global *= 2;
        #endif

        printf("\nInitial tree size: %llu", initial_tree_size);
        printf("\nGPU Tree size: %llu\nTotal tree size: %llu\nNumber of solutions found: %llu.\n", gpu_tree_size, (initial_tree_size + gpu_tree_size), qtd_sols_global);
        printf("\nElapsed total: %.3f\n", (final_time - initial_time));
        
        free(root_prefixes_h);
    }
    Kokkos::finalize();
    return 0;
}