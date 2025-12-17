#include "Greedy.h"
#include "JSSPLib.h" // solveJSSP declaration

std::vector<ScheduledOp> GreedySolver::solve(const Problem& p)
{
    return solveJSSP(p);
}