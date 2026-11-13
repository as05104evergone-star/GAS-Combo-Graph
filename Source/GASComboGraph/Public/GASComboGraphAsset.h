#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GASComboGraphAsset.generated.h"

class UAnimMontage;
class UDataTable;
class UEdGraph;
class UGameplayEffect;
class UInputAction;

USTRUCT()
struct GASCOMBOGRAPH_API FGASComboMove
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="招式", meta=(DisplayName="招式名"))
    FName Name;

    UPROPERTY(EditAnywhere, Category="招式", meta=(DisplayName="蒙太奇"))
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, Category="接招", meta=(DisplayName="开始收输入（动画秒）", ClampMin="0.0"))
    float BufferOpensAt = 0.15f;

    UPROPERTY(EditAnywhere, Category="接招", meta=(DisplayName="切段（动画秒）", ClampMin="0.0"))
    float CommitAt = 0.4f;

    UPROPERTY(EditAnywhere, Category="伤害", meta=(DisplayName="伤害表行"))
    FName DamageRow;

    UPROPERTY(EditAnywhere, Category="伤害", meta=(DisplayName="伤害开始（动画秒）", ClampMin="0.0"))
    float HitOpensAt = 0.12f;

    UPROPERTY(EditAnywhere, Category="伤害", meta=(DisplayName="伤害结束（动画秒）", ClampMin="0.0"))
    float HitClosesAt = 0.32f;

    UPROPERTY(EditAnywhere, Category="取消", meta=(DisplayName="允许移动取消"))
    bool bAllowMovementCancel = false;
};

USTRUCT()
struct GASCOMBOGRAPH_API FGASComboTransition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category="转移", meta=(DisplayName="输入动作"))
    TObjectPtr<UInputAction> Input;

    UPROPERTY(EditAnywhere, Category="转移", meta=(DisplayName="优先级"))
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, Category="转移", meta=(DisplayName="要求本段造成伤害"))
    bool bRequiresHit = false;

    UPROPERTY(EditAnywhere, Category="转移", meta=(DisplayName="自身标签条件"))
    FGameplayTagQuery OwnerTags;

    UPROPERTY()
    int32 From = INDEX_NONE;

    UPROPERTY()
    int32 To = INDEX_NONE;
};

UCLASS(BlueprintType, meta=(DisplayName="GAS 连招图"))
class GASCOMBOGRAPH_API UGASComboGraphAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="连招", meta=(DisplayName="伤害数据表"))
    TObjectPtr<UDataTable> DamageTable;

    UPROPERTY(EditAnywhere, Category="连招", meta=(DisplayName="伤害效果"))
    TSubclassOf<UGameplayEffect> DamageEffect;

    UPROPERTY(EditAnywhere, Category="连招", meta=(DisplayName="起手输入"))
    TObjectPtr<UInputAction> StartInput;

    UPROPERTY()
    TArray<FGASComboMove> Moves;

    UPROPERTY()
    TArray<FGASComboTransition> Transitions;

    UPROPERTY()
    int32 Entry = INDEX_NONE;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UEdGraph> EditorGraph;
#endif

    bool ResolveDamage(FName Row, float& OutDamage) const;
    bool ValidateGraph(TArray<FText>& Errors) const;

#if WITH_EDITOR
    bool CompileGraph(TArray<FText>& Errors);
    virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif
};
