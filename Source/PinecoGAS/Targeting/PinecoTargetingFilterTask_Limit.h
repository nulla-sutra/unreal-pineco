// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingTask.h"
#include "PinecoTargetingFilterTask_Limit.generated.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPinecoImmediateTargetingRequest, const FTargetingRequestHandle&);
#endif

/** Final count limit configured on a native Targeting Preset's task instance, after filtering and sorting. */
UCLASS(NotBlueprintable)
class PINECOGAS_API UPinecoTargetingFilterTask_Limit : public UTargetingTask
{
    GENERATED_BODY()

public:
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    /** Final results of an immediate request. The handle is live only during this notification. */
    static FOnPinecoImmediateTargetingRequest& OnImmediateRequest();
#endif
    /** Read-only during requests; zero is unlimited. Configure this task instance in the preset. */
    UPROPERTY(EditAnywhere, Category="Targeting|Limit", meta=(ClampMin="0", UIMin="0"))
    int32 MaxTargets = 0;

    /** Keep the first MaxTargets results without changing their hits, scores or relative order; zero is unlimited. */
    virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
