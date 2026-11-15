// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoGAS/Targeting/PinecoTargetingFilterTask_Limit.h"

#include "Misc/ScopeExit.h"
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
#include "PinecoGAS/Targeting/PinecoWindowQueryDebug.h"
#endif
#include "Types/TargetingSystemTypes.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
FOnPinecoImmediateTargetingRequest& UPinecoTargetingFilterTask_Limit::OnImmediateRequest()
{
    static FOnPinecoImmediateTargetingRequest Event;
    return Event;
}
#endif

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
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
        // Later tasks could still remove targets; publish here only when this is the terminal task.
        if (OnImmediateRequest().IsBound() && !IsAsyncTargetingRequest(TargetingHandle)
            && !UE::TargetingSystem::TTargetingDataStore<FPinecoWindowQueryDebug>::Find(TargetingHandle))
        {
            const auto* TaskSet = FTargetingTaskSet::Find(TargetingHandle);
            if (TaskSet && *TaskSet && !(*TaskSet)->Tasks.IsEmpty() && (*TaskSet)->Tasks.Last() == this)
            {
                OnImmediateRequest().Broadcast(TargetingHandle);
            }
        }
#endif
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
