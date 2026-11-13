#include "GASComboGraphAsset.h"

#include "Animation/AnimMontage.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GASCombatCoreAttributeSet.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "GASComboGraphNodes.h"
#include "UObject/ObjectSaveContext.h"
#endif

#define LOCTEXT_NAMESPACE "GASComboGraphAsset"

bool UGASComboGraphAsset::ResolveDamage(FName Row, float& OutDamage) const
{
    OutDamage = 0.f;
    const uint8* Data = DamageTable ? DamageTable->FindRowUnchecked(Row) : nullptr;
    if (!Data)
    {
        return false;
    }

    const FNumericProperty* Value = CastField<FNumericProperty>(DamageTable->FindTableProperty(TEXT("Value")));
    const FNameProperty* Unit = CastField<FNameProperty>(DamageTable->FindTableProperty(TEXT("Unit")));
    if (!Value || !Value->IsFloatingPoint() || !Unit || Unit->GetPropertyValue_InContainer(Data) != FName(TEXT("点")))
    {
        return false;
    }

    const double Number = Value->GetFloatingPointPropertyValue(Value->ContainerPtrToValuePtr<void>(Data));
    if (!FMath::IsFinite(Number) || Number <= 0.0 || Number > TNumericLimits<float>::Max())
    {
        return false;
    }
    OutDamage = static_cast<float>(Number);
    return OutDamage > 0.f;
}

bool UGASComboGraphAsset::ValidateGraph(TArray<FText>& Errors) const
{
    if (!Moves.IsValidIndex(Entry) || !StartInput)
    {
        Errors.Add(LOCTEXT("MissingEntry", "请连接起手节点并设置起手输入。"));
    }

    const bool bDealsDamage = Moves.ContainsByPredicate([](const FGASComboMove& Move) { return !Move.DamageRow.IsNone(); });
    if (bDealsDamage)
    {
        const FGameplayTag DamageTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Damage"), false);
        const UGameplayEffect* Effect = DamageEffect.GetDefaultObject();
        const bool bValidEffect = DamageTag.IsValid() && Effect && Effect->DurationPolicy == EGameplayEffectDurationType::Instant
            && Effect->Modifiers.Num() == 1 && Effect->Executions.IsEmpty()
            && Effect->Modifiers[0].Attribute == UGASCombatCoreAttributeSet::GetIncomingDamageAttribute()
            && Effect->Modifiers[0].ModifierOp == EGameplayModOp::Additive
            && Effect->Modifiers[0].ModifierMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller
            && Effect->Modifiers[0].ModifierMagnitude.GetSetByCallerFloat().DataTag == DamageTag;
        if (!bValidEffect)
        {
            Errors.Add(LOCTEXT("MissingEffect", "伤害效果需要为 Instant，以 Data.Damage 向 IncomingDamage 加值。"));
        }
    }

    TSet<FName> Names;
    for (int32 Index = 0; Index < Moves.Num(); ++Index)
    {
        const FGASComboMove& Move = Moves[Index];
        const FText Name = Move.Name.IsNone() ? FText::AsNumber(Index + 1) : FText::FromName(Move.Name);
        if (Move.Name.IsNone() || Names.Contains(Move.Name))
        {
            Errors.Add(FText::Format(LOCTEXT("DuplicateName", "招式 {0} 需要一个唯一名称。"), Name));
        }
        Names.Add(Move.Name);

        if (!Move.Montage || Move.Montage->CompositeSections.Num() != 1 || !Move.Montage->bEnableAutoBlendOut
            || !FMath::IsFinite(Move.Montage->RateScale) || Move.Montage->RateScale <= 0.f
            || !Move.Montage->CompositeSections[0].NextSectionName.IsNone()
            || Move.Montage->SlotAnimTracks.IsEmpty() || Move.Montage->GetPlayLength() <= 0.f)
        {
            Errors.Add(FText::Format(LOCTEXT("BadMontage", "{0} 需要一个单 Section、自然收尾的 Montage。"), Name));
        }
        else if (Transitions.ContainsByPredicate([Index](const FGASComboTransition& Edge) { return Edge.From == Index; }))
        {
            const float BlendLead = Move.Montage->BlendOutTriggerTime >= 0.f
                ? Move.Montage->BlendOutTriggerTime : Move.Montage->GetDefaultBlendOutTime();
            const float End = Move.Montage->GetPlayLength() - BlendLead * Move.Montage->RateScale;
            if (!FMath::IsFinite(Move.BufferOpensAt) || !FMath::IsFinite(Move.CommitAt)
                || Move.BufferOpensAt < 0.f || Move.BufferOpensAt >= Move.CommitAt || Move.CommitAt >= End)
            {
                Errors.Add(FText::Format(LOCTEXT("BadWindow", "{0} 的接招窗口需要早于 Montage 自动 Blend Out。"), Name));
            }
        }

        float Amount = 0.f;
        if (!Move.DamageRow.IsNone() && !ResolveDamage(Move.DamageRow, Amount))
        {
            Errors.Add(FText::Format(LOCTEXT("BadRow", "{0} 的伤害行 {1} 缺失，或 Value/Unit 需要为正数及“点”。"),
                Name, FText::FromName(Move.DamageRow)));
        }
        if (!Move.DamageRow.IsNone() && Move.Montage
            && (!FMath::IsFinite(Move.HitOpensAt) || !FMath::IsFinite(Move.HitClosesAt)
                || Move.HitOpensAt < 0.f || Move.HitOpensAt >= Move.HitClosesAt
                || Move.HitClosesAt > Move.Montage->GetPlayLength()))
        {
            Errors.Add(FText::Format(LOCTEXT("BadHitWindow", "{0} 的伤害窗口需要落在 Montage 时长内。"), Name));
        }
    }

    for (int32 Index = 0; Index < Transitions.Num(); ++Index)
    {
        const FGASComboTransition& Edge = Transitions[Index];
        if (!Edge.Input || !Moves.IsValidIndex(Edge.From) || !Moves.IsValidIndex(Edge.To))
        {
            Errors.Add(LOCTEXT("BadTransition", "每个转移都需要来源招式、目标招式和输入动作。"));
            continue;
        }
        if (Edge.bRequiresHit && Moves[Edge.From].DamageRow.IsNone())
        {
            Errors.Add(FText::Format(LOCTEXT("NoHitSource", "{0} 的转移要求命中，但该招式没有配置伤害行。"),
                FText::FromName(Moves[Edge.From].Name)));
        }
        for (int32 Other = Index + 1; Other < Transitions.Num(); ++Other)
        {
            const FGASComboTransition& Candidate = Transitions[Other];
            if (Edge.From == Candidate.From && Edge.Input == Candidate.Input && Edge.Priority == Candidate.Priority)
            {
                Errors.Add(FText::Format(LOCTEXT("Ambiguous", "{0} 的同输入转移需要不同优先级。"),
                    FText::FromName(Moves[Edge.From].Name)));
                break;
            }
        }
    }

    if (Moves.IsValidIndex(Entry))
    {
        TSet<int32> Reached;
        TArray<int32> Pending{ Entry };
        while (!Pending.IsEmpty())
        {
            const int32 Move = Pending.Pop();
            if (Reached.Contains(Move))
            {
                continue;
            }
            Reached.Add(Move);
            for (const FGASComboTransition& Edge : Transitions)
            {
                if (Edge.From == Move && Moves.IsValidIndex(Edge.To))
                {
                    Pending.Add(Edge.To);
                }
            }
        }
        for (int32 Index = 0; Index < Moves.Num(); ++Index)
        {
            if (!Reached.Contains(Index))
            {
                Errors.Add(FText::Format(LOCTEXT("Unreachable", "招式 {0} 无法从起手到达。"),
                    FText::FromName(Moves[Index].Name)));
            }
        }
    }
    return Errors.IsEmpty();
}

#if WITH_EDITOR

namespace
{
    const UEdGraphNode* GetSingleLink(const UEdGraphNode& Node, FName PinName)
    {
        const UEdGraphPin* Pin = Node.FindPin(PinName);
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0]->GetOwningNode() : nullptr;
    }
}

bool UGASComboGraphAsset::CompileGraph(TArray<FText>& Errors)
{
    Errors.Reset();
    Moves.Reset();
    Transitions.Reset();
    Entry = INDEX_NONE;
    if (!EditorGraph)
    {
        Errors.Add(LOCTEXT("MissingGraph", "图资产缺少编辑器图。"));
        return false;
    }

    TMap<const UEdGraphNode*, int32> MoveIndices;
    const UGASComboEntryNode* EntryNode = nullptr;
    for (const UEdGraphNode* Node : EditorGraph->Nodes)
    {
        if (const UGASComboMoveNode* MoveNode = Cast<UGASComboMoveNode>(Node))
        {
            MoveIndices.Add(MoveNode, Moves.Add(MoveNode->Move));
        }
        else if (const UGASComboEntryNode* FoundEntry = Cast<UGASComboEntryNode>(Node))
        {
            if (EntryNode)
            {
                Errors.Add(LOCTEXT("DuplicateEntry", "图中只能有一个起手节点。"));
            }
            EntryNode = FoundEntry;
        }
    }
    if (EntryNode)
    {
        if (const int32* Index = MoveIndices.Find(GetSingleLink(*EntryNode, TEXT("First"))))
        {
            Entry = *Index;
        }
    }
    for (const UEdGraphNode* Node : EditorGraph->Nodes)
    {
        if (const UGASComboTransitionNode* EdgeNode = Cast<UGASComboTransitionNode>(Node))
        {
            FGASComboTransition Edge = EdgeNode->Transition;
            const UEdGraphPin* InputPin = EdgeNode->FindPin(TEXT("In"));
            const UEdGraphPin* OutputPin = EdgeNode->FindPin(TEXT("Out"));
            if (!Edge.Input && InputPin && InputPin->LinkedTo.IsEmpty() && OutputPin && OutputPin->LinkedTo.IsEmpty())
            {
                continue;
            }
            const int32* From = MoveIndices.Find(GetSingleLink(*EdgeNode, TEXT("In")));
            const int32* To = MoveIndices.Find(GetSingleLink(*EdgeNode, TEXT("Out")));
            Edge.From = From ? *From : INDEX_NONE;
            Edge.To = To ? *To : INDEX_NONE;
            Transitions.Add(Edge);
        }
    }
    if (!ValidateGraph(Errors))
    {
        Entry = INDEX_NONE;
        return false;
    }
    return true;
}

void UGASComboGraphAsset::PreSave(FObjectPreSaveContext SaveContext)
{
    Super::PreSave(SaveContext);
    TArray<FText> Errors;
    CompileGraph(Errors);
    for (const FText& Error : Errors)
    {
        UE_LOG(LogTemp, Warning, TEXT("GAS 连招图 %s: %s"), *GetName(), *Error.ToString());
    }
}

#endif
#undef LOCTEXT_NAMESPACE
