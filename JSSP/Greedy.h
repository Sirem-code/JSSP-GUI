#pragma once

#include "GA.h" // defines ISolver and includes JSSPLib.h

// Simple Greedy solver adapter that exposes the existing solveJSSP() as an ISolver.
struct GreedySolver : public ISolver
{
    const char* name() const override { return "Greedy"; }
    std::vector<ScheduledOp> solve(const Problem& p) override;
};
