#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GASComboRunnerComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UGASCombatBridgeComponent;
class UGASComboGraphAsset;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGASComboMoveStarted, FName, Move, int32, SwingId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGASComboMoveEnded, int32, SwingId, bool, bInterrupted);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent, DisplayName="GAS 连招执行器"))
class GASCOMBOGRAPH_API UGASComboRunnerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGASComboRunnerComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="连招")
    TObjectPtr<UGASComboGraphAsset> ComboGraph;

    UPROPERTY(EditAnywhere, Category="连招", meta=(ClampMin="0.0"))
    float CancelBlendOut = 0.15f;

    UPROPERTY(BlueprintAssignable, Category="连招")
    FGASComboMoveStarted OnMoveStarted;

    UPROPERTY(BlueprintAssignable, Category="连招")
    FGASComboMoveEnded OnMoveEnded;

    UFUNCTION(BlueprintCallable, Category="连招", meta=(DisplayName="提交连招输入"))
    bool SubmitInput(const UInputAction* Input);

    UFUNCTION(BlueprintCallable, Category="连招", meta=(DisplayName="尝试移动取消"))
    bool TryMovementCancel();

    UFUNCTION(BlueprintCallable, Category="连招", meta=(DisplayName="设置外部动作阻塞"))
    void SetActionBlocked(bool bBlocked);

    UFUNCTION(BlueprintCallable, Category="连招", meta=(DisplayName="按挥击编号提交命中"))
    bool ReportHit(int32 SwingId, AActor* Target, AActor* EffectCauser);

    UFUNCTION(BlueprintPure, Category="连招", meta=(DisplayName="当前挥击编号"))
    int32 GetSwingId() const { return ActiveSwing; }

    UFUNCTION(BlueprintPure, Category="连招", meta=(DisplayName="是否在本段伤害窗口"))
    bool IsHitWindowOpen(int32 SwingId) const;

    UFUNCTION(BlueprintPure, Category="连招", meta=(DisplayName="连招调试文本"))
    FString GetComboDebugText() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    UFUNCTION()
    void HandleDeath(AActor* Killer, AActor* DamageCauser);

    bool CanAct() const;
    UAnimInstance* GetOwnerAnimInstance() const;
    bool StartMove(int32 Index);
    void FinishMove(bool bInterrupted, bool bStopMontage);
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 SwingId);
    void CommitTransition();

    TWeakObjectPtr<UGASCombatBridgeComponent> Bridge;
    UPROPERTY(Transient)
    TObjectPtr<UGASComboGraphAsset> RuntimeGraph;

    TWeakObjectPtr<UAnimInstance> ActiveAnimInstance;
    TWeakObjectPtr<UAnimMontage> ActiveMontage;
    TSet<TWeakObjectPtr<AActor>> HitTargets;
    TWeakObjectPtr<const UInputAction> BufferedInput;
    int32 ActiveMove = INDEX_NONE;
    int32 ActiveMontageId = INDEX_NONE;
    int32 ActiveSwing = 0;
    int32 LastSwingId = 0;
    float ActiveDamage = 0.f;
    bool bActionBlocked = false;
    bool bShuttingDown = false;
    bool bGraphValid = false;
    bool bTransitionCommitted = false;
    bool bConfirmedHit = false;
    bool bChangingMove = false;
    FString LastResult;
};
