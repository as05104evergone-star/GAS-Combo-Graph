#include "GASComboGraphSchema.h"

#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetData.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "GASComboGraphNodes.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "GASComboGraphSchema"

void UGASComboGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
    FGraphNodeCreator<UGASComboEntryNode> Creator(Graph);
    Creator.CreateNode();
    Creator.Finalize();
}

void UGASComboGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& Builder) const
{
    if (!Builder.FromPin || !Builder.FromPin->GetOwningNode()->IsA<UGASComboMoveNode>())
    {
        auto Action = MakeShared<FEdGraphSchemaAction_NewNode>(
            FText::GetEmpty(), LOCTEXT("Move", "添加招式"), LOCTEXT("MoveHelp", "为一个 Montage 创建招式节点"), 0);
        Action->NodeTemplate = NewObject<UGASComboMoveNode>(Builder.OwnerOfTemporaries);
        Builder.AddAction(Action);
    }
    if (!Builder.FromPin || Builder.FromPin->GetOwningNode()->IsA<UGASComboMoveNode>())
    {
        auto Action = MakeShared<FEdGraphSchemaAction_NewNode>(
            FText::GetEmpty(), LOCTEXT("Transition", "添加转移"), LOCTEXT("TransitionHelp", "在两个招式间设置输入与条件"), 0);
        Action->NodeTemplate = NewObject<UGASComboTransitionNode>(Builder.OwnerOfTemporaries);
        Builder.AddAction(Action);
    }
}

const FPinConnectionResponse UGASComboGraphSchema::CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const
{
    if (A && B && A->Direction != B->Direction)
    {
        const UEdGraphPin* Out = A->Direction == EGPD_Output ? A : B;
        const UEdGraphPin* In = A->Direction == EGPD_Input ? A : B;
        const UEdGraphNode* From = Out->GetOwningNode();
        const UEdGraphNode* To = In->GetOwningNode();
        if (From && To && From != To && From->GetGraph() == To->GetGraph())
        {
            if ((From->IsA<UGASComboEntryNode>() || From->IsA<UGASComboTransitionNode>()) && To->IsA<UGASComboMoveNode>())
            {
                if (Out->LinkedTo.IsEmpty())
                {
                    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText::GetEmpty());
                }
                return FPinConnectionResponse(Out == A ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B,
                    LOCTEXT("ReplaceOutput", "替换当前目标"));
            }
            if (From->IsA<UGASComboMoveNode>() && To->IsA<UGASComboTransitionNode>())
            {
                if (In->LinkedTo.IsEmpty())
                {
                    return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText::GetEmpty());
                }
                return FPinConnectionResponse(In == A ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B,
                    LOCTEXT("ReplaceInput", "替换当前来源"));
            }
        }
    }
    return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("InvalidLink", "起手连接招式；招式通过转移连接。"));
}

void UGASComboGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* Graph,
    FString& Tooltip, bool& bOk) const
{
    bOk = Assets.ContainsByPredicate([](const FAssetData& Asset)
    {
        return Asset.AssetClassPath == UAnimMontage::StaticClass()->GetClassPathName();
    });
    Tooltip = bOk ? TEXT("从 Montage 创建招式") : TEXT("请拖入 Montage");
}

void UGASComboGraphSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2f& Position, UEdGraph* Graph) const
{
    const FScopedTransaction Transaction(LOCTEXT("DropMontages", "从 Montage 创建招式"));
    Graph->Modify();
    int32 Offset = 0;
    for (const FAssetData& Asset : Assets)
    {
        if (UAnimMontage* Montage = Cast<UAnimMontage>(Asset.GetAsset()))
        {
            FGraphNodeCreator<UGASComboMoveNode> Creator(*Graph);
            UGASComboMoveNode* Node = Creator.CreateNode();
            Node->NodePosX = FMath::RoundToInt(Position.X) + Offset;
            Node->NodePosY = FMath::RoundToInt(Position.Y);
            Node->Move.Name = Montage->GetFName();
            Node->Move.Montage = Montage;
            Creator.Finalize();
            Offset += 350;
        }
    }
    Graph->NotifyGraphChanged();
}

UGASComboGraphFactory::UGASComboGraphFactory()
{
    SupportedClass = UGASComboGraphAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UGASComboGraphFactory::FactoryCreateNew(UClass* Class, UObject* Parent, FName Name, EObjectFlags Flags,
    UObject* Context, FFeedbackContext* Warn)
{
    UGASComboGraphAsset* Asset = NewObject<UGASComboGraphAsset>(Parent, Class, Name, Flags | RF_Transactional);
    Asset->EditorGraph = NewObject<UEdGraph>(Asset, TEXT("ComboGraph"), RF_Transactional);
    Asset->EditorGraph->Schema = UGASComboGraphSchema::StaticClass();
    Asset->EditorGraph->GetSchema()->CreateDefaultNodesForGraph(*Asset->EditorGraph);
    return Asset;
}

#undef LOCTEXT_NAMESPACE
