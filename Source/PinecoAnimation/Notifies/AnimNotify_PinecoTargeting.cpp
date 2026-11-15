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
    // Only the playback that owns this notification may address its ability's target actor.
    const auto* Playback = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
    const auto* Ability = ASC->GetAnimatingAbility();
    const auto* Info = Ability ? Ability->GetCurrentActorInfo() : nullptr;
    const auto* Anim = Info ? Info->GetAnimInstance() : nullptr;
    const auto* Montage = Cast<UAnimMontage>(Animation);
    const auto* Instance = Anim && Montage ? Anim->GetActiveInstanceForMontage(Montage) : nullptr;
    if (Playback && Instance && Instance->GetInstanceID() == Playback->MontageInstanceID
        && Info->SkeletalMeshComponent.Get() == MeshComp && Ability->GetCurrentMontage() == Montage)
    {
        Payload.ContextHandle = ASC->MakeEffectContext();
        Payload.ContextHandle.SetAbility(Ability);
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
