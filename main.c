/** main.c
 * ===========================================================
 * Name: Chandra Mohan, Sweta
 * Section: M3
 * Project: PEX2 - CPU Scheduling Simulator
 * Purpose: Entry point for the multi-threaded CPU scheduling simulator.
 *          Reads simulation parameters from command-line arguments or
 *          prompts the user interactively. Creates one scheduling thread
 *          per CPU, drives the simulation clock loop, generates processes
 *          stochastically using a fixed random seed, and prints final
 *          queue state to stdout at the end of the simulation.
 *          Supports sequential (default) and parallel CPU signaling modes;
 *          see usage() for the full argument list and a description of each.
 * ===========================================================
 * Documentation Statement: none
 * ===========================================================
 * ======================================================================
 * Required Features Not Included:1
 *       1) none
 *       2)
 *       3)
 * ======================================================================
 * Known Bugs:
 *       1) none
 *       2)
 *       3)
 * ====================================================================== */

#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "processQueue.h"
#include "CPUs.h"

// Maximum characters read from interactive input
#define MAX_INPUT_LEN 128

// Process generation parameters (govern the randomly generated workload)
#define MAX_TIME_BETWEEN 7   // inter-arrival gap is rand_r() % MAX_TIME_BETWEEN (0 to 6 steps)
#define MAX_BURST        10  // CPU burst is (rand_r() % MAX_BURST) + 1 (1 to 10 timesteps)
#define MAX_PRIORITY      6  // priority is rand_r() % MAX_PRIORITY (0 to 5; lower = higher priority)

void genProcess(SharedVars* shared, int* timeToNextProcess,
                int* nextPid, int* randSeed, int currTime);
void init(int numProcesses, SharedVars* svars, int quantum);
void usage(char** argv);
void preGenAndPrint(int simTime);

int main(int argc, char* argv[]) {

    int simTime = 0;          // total simulation duration in timesteps
    int numCPUs = 0;          // number of CPU scheduling threads to create
    int cpuType = 0;          // scheduling algorithm selection (1–6)
    int quantum = 0;          // round-robin time slice; only used when cpuType == 4
    bool debugOutput = false;       // when true, print queue snapshots each timestep
    bool parallelSignaling = false; // when true, signal all CPUs concurrently (non-deterministic);
                                    // default false = sequential (one CPU at a time, deterministic)
    char input[MAX_INPUT_LEN];      // buffer for interactive prompts

    // parse command-line arguments; any missing values are requested interactively
    if (argc > 1) {
        if (!strcmp(argv[1], "-h")) {
            usage(argv);
        }
        debugOutput = (bool) atoi(argv[1]);
        if (argc > 2) {
            simTime = atoi(argv[2]);
            if (argc > 3) {
                numCPUs = atoi(argv[3]);
                if (argc > 4) {
                    cpuType = atoi(argv[4]);
                    if (argc > 5) {
                        quantum = atoi(argv[5]);
                        if (argc > 6) {
                            parallelSignaling = (bool) atoi(argv[6]);
                            if (argc > 7) {
                                usage(argv);    // too many arguments
                            }
                        }
                    }
                }
            }
        }
    }

    // prompt for any parameters not supplied on the command line
    while (simTime <= 0) {
        fprintf(stderr, "Enter simulation time: ");
        fgets(input, MAX_INPUT_LEN, stdin);
        simTime = atoi(input);
    }

    while (numCPUs <= 0) {
        fprintf(stderr, "Enter number of CPUs: ");
        fgets(input, MAX_INPUT_LEN, stdin);
        numCPUs = atoi(input);
    }

    while (cpuType < 1 || cpuType > 6) {
        fprintf(stderr, "Select a CPU Type:\n"
                        "\t1: First In First Out\n"
                        "\t2: Shortest Job First\n"
                        "\t3: Priority (non-preemptive)\n"
                        "\t4: Round Robin\n"
                        "\t5: Shortest Remaining Time First\n"
                        "\t6: Priority (preemptive)\n");
        fgets(input, MAX_INPUT_LEN, stdin);
        cpuType = atoi(input);
    }

    // echo the selected algorithm to stderr (not graded output)
    switch (cpuType) {
        case 1: fprintf(stderr, "Using First In First Out CPU\n");            break;
        case 2: fprintf(stderr, "Using Shortest Job First CPU\n");            break;
        case 3: fprintf(stderr, "Using Priority (non-preemptive) CPU\n");     break;
        case 4: fprintf(stderr, "Using Round Robin CPU\n");                   break;
        case 5: fprintf(stderr, "Using Shortest Remaining Time First CPU\n"); break;
        case 6: fprintf(stderr, "Using Priority (preemptive) CPU\n");         break;
    }

    // quantum is only required for Round Robin
    while (cpuType == 4 && quantum <= 0) {
        fprintf(stderr, "Enter quantum length: ");
        fgets(input, MAX_INPUT_LEN, stdin);
        quantum = atoi(input);
    }

    fprintf(stderr, "Signaling: %s\n",
            parallelSignaling ? "parallel (all CPUs concurrent, non-deterministic)"
                              : "sequential (one CPU at a time, deterministic — default)");

    // print a table of all processes that will arrive during the simulation
    preGenAndPrint(simTime);

    // initialize shared state: queues, named semaphores, and mutexes
    SharedVars vars;
    init(numCPUs, &vars, quantum);

    int processId = 0;          // PID assigned to the next generated process
    int timeToNextProcess = 0;  // countdown to next process arrival (0 means arrive now)
    int seed = 0;               // rand_r seed; fixed at 0 for deterministic, reproducible output

    // create one scheduling thread per simulated CPU
    pthread_t* cpuThreads = malloc(sizeof(pthread_t) * numCPUs);
    CpuParams* params = malloc(sizeof(CpuParams) * numCPUs);
    int i = 0;
    for (i = 0; i < numCPUs; i++) {
        params[i].threadNumber = i;
        params[i].svars = &vars;
        switch (cpuType) {
            case 1: pthread_create(&(cpuThreads[i]), NULL, &FIFOcpu, &(params[i])); break;
            case 2: pthread_create(&(cpuThreads[i]), NULL, &SJFcpu,  &(params[i])); break;
            case 3: pthread_create(&(cpuThreads[i]), NULL, &NPPcpu,  &(params[i])); break;
            case 4: pthread_create(&(cpuThreads[i]), NULL, &RRcpu,   &(params[i])); break;
            case 5: pthread_create(&(cpuThreads[i]), NULL, &SRTFcpu, &(params[i])); break;
            case 6: pthread_create(&(cpuThreads[i]), NULL, &PPcpu,   &(params[i])); break;
        }
    }

    // main simulation loop — this thread acts as the clock
    fprintf(stderr, "Beginning simulation...\n");
    int time = 0;
    for (time = 0; time < simTime; time++) {

        // optionally print queue state at the start of each timestep (debug mode)
        if (debugOutput) {
            printf("\nstart of time %d\n", time);
            printf("ready:\n");
            qPrint(vars.readyQ);
            printf("\nfinished:\n");
            qPrint(vars.finishedQ);
            printf("\n");
        }

        // generate a new process if one is due to arrive this timestep
        genProcess(&vars, &timeToNextProcess, &processId, &seed, time);

        // Signal CPUs to do one timestep of scheduling work, then wait for all
        // to finish before the clock advances.
        //
        // Sequential (default): signal one CPU at a time and wait for it before
        // signaling the next. Only one CPU holds the ready queue lock per step,
        // so which CPU dequeues which process is fully deterministic. This is
        // required for reproducible output that can be diffed against reference files.
        //
        // Parallel: signal all CPUs at once, then wait for all. CPUs run
        // concurrently and race to dequeue processes; output is non-deterministic
        // with multiple CPUs and cannot be compared against reference output files.
        // Useful for observing true concurrent scheduling or performance experiments.
        int j = 0;
        if (parallelSignaling) {
            for (j = 0; j < numCPUs; j++) {
                sem_post(vars.cpuSems[j]);
            }
            for (j = 0; j < numCPUs; j++) {
                sem_wait(vars.mainSem);
            }
        } else {
            for (j = 0; j < numCPUs; j++) {
                sem_post(vars.cpuSems[j]);
                sem_wait(vars.mainSem);
            }
        }

        // increment wait-time counters for every process still in the ready queue
        incrementWaitTimes(&(vars.readyQ));
    }

    fprintf(stderr, "Simulation finished!\n");

    // print final queue state to stdout (this is the graded output)
    printf("end of simulation\n");
    printf("ready:\n");
    qPrint(vars.readyQ);
    printf("\nfinished:\n");
    qSort(&(vars.finishedQ));
    qPrint(vars.finishedQ);

    // unlink named semaphores so they don't persist in the filesystem;
    // sem_close is omitted because CPU threads are still alive and blocked
    // on sem_wait — the OS reclaims all semaphore resources on process exit
    char semName[MAX_INPUT_LEN];
    semName[0] = '\0';
    pid_t pid = getpid();
    int k = 0;
    for (k = 0; k < numCPUs; k++) {
        snprintf(semName, sizeof(semName), "/pex2_%d_%d", pid, k);
        sem_unlink(semName);
    }
    snprintf(semName, sizeof(semName), "/pex2_%d_m", pid);
    sem_unlink(semName);

    free(cpuThreads);
    free(params);

    return 0;
}

void init(int numProcesses, SharedVars* svars, int quantum) {
    initQueue(&(svars->readyQ));
    initQueue(&(svars->finishedQ));

    // named semaphores are used instead of sem_init because unnamed POSIX semaphores
    // (sem_init) are not supported on macOS; named semaphores work on both platforms
    svars->cpuSems = malloc(sizeof(sem_t*) * numProcesses);
    char name[MAX_INPUT_LEN];
    name[0] = '\0';
    pid_t pid = getpid();
    int i = 0;
    for (i = 0; i < numProcesses; i++) {
        snprintf(name, sizeof(name), "/pex2_%d_%d", pid, i);
        sem_unlink(name);   // remove any stale semaphore left by a prior run
        svars->cpuSems[i] = sem_open(name, O_CREAT | O_EXCL, 0600, 0);
    }
    snprintf(name, sizeof(name), "/pex2_%d_m", pid);
    sem_unlink(name);
    svars->mainSem = sem_open(name, O_CREAT | O_EXCL, 0600, 0);

    pthread_mutex_init(&(svars->readyQLock), NULL);
    pthread_mutex_init(&(svars->finishedQLock), NULL);
    svars->quantum = quantum;
}

void usage(char** argv) {
    fprintf(stderr,
            "\nUsage: %s [debug (0|1) [simTime [numCPUs [cpuType [quantum [parallel (0|1)]]]]]]\n"
            "\n"
            "  All arguments are positional and optional; missing values are prompted\n"
            "  interactively. Supply 0 for quantum when not using Round Robin.\n",
            argv[0]);
    fprintf(stderr,
            "\nCPU types:\n"
            "\t1: First In First Out\n"
            "\t2: Shortest Job First\n"
            "\t3: Priority (non-preemptive)\n"
            "\t4: Round Robin\n"
            "\t5: Shortest Remaining Time First\n"
            "\t6: Priority (preemptive)\n");
    fprintf(stderr,
            "\nSignaling mode (parallel argument, default 0):\n"
            "\t0: Sequential — signal one CPU at a time and wait for it to finish\n"
            "\t   before signaling the next. Execution within each timestep is\n"
            "\t   serialized, making multi-CPU output fully deterministic and\n"
            "\t   reproducible. Required for grading (diff against reference files).\n"
            "\t1: Parallel — signal all CPUs simultaneously, then wait for all to\n"
            "\t   finish. CPUs race to dequeue processes; output is non-deterministic\n"
            "\t   with multiple CPUs and will not match reference output files.\n"
            "\t   Use to observe true concurrent scheduling behavior.\n");
    exit(0);
}

// Pre-generates all processes that would arrive during the simulation using
// the same rand_r seed and arrival logic as the live sim, then prints them
// as a table to stderr. The sim's own seed is independent, so output is unaffected.
void preGenAndPrint(int simTime) {
    unsigned int seed = 0;
    int timeToNext = 0;

    // count arrivals first so we can size the table header
    // then collect into a stack-allocated array (max one process per timestep)
    int arrivalTime[simTime];
    int burst[simTime];
    int priority[simTime];
    int count = 0;

    int t = 0;
    for (t = 0; t < simTime; t++) {
        if (timeToNext <= 0) {
            priority[count] = rand_r(&seed) % MAX_PRIORITY;
            burst[count]    = (rand_r(&seed) % MAX_BURST) + 1;
            arrivalTime[count] = t;
            count++;
            timeToNext = rand_r(&seed) % MAX_TIME_BETWEEN;
        }
        timeToNext--;
    }

    fprintf(stderr, "\n--- Incoming Process Table (seed=0) ---\n");
    fprintf(stderr, "%-6s %-10s %-8s %-8s\n", "PID", "Arrival", "Burst", "Priority");
    fprintf(stderr, "%-6s %-10s %-8s %-8s\n", "---", "-------", "-----", "--------");
    int i = 0;
    for (i = 0; i < count; i++) {
        fprintf(stderr, "%-6d %-10d %-8d %-8d\n", i, arrivalTime[i], burst[i], priority[i]);
    }
    fprintf(stderr, "---------------------------------------\n\n");
}

// Generates a new process and inserts it into the ready queue when
// timeToNextProcess reaches zero; uses rand_r for deterministic output.
void genProcess(SharedVars* shared, int* timeToNextProcess,
                int* nextPid, int* randSeed, int currTime) {
    if ((*timeToNextProcess) <= 0) {    // a process is due to arrive this timestep
        Process* proc = malloc(sizeof(Process));
        proc->PID            = *nextPid;
        proc->arrivalTime    = currTime;
        proc->priority       = rand_r((unsigned int*) randSeed) % MAX_PRIORITY;
        proc->burstTotal     = (rand_r((unsigned int*) randSeed) % MAX_BURST) + 1;
        proc->burstRemaining = proc->burstTotal;
        proc->initialWait    = 0;
        proc->totalWait      = 0;
        proc->requeued       = false;

        qInsert(&(shared->readyQ), proc);

        (*nextPid)++;
        // schedule the next arrival at a random number of timesteps in the future
        (*timeToNextProcess) = rand_r((unsigned int*) randSeed) % MAX_TIME_BETWEEN;
    }
    (*timeToNextProcess)--;     // count down toward the next arrival
}
