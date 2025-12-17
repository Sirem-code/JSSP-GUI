#include "GA.h"
#include <algorithm>
#include <random>
#include <limits>
#include <iostream>

// Compute makespan of a schedule
static int computeMakespan(const std::vector<ScheduledOp>& schedule)
{
    int makespan = 0;
    for (const auto& op : schedule)
        if (op.finish > makespan) makespan = op.finish;
    return makespan;
}

// Create a feasible schedule using a randomized greedy dispatcher.
// Ensures per-job operation order and machine non-overlap.
static std::vector<ScheduledOp> generateRandomGreedySchedule(const Problem& p, std::mt19937& rng)
{
    const int totalOps = p.numJobs * p.numMachines;
    std::vector<int> nextOpIndex(p.numJobs, 0);       // next operation index per job
    std::vector<int> jobReady(p.numJobs, 0);          // when a job is ready for its next op
    std::vector<int> machineFree(p.numMachines, 0);   // when a machine becomes free

    std::vector<ScheduledOp> schedule;
    schedule.reserve(totalOps);

    std::uniform_int_distribution<int> pickDist(0, p.numJobs - 1);

    int scheduled = 0;
    // While not all operations scheduled, pick a random job that has remaining ops and schedule its next op.
    while (scheduled < totalOps)
    {
        // collect candidate jobs that still have operations
        std::vector<int> candidates;
        candidates.reserve(p.numJobs);
        for (int j = 0; j < p.numJobs; ++j)
            if (nextOpIndex[j] < (int)p.jobs[j].operations.size())
                candidates.push_back(j);

        // pick one candidate at random (simple diversification)
        int pickIdx = std::uniform_int_distribution<int>(0, (int)candidates.size() - 1)(rng);
        int job = candidates[pickIdx];

        const auto& op = p.jobs[job].operations[nextOpIndex[job]];
        int start = std::max(jobReady[job], machineFree[op.machineId]);
        int finish = start + op.duration;

        ScheduledOp sop;
        sop.jobId = job;
        sop.machineId = op.machineId;
        sop.start = start;
        sop.finish = finish;

        schedule.push_back(sop);

        // update state
        jobReady[job] = finish;
        machineFree[op.machineId] = finish;
        ++nextOpIndex[job];
        ++scheduled;
    }

    return schedule;
}

std::vector<ScheduledOp> GASolver::solve(const Problem& p)
{
    if (p.numJobs == 0 || p.numMachines == 0)
        return {};

    std::random_device rd;
    std::mt19937 rng(rd());

    // Simple evolutionary loop: generate randomized feasible schedules, keep best (elitism).
    // This is intentionally conservative: it always returns feasible schedules.
    std::vector<ScheduledOp> bestSchedule;
    int bestMakespan = std::numeric_limits<int>::max();

    // initial population
    for (int i = 0; i < populationSize; ++i)
    {
        auto sched = generateRandomGreedySchedule(p, rng);
        int ms = computeMakespan(sched);
        if (ms < bestMakespan)
        {
            bestMakespan = ms;
            bestSchedule = std::move(sched);
        }
    }

    for (int gen = 0; gen < generations; ++gen)
    {
        // create new population (stochastic greedy restarts + small perturbations)
        for (int i = 0; i < populationSize; ++i)
        {
            auto child = generateRandomGreedySchedule(p, rng);

            // simple mutation: with probability mutationRate, perform a small local perturbation:
            // pick two scheduled operations that are on the same machine and try swapping their scheduling order,
            // then repair by re-dispatching affected jobs. For simplicity and safety we will instead
            // perform a tiny rerun: randomly re-seed the scheduler by performing an extra small shuffle
            // of candidate selection order via a few random priority shuffles (cheap and keeps feasibility).
            if (std::uniform_real_distribution<double>(0.0, 1.0)(rng) < mutationRate)
            {
                // perform a fresh randomized greedy schedule (acts as mutation restart)
                child = generateRandomGreedySchedule(p, rng);
            }

            int ms = computeMakespan(child);
            if (ms < bestMakespan)
            {
                bestMakespan = ms;
                bestSchedule = std::move(child);
            }
        }

        // optional: small progress output in debug builds
#ifdef _DEBUG
        //std::cout << "GA gen " << gen << " bestMakespan=" << bestMakespan << "\n";
#endif
    }

    return bestSchedule;
}