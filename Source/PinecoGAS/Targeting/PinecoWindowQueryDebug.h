#pragma once

#include "CoreTypes.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
#include "Types/TargetingSystemDataStores.h"

// A window reports after cross-tick deduplication, even when its preset ends in a Limit task.
// The native store follows the handle through deferred execution and removes the tag on release.
struct FPinecoWindowQueryDebug {};
DECLARE_TARGETING_DATA_STORE(FPinecoWindowQueryDebug)
#endif
