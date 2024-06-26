#include "optimizations/optimizations.h"
#include "optimizations/cf.h"
#include "optimizations/cp.h"
#include "optimizations/dce.h"

void optimization_apply(struct llir *llir, enum optimzation optimizations)
{
	if (optimizations & OPTIMIZATION_CF)
		optimization_constant_folding(llir);
	if (optimizations & OPTIMIZATION_CP)
		optimization_copy_propagation(llir);
	if (optimizations & OPTIMIZATION_DCE)
		optimization_dead_code_elimination(llir);
}
