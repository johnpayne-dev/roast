#pragma once
#include "assembly/llir.h"

void ssa_transform(struct llir *llir);

void ssa_inverse_transform(struct llir *llir);
