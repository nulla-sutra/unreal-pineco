// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "AnimNotifies/AnimNotify_PlayMontageNotify.h"
#include "AnimNotifyState_PinecoTargetingWindow.generated.h"

class UTargetingPreset;

/** Declares which native preset a targeting consumer uses during this montage interval. */
UCLASS(meta=(DisplayName="Pineco Targeting Window"))
class PINECOANIMATION_API UAnimNotifyState_PinecoTargetingWindow : public UAnimNotify_PlayMontageNotifyWindow
{
    GENERATED_BODY()

public:
    explicit UAnimNotifyState_PinecoTargetingWindow(const FObjectInitializer& ObjectInitializer);

    /** Shared configuration. Hit history belongs to the execution of the window on each actor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pineco|Targeting")
    TObjectPtr<UTargetingPreset> TargetingPreset = nullptr;

    virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& Payload) override;
    virtual void BranchingPointNotifyTick(FBranchingPointNotifyPayload& Payload, float FrameDeltaTime) override;
    virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& Payload) override;
};
