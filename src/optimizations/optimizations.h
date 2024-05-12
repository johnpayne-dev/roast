#pragma once
#include "assembly/llir.h"

enum optimzation {
	OPTIMIZATION_CF = 1 << 0,
	OPTIMIZATION_CP = 1 << 1,
	OPTIMIZATION_DCE = 1 << 2,
	OPTIMIZATION_ALL = ~0,
};

void optimization_apply(struct llir *llir, enum optimzation optimizations);
