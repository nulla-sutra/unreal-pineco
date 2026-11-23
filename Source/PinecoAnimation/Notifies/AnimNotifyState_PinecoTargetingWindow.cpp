// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoAnimation/Notifies/AnimNotifyState_PinecoTargetingWindow.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "PinecoGAS/Targeting/PinecoTargetActor.h"
#include "TargetingSystem/TargetingPreset.h"

namespace
{
    APinecoTargetActor* FindWindowTargetActor(const UAbilitySystemComponent& ASC, const USkeletalMeshComponent& Mesh)
    {
        APinecoTargetActor* Result = nullptr;
        for (const auto& Actor : ASC.SpawnedTargetActors)
        {
            auto* Target = Cast<APinecoTargetActor>(Actor.Get());
            if (!IsValid(Target) || Target->IsActorBeingDestroyed())
            {
                continue;
            }

            const auto* Ability = Target->OwningAbility.Get();
            const auto* Info = IsValid(Ability) && Ability->IsActive() ? Ability->GetCurrentActorInfo() : nullptr;
            if (!Info || Info->AvatarActor.Get() != Mesh.GetOwner() || Info->SkeletalMeshComponent.Get() != &Mesh)
            {
                continue;
            }
            // The window can publish to only one active target actor on this mesh.
            if (Result)
            {
                return nullptr;
            }
            Result = Target;
        }
        return Result;
    }

    void ForwardWindow(const UAnimNotifyState_PinecoTargetingWindow& Window,
                       const FBranchingPointNotifyPayload& Payload, EPinecoWindowPhase Phase)
    {
        auto* Mesh = Payload.SkelMeshComponent;
        auto* Avatar = IsValid(Mesh) ? Mesh->GetOwner() : nullptr;
        if (!IsValid(Avatar) || !Avatar->HasAuthority() || !Payload.NotifyEvent || Payload.MontageInstanceID == INDEX_NONE)
        {
            return;
        }
        const auto* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Avatar);
        auto* Target = IsValid(ASC) ? FindWindowTargetActor(*ASC, *Mesh) : nullptr;
        auto* Anim = Mesh->GetAnimInstance();
        auto* Montage = Cast<UAnimMontage>(Payload.SequenceAsset);
        if (!Target || !Anim || !Montage)
        {
            return;
        }

        FPinecoWindowInput Input;
        Input.Source = &Window;
        Input.PlaybackId = Payload.MontageInstanceID;
        if (Phase == EPinecoWindowPhase::Begin)
        {
            const auto* Instance = Anim->GetActiveInstanceForMontage(Montage);
            if (!Instance || Instance->GetInstanceID() != Input.PlaybackId)
            {
                return;
            }
            Input.TravelPreset = Window.TargetingPreset.Get();
            const auto Section = Montage->GetSectionName(Montage->GetSectionIndexFromPosition(Payload.NotifyEvent->GetTime()));
            Input.IsPlaybackCurrent = [WeakAnim = MakeWeakObjectPtr(Anim), WeakMontage = MakeWeakObjectPtr(Montage),
                                       Id = Input.PlaybackId, Section]
            {
                auto* AnimInstance = WeakAnim.Get();
                auto* Asset = WeakMontage.Get();
                const auto* Playing = AnimInstance && Asset ? AnimInstance->GetActiveInstanceForMontage(Asset) : nullptr;
                return Playing && Playing->IsActive() && Playing->GetInstanceID() == Id
                    && Playing->GetCurrentSection() == Section;
            };
        }
        Target->UpdateWindow(Phase, Input);
    }
}

UAnimNotifyState_PinecoTargetingWindow::UAnimNotifyState_PinecoTargetingWindow(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    NotifyName = TEXT("Pineco Targeting Window");
#if WITH_EDITORONLY_DATA
    bShouldFireInEditor = false;
#endif
}

void UAnimNotifyState_PinecoTargetingWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload)
{
    Super::BranchingPointNotifyBegin(Payload);
    ForwardWindow(*this, Payload, EPinecoWindowPhase::Begin);
}

void UAnimNotifyState_PinecoTargetingWindow::BranchingPointNotifyTick(FBranchingPointNotifyPayload& Payload, float FrameDeltaTime)
{
    Super::BranchingPointNotifyTick(Payload, FrameDeltaTime);
    ForwardWindow(*this, Payload, EPinecoWindowPhase::Sample);
}

void UAnimNotifyState_PinecoTargetingWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload)
{
    Super::BranchingPointNotifyEnd(Payload);
    ForwardWindow(*this, Payload, Payload.bReachedEnd ? EPinecoWindowPhase::End : EPinecoWindowPhase::Cancel);
}
