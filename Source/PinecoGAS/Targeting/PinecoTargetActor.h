// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTargetActor.h"
#include "GameplayTagContainer.h"
#include "PinecoTargetActor.generated.h"

class UAbilitySystemComponent;
class UTargetingPreset;
struct FGameplayEventData;
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
struct FTargetingRequestHandle;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPinecoWindowQuery, const FTargetingRequestHandle&, TConstArrayView<AActor*>);
#endif

enum class EPinecoWindowPhase : uint8 { Begin, Sample, End, Cancel };

/** Local input from a window producer. Only Begin supplies configuration. */
struct FPinecoWindowInput
{
    const UObject* Source = nullptr;
    int32 PlaybackId = INDEX_NONE;
    const UTargetingPreset* TravelPreset = nullptr;
    TFunction<bool()> IsPlaybackCurrent;
};

/** Produces authoritative preset results for GAS WaitTargetData(CustomMulti). */
UCLASS(NotPlaceable)
class PINECOGAS_API APinecoTargetActor : public AGameplayAbilityTargetActor
{
    GENERATED_BODY()

public:
    APinecoTargetActor();

    /** Exact local event for a point query. An empty tag disables point events. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pineco|Targeting", meta=(ExposeOnSpawn=true))
    FGameplayTag PointEventTag;

    void UpdateWindow(EPinecoWindowPhase Phase, const FPinecoWindowInput& Input);

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
    /** Completed window sample, including misses. The handle and new actors are valid during the callback. */
    static FOnPinecoWindowQuery& OnWindowQuery();
#endif

    virtual void StartTargeting(UGameplayAbility* Ability) override;
    virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
    virtual bool IsConfirmTargetingAllowed() override;

private:
    struct FWindow;

    bool CanTarget() const;
    void OnPointEvent(const FGameplayEventData* Payload);
    void ExecuteQuery(const UTargetingPreset& Preset, const TSharedPtr<FWindow>& Window = nullptr);

    TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
    FDelegateHandle PointBinding;
    // One active attack window per targeting execution; a newer Begin supersedes pending work.
    TSharedPtr<FWindow> ActiveWindow;
};
