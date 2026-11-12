// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "PinecoTargetingFilterTask_GAS.generated.h"

/** ASC eligibility only; native Targeting owns iteration, result removal and task completion. */
UCLASS(NotBlueprintable)
class PINECOGAS_API UPinecoTargetingFilterTask_GAS : public UTargetingFilterTask_BasicFilterTemplate
{
    GENERATED_BODY()

public:
    UPinecoTargetingFilterTask_GAS(const FObjectInitializer& ObjectInitializer);

protected:
    /** True excludes invalid targets, the source actor, other worlds and targets without a valid ASC. */
    virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
                                    const FTargetingDefaultResultData& TargetData) const override;
};
