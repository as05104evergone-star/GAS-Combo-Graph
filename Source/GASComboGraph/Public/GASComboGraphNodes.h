#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "GASComboGraphAsset.h"
#include "GASComboGraphNodes.generated.h"

UCLASS()
class GASCOMBOGRAPH_API UGASComboEntryNode : public UEdGraphNode
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual void AllocateDefaultPins() override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual bool CanUserDeleteNode() const override { return false; }
#endif
};

UCLASS()
class GASCOMBOGRAPH_API UGASComboMoveNode : public UEdGraphNode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="招式", meta=(ShowOnlyInnerProperties))
    FGASComboMove Move;

#if WITH_EDITOR
    virtual void AllocateDefaultPins() override;
    virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.08f, 0.33f, 0.5f); }
#endif
};

UCLASS()
class GASCOMBOGRAPH_API UGASComboTransitionNode : public UEdGraphNode
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="转移", meta=(ShowOnlyInnerProperties))
    FGASComboTransition Transition;

#if WITH_EDITOR
    virtual void AllocateDefaultPins() override;
    virtual void AutowireNewNode(UEdGraphPin* FromPin) override;
    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.35f, 0.21f, 0.08f); }
#endif
};
