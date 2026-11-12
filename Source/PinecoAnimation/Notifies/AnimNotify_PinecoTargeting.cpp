// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoAnimation/Notifies/AnimNotify_PinecoTargeting.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TargetingSystem/TargetingPreset.h"

UAnimNotify_PinecoTargeting::UAnimNotify_PinecoTargeting()
{
#if WITH_EDITORONLY_DATA
    bShouldFireInEditor = false;
#endif
}

void UAnimNotify_PinecoTargeting::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                                       const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    const auto* World = IsValid(MeshComp) ? MeshComp->GetWorld() : nullptr;
    auto* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
    if (!World || !World->IsGameWorld() || !IsValid(Owner) || !IsValid(Animation) || !EventTag.IsValid()
        || !IsValid(TargetingPreset.Get())
        || !IsValid(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner)))
    {
        return;
    }

    FGameplayEventData Payload;
    Payload.EventTag = EventTag;
    Payload.Instigator = Owner;
    Payload.Target = Owner;
    // Notify objects are shared across actors and playbacks; only pass immutable asset configuration.
    Payload.OptionalObject = TargetingPreset;
    Payload.OptionalObject2 = Animation;
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

FString UAnimNotify_PinecoTargeting::GetNotifyName_Implementation() const
{
    return TEXT("Pineco Targeting");
}
