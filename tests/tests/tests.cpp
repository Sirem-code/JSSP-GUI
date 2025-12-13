#define CATCH_CONFIG_MAIN
#include <catch.hpp>

#include "../JSSP/JSSPLib.h"
#include <fstream>
#include <cstdio>
#include <string>

// Helper to write a small test problem file.
// Format: first two integers -> numJobs numMachines, then for each job, numMachines pairs (machine duration)
static std::string writeTempProblemFile(const std::string& path)
{
    std::ofstream out(path);
    REQUIRE(out.good());
    // example: 2 jobs, 2 machines
    out << "2 2\n";
    // job 0: machine 0 dur 3, machine 1 dur 2
    out << "0 3 1 2\n";
    // job 1: machine 1 dur 4, machine 0 dur 1
    out << "1 4 0 1\n";
    out.close();
    return path;
}

TEST_CASE("filenameFromPath handles paths", "[util]")
{
    REQUIRE(filenameFromPath("C:\\path\\to\\file.txt") == "file.txt");
    REQUIRE(filenameFromPath("/home/user/example.dat") == "example.dat");
    REQUIRE(filenameFromPath("simplefile") == "simplefile");
}

TEST_CASE("loadProblem reads a valid file and reports sizes", "[io]")
{
    const std::string tmp = "tests_tmp_problem.txt";
    writeTempProblemFile(tmp);

    Problem p = loadProblem(tmp);
    REQUIRE(p.numJobs == 2);
    REQUIRE(p.numMachines == 2);
    REQUIRE(p.jobs.size() == 2);
    REQUIRE(p.jobs[0].operations.size() == 2);
    REQUIRE(p.jobs[1].operations.size() == 2);

    // cleanup
    std::remove(tmp.c_str());
}

TEST_CASE("solveJSSP produces schedule of correct size and non-overlapping per job", "[solver]")
{
    Problem p;
    p.name = "test";
    p.numJobs = 2;
    p.numMachines = 2;
    p.jobs.resize(2);
    // job 0 ops
    p.jobs[0].operations.push_back({ 0, 0, 3 });
    p.jobs[0].operations.push_back({ 0, 1, 2 });
    // job 1 ops
    p.jobs[1].operations.push_back({ 1, 1, 4 });
    p.jobs[1].operations.push_back({ 1, 0, 1 });

    auto schedule = solveJSSP(p);
    REQUIRE(schedule.size() == 4);

    // each job's operations must have non-decreasing start times
    std::vector<int> lastFinish(p.numJobs, 0);
    for (const auto& op : schedule)
    {
        REQUIRE(op.start >= 0);
        REQUIRE(op.finish >= op.start);
        REQUIRE(op.start >= lastFinish[op.jobId]);
        lastFinish[op.jobId] = op.finish;
    }
}

TEST_CASE("exportSolutionTXT writes output file", "[io]")
{
    Problem p;
    p.name = "myprob";
    p.numJobs = 1;
    p.numMachines = 1;
    p.jobs.resize(1);
    p.jobs[0].operations.push_back({ 0,0,5 });
    auto schedule = solveJSSP(p);

    const std::string outpath = "tests_tmp_solution.txt";
    exportSolutionTXT(p, schedule, outpath);

    std::ifstream in(outpath);
    REQUIRE(in.good());
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    REQUIRE(contents.find("Makespan:") != std::string::npos);
    std::remove(outpath.c_str());
}