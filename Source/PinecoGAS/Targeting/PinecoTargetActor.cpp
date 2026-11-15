// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "PinecoGAS/Targeting/PinecoTargetActor.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
#include "PinecoGAS/Targeting/PinecoWindowQueryDebug.h"
#endif
#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Types/TargetingSystemTypes.h"
#include "UObject/StrongObjectPtr.h"

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
DEFINE_TARGETING_DATA_STORE(FPinecoWindowQueryDebug)

FOnPinecoWindowQuery& APinecoTargetActor::OnWindowQuery()
{
    static FOnPinecoWindowQuery Event;
    return Event;
}
#endif

struct APinecoTargetActor::FWindow
{
    TWeakObjectPtr<const UObject> Source;
    int32 PlaybackId = INDEX_NONE;
    TFunction<bool()> IsPlaybackCurrent;
    TStrongObjectPtr<const UTargetingPreset> TravelPreset;
    TSet<TWeakObjectPtr<AActor>> HitActors;
    int32 PendingQueries = 0;
    bool bClosed = false;

    bool IsCurrent() const
    {
        return Source.IsValid() && (!IsPlaybackCurrent || IsPlaybackCurrent());
    }
};

APinecoTargetActor::APinecoTargetActor()
{
    bReplicates = false;
    ShouldProduceTargetDataOnServer = true;
    bDestroyOnConfirmation = false;
    PrimaryActorTick.bCanEverTick = false;
}

void APinecoTargetActor::StartTargeting(UGameplayAbility* Ability)
{
    Super::StartTargeting(Ability);
    const auto* Info = Ability ? Ability->GetCurrentActorInfo() : nullptr;
    BoundASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
    if (!Info || !Info->IsNetAuthority() || !Info->AvatarActor.IsValid() || !BoundASC.IsValid())
    {
        CancelTargeting();
        return;
    }

    SourceActor = Info->AvatarActor.Get();
    if (PointEventTag.IsValid())
    {
        PointBinding = BoundASC->GenericGameplayEventCallbacks.FindOrAdd(PointEventTag)
            .AddUObject(this, &ThisClass::OnPointEvent);
    }
}

bool APinecoTargetActor::IsConfirmTargetingAllowed()
{
    // Only a completed preset query supplies data; generic confirm would publish an empty batch.
    return false;
}

bool APinecoTargetActor::CanTarget() const
{
    return BoundASC.IsValid() && IsValid(SourceActor) && !IsActorBeingDestroyed()
        && IsValid(OwningAbility) && OwningAbility->IsActive() && OwningAbility->GetAvatarActorFromActorInfo() == SourceActor;
}

void APinecoTargetActor::UpdateWindow(EPinecoWindowPhase Phase, const FPinecoWindowInput& Input)
{
    if (!CanTarget() || !IsValid(Input.Source) || Input.PlaybackId == INDEX_NONE)
    {
        return;
    }
    if (Phase == EPinecoWindowPhase::Begin)
    {
        ActiveWindow.Reset();
        if (!IsValid(Input.TravelPreset))
        {
            return;
        }
        const auto Window = MakeShared<FWindow>();
        Window->Source = Input.Source;
        Window->PlaybackId = Input.PlaybackId;
        Window->TravelPreset.Reset(Input.TravelPreset);
        Window->IsPlaybackCurrent = Input.IsPlaybackCurrent;
        ActiveWindow = Window;
    }

    if (!ActiveWindow || ActiveWindow->Source.Get() != Input.Source || ActiveWindow->PlaybackId != Input.PlaybackId)
    {
        return;
    }
    const auto Window = ActiveWindow;
    if (Phase == EPinecoWindowPhase::Cancel || !Window->IsCurrent())
    {
        ActiveWindow.Reset();
        return;
    }
    if (Window->bClosed)
    {
        return;
    }
    if (Phase == EPinecoWindowPhase::End)
    {
        Window->bClosed = true;
        if (Window->PendingQueries == 0)
        {
            ActiveWindow.Reset();
        }
        return;
    }
    // Sample even at rest: another actor can enter the box while the attacker is stationary.
    ++Window->PendingQueries;
    ExecuteQuery(*Window->TravelPreset.Get(), Window);
}

void APinecoTargetActor::OnPointEvent(const FGameplayEventData* Payload)
{
    if (!Payload || !CanTarget()
        || Payload->Instigator != SourceActor || Payload->Target != SourceActor
        || Payload->ContextHandle.GetAbilityInstance_NotReplicated() != OwningAbility)
    {
        return;
    }

    const auto* Preset = Cast<UTargetingPreset>(Payload->OptionalObject.Get());
    if (IsValid(Preset))
    {
        ExecuteQuery(*Preset);
    }
}

void APinecoTargetActor::ExecuteQuery(const UTargetingPreset& Preset, const TSharedPtr<FWindow>& Window)
{
    auto* Subsystem = UTargetingSubsystem::Get(GetWorld());
    if (!Subsystem)
    {
        if (Window && Window == ActiveWindow)
        {
            ActiveWindow.Reset();
        }
        return;
    }

    FTargetingSourceContext Source;
    Source.SourceActor = SourceActor;
    Source.InstigatorActor = SourceActor;
    auto Handle = UTargetingSubsystem::MakeTargetRequestHandle(&Preset, Source);
    FTargetingImmediateTaskData::FindOrAdd(Handle).bReleaseOnCompletion = true;
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    if (Window)
    {
        UE::TargetingSystem::TTargetingDataStore<FPinecoWindowQueryDebug>::FindOrAdd(Handle);
    }
#endif

    // The subsystem stores a pointer to the preset's task set and may defer an immediate request.
    Subsystem->ExecuteTargetingRequestWithHandle(Handle, FTargetingRequestDelegate::CreateWeakLambda(this,
        [this, Window, Subsystem, KeepPreset = TStrongObjectPtr<const UTargetingPreset>(&Preset)](FTargetingRequestHandle Result)
        {
            if (!CanTarget() || (Window && ActiveWindow != Window))
            {
                return;
            }
            if (Window && !Window->IsCurrent())
            {
                ActiveWindow.Reset();
                return;
            }
            TArray<AActor*> Actors;
            Subsystem->GetTargetingResultsActors(Result, Actors);
            if (Window)
            {
                Actors.RemoveAll([&](AActor* Actor)
                {
                    bool bAlreadyHit = false;
                    Window->HitActors.Add(Actor, &bAlreadyHit);
                    return bAlreadyHit;
                });
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
                OnWindowQuery().Broadcast(Result, Actors);
#endif
            }
            if (!Actors.IsEmpty())
            {
                TargetDataReadyDelegate.Broadcast(UAbilitySystemBlueprintLibrary::AbilityTargetDataFromActorArray(Actors, false));
            }
            if (Window && ActiveWindow == Window)
            {
                --Window->PendingQueries;
                if (Window->bClosed && Window->PendingQueries == 0)
                {
                    ActiveWindow.Reset();
                }
            }
        }));
}

void APinecoTargetActor::EndPlay(EEndPlayReason::Type EndPlayReason)
{
    ActiveWindow.Reset();
    if (auto* ASC = BoundASC.Get())
    {
        if (auto* Callbacks = ASC->GenericGameplayEventCallbacks.Find(PointEventTag))
        {
            Callbacks->Remove(PointBinding);
        }
        ASC->SpawnedTargetActors.Remove(this);
    }
    BoundASC.Reset();
    Super::EndPlay(EndPlayReason);
}
