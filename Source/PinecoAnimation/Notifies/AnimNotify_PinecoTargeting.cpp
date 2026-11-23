// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoAnimation/Notifies/AnimNotify_PinecoTargeting.h"

#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PinecoGAS/Targeting/PinecoTargetActor.h"
#include "TargetingSystem/TargetingPreset.h"

namespace
{
    const UGameplayAbility* FindPointTargetAbility(const UAbilitySystemComponent& ASC,
        const USkeletalMeshComponent& Mesh, FGameplayTag EventTag)
    {
        const UGameplayAbility* Result = nullptr;
        for (const auto& Actor : ASC.SpawnedTargetActors)
        {
            const auto* Target = Cast<APinecoTargetActor>(Actor.Get());
            if (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->PointEventTag != EventTag)
            {
                continue;
            }

            const auto* Ability = Target->OwningAbility.Get();
            const auto* Info = IsValid(Ability) && Ability->IsActive() ? Ability->GetCurrentActorInfo() : nullptr;
            if (!Info || Info->AvatarActor.Get() != Mesh.GetOwner() || Info->SkeletalMeshComponent.Get() != &Mesh)
            {
                continue;
            }
            // One event must address at most one target actor on this ASC.
            if (Result)
            {
                return nullptr;
            }
            Result = Ability;
        }
        return Result;
    }
}

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
    auto* ASC = IsValid(Owner) ? UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner) : nullptr;
    if (!World || !World->IsGameWorld() || !IsValid(Owner) || !IsValid(Animation) || !EventTag.IsValid()
        || !IsValid(TargetingPreset.Get()) || !IsValid(ASC))
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
    // Route the live playback to the target actor listening for this event.
    const auto* Playback = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    const auto* Anim = MeshComp->GetAnimInstance();
    const auto* Montage = Cast<UAnimMontage>(Animation);
    const auto* Instance = Anim && Montage ? Anim->GetActiveInstanceForMontage(Montage) : nullptr;
    if (Playback && Instance && Instance->GetInstanceID() == Playback->MontageInstanceID)
    {
        if (const auto* Ability = FindPointTargetAbility(*ASC, *MeshComp, EventTag))
        {
            Payload.ContextHandle = ASC->MakeEffectContext();
            Payload.ContextHandle.SetAbility(Ability);
        }
    }
    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, Payload);
}

void UAnimNotify_PinecoTargeting::BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload)
{
    // The base branching-point adapter calls Notify with an empty reference.
    FAnimNotifyEventReference Reference(BranchingPointPayload.NotifyEvent, BranchingPointPayload.SequenceAsset);
    Reference.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(BranchingPointPayload.MontageInstanceID);
    Notify(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, Reference);
}

FString UAnimNotify_PinecoTargeting::GetNotifyName_Implementation() const
{
    return TEXT("Pineco Targeting");
}
