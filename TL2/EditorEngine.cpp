#pragma once
#include"pch.h"
#include "EditorEngine.h"
#include"GameEngine.h"
#include "World.h"
#include "SelectionManager.h"
#include "FViewportClient.h"
#include"SViewportWindow.h"
#include"SMultiViewportWindow.h"
#include "EditorClipboard.h"
#include "UI/UIManager.h"
#include "GizmoActor.h"
UEditorEngine::UEditorEngine()
{
}

void UEditorEngine::Tick(float DeltaSeconds)
{
    // 지연 삭제 처리 (이전 프레임에서 요청된 삭제)
    if (bPIEShutdownRequested)
    {
        // GameEngine 삭제
        if (PendingDeleteGameEngine)
        {
            ObjectFactory::DeleteObject(PendingDeleteGameEngine);
            //  PendingDeleteGameEngine->EndGame();  // CleanupWorld 호출
            PendingDeleteGameEngine = nullptr;
        }

        // PIE 월드 삭제
        if (PendingDeletePIEWorld)
        {
            ObjectFactory::DeleteObject(PendingDeletePIEWorld);
            PendingDeletePIEWorld = nullptr;
        }

        bPIEShutdownRequested = false;
    }

    // PIE 실행 중이면 PIE 월드만 Tick
    if (GameEngine && GameEngine->GameWorld)
    {
        GameEngine->Tick(DeltaSeconds);
    }
    else
    {
        // 에디터 모드일 때만 에디터 월드 Tick
        for (size_t i = 0; i < WorldContexts.size(); ++i)
        {
            UWorld* World = WorldContexts[i].World();
            if (!World) continue;

            if (WorldContexts[i].WorldType == EWorldType::Editor)
            {
                World->Tick(DeltaSeconds);
            }
        }

        // 에디터 전용 단축키 처리 (PIE 중이 아닐 때만)
        ProcessEditorShortcuts();

        // Alt+드래그 복제 처리
        ProcessAltDragDuplication();
    }
}

void UEditorEngine::Render()
{
    // PIE 실행 중이면 PIE 월드만 렌더링
    if (GameEngine && GameEngine->GameWorld)
    {
        GameEngine->Render();
    }
    else
    {
        // 에디터 모드일 때만 에디터 월드 렌더링
        for (size_t i = 0; i < WorldContexts.size(); ++i)
        {
            UWorld* World = WorldContexts[i].World();
            if (!World) continue;

            if (WorldContexts[i].WorldType == EWorldType::Editor)
            {
                World->Render();
            }
        }
    }
}

void UEditorEngine::StartPIE()
{
    UWorld* EditorWorld = GetWorld(EWorldType::Editor);
    if (!EditorWorld) return;

    // PIE 시작 전 에디터 선택 해제 (PIE와 에디터 선택 분리)
    USelectionManager::GetInstance().ClearSelection();

    UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorld);
    if (!PIEWorld) return;
     

    FWorldContext PieCtx;
    PieCtx.SetWorld(PIEWorld, EWorldType::PIE);
    WorldContexts.push_back(PieCtx);

    // GWorld를 PIE 월드로 전환
    GWorld = PIEWorld;

    // 메인 뷰포트 ViewportClient를 PIE 월드로 전환
    PIEWorld->GetMainViewport()->GetViewportClient()->SetWorld(PIEWorld);


    GameEngine = NewObject<UGameEngine>();
    GameEngine->StartGame(PIEWorld);

    PIEWorld->InitializeActorsForPlay();
}

void UEditorEngine::EndPIE()
{
    if (!GameEngine)
        return;

    UWorld* PIEWorld = nullptr;

    // PIE 월드 찾기
    for (auto& Context : WorldContexts)
    {
        if (Context.WorldType == EWorldType::PIE)
        {
            PIEWorld = Context.World();
            break;
        }
    }

    // PIE 종료 시 선택 해제 (PIE 액터 참조 제거)
    USelectionManager::GetInstance().ClearSelection();

    // ViewportClient의 World를 에디터 월드로 복원
    UWorld* EditorWorld = GetWorld(EWorldType::Editor);
    GWorld = EditorWorld;

    if (EditorWorld && EditorWorld->GetMainViewport())
    {
        if (FViewportClient* ViewportClient = EditorWorld->GetMainViewport()->GetViewportClient())
        {
            ViewportClient->SetWorld(EditorWorld);
        }
    }

    // GameEngine 정리 (CleanupWorld 호출)
    if (GameEngine)
    {
        // 지연 삭제를 위해 저장
        PendingDeleteGameEngine = GameEngine;

        GameEngine = nullptr;  // 즉시 nullptr로 설정해서 Tick/Render에서 안 쓰도록
    }

    // PIE 월드 지연 삭제 예약
    if (PIEWorld)
    {
        PendingDeletePIEWorld = PIEWorld;
    }

    // 다음 Tick에서 삭제하도록 플래그 설정
    bPIEShutdownRequested = true;

    // PIE WorldContext 제거 (즉시)
    for (int i = (int)WorldContexts.size() - 1; i >= 0; --i)
    {
        if (WorldContexts[i].WorldType == EWorldType::PIE)
        {
            WorldContexts.erase(WorldContexts.begin() + i);
            break;
        }
    }
}

void UEditorEngine::ProcessEditorShortcuts()
{
    UInputManager& InputManager = UInputManager::GetInstance();

    // Ctrl 키가 눌려있는지 확인
    bool bCtrlDown = InputManager.IsKeyDown(VK_CONTROL);

    if (bCtrlDown)
    {
        // Ctrl+C: Copy (이번 프레임에 처음 눌렸을 때만)
        if (InputManager.IsKeyPressed('C'))  // VK 'C' = 0x43
        {
            TArray<AActor*> SelectedActors = USelectionManager::GetInstance().GetSelectedActors();
            if (!SelectedActors.empty())
            {
                UEditorClipboard::GetInstance().Copy(SelectedActors);
            }
            else
            {
                UE_LOG("EditorEngine: No actors selected to copy");
            }
        }
        // Ctrl+V: Paste
        else if (InputManager.IsKeyPressed('V'))  // VK 'V' = 0x56
        {
            if (UEditorClipboard::GetInstance().HasCopiedActors())
            {
                UWorld* EditorWorld = GetWorld(EWorldType::Editor);
                if (!EditorWorld) return;

                TArray<AActor*> PastedActors = UEditorClipboard::GetInstance().Paste(EditorWorld);

                // 붙여넣은 액터들을 선택
                if (!PastedActors.empty())
                {
                    USelectionManager::GetInstance().ClearSelection();
                    for (AActor* Actor : PastedActors)
                    {
                        USelectionManager::GetInstance().SelectActor(Actor);
                    }

                    // 첫 번째 액터를 Gizmo 타겟으로 설정
                    AGizmoActor* GizmoActor = EditorWorld->GetGizmoActor();
                    if (GizmoActor && !PastedActors.empty())
                    {
                        GizmoActor->SetTargetActor(PastedActors[0]);
                        GizmoActor->SetActorLocation(PastedActors[0]->GetActorLocation());
                    }

                    // UI 업데이트
                    UUIManager::GetInstance().SetPickedActor(PastedActors[0]);
                }
            }
            else
            {
                UE_LOG("EditorEngine: Nothing to paste");
            }
        }
    }
}

void UEditorEngine::ProcessAltDragDuplication()
{
    UInputManager& InputManager = UInputManager::GetInstance();
    UWorld* EditorWorld = GetWorld(EWorldType::Editor);
    if (!EditorWorld) return;

    AGizmoActor* GizmoActor = EditorWorld->GetGizmoActor();
    if (!GizmoActor) return;

    // Alt 키가 눌려있고, 마우스 왼쪽 버튼이 눌려있고, 기즈모가 드래그 중인지 확인
    bool bAltDown = InputManager.IsKeyDown(VK_MENU);
    bool bLeftButtonDown = InputManager.IsMouseButtonDown(LeftButton);
    bool bGizmoDragging = GizmoActor->GetbIsDragging();

    // Alt+드래그가 시작되었을 때 (드래그 중이고 아직 복제하지 않았을 때)
    if (bAltDown && bLeftButtonDown && bGizmoDragging && !bAltDragDuplicationHandled)
    {
        AActor* TargetActor = GizmoActor->GetTargetActor();
        if (TargetActor)
        {
            // 액터 복제
            AActor* DuplicatedActor = Cast<AActor>(TargetActor->Duplicate());

            if (DuplicatedActor)
            {
                // 월드에 스폰
                EditorWorld->SpawnActor(DuplicatedActor);

                // 복제된 액터를 선택
                USelectionManager::GetInstance().ClearSelection();
                USelectionManager::GetInstance().SelectActor(DuplicatedActor);

                // 기즈모 타겟을 복제된 액터로 변경
                GizmoActor->SetTargetActor(DuplicatedActor);
                GizmoActor->SetActorLocation(DuplicatedActor->GetActorLocation());

                // UI 업데이트
                UUIManager::GetInstance().SetPickedActor(DuplicatedActor);

                // 복제 처리 완료 표시
                bAltDragDuplicationHandled = true;

                UE_LOG("EditorEngine: Alt+Drag duplication completed");
            }
        }
    }

    // 마우스 버튼이 떼어지면 복제 상태 리셋
    if (!bLeftButtonDown)
    {
        bAltDragDuplicationHandled = false;
    }
}

UEditorEngine::~UEditorEngine()
{
}

