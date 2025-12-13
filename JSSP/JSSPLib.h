#pragma once

#include <string>
#include <vector>

// no std::filesystem to keep dependencies minimal

struct Operation
{
    int jobId;
    int machineId;
    int duration;
};

struct Job
{
    std::vector<Operation> operations;
};

struct MachineSchedule
{
    int time = 0;
};

struct ScheduledOp
{
    int jobId;
    int machineId;
    int start;
    int finish;
};

struct Problem
{
    std::string name;
    int numJobs = 0;
    int numMachines = 0;
    std::vector<Job> jobs;
};

// Helper: extract filename from a path without <filesystem>
std::string filenameFromPath(const std::string& path);

// Load JSSP problem from file
Problem loadProblem(const std::string& filepath);

// Greedy solver
std::vector<ScheduledOp> solveJSSP(const Problem& p);

// Export solution to simple TXT
void exportSolutionTXT(const Problem& p, const std::vector<ScheduledOp>& schedule, const std::string& filename);