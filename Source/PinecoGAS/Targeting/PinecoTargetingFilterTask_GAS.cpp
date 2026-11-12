// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoGAS/Targeting/PinecoTargetingFilterTask_GAS.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Types/TargetingSystemTypes.h"

UPinecoTargetingFilterTask_GAS::UPinecoTargetingFilterTask_GAS(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UPinecoTargetingFilterTask_GAS::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle,
                                                      const FTargetingDefaultResultData& TargetData) const
{
    if (!IsInGameThread() || !TargetingHandle.IsValid())
    {
        return true;
    }

    const auto* SourceContext = FTargetingSourceContext::Find(TargetingHandle);
    const auto* SourceActor = SourceContext ? SourceContext->SourceActor.Get() : nullptr;
    const auto* World = IsValid(SourceActor) ? SourceActor->GetWorld() : nullptr;
    auto* TargetActor = TargetData.HitResult.GetActor();
    if (!IsValid(World) || !IsValid(TargetActor) || TargetActor == SourceActor
        || TargetActor->GetWorld() != World)
    {
        return true;
    }

    const auto* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    return !IsValid(TargetASC) || TargetASC->GetWorld() != World;
}
