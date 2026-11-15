#include "PinecoDebug/Categories/PinecoGameplayDebuggerCategory.h"

#if WITH_GAMEPLAY_DEBUGGER && ENABLE_DRAW_DEBUG && (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Tasks/TargetingSelectionTask_AOE.h"
#include "Types/TargetingSystemTypes.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogPinecoTargetingDisplay, Log, All);

namespace
{
    TArray<FGameplayDebuggerCategory_Pineco*> Instances;

    double GetServerTime(const UWorld& World)
    {
        const auto* GameState = World.GetGameState();
        return GameState ? GameState->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
    }
}

FGameplayDebuggerCategory_Pineco::FGameplayDebuggerCategory_Pineco()
{
    // The client rebuilds the native outlines from compact query parameters.
    CollectDataInterval = 0.05f;
    SetDataPackReplication(&DrawPacket);
    // A large batch of hit markers must not hold up the next window pose.
    WindowPackId = SetDataPackReplication(&WindowPacket, EGameplayDebuggerDataPack::Persistent);
    Instances.Add(this);
}

FGameplayDebuggerCategory_Pineco::~FGameplayDebuggerCategory_Pineco()
{
    Instances.RemoveSingleSwap(this);
}

bool FGameplayDebuggerCategory_Pineco::Watches(const AActor* Source) const
{
    const auto* Rep = GetReplicator();
    return Rep && Rep->HasAuthority() && Rep->IsEnabled() && IsCategoryEnabled() && Rep->GetDebugActor() == Source;
}

bool FGameplayDebuggerCategory_Pineco::IsWatching(const AActor* Source)
{
    return Source && Instances.ContainsByPredicate([Source](const auto* Instance) { return Instance->Watches(Source); });
}

void FGameplayDebuggerCategory_Pineco::SelectSource(const AActor* Source)
{
    if (SourceActor.Get() != Source)
    {
        OnGameplayDebuggerDeactivated();
        SourceActor = Source;
    }
}

void FGameplayDebuggerCategory_Pineco::ReadVolumes(const FTargetingRequestHandle& Handle, TArray<FQueryVolume>& Volumes)
{
    const auto* TaskSet = FTargetingTaskSet::Find(Handle);
    const auto* Context = FTargetingSourceContext::Find(Handle);
    const auto* Source = Context ? Context->SourceActor.Get() : nullptr;
    if (!TaskSet || !*TaskSet || !IsValid(Source)) return;
    for (const auto& Task : (*TaskSet)->Tasks)
    {
        FQueryVolume V;
        if (const auto* AOE = Cast<UTargetingSelectionTask_AOE>(Task.Get());
            AOE && AOE->GetClass() == UTargetingSelectionTask_AOE::StaticClass())
        {
            V.Start = AOE->GetSourceLocation(Handle) + AOE->GetSourceOffset(Handle);
            V.Rotation = (AOE->GetSourceRotation(Handle) * AOE->GetSourceRotationOffset(Handle).Quaternion()).Rotator();
            const auto Shape = AOE->GetCollisionShape();
            switch (AOE->GetShapeType())
            {
            case ETargetingAOEShape::Box:
                V.Type = EVolume::Box;
                V.Extent = Shape.GetExtent();
                break;
            case ETargetingAOEShape::Cylinder:
                V.Type = EVolume::Cylinder;
                V.Extent = Shape.GetExtent();
                break;
            case ETargetingAOEShape::Sphere:
                V.Type = EVolume::Sphere;
                V.Extent.X = Shape.GetSphereRadius();
                break;
            case ETargetingAOEShape::Capsule:
                V.Type = EVolume::Capsule;
                V.Extent.X = Shape.GetCapsuleRadius();
                V.Extent.Z = Shape.GetCapsuleHalfHeight();
                break;
            default:
                continue;
            }
        }
        else
        {
            continue;
        }
        if (!V.Start.ContainsNaN() && !V.Rotation.ContainsNaN() && !V.Extent.ContainsNaN())
        {
            Volumes.Add(V);
        }
    }
}

void FGameplayDebuggerCategory_Pineco::OnImmediateRequest(const FTargetingRequestHandle& Handle)
{
    RecordRequest(Handle, {});
}

void FGameplayDebuggerCategory_Pineco::OnWindowQuery(const FTargetingRequestHandle& Handle, TConstArrayView<AActor*> NewTargets)
{
    RecordRequest(Handle, NewTargets);
}

void FGameplayDebuggerCategory_Pineco::RecordRequest(const FTargetingRequestHandle& Handle, TOptional<TConstArrayView<AActor*>> WindowTargets)
{
    const auto* Context = FTargetingSourceContext::Find(Handle);
    const auto* Source = Context ? Context->SourceActor.Get() : nullptr;
    if (!IsValid(Source) || !IsWatching(Source)) return;
    const auto* Results = FTargetingDefaultResultsSet::Find(Handle);
    TArray<FQueryVolume> Volumes;
    ReadVolumes(Handle, Volumes);
    const auto Now = Source->GetWorld()->GetTimeSeconds();
    const float Lifetime = FMath::Max(0.f, UTargetingSubsystem::GetOverrideTargetingLifeTime());
    TMap<AActor*, FVector> Points;
    if (Results && (!WindowTargets || !WindowTargets->IsEmpty()))
    {
        for (const auto& Entry : Results->TargetResults)
        {
            const auto& Hit = Entry.HitResult;
            auto* Target = Hit.GetActor();
            if (!IsValid(Target) || Target->GetWorld() != Source->GetWorld() || Hit.ImpactPoint.ContainsNaN()) continue;
            if (WindowTargets ? WindowTargets->Contains(Target) : Target->IsA<APawn>())
            {
                Points.FindOrAdd(Target) = Hit.ImpactPoint;
            }
        }
    }
    for (auto* Instance : Instances)
    {
        if (!Instance->Watches(Source)) continue;
        Instance->SelectSource(Source);
        if (WindowTargets)
        {
            auto& Packet = Instance->WindowPacket;
            Packet.ActorCounter = Instance->GetReplicator()->GetDebugActorCounter();
            Packet.Sequence = ++Instance->NextWindowSequence;
            Packet.ServerTime = Now;
            Packet.Lifetime = Lifetime;
            Packet.Volumes = Volumes;
            UE_LOG(LogPinecoTargetingDisplay, VeryVerbose, TEXT("Window query actor=%s sample=%u server=%.6f new=%d"),
                *Source->GetName(), Packet.Sequence, Now, WindowTargets->Num());
        }
        else
        {
            for (const auto& Volume : Volumes)
            {
                auto& Sample = Instance->PointVolumes.AddDefaulted_GetRef();
                Sample.Volume = Volume;
                Sample.ExpiresAt = Now + Lifetime;
            }
        }
        for (const auto& Point : Points)
        {
            auto& Marker = WindowTargets ? Instance->WindowHitMarkers.AddDefaulted_GetRef() : Instance->TargetMarkers.FindOrAdd(Point.Key);
            Marker.Location = Point.Value;
            Marker.ExpiresAt = Now + Lifetime;
            Marker.bPending = true;
        }
        Instance->bQueryNeedsNetUpdate = true;
        Instance->ForceImmediateCollect();
    }
    CaptureFacingSector(Handle);
}

// An immediate request runs all tasks in one pass, so the source pose here is the pose used by the filter.
void FGameplayDebuggerCategory_Pineco::CaptureFacingSector(const FTargetingRequestHandle& Handle)
{
    const auto* TaskSet = FTargetingTaskSet::Find(Handle);
    const auto* Source = FTargetingSourceContext::Find(Handle);
    const auto* Actor = Source ? Source->SourceActor.Get() : nullptr;
    if (!TaskSet || !*TaskSet || !IsValid(Actor))
    {
        return;
    }

    // The preset already owns an instance of the Blueprint. Resolve its loaded class without loading an asset.
    static const FSoftClassPath FilterPath(TEXT("/Pineco/Targeting/BP_PinecoTargetingFilter_FacingAngle.BP_PinecoTargetingFilter_FacingAngle_C"));
    const auto* FilterClass = FilterPath.ResolveClass();
    if (!FilterClass)
    {
        return;
    }

    const UTargetingSelectionTask_AOE* Cylinder = nullptr;
    const UObject* Filter = nullptr;
    for (const auto& Task : (*TaskSet)->Tasks)
    {
        if (const auto* AOE = Cast<UTargetingSelectionTask_AOE>(Task.Get()))
        {
            if (Cylinder || AOE->GetClass() != UTargetingSelectionTask_AOE::StaticClass()
                || AOE->GetShapeType() != ETargetingAOEShape::Cylinder)
            {
                return;
            }
            Cylinder = AOE;
        }
        else if (Task && Task->GetClass() == FilterClass)
        {
            Filter = Task.Get();
        }
    }

    // A single circular arc describes the intersection only when the cylinder and angle share their XY origin.
    if (!Cylinder || !Filter || !Cylinder->GetSourceOffset(Handle).IsNearlyZero())
    {
        return;
    }
    static const FName FullAngleName(TEXT("FullAngleDegrees"));
    const auto* AngleProperty = FindFProperty<FFloatProperty>(FilterClass, FullAngleName);
    if (!AngleProperty)
    {
        return;
    }
    const auto Radius = static_cast<float>(Cylinder->GetCollisionShape().GetExtent().X);
    const auto Angle = AngleProperty->GetPropertyValue_InContainer(Filter);
    const auto Forward = Actor->GetActorForwardVector().GetSafeNormal2D();
    const auto* World = Actor->GetWorld();
    if (!World || !FMath::IsFinite(Radius) || Radius <= 0.f || !FMath::IsFinite(Angle) || Forward.IsNearlyZero())
    {
        return;
    }

    const auto HalfDegrees = 0.5f * FMath::Clamp(Angle, 0.f, 360.f);
    // Move only Z to put the outline just above the floor in the GYM.
    const auto Center = Actor->GetActorLocation() + FVector::UpVector * (3.f - Actor->GetSimpleCollisionHalfHeight());
    FGameplayDebuggerCategory_Pineco::RecordSector(Actor, Center, Forward, Radius, HalfDegrees);
}

void FGameplayDebuggerCategory_Pineco::RecordSector(const AActor* Source, FVector Center, FVector Forward,
    float Radius, float HalfDegrees)
{
    // Store the request pose. Actors may have moved before debugger replication collects it.
    TArray<FVector> Segments;
    constexpr int32 ArcSegments = 48;
    Segments.Reserve(ArcSegments * 2 + 4);
    auto Previous = Center + Forward.RotateAngleAxis(-HalfDegrees, FVector::UpVector) * Radius;
    if (HalfDegrees < 180.f)
    {
        Segments.Add(Center);
        Segments.Add(Previous);
    }
    if (HalfDegrees > 0.f)
    {
        for (int32 I = 1; I <= ArcSegments; ++I)
        {
            const auto Angle = -HalfDegrees + (2.f * HalfDegrees * I) / ArcSegments;
            const auto Next = Center + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Radius;
            Segments.Add(Previous);
            Segments.Add(Next);
            Previous = Next;
        }
        if (HalfDegrees < 180.f)
        {
            Segments.Add(Center);
            Segments.Add(Previous);
        }
    }
    for (auto* Instance : Instances)
    {
        if (!Instance->Watches(Source)) continue;
        Instance->SelectSource(Source);
        Instance->Sector = FGameplayDebuggerShape::MakeSegmentList(Segments, 2.f, FColor::Cyan);
        Instance->Description = FString::Printf(TEXT("Facing filter  %.1f degrees  radius %.1f cm"), 2.f * HalfDegrees, Radius);
        Instance->ExpiresAt = Source->GetWorld()->GetTimeSeconds() + FMath::Max(0.f, UTargetingSubsystem::GetOverrideTargetingLifeTime());
        Instance->bPendingSample = true;
        Instance->ForceImmediateCollect();
    }
}

void FGameplayDebuggerCategory_Pineco::OnGameplayDebuggerDeactivated()
{
    SourceActor.Reset();
    PointVolumes.Reset();
    WindowHitMarkers.Reset();
    TargetMarkers.Reset();
    DrawPacket = {};
    WindowPacket = {};
    LocalWindowTrail.Reset();
    LastDrawnSequence = 0;
    Description.Reset();
    bPendingSample = false;
    bQueryNeedsNetUpdate = false;
}

void FGameplayDebuggerCategory_Pineco::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
    if (!IsValid(DebugActor) || SourceActor.Get() != DebugActor)
    {
        OnGameplayDebuggerDeactivated();
        return;
    }
    // A zero-duration request is still eligible for one sample, including a miss.
    const auto Now = DebugActor->GetWorld()->GetTimeSeconds();
    DrawPacket.ActorCounter = GetReplicator()->GetDebugActorCounter();
    PointVolumes.RemoveAll([Now](const FTimedVolume& Sample) { return !Sample.IsVisible(Now); });
    for (auto& Sample : PointVolumes)
    {
        DrawPacket.Volumes.Add(Sample.Volume);
        Sample.bPending = false;
    }
    WindowHitMarkers.RemoveAll([Now](const FTargetMarker& Marker) { return !Marker.IsVisible(Now); });
    for (auto& Marker : WindowHitMarkers)
    {
        DrawPacket.Points.Add(Marker.Location);
        Marker.bPending = false;
    }
    for (auto It = TargetMarkers.CreateIterator(); It; ++It)
    {
        auto& Marker = It.Value();
        if (!Marker.IsVisible(Now))
        {
            It.RemoveCurrent();
            continue;
        }
        DrawPacket.Points.Add(Marker.Location);
        Marker.bPending = false;
    }
    if (!Description.IsEmpty() && (bPendingSample || Now <= ExpiresAt))
    {
        AddTextLine(Description);
        AddShape(Sector);
        bPendingSample = false;
    }
    if (bQueryNeedsNetUpdate)
    {
        GetReplicator()->ForceNetUpdate();
        bQueryNeedsNetUpdate = false;
    }
}

void FGameplayDebuggerCategory_Pineco::OnDataPackReplicated(int32 DataPackId)
{
    if (DataPackId != WindowPackId || WindowPacket.Sequence == 0) return;
    const auto* Rep = GetReplicator();
    if (!Rep || !Rep->GetWorld()) return;
    UE_LOG(LogPinecoTargetingDisplay, VeryVerbose, TEXT("Window received actor=%s sample=%u server=%.6f received=%.6f"),
        *Rep->GetDebugActorName().ToString(), WindowPacket.Sequence, WindowPacket.ServerTime, GetServerTime(*Rep->GetWorld()));
}

void FGameplayDebuggerCategory_Pineco::DrawVolume(UWorld* World, const FQueryVolume& V)
{
    const auto Quat = V.Rotation.Quaternion();
    switch (V.Type)
    {
    case EVolume::Box:
        DrawDebugBox(World, V.Start, V.Extent, Quat, FColor::Red, false, 0.f, 0, 2.f);
        break;
    case EVolume::Cylinder:
    {
        const auto Axis = Quat.GetUpVector() * V.Extent.Z;
        DrawDebugCylinder(World, V.Start - Axis, V.Start + Axis, V.Extent.X, 32, FColor::Red, false, 0.f, 0, 2.f);
        break;
    }
    case EVolume::Sphere:
    case EVolume::Capsule:
        DrawDebugCapsule(World, V.Start, V.Type == EVolume::Sphere ? V.Extent.X : V.Extent.Z, V.Extent.X,
            Quat, FColor::Red, false, 0.f, 0, 2.f);
        break;
    }
}

void FGameplayDebuggerCategory_Pineco::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& Canvas)
{
    auto* World = Canvas.GetWorld();
    const auto* Rep = GetReplicator();
    if (!World || !Rep || !Rep->GetDebugActor()) return;
    const auto ActorCounter = Rep->GetDebugActorCounter();
    if (TrailActorCounter != ActorCounter)
    {
        LocalWindowTrail.Reset();
        TrailActorCounter = ActorCounter;
        LastDrawnSequence = 0;
    }
    const auto Now = GetServerTime(*World);
    LocalWindowTrail.RemoveAll([Now](const FTimedVolume& Sample) { return Now >= Sample.ExpiresAt; });
    if (WindowPacket.ActorCounter == ActorCounter && WindowPacket.Sequence > LastDrawnSequence)
    {
        LastDrawnSequence = WindowPacket.Sequence;
        const double Expiration = WindowPacket.ServerTime + WindowPacket.Lifetime;
        // Consume at most one received pose per draw. Zero lifetime still gets one local frame.
        if ((WindowPacket.Lifetime == 0.f || Now < Expiration) && !WindowPacket.Volumes.IsEmpty())
        {
            for (const auto& Volume : WindowPacket.Volumes)
            {
                auto& Sample = LocalWindowTrail.AddDefaulted_GetRef();
                Sample.Volume = Volume;
                Sample.ExpiresAt = WindowPacket.Lifetime > 0.f ? Expiration : Now;
            }
            UE_LOG(LogPinecoTargetingDisplay, VeryVerbose, TEXT("Window drawn actor=%s sample=%u server=%.6f drawn=%.6f"),
                *Rep->GetDebugActorName().ToString(), LastDrawnSequence, WindowPacket.ServerTime, Now);
        }
    }
    for (const auto& Sample : LocalWindowTrail)
    {
        DrawVolume(World, Sample.Volume);
    }
    if (ActorCounter != DrawPacket.ActorCounter) return;
    for (const auto& Volume : DrawPacket.Volumes)
    {
        DrawVolume(World, Volume);
    }
    for (const auto& Point : DrawPacket.Points)
    {
        DrawDebugPoint(World, Point, 16.f, FColor::Green, false, 0.f);
    }
}
#endif
