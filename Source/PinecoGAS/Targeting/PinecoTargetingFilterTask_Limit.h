// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingTask.h"
#include "PinecoTargetingFilterTask_Limit.generated.h"

/** Final count limit configured on a native Targeting Preset's task instance, after filtering and sorting. */
UCLASS(NotBlueprintable)
class PINECOGAS_API UPinecoTargetingFilterTask_Limit : public UTargetingTask
{
    GENERATED_BODY()

public:
    /** Read-only during requests; zero is unlimited. Configure this task instance in the preset. */
    UPROPERTY(EditAnywhere, Category="Targeting|Limit", meta=(ClampMin="0", UIMin="0"))
    int32 MaxTargets = 0;

    /** Keep the first MaxTargets results without changing their hits, scores or relative order; zero is unlimited. */
    virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
