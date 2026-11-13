#include "GASComboGraphNodes.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "InputAction.h"

#define LOCTEXT_NAMESPACE "GASComboGraphNodes"

namespace
{
    void ConnectNewNode(UEdGraphNode& Node, UEdGraphPin* FromPin)
    {
        if (FromPin)
        {
            const FName OtherPin = FromPin->Direction == EGPD_Output ? TEXT("In") : TEXT("Out");
            Node.GetSchema()->TryCreateConnection(FromPin, Node.FindPin(OtherPin));
        }
    }
}

void UGASComboEntryNode::AllocateDefaultPins()
{
    CreatePin(EGPD_Output, TEXT("Combo"), TEXT("First"));
}

FText UGASComboEntryNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return LOCTEXT("Entry", "起手");
}

void UGASComboMoveNode::AllocateDefaultPins()
{
    CreatePin(EGPD_Input, TEXT("Combo"), TEXT("In"));
    CreatePin(EGPD_Output, TEXT("Combo"), TEXT("Out"));
}

void UGASComboMoveNode::AutowireNewNode(UEdGraphPin* FromPin)
{
    ConnectNewNode(*this, FromPin);
}

FText UGASComboMoveNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return Move.Name.IsNone() ? LOCTEXT("Move", "招式") : FText::FromName(Move.Name);
}

void UGASComboTransitionNode::AllocateDefaultPins()
{
    CreatePin(EGPD_Input, TEXT("Combo"), TEXT("In"));
    CreatePin(EGPD_Output, TEXT("Combo"), TEXT("Out"));
}

void UGASComboTransitionNode::AutowireNewNode(UEdGraphPin* FromPin)
{
    ConnectNewNode(*this, FromPin);
}

FText UGASComboTransitionNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
    return Transition.Input
        ? FText::Format(LOCTEXT("InputTransition", "{0} · 优先级 {1}"), FText::FromName(Transition.Input->GetFName()), FText::AsNumber(Transition.Priority))
        : LOCTEXT("Transition", "转移");
}

#undef LOCTEXT_NAMESPACE
#endif
