#include "GASComboRunnerComponent.h"

#include "AlphaBlend.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GASCombatBridgeComponent.h"
#include "GASCombatCoreAttributeSet.h"
#include "GASCombatStatics.h"
#include "GASComboGraphAsset.h"
#include "InputAction.h"

UGASComboRunnerComponent::UGASComboRunnerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGASComboRunnerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        AddTickPrerequisiteComponent(Character->GetMesh());
    }
    Bridge = GetOwner()->FindComponentByClass<UGASCombatBridgeComponent>();
    if (UGASCombatBridgeComponent* Combat = Bridge.Get())
    {
        Combat->OnDeath.AddDynamic(this, &UGASComboRunnerComponent::HandleDeath);
    }

    TArray<FText> Errors;
    if (ComboGraph)
    {
        // Save and validation can rebuild the editor asset while PIE is running.
        RuntimeGraph = NewObject<UGASComboGraphAsset>(this);
        RuntimeGraph->StartInput = ComboGraph->StartInput;
        RuntimeGraph->DamageTable = ComboGraph->DamageTable;
        RuntimeGraph->DamageEffect = ComboGraph->DamageEffect;
        RuntimeGraph->Entry = ComboGraph->Entry;
        RuntimeGraph->Moves = ComboGraph->Moves;
        RuntimeGraph->Transitions = ComboGraph->Transitions;
    }
    bGraphValid = RuntimeGraph && RuntimeGraph->ValidateGraph(Errors);
    if (!bGraphValid)
    {
        LastResult = Errors.IsEmpty() ? TEXT("未配置连招图") : Errors[0].ToString();
    }
}

void UGASComboRunnerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bShuttingDown = true;
    if (UGASCombatBridgeComponent* Combat = Bridge.Get())
    {
        Combat->OnDeath.RemoveDynamic(this, &UGASComboRunnerComponent::HandleDeath);
    }
    FinishMove(true, true);
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        RemoveTickPrerequisiteComponent(Character->GetMesh());
    }
    Bridge.Reset();
    RuntimeGraph = nullptr;
    bGraphValid = false;
    bActionBlocked = false;
    Super::EndPlay(EndPlayReason);
}

bool UGASComboRunnerComponent::CanAct() const
{
    AActor* Owner = GetOwner();
    if (bActionBlocked || bShuttingDown || !bGraphValid || !IsValid(Owner) || !Owner->HasAuthority()
        || !Bridge.IsValid() || !Bridge->IsCombatReady())
    {
        return false;
    }

    const UGASCombatCoreAttributeSet* Core = UGASCombatStatics::GetCombatAttributeSet(Owner);
    return Core && !Core->IsDead();
}

UAnimInstance* UGASComboRunnerComponent::GetOwnerAnimInstance() const
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
    return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

bool UGASComboRunnerComponent::SubmitInput(const UInputAction* Input)
{
    if (!bGraphValid)
    {
        return false;
    }
    if (!Input || bChangingMove || !CanAct())
    {
        LastResult = TEXT("当前角色无法接招");
        if (!Bridge.IsValid() || !Bridge->IsCombatReady())
        {
            LastResult = TEXT("GAS 战斗桥接未就绪");
        }
        else if (!UGASCombatStatics::GetCombatAttributeSet(GetOwner()))
        {
            LastResult = TEXT("角色缺少通用战斗属性集 GASCombatCoreAttributeSet");
        }
        return false;
    }
    if (!ActiveSwing)
    {
        if (Input != RuntimeGraph->StartInput)
        {
            LastResult = TEXT("输入与图的起手输入不一致");
            return false;
        }
        const TGuardValue<bool> Guard(bChangingMove, true);
        return StartMove(RuntimeGraph->Entry);
    }

    UAnimInstance* Anim = ActiveAnimInstance.Get();
    const FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(ActiveMontageId) : nullptr;
    const FGASComboMove& Move = RuntimeGraph->Moves[ActiveMove];
    if (bTransitionCommitted || !Instance || !Instance->IsActive()
        || Instance->GetPosition() < Move.BufferOpensAt || Instance->GetPosition() >= Move.CommitAt)
    {
        LastResult = TEXT("接招窗口未开放");
        return false;
    }
    if (!RuntimeGraph->Transitions.ContainsByPredicate([this, Input](const FGASComboTransition& Edge)
        { return Edge.From == ActiveMove && Edge.Input == Input; }))
    {
        LastResult = TEXT("当前招式没有对应的输入转移");
        return false;
    }
    BufferedInput = Input;
    LastResult = TEXT("已记录输入，等待切段");
    return true;
}

bool UGASComboRunnerComponent::StartMove(int32 Index)
{
    if (!CanAct() || !RuntimeGraph->Moves.IsValidIndex(Index) || ActiveSwing)
    {
        return false;
    }

    const FGASComboMove& Move = RuntimeGraph->Moves[Index];
    UAnimInstance* Anim = GetOwnerAnimInstance();
    float Damage = 0.f;
    if (!Anim || !Move.Montage || (!Move.DamageRow.IsNone() && !RuntimeGraph->ResolveDamage(Move.DamageRow, Damage)))
    {
        LastResult = TEXT("缺少 AnimInstance、Montage 或有效伤害");
        return false;
    }

    UAnimMontage* Montage = Move.Montage;
    if (Anim->Montage_Play(Montage, 1.f, EMontagePlayReturnType::MontageLength, 0.f, false) <= 0.f)
    {
        LastResult = TEXT("Montage 起播失败，请检查骨架");
        return false;
    }
    FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
    if (!Instance)
    {
        LastResult = TEXT("未取得起播实例");
        return false;
    }
    if (!CanAct() || ActiveSwing)
    {
        const FAlphaBlend Blend(FMath::Max(0.f, CancelBlendOut));
        Instance->Stop(Blend);
        return false;
    }

    LastSwingId = LastSwingId == MAX_int32 ? 1 : LastSwingId + 1;
    ActiveSwing = LastSwingId;
    ActiveMove = Index;
    ActiveMontageId = Instance->GetInstanceID();
    ActiveAnimInstance = Anim;
    ActiveMontage = Montage;
    ActiveDamage = Damage;
    LastResult = TEXT("招式已起播");
    Instance->OnMontageEnded.BindUObject(this, &UGASComboRunnerComponent::HandleMontageEnded, ActiveSwing);
    SetComponentTickEnabled(true);
    OnMoveStarted.Broadcast(Move.Name, ActiveSwing);
    return true;
}

void UGASComboRunnerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!ActiveSwing)
    {
        return;
    }
    if (!CanAct())
    {
        FinishMove(true, true);
        return;
    }

    UAnimInstance* Anim = ActiveAnimInstance.Get();
    FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(ActiveMontageId) : nullptr;
    if (!Instance || GetOwnerAnimInstance() != Anim)
    {
        FinishMove(true, true);
        return;
    }
    if (Instance->IsActive() && !bTransitionCommitted && BufferedInput.IsValid()
        && Instance->GetPosition() >= RuntimeGraph->Moves[ActiveMove].CommitAt)
    {
        CommitTransition();
    }
}

void UGASComboRunnerComponent::CommitTransition()
{
    const UInputAction* Input = BufferedInput.Get();
    BufferedInput.Reset();
    bTransitionCommitted = true;

    const UAbilitySystemComponent* ASC = Bridge->GetAbilitySystemComponent();
    FGameplayTagContainer Tags;
    if (ASC)
    {
        ASC->GetOwnedGameplayTags(Tags);
    }

    const FGASComboTransition* Best = nullptr;
    for (const FGASComboTransition& Edge : RuntimeGraph->Transitions)
    {
        if (Edge.From == ActiveMove && Edge.Input == Input && (!Edge.bRequiresHit || bConfirmedHit)
            && (Edge.OwnerTags.IsEmpty() || (ASC && Edge.OwnerTags.Matches(Tags)))
            && (!Best || Edge.Priority > Best->Priority))
        {
            Best = &Edge;
        }
    }
    if (!Best)
    {
        LastResult = TEXT("命中或自身标签条件未满足");
        return;
    }

    const int32 NextMove = Best->To;
    const TGuardValue<bool> Guard(bChangingMove, true);
    FinishMove(false, true);
    StartMove(NextMove);
}

bool UGASComboRunnerComponent::TryMovementCancel()
{
    if (!CanAct() || !ActiveSwing || !RuntimeGraph->Moves[ActiveMove].bAllowMovementCancel)
    {
        return false;
    }
    FinishMove(true, true);
    return true;
}

void UGASComboRunnerComponent::SetActionBlocked(bool bBlocked)
{
    if (bShuttingDown)
    {
        return;
    }
    bActionBlocked = bBlocked;
    if (bBlocked)
    {
        FinishMove(true, true);
    }
}

void UGASComboRunnerComponent::HandleDeath(AActor* Killer, AActor* DamageCauser)
{
    FinishMove(true, true);
}

void UGASComboRunnerComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 SwingId)
{
    if (ActiveSwing == SwingId && ActiveMontage.Get() == Montage)
    {
        if (!bInterrupted && CanAct() && !bTransitionCommitted && BufferedInput.IsValid())
        {
            CommitTransition();
        }
        if (ActiveSwing == SwingId)
        {
            FinishMove(bInterrupted || !CanAct(), false);
        }
    }
}

void UGASComboRunnerComponent::FinishMove(bool bInterrupted, bool bStopMontage)
{
    if (!ActiveSwing)
    {
        return;
    }

    const int32 FinishedSwing = ActiveSwing;
    const int32 MontageId = ActiveMontageId;
    UAnimInstance* Anim = ActiveAnimInstance.Get();
    ActiveSwing = 0;
    ActiveMove = INDEX_NONE;
    ActiveMontageId = INDEX_NONE;
    ActiveAnimInstance.Reset();
    ActiveMontage.Reset();
    ActiveDamage = 0.f;
    BufferedInput.Reset();
    HitTargets.Reset();
    bTransitionCommitted = false;
    bConfirmedHit = false;
    SetComponentTickEnabled(false);

    if (FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(MontageId) : nullptr)
    {
        Instance->OnMontageEnded.Unbind();
        if (bStopMontage)
        {
            const FAlphaBlend Blend(FMath::Max(0.f, CancelBlendOut));
            Instance->Stop(Blend);
        }
    }
    LastResult = bInterrupted ? TEXT("招式已中断") : TEXT("招式已结束");
    if (!bShuttingDown)
    {
        OnMoveEnded.Broadcast(FinishedSwing, bInterrupted);
    }
}

bool UGASComboRunnerComponent::IsHitWindowOpen(int32 SwingId) const
{
    if (!CanAct() || !ActiveSwing || ActiveSwing != SwingId || ActiveDamage <= 0.f)
    {
        return false;
    }

    UAnimInstance* Anim = ActiveAnimInstance.Get();
    const FAnimMontageInstance* Instance = Anim ? Anim->GetMontageInstanceForID(ActiveMontageId) : nullptr;
    if (!Instance || !Instance->IsActive() || GetOwnerAnimInstance() != Anim)
    {
        return false;
    }
    const FGASComboMove& Move = RuntimeGraph->Moves[ActiveMove];
    return Instance->GetPosition() >= Move.HitOpensAt && Instance->GetPosition() < Move.HitClosesAt;
}

bool UGASComboRunnerComponent::ReportHit(int32 SwingId, AActor* Target, AActor* EffectCauser)
{
    if (!IsHitWindowOpen(SwingId) || !IsValid(Target) || Target == GetOwner() || HitTargets.Contains(Target))
    {
        return false;
    }
    const UGASCombatBridgeComponent* TargetBridge = Target->FindComponentByClass<UGASCombatBridgeComponent>();
    const UGASCombatCoreAttributeSet* Core = UGASCombatStatics::GetCombatAttributeSet(Target);
    if (!TargetBridge || !TargetBridge->IsCombatReady() || !Core || Core->IsDead())
    {
        return false;
    }

    HitTargets.Add(Target);
    const float Before = Core->GetHealth();
    const TWeakObjectPtr<const UGASCombatCoreAttributeSet> TargetCore(Core);
    UGASCombatStatics::ApplyDamageToTarget(GetOwner(), Target, RuntimeGraph->DamageEffect, ActiveDamage, 1, EffectCauser);
    const bool bApplied = TargetCore.IsValid() && TargetCore->GetHealth() < Before;
    if (ActiveSwing == SwingId && bApplied)
    {
        bConfirmedHit = true;
    }
    return bApplied;
}

FString UGASComboRunnerComponent::GetComboDebugText() const
{
    const FString MoveName = ActiveSwing ? RuntimeGraph->Moves[ActiveMove].Name.ToString() : TEXT("空闲");
    return FString::Printf(TEXT("%s | 挥击 %d | %s"), *MoveName, ActiveSwing, *LastResult);
}
