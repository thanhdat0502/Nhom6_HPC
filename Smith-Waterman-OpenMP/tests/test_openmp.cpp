#include <omp.h>

#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    omp_set_dynamic(0);
    const int requested = std::min(4, omp_get_num_procs());
    std::vector<int> seen(static_cast<std::size_t>(requested), 0);
    int actual = 0;
#pragma omp parallel num_threads(requested) shared(seen, actual)
    {
        const int id = omp_get_thread_num();
        seen[static_cast<std::size_t>(id)] = 1;
#pragma omp single
        actual = omp_get_num_threads();
#pragma omp critical
        std::cout << "Thread ID: " << id << '\n';
    }
    std::cout << "OpenMP macro: " << _OPENMP << "\nProcessors: " << omp_get_num_procs()
              << "\nRequested threads: " << requested << "\nActual threads: " << actual << '\n';
    if (actual != requested || std::count(seen.begin(), seen.end(), 1) != requested) {
        std::cerr << "OpenMP runtime did not supply all requested threads. Check OMP_THREAD_LIMIT.\n";
        return 1;
    }
    return 0;
}
