// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_PinecoTargeting.generated.h"

class UTargetingPreset;

/** Emits a local GameplayEvent carrying a native preset; the receiving ability owns targeting and effects. */
UCLASS(meta=(DisplayName="Pineco Targeting"))
class PINECOANIMATION_API UAnimNotify_PinecoTargeting : public UAnimNotify
{
    GENERATED_BODY()

public:
    UAnimNotify_PinecoTargeting();

    /** Local GameplayEvent consumed by the project's native WaitGameplayEvent task. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pineco|Targeting")
    FGameplayTag EventTag;

    /** Shared asset configuration, passed as GameplayEvent.OptionalObject without executing or modifying it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pineco|Targeting")
    TObjectPtr<UTargetingPreset> TargetingPreset = nullptr;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
                        const FAnimNotifyEventReference& EventReference) override;

    virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
    virtual FString GetNotifyName_Implementation() const override;
};
