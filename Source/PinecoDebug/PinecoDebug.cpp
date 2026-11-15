// Copyright 2019-Present tarnishablec. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "DrawDebugHelpers.h"

#if WITH_GAMEPLAY_DEBUGGER && ENABLE_DRAW_DEBUG && (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#include "GameplayDebugger.h"
#include "PinecoGAS/Targeting/PinecoTargetActor.h"
#include "PinecoGAS/Targeting/PinecoTargetingFilterTask_Limit.h"
#include "PinecoDebug/Categories/PinecoGameplayDebuggerCategory.h"

class FPinecoDebugModule final : public IModuleInterface
{
    virtual void StartupModule() override
    {
        auto& Debugger = IGameplayDebugger::Get();
        RequestHandle = UPinecoTargetingFilterTask_Limit::OnImmediateRequest().AddStatic(&FGameplayDebuggerCategory_Pineco::OnImmediateRequest);
        WindowHandle = APinecoTargetActor::OnWindowQuery().AddStatic(&FGameplayDebuggerCategory_Pineco::OnWindowQuery);
        Debugger.RegisterCategory(TEXT("Pineco"), IGameplayDebugger::FOnGetCategory::CreateLambda(
            []() -> TSharedRef<FGameplayDebuggerCategory> { return MakeShared<FGameplayDebuggerCategory_Pineco>(); }));
        Debugger.NotifyCategoriesChanged();
    }

    virtual void ShutdownModule() override
    {
        UPinecoTargetingFilterTask_Limit::OnImmediateRequest().Remove(RequestHandle);
        APinecoTargetActor::OnWindowQuery().Remove(WindowHandle);
        if (IGameplayDebugger::IsAvailable())
        {
            auto& Debugger = IGameplayDebugger::Get();
            Debugger.UnregisterCategory(TEXT("Pineco"));
            Debugger.NotifyCategoriesChanged();
        }
    }
private:
    FDelegateHandle RequestHandle;
    FDelegateHandle WindowHandle;
};
IMPLEMENT_MODULE(FPinecoDebugModule, PinecoDebug)
#else
IMPLEMENT_MODULE(FDefaultModuleImpl, PinecoDebug)
#endif
