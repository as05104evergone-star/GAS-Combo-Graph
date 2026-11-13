#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "Factories/Factory.h"
#include "GASComboGraphSchema.generated.h"

UCLASS()
class UGASComboGraphSchema : public UEdGraphSchema
{
    GENERATED_BODY()

    virtual void CreateDefaultNodesForGraph(UEdGraph& Graph) const override;
    virtual void GetGraphContextActions(FGraphContextMenuBuilder& Builder) const override;
    virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
    virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2f& Position, UEdGraph* Graph) const override;
    virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* Graph,
        FString& Tooltip, bool& bOk) const override;
};

UCLASS()
class UGASComboGraphFactory : public UFactory
{
    GENERATED_BODY()

public:
    UGASComboGraphFactory();
    virtual UObject* FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags,
        UObject* Context, FFeedbackContext* Warn) override;
};
