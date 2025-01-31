#include "finders.h"
#include "generator.h"
#include <stdio.h>
#include <omp.h>
#include <chrono>

using namespace std::chrono;

int main()
{
    int radius = 5;
    int structType = Village;
    int mc = MC_1_20;
    auto start = high_resolution_clock::now();
    Generator g;
    setupGenerator(&g, mc, 0);

    StructureConfig sconf;
    getStructureConfig(Village, mc, &sconf);

    int region_size = sconf.regionSize; 

    int size = 5 * region_size;

    for (uint64_t seed = 0; seed < (1L<<48L); seed++) {
        applySeed(&g, DIM_OVERWORLD, seed);
        for (int rx = -radius; rx < radius; rx++) {
            for (int rz = -radius; rz < radius; rz++) {
                Pos p;
                getStructurePos(structType, mc, seed, rx, rz, &p);
                if (isViableStructurePos(structType, &g, p.x, p.z, 0)) {
                    goto next;
                }

            }
        }
        printf("Seed %d has 0 villages!\n");
        next:
            ;
        if(seed % 100000 == 0){
            auto checkpoint_end = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(checkpoint_end - start);
            double sps = ((double)seed/(double)duration.count())*1000.0;
            printf("Thread: %d Speed: %f sps\n", omp_get_thread_num(), sps);
        }
    }
}