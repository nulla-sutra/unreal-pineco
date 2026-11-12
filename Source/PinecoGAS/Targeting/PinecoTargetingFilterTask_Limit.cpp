// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoGAS/Targeting/PinecoTargetingFilterTask_Limit.h"

#include "Misc/ScopeExit.h"
#include "Types/TargetingSystemTypes.h"

void UPinecoTargetingFilterTask_Limit::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
    if (!IsInGameThread() || !TargetingHandle.IsValid())
    {
        return;
    }

    Super::Execute(TargetingHandle);
    SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
    ON_SCOPE_EXIT
    {
        SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
    };

    auto* ResultData = FTargetingDefaultResultsSet::Find(TargetingHandle);
    if (!ResultData)
    {
        return;
    }

    if (MaxTargets < 0)
    {
        ResultData->TargetResults.Reset();
        return;
    }

    if (MaxTargets > 0 && ResultData->TargetResults.Num() > MaxTargets)
    {
        ResultData->TargetResults.SetNum(MaxTargets, EAllowShrinking::No);
    }
}
