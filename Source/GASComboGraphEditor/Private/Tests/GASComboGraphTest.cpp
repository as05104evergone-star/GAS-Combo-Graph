#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GASComboGraphNodes.h"
#include "GASComboGraphSchema.h"
#include "GASComboRunnerComponent.h"
#include "GASCombatBridgeComponent.h"
#include "GASCombatCoreAttributeSet.h"
#include "GASCombatStatics.h"
#include "InputAction.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SavePackage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGASComboProjectTest, "GASComboGraph.AnimationSample.GraphAndCombat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGASComboProjectTest::RunTest(const FString& Parameters)
{
    if (FString(FApp::GetProjectName()) != TEXT("Animation_Sample"))
    {
        AddInfo(TEXT("This integration test uses Animation_Sample content."));
        return true;
    }

    const auto MontagePath = [](int32 Index)
    {
        return FString::Printf(TEXT("/Game/GhostSamurai_Bundle/GhostSamurai/Katana/Apose/Attack/GhostSamurai_APose_Attack01_%d_ALL_Root_Montage"), Index);
    };
    UAnimMontage* First = LoadObject<UAnimMontage>(nullptr, *MontagePath(1));
    UAnimMontage* Second = LoadObject<UAnimMontage>(nullptr, *MontagePath(2));
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/GhostSamurai_Bundle/Demo/Characters/Mannequin_UE4/Meshes/SK_Mannequin"));
    UInputAction* Attack = LoadObject<UInputAction>(nullptr, TEXT("/Game/Game/GhostSamuraiGameplay/Input/IA_GS_Attack"));
    UDataTable* DamageTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Combo_Demo/BP/DataTables/DT_Combat"));
    UClass* DamageEffect = LoadClass<UGameplayEffect>(nullptr, TEXT("/Game/Combo_Demo/BP/GAS/GE_Damage.GE_Damage_C"));
    if (!TestNotNull(TEXT("First montage"), First) || !TestNotNull(TEXT("Second montage"), Second)
        || !TestNotNull(TEXT("Preview mesh"), Mesh) || !TestNotNull(TEXT("Attack action"), Attack)
        || !TestNotNull(TEXT("Damage table"), DamageTable) || !TestNotNull(TEXT("Damage effect"), DamageEffect))
    {
        return false;
    }

    UGASComboGraphFactory* Factory = NewObject<UGASComboGraphFactory>();
    UGASComboGraphAsset* Asset = CastChecked<UGASComboGraphAsset>(Factory->FactoryCreateNew(
        UGASComboGraphAsset::StaticClass(), GetTransientPackage(), NAME_None, RF_Transient, nullptr, GWarn));
    Asset->StartInput = Attack;
    Asset->DamageTable = DamageTable;
    Asset->DamageEffect = DamageEffect;
    UEdGraph& Graph = *Asset->EditorGraph;
    const UEdGraphSchema* Schema = Graph.GetSchema();

    const auto AddMove = [&Graph](FName Name, UAnimMontage* Montage, FName Row)
    {
        FGraphNodeCreator<UGASComboMoveNode> Creator(Graph);
        UGASComboMoveNode* Node = Creator.CreateNode();
        Node->Move.Name = Name;
        Node->Move.Montage = Montage;
        Node->Move.DamageRow = Row;
        Node->Move.BufferOpensAt = 0.1f;
        Node->Move.CommitAt = 0.3f;
        Node->Move.HitOpensAt = 0.1f;
        Node->Move.HitClosesAt = 0.25f;
        Node->Move.bAllowMovementCancel = true;
        Creator.Finalize();
        return Node;
    };
    UEdGraphNode* Entry = Graph.Nodes[0];
    UGASComboMoveNode* Light1 = AddMove(TEXT("Light1"), First, TEXT("combat.dmg.light1"));
    UGASComboMoveNode* Light2 = AddMove(TEXT("Light2"), Second, TEXT("combat.dmg.light2"));
    FGraphNodeCreator<UGASComboTransitionNode> EdgeCreator(Graph);
    UGASComboTransitionNode* Edge = EdgeCreator.CreateNode();
    Edge->Transition.Input = Attack;
    Edge->Transition.bRequiresHit = true;
    EdgeCreator.Finalize();
    Light1->AutowireNewNode(Entry->FindPin(TEXT("First")));
    TestTrue(TEXT("Link Entry to Light1"), Entry->FindPin(TEXT("First"))->LinkedTo.Contains(Light1->FindPin(TEXT("In"))));
    Edge->AutowireNewNode(Light1->FindPin(TEXT("Out")));
    TestTrue(TEXT("Link Light1 to transition"), Light1->FindPin(TEXT("Out"))->LinkedTo.Contains(Edge->FindPin(TEXT("In"))));
    TestTrue(TEXT("Link transition to Light2"), Schema->TryCreateConnection(Edge->FindPin(TEXT("Out")), Light2->FindPin(TEXT("In"))));

    TArray<FText> Errors;
    FGraphNodeCreator<UGASComboTransitionNode> DraftCreator(Graph);
    UGASComboTransitionNode* Draft = DraftCreator.CreateNode();
    DraftCreator.Finalize();
    TestTrue(TEXT("An unconnected blank transition does not block the playable graph"), Asset->CompileGraph(Errors));
    TestEqual(TEXT("An empty draft is not included in runtime transitions"), Asset->Transitions.Num(), 1);
    TestTrue(TEXT("Connect one side of the draft"), Schema->TryCreateConnection(Light2->FindPin(TEXT("Out")), Draft->FindPin(TEXT("In"))));
    TestFalse(TEXT("An incomplete connected transition is still an error"), Asset->CompileGraph(Errors));
    Draft->DestroyNode();
    Light1->Move.DamageRow = TEXT("missing.damage");
    TestFalse(TEXT("Missing damage row invalidates the graph"), Asset->CompileGraph(Errors));
    TestEqual(TEXT("Invalid graph has no runnable entry"), Asset->Entry, INDEX_NONE);
    Light1->Move.DamageRow = TEXT("combat.dmg.light1");
    if (!TestTrue(TEXT("Graph compiles from project assets"), Asset->CompileGraph(Errors)))
    {
        for (const FText& Error : Errors)
        {
            AddError(Error.ToString());
        }
        return false;
    }

    if (FParse::Param(FCommandLine::Get(), TEXT("WriteGASComboExample")))
    {
        UAnimMontage* Third = LoadObject<UAnimMontage>(nullptr, *MontagePath(3));
        if (!TestNotNull(TEXT("Third sample montage"), Third))
        {
            return false;
        }
        UGASComboMoveNode* Light3 = AddMove(TEXT("轻击3"), Third, TEXT("combat.dmg.light3"));
        FGraphNodeCreator<UGASComboTransitionNode> NextEdgeCreator(Graph);
        UGASComboTransitionNode* NextEdge = NextEdgeCreator.CreateNode();
        NextEdge->Transition.Input = Attack;
        NextEdgeCreator.Finalize();
        Edge->Transition.bRequiresHit = false;
        TestTrue(TEXT("Link second sample edge"), Schema->TryCreateConnection(Light2->FindPin(TEXT("Out")), NextEdge->FindPin(TEXT("In"))));
        TestTrue(TEXT("Link third sample move"), Schema->TryCreateConnection(NextEdge->FindPin(TEXT("Out")), Light3->FindPin(TEXT("In"))));
        Light1->Move.Name = TEXT("轻击1");
        Light2->Move.Name = TEXT("轻击2");
        const TArray<UEdGraphNode*> Layout{ Entry, Light1, Edge, Light2, NextEdge, Light3 };
        for (int32 Index = 0; Index < Layout.Num(); ++Index)
        {
            Layout[Index]->NodePosX = Index * 250;
            Layout[Index]->NodePosY = 96;
        }
        for (UGASComboMoveNode* Node : { Light1, Light2, Light3 })
        {
            Node->Move.BufferOpensAt = Node->Move.Montage->GetPlayLength() * 0.18f;
            Node->Move.CommitAt = Node->Move.Montage->GetPlayLength() * 0.48f;
            Node->Move.HitOpensAt = Node->Move.Montage->GetPlayLength() * 0.1f;
            Node->Move.HitClosesAt = Node->Move.Montage->GetPlayLength() * 0.42f;
        }
        if (!Asset->CompileGraph(Errors))
        {
            for (const FText& Error : Errors)
            {
                AddError(Error.ToString());
            }
            return false;
        }
        const FString PackageName = TEXT("/GASComboGraph/Examples/CG_GhostSamurai_Light");
        UPackage* Package = CreatePackage(*PackageName);
        UGASComboGraphAsset* Example = DuplicateObject<UGASComboGraphAsset>(Asset, Package, TEXT("CG_GhostSamurai_Light"));
        Example->ClearFlags(RF_Transient);
        Example->SetFlags(RF_Public | RF_Standalone);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return TestTrue(TEXT("Save three-move sample"), UPackage::SavePackage(Package, Example,
            *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), Args));
    }

    const FString ExamplePath = TEXT("/GASComboGraph/Examples/CG_GhostSamurai_Light");
    UGASComboGraphAsset* Example = LoadObject<UGASComboGraphAsset>(nullptr, *ExamplePath);
    if (!TestNotNull(TEXT("Saved example graph"), Example))
    {
        return false;
    }
    TestTrue(TEXT("Saved sample is valid"), Example->ValidateGraph(Errors));
    TestEqual(TEXT("Saved sample has three moves"), Example->Moves.Num(), 3);
    TestEqual(TEXT("Saved sample has two transitions"), Example->Transitions.Num(), 2);
    if (FApp::CanEverRender())
    {
        UAssetEditorSubsystem* AssetEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        TestTrue(TEXT("Native graph editor opens the saved sample"), AssetEditors->OpenEditorForAsset(Example));
        const IAssetEditorInstance* Instance = AssetEditors->FindEditorForAsset(Example, false);
        TestTrue(TEXT("Saved sample opens in GAS Combo Graph editor"),
            Instance && Instance->GetEditorName() == FName(TEXT("GASComboGraphEditor")));
        AssetEditors->CloseAllEditorsForAsset(Example);
    }

    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    ACharacter* Owner = World->SpawnActor<ACharacter>();
    AActor* Target = World->SpawnActor<AActor>();
    if (!Owner || !Target)
    {
        AddError(TEXT("Failed to spawn test actors."));
        return false;
    }
    Owner->GetMesh()->SetSkeletalMeshAsset(Mesh);
    Owner->GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    Owner->GetMesh()->SetAnimInstanceClass(UAnimInstance::StaticClass());
    Owner->GetMesh()->InitAnim(true);
    UAnimInstance* Anim = Owner->GetMesh()->GetAnimInstance();
    if (!TestNotNull(TEXT("Animation instance"), Anim))
    {
        return false;
    }

    const auto InitializeBridge = [](AActor& Actor)
    {
        auto* Bridge = NewObject<UGASCombatBridgeComponent>(&Actor);
        Actor.AddInstanceComponent(Bridge);
        Bridge->bAutoInitializeOnBeginPlay = false;
        Bridge->AttributeSets = { UGASCombatCoreAttributeSet::StaticClass() };
        Bridge->RegisterComponent();
        return Bridge->InitializeCombat()
            && UGASCombatStatics::SetAttributeBaseValue(&Actor, UGASCombatCoreAttributeSet::GetMaxHealthAttribute(), 100.f)
            && UGASCombatStatics::SetAttributeBaseValue(&Actor, UGASCombatCoreAttributeSet::GetHealthAttribute(), 100.f);
    };
    if (!TestTrue(TEXT("Source GAS initialized"), InitializeBridge(*Owner))
        || !TestTrue(TEXT("Target GAS initialized"), InitializeBridge(*Target)))
    {
        return false;
    }

    auto* Runner = NewObject<UGASComboRunnerComponent>(Owner);
    Owner->AddInstanceComponent(Runner);
    Runner->ComboGraph = Asset;
    Runner->RegisterComponent();
    Owner->DispatchBeginPlay();

    Light1->Move.DamageRow = TEXT("missing.damage");
    TestFalse(TEXT("Recompiling an editor asset invalidates only the saved graph"), Asset->CompileGraph(Errors));
    if (!TestTrue(TEXT("Start first move"), Runner->SubmitInput(Attack)))
    {
        return false;
    }
    Light1->Move.DamageRow = TEXT("combat.dmg.light1");
    const int32 FirstSwing = Runner->GetSwingId();
    TestFalse(TEXT("Reject damage before the window"), Runner->ReportHit(FirstSwing, Target, nullptr));
    TestFalse(TEXT("Reject input before the window"), Runner->SubmitInput(Attack));
    Anim->Montage_SetPosition(First, 0.15f);
    TestTrue(TEXT("Record one valid attack input"), Runner->SubmitInput(Attack));
    TestTrue(TEXT("Apply project GAS damage"), Runner->ReportHit(FirstSwing, Target, nullptr));
    TestFalse(TEXT("Reject duplicate damage in the same swing"), Runner->ReportHit(FirstSwing, Target, nullptr));
    float ExpectedDamage = 0.f;
    TestTrue(TEXT("Read expected project damage"), Asset->ResolveDamage(Light1->Move.DamageRow, ExpectedDamage));
    TestEqual(TEXT("Subtract the configured amount once"), UGASCombatStatics::GetCombatAttributeSet(Target)->GetHealth(), 100.f - ExpectedDamage);

    Anim->Montage_SetPosition(First, 0.31f);
    Runner->PrimaryComponentTick.ExecuteTick(0.016f, LEVELTICK_All, ENamedThreads::GameThread, nullptr);
    TestTrue(TEXT("Hit-confirm transition starts next move"), Runner->GetSwingId() > FirstSwing);
    TestTrue(TEXT("Second montage is active"), Anim->Montage_IsActive(Second));
    TestFalse(TEXT("Old swing cannot report a hit"), Runner->ReportHit(FirstSwing, Target, nullptr));
    TestTrue(TEXT("Movement cancels this move"), Runner->TryMovementCancel());
    TestEqual(TEXT("Movement cancel clears swing"), Runner->GetSwingId(), 0);

    TestTrue(TEXT("Return to entry on next attack"), Runner->SubmitInput(Attack));
    Anim->Montage_SetPosition(First, 0.15f);
    TestTrue(TEXT("Buffer without a hit"), Runner->SubmitInput(Attack));
    const int32 WhiffSwing = Runner->GetSwingId();
    Anim->Montage_SetPosition(First, 0.31f);
    Runner->PrimaryComponentTick.ExecuteTick(0.016f, LEVELTICK_All, ENamedThreads::GameThread, nullptr);
    TestEqual(TEXT("Whiff does not follow hit-only edge"), Runner->GetSwingId(), WhiffSwing);
    Runner->SetActionBlocked(true);
    TestEqual(TEXT("External interruption clears swing"), Runner->GetSwingId(), 0);
    TestFalse(TEXT("External block rejects restart"), Runner->SubmitInput(Attack));
    Runner->SetActionBlocked(false);
    TestTrue(TEXT("Release external block"), Runner->SubmitInput(Attack));
    const int32 LastSwing = Runner->GetSwingId();
    UGASCombatStatics::ApplyDamageToTarget(Target, Owner, DamageEffect, 500.f);
    TestEqual(TEXT("Death event clears the active swing"), Runner->GetSwingId(), 0);
    TestFalse(TEXT("Dead owner cannot restart"), Runner->SubmitInput(Attack));
    TestFalse(TEXT("Dead owner cannot report an old hit"), Runner->ReportHit(LastSwing, Target, nullptr));
    return true;
}

#endif
