#pragma once
#include "JSSPLib.h"
#include <vector>

struct ISolver
{
    virtual ~ISolver() = default;
    virtual const char* name() const = 0;
    virtual std::vector<ScheduledOp> solve(const Problem& p) = 0;
};

// Genetic Algorithm Solver (practical, feasibility-first implementation)
struct GASolver : public ISolver
{
    // Parameters (tweakable from UI)
    int populationSize = 30;
    int generations = 50;
    double mutationRate = 0.1;
    int tournamentSize = 3;

    const char* name() const override { return "Genetic Algorithm"; }
    std::vector<ScheduledOp> solve(const Problem& p) override;
};