#include "JSSPLib.h"

#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <climits> // for INT_MAX / INT_MIN

// Helper: extract filename from a path without <filesystem>
std::string filenameFromPath(const std::string& path)
{
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

// Load JSSP problem from file
Problem loadProblem(const std::string& filepath)
{
    Problem p;
    p.name = filenameFromPath(filepath);
    std::ifstream in(filepath);
    if (!in)
        throw std::runtime_error("Cannot open file: " + filepath);

    in >> p.numJobs >> p.numMachines;
    if (!in || p.numJobs <= 0 || p.numMachines <= 0)
        throw std::runtime_error("Invalid header in problem file: " + filepath);

    p.jobs.clear();
    p.jobs.resize(p.numJobs);

    for (int j = 0; j < p.numJobs; j++)
    {
        for (int o = 0; o < p.numMachines; o++)
        {
            int m, d;
            in >> m >> d;
            if (!in)
                throw std::runtime_error("Unexpected EOF or parse error in file: " + filepath);
            if (m < 0 || m >= p.numMachines)
                throw std::runtime_error("Machine id out of range in file: " + filepath);
            if (d < 0)
                throw std::runtime_error("Negative duration in file: " + filepath);
            p.jobs[j].operations.push_back({ j, m, d });
        }
    }
    return p;
}

// Greedy solver
std::vector<ScheduledOp> solveJSSP(const Problem& p)
{
    std::vector<MachineSchedule> machines(p.numMachines);
    std::vector<int> jobCompletionTime(p.numJobs, 0);
    std::vector<ScheduledOp> schedule;

    for (auto& job : p.jobs)
    {
        for (auto& op : job.operations)
        {
            int startTime = std::max(jobCompletionTime[op.jobId], machines[op.machineId].time);
            int finishTime = startTime + op.duration;
            machines[op.machineId].time = finishTime;
            jobCompletionTime[op.jobId] = finishTime;
            schedule.push_back({ op.jobId, op.machineId, startTime, finishTime });
        }
    }
    return schedule;
}

// Export solution to simple TXT
void exportSolutionTXT(const Problem& p, const std::vector<ScheduledOp>& schedule, const std::string& filename)
{
    std::ofstream out(filename);
    if (!out)
    {
        std::cerr << "Error: Cannot write to " << filename << "\n";
        return;
    }
    int makespan = 0;
    for (auto& op : schedule)
        if (op.finish > makespan)
            makespan = op.finish;

    out << "Problem: " << p.name << "\n";
    out << "Jobs: " << p.numJobs << "\n";
    out << "Machines: " << p.numMachines << "\n";
    out << "Makespan: " << makespan << "\n";
    out << "Schedule (job machine start finish):\n";
    for (auto& op : schedule)
    {
        out << op.jobId << " " << op.machineId << " " << op.start << " " << op.finish << "\n";
    }
    std::cout << "Solution exported to " << filename << "\n";
}