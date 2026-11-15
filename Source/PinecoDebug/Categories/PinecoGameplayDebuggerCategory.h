#pragma once

#include "DrawDebugHelpers.h"

#if WITH_GAMEPLAY_DEBUGGER && ENABLE_DRAW_DEBUG && (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#include "GameplayDebuggerCategory.h"

struct FTargetingRequestHandle;

class FGameplayDebuggerCategory_Pineco final : public FGameplayDebuggerCategory
{
public:
    FGameplayDebuggerCategory_Pineco();
    virtual ~FGameplayDebuggerCategory_Pineco() override;
    virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
    virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& Canvas) override;
    virtual void OnDataPackReplicated(int32 DataPackId) override;
    virtual void OnGameplayDebuggerDeactivated() override;

    static void OnImmediateRequest(const FTargetingRequestHandle& Handle);
    static void OnWindowQuery(const FTargetingRequestHandle& Handle, TConstArrayView<AActor*> NewTargets);

private:
    static void RecordRequest(const FTargetingRequestHandle& Handle, TOptional<TConstArrayView<AActor*>> WindowTargets);
    static bool IsWatching(const AActor* Source);
    static void RecordSector(const AActor* Source, FVector Center, FVector Forward, float Radius, float HalfDegrees);
    static void CaptureFacingSector(const FTargetingRequestHandle& Handle);

    bool Watches(const AActor* Source) const;
    void SelectSource(const AActor* Source);
    enum class EVolume : uint8 { Box, Cylinder, Sphere, Capsule };
    struct FQueryVolume
    {
        EVolume Type = EVolume::Box;
        FVector Start = FVector::ZeroVector;
        FVector Extent = FVector::ZeroVector;
        FRotator Rotation = FRotator::ZeroRotator;

        void Serialize(FArchive& Ar)
        {
            Ar << Type << Start << Extent << Rotation;
        }

        friend FArchive& operator<<(FArchive& Ar, FQueryVolume& V)
        {
            V.Serialize(Ar);
            return Ar;
        }
    };
    struct FDrawPacket
    {
        int16 ActorCounter = 0;
        TArray<FQueryVolume> Volumes;
        TArray<FVector> Points;
        void Serialize(FArchive& Ar) { Ar << ActorCounter << Volumes << Points; }
    };
    struct FWindowPacket
    {
        int16 ActorCounter = 0;
        uint32 Sequence = 0;
        double ServerTime = 0.0;
        float Lifetime = 0.f;
        TArray<FQueryVolume> Volumes;
        void Serialize(FArchive& Ar) { Ar << ActorCounter << Sequence << ServerTime << Lifetime << Volumes; }
    };
    struct FSampleLifetime
    {
        double ExpiresAt = 0.0;
        bool bPending = true;
        bool IsVisible(double Now) const { return bPending || Now <= ExpiresAt; }
    };
    struct FTimedVolume : FSampleLifetime
    {
        FQueryVolume Volume;
    };
    struct FTargetMarker : FSampleLifetime
    {
        FVector Location = FVector::ZeroVector;
    };
    static void ReadVolumes(const FTargetingRequestHandle& Handle, TArray<FQueryVolume>& Volumes);
    static void DrawVolume(UWorld* World, const FQueryVolume& Volume);
    FDrawPacket DrawPacket;
    FWindowPacket WindowPacket;
    int32 WindowPackId = INDEX_NONE;
    uint32 NextWindowSequence = 0;
    uint32 LastDrawnSequence = 0;
    int16 TrailActorCounter = 0;
    TArray<FTimedVolume> LocalWindowTrail;
    TArray<FTimedVolume> PointVolumes;
    TArray<FTargetMarker> WindowHitMarkers;
    TMap<TWeakObjectPtr<AActor>, FTargetMarker> TargetMarkers;
    TWeakObjectPtr<const AActor> SourceActor;
    FGameplayDebuggerShape Sector;
    FString Description;
    double ExpiresAt = 0.0;
    bool bPendingSample = false;
    bool bQueryNeedsNetUpdate = false;
};
#endif
