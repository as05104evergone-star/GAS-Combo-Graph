#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GASComboGraphNodes.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "GASComboGraphEditor"

namespace
{
    const FName ComboTabName(TEXT("GASComboGraph.Graph"));
    const FName DetailsTabName(TEXT("GASComboGraph.Details"));

    class SComboGraphEditor : public SGraphEditor
    {
    public:
        void Construct(const FArguments& Args, FSimpleDelegate InDeleteSelection)
        {
            DeleteSelection = MoveTemp(InDeleteSelection);
            SGraphEditor::Construct(Args);
        }

        virtual bool SupportsKeyboardFocus() const override { return true; }

        virtual FReply OnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
        {
            if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                FSlateApplication::Get().SetUserFocus(Event.GetUserIndex(), SharedThis(this), EFocusCause::Mouse);
            }
            return FReply::Unhandled();
        }

        virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override
        {
            if (Event.GetKey() == EKeys::Delete && !Event.GetModifierKeys().AnyModifiersDown())
            {
                if (!Event.IsRepeat())
                {
                    DeleteSelection.ExecuteIfBound();
                }
                return FReply::Handled();
            }
            return FReply::Unhandled();
        }

    private:
        FSimpleDelegate DeleteSelection;
    };

    class FComboEditor : public FAssetEditorToolkit, public FSelfRegisteringEditorUndoClient
    {
    public:
        void Open(UGASComboGraphAsset& Asset)
        {
            GraphAsset = &Asset;
            FDetailsViewArgs Args;
            Args.bAllowSearch = true;
            Details = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(Args);
            Details->SetObject(&Asset);
            SGraphEditor::FGraphEditorEvents Events;
            Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FComboEditor::SelectionChanged);
            Graph = SNew(SComboGraphEditor, FSimpleDelegate::CreateSP(this, &FComboEditor::DeleteSelected))
                .GraphToEdit(Asset.EditorGraph)
                .GraphEvents(Events)
                .IsEditable(true);

            Details->OnFinishedChangingProperties().AddSP(this, &FComboEditor::RefreshGraph);

            const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("GASComboGraphEditor_v1")
                ->AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
                    ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.72f)->AddTab(ComboTabName, ETabState::OpenedTab))
                    ->Split(FTabManager::NewStack()->SetSizeCoefficient(0.28f)->AddTab(DetailsTabName, ETabState::OpenedTab)));
            InitAssetEditor(EToolkitMode::Standalone, nullptr, GetToolkitFName(), Layout, true, true, &Asset);
        }

        virtual FName GetToolkitFName() const override { return TEXT("GASComboGraphEditor"); }
        virtual FText GetBaseToolkitName() const override { return LOCTEXT("Title", "GAS 连招图"); }
        virtual FString GetWorldCentricTabPrefix() const override { return TEXT("GAS Combo Graph"); }
        virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.1f, 0.4f, 0.6f, 1.f); }

        virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& Manager) override
        {
            FAssetEditorToolkit::RegisterTabSpawners(Manager);
            Manager->RegisterTabSpawner(ComboTabName, FOnSpawnTab::CreateSP(this, &FComboEditor::SpawnGraph))
                .SetDisplayName(LOCTEXT("GraphTab", "连招图"));
            Manager->RegisterTabSpawner(DetailsTabName, FOnSpawnTab::CreateSP(this, &FComboEditor::SpawnDetails))
                .SetDisplayName(LOCTEXT("DetailsTab", "详细信息"));
        }

        virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& Manager) override
        {
            Manager->UnregisterTabSpawner(ComboTabName);
            Manager->UnregisterTabSpawner(DetailsTabName);
            FAssetEditorToolkit::UnregisterTabSpawners(Manager);
        }

    private:
        TSharedRef<SDockTab> SpawnGraph(const FSpawnTabArgs& Args)
        {
            return SNew(SDockTab)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(6.f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("Validate", "校验连招图"))
                            .OnClicked(this, &FComboEditor::Validate)
                        ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
                        [
                            SNew(SButton)
                            .Text(LOCTEXT("DeleteSelection", "删除所选节点"))
                            .ToolTipText(LOCTEXT("DeleteShortcut", "删除所选节点（Delete）"))
                            .IsEnabled(this, &FComboEditor::CanDeleteSelected)
                            .OnClicked_Lambda([this]
                            {
                                DeleteSelected();
                                return FReply::Handled();
                            })
                        ]
                    ]
                    + SVerticalBox::Slot().FillHeight(1.f)[Graph.ToSharedRef()]
                ];
        }

        TSharedRef<SDockTab> SpawnDetails(const FSpawnTabArgs& Args)
        {
            return SNew(SDockTab)[Details.ToSharedRef()];
        }

        void SelectionChanged(const FGraphPanelSelectionSet& Selection)
        {
            TArray<UObject*> Objects;
            for (UObject* Object : Selection)
            {
                if (Object && !Object->IsA<UGASComboEntryNode>())
                {
                    Objects.Add(Object);
                }
            }
            if (Objects.IsEmpty())
            {
                Objects.Add(GraphAsset.Get());
            }
            Details->SetObjects(Objects);
        }

        void RefreshGraph(const FPropertyChangedEvent& Event)
        {
            Graph->NotifyGraphChanged();
        }

        virtual bool MatchesContext(const FTransactionContext& Context,
            const TArray<TPair<UObject*, FTransactionObjectEvent>>& Objects) const override
        {
            const UGASComboGraphAsset* Asset = GraphAsset.Get();
            return Asset && Objects.ContainsByPredicate([Asset](const auto& Entry)
            {
                return Entry.Key && (Entry.Key == Asset || Entry.Key->IsIn(Asset));
            });
        }

        virtual void PostUndo(bool bSuccess) override
        {
            if (bSuccess && Graph)
            {
                Graph->ClearSelectionSet();
                Graph->NotifyGraphChanged();
            }
        }

        virtual void PostRedo(bool bSuccess) override
        {
            PostUndo(bSuccess);
        }

        bool CanDeleteSelected() const
        {
            for (const UObject* Object : Graph->GetSelectedNodes())
            {
                if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object); Node && Node->CanUserDeleteNode())
                {
                    return true;
                }
            }
            return false;
        }

        void DeleteSelected()
        {
            TArray<UEdGraphNode*> NodesToDelete;
            for (UObject* Object : Graph->GetSelectedNodes())
            {
                if (UEdGraphNode* Node = Cast<UEdGraphNode>(Object); Node && Node->CanUserDeleteNode())
                {
                    NodesToDelete.Add(Node);
                }
            }
            if (NodesToDelete.IsEmpty())
            {
                return;
            }

            const FScopedTransaction Transaction(LOCTEXT("DeleteNodes", "删除连招节点"));
            Graph->GetCurrentGraph()->Modify();
            Graph->ClearSelectionSet();
            for (UEdGraphNode* Node : NodesToDelete)
            {
                Node->Modify();
                Node->DestroyNode();
            }
        }

        FReply Validate()
        {
            TArray<FText> Errors;
            if (GraphAsset->CompileGraph(Errors))
            {
                FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Valid", "连招图校验通过。保存后可在 PIE 里试招。"));
            }
            else
            {
                FMessageDialog::Open(EAppMsgType::Ok, FText::Join(FText::FromString(TEXT("\n")), Errors));
            }
            return FReply::Handled();
        }

        TWeakObjectPtr<UGASComboGraphAsset> GraphAsset;
        TSharedPtr<SGraphEditor> Graph;
        TSharedPtr<IDetailsView> Details;
    };

    class FComboAssetActions : public FAssetTypeActions_Base
    {
    public:
        virtual FText GetName() const override { return LOCTEXT("AssetName", "GAS 连招图"); }
        virtual FColor GetTypeColor() const override { return FColor(50, 145, 205); }
        virtual UClass* GetSupportedClass() const override { return UGASComboGraphAsset::StaticClass(); }
        virtual uint32 GetCategories() override { return EAssetTypeCategories::Gameplay; }

        virtual void OpenAssetEditor(const TArray<UObject*>& Objects, TSharedPtr<IToolkitHost> Host) override
        {
            for (UObject* Object : Objects)
            {
                if (UGASComboGraphAsset* Asset = Cast<UGASComboGraphAsset>(Object); Asset && Asset->EditorGraph)
                {
                    MakeShared<FComboEditor>()->Open(*Asset);
                }
            }
        }
    };

    class FGASComboGraphEditorModule : public IModuleInterface
    {
        virtual void StartupModule() override
        {
            Actions = MakeShared<FComboAssetActions>();
            FAssetToolsModule::GetModule().Get().RegisterAssetTypeActions(Actions.ToSharedRef());
        }

        virtual void ShutdownModule() override
        {
            if (Actions && FModuleManager::Get().IsModuleLoaded("AssetTools"))
            {
                FAssetToolsModule::GetModule().Get().UnregisterAssetTypeActions(Actions.ToSharedRef());
            }
            Actions.Reset();
        }

        TSharedPtr<FComboAssetActions> Actions;
    };
}

IMPLEMENT_MODULE(FGASComboGraphEditorModule, GASComboGraphEditor)

#undef LOCTEXT_NAMESPACE
