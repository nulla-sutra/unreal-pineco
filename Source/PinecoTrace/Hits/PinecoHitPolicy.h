// Copyright 2019-Present tarnishablec. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PinecoHitPolicy.generated.h"

/** Value-only policy for one hit group's lifetime; accepted-hit history belongs to the group. */
USTRUCT(BlueprintType)
struct PINECOTRACE_API FPinecoHitPolicy
{
    GENERATED_BODY()

    /** Count each valid body separately instead of sharing an actor-wide allowance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pineco|Hit")
    bool bPerBody = false;

    /** Maximum accepted hits per target identity over the group's lifetime; zero is unlimited. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pineco|Hit", meta=(ClampMin="0"))
    int32 MaxHitsPerTarget = 1;

    /** Minimum simulation seconds between accepted hits on the same identity; zero adds no delay. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pineco|Hit", meta=(ClampMin="0", Units="s"))
    double MinHitIntervalSeconds = 0.0;
};
