#include "finders.h"
#include "generator.h"
#include <stdio.h>
#include <chrono>
#include "boinc/boinc_api.h"
#include "boinc/filesys.h"

using namespace std::chrono;

struct checkpoint_vars {
    unsigned long long offset;
    uint64_t elapsed_chkpoint;
};
checkpoint_vars curr_checkpoint;
uint64_t elapsed_chkpoint = 0;

void loadCheckpoint() {
    FILE *checkpoint_data = boinc_fopen("loneseed-checkpoint", "rb");
    if(!checkpoint_data){
        fprintf(stderr, "No checkpoint to load\n");
        curr_checkpoint.offset = 0;
        curr_checkpoint.elapsed_chkpoint = 0;
    }
    else {
        boinc_begin_critical_section();

        fread(&curr_checkpoint, sizeof(curr_checkpoint), 1, checkpoint_data);
        fprintf(stderr, "Checkpoint loaded, task time %.2f s, seed pos: %d\n", curr_checkpoint.elapsed_chkpoint, curr_checkpoint.offset);
        fclose(checkpoint_data);

        boinc_end_critical_section();
    }
}

void saveCheckpoint() {
    boinc_begin_critical_section(); // Boinc should not interrupt this

    FILE *checkpoint_data = boinc_fopen("loneseed-checkpoint", "wb");
    fwrite(&curr_checkpoint, sizeof(curr_checkpoint), 1, checkpoint_data);
    fflush(checkpoint_data);
    fclose(checkpoint_data);
    
    boinc_end_critical_section();
    boinc_checkpoint_completed(); // Checkpointing completed
}

int main(int argc, char **argv){
    uint64_t block_min = 0;
    uint64_t block_max = 0;
    int radius = 5;
    for (int i = 1; i < argc; i += 2) {
        const char *param = argv[i];
        if (strcmp(param, "-s") == 0 || strcmp(param, "--start") == 0) {
            sscanf(argv[i + 1], "%llu", &block_min);
        } else if (strcmp(param, "-e") == 0 || strcmp(param, "--end") == 0) {
            sscanf(argv[i + 1], "%llu", &block_max);
        } else if (strcmp(param, "-r") == 0 || strcmp(param, "--radius") == 0){
            radius = atoi(argv[i+1]);
        }
          else {
            fprintf(stderr,"Unknown parameter: %s\n", param);
        }
    }
    BOINC_OPTIONS options;
    boinc_options_defaults(options);
    options.normal_thread_priority = true;
    boinc_init_options(&options);
    
    loadCheckpoint();

    FILE* seedsout = fopen("seeds.txt", "a+");
    

    int structType = Village;
    int mc = MC_1_20;

    Generator g;
    setupGenerator(&g, mc, 0);

    StructureConfig sconf;
    getStructureConfig(Village, mc, &sconf);

    auto start = high_resolution_clock::now();
	
    fprintf(stderr, "starting...\n");
    for (uint64_t seed = block_min * 32768 * 32; seed < block_max * 32768 * 32; seed++) {
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
        fprintf(seedsout,"%llu\n", seed);
        fflush(seedsout);
        next:
            ;
        if(seed % 100000 == 0 || boinc_time_to_checkpoint()){
            boinc_begin_critical_section(); 
            
            boinc_delete_file("checkpoint.txt");
            FILE *checkpoint_data = boinc_fopen("checkpoint.txt", "wb");
            
            auto checkpoint_end = high_resolution_clock::now();
            auto duration = duration_cast<milliseconds>(checkpoint_end - start);

            struct checkpoint_vars data_store;
            data_store.offset = seed - block_min;
            data_store.elapsed_chkpoint = elapsed_chkpoint + duration.count()/1000;


            
            fwrite(&data_store, sizeof(data_store), 1, checkpoint_data);
            fclose(checkpoint_data);

            boinc_end_critical_section();
            boinc_checkpoint_completed();
        }
        double frac = (double)(seed+1 - block_min) / (double)(block_max - block_min);
        boinc_fraction_done(frac);
    }
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);
    int checked = 32768*32*(block_max - block_min);
    fprintf(stderr, "checked = %" PRIu64 "\n", checked);
    fprintf(stderr, "time taken = %f\n", ((double)duration.count()/1000.0)+(double)elapsed_chkpoint);

	double seeds_per_second = checked / ((double)duration.count()/1000.0)+(double)elapsed_chkpoint;
	fprintf(stderr, "seeds per second: %f\n", seeds_per_second);
    boinc_finish(0);
}