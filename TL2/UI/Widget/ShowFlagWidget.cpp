#include "pch.h"
#include "ShowFlagWidget.h"
#include "../../World.h"
#include "../../SViewportWindow.h"
#include "../../FViewport.h"
#include "../UIManager.h"
#include "../../Renderer.h"

UShowFlagWidget::UShowFlagWidget()
{
}

UShowFlagWidget::~UShowFlagWidget()
{
}

void UShowFlagWidget::Initialize()
{
    // 기본값으로 초기화
    bPrimitives = true;
    bStaticMeshes = true; 
    bWireframe = false;
    bBillboardText = false;
    bBoundingBoxes = false;
    bGrid = true;
    bLighting = true;
}

void UShowFlagWidget::Update()
{
    // 매 프레임마다 업데이트할 내용이 있다면 여기에 추가
}

void UShowFlagWidget::RenderWidget()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 메인 뷰포트 가져오기
    SViewportWindow* MainViewportWindow = World->GetMainViewport();
    if (!MainViewportWindow) return;

    FViewport* Viewport = MainViewportWindow->GetViewport();
    if (!Viewport) return;

    // Viewport의 현재 Show Flag 상태와 동기화
    SyncWithViewport(Viewport);
    
    // Show Flag 섹션 헤더
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    bool bHeaderOpen = ImGui::CollapsingHeader("Show Flags", bIsExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor();
    
    if (bHeaderOpen)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, SectionColor);
        
        // 컴팩트 모드 토글
        ImGui::Checkbox("Compact Mode", &bCompactMode);
        ImGui::SameLine();
        ImGui::Checkbox("Show Tooltips", &bShowTooltips);
        
        ImGui::Separator();
        
        if (ImGui::BeginChild("ShowFlagsContent", ImVec2(0, bCompactMode ? 120.0f : 200.0f), true))
        {
            if (bCompactMode)
            {
                // 컴팩트 모드: 중요한 것들만 간단히
                RenderShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives, Viewport);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid, Viewport);

                RenderShowFlagCheckbox("Static Meshes", EEngineShowFlags::SF_StaticMeshes, Viewport);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Text", EEngineShowFlags::SF_BillboardText, Viewport);

                RenderShowFlagCheckbox("Bounds", EEngineShowFlags::SF_BoundingBoxes, Viewport);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Wireframe", EEngineShowFlags::SF_Wireframe, Viewport);
            }
            else
            {
                // 전체 제어 버튼들
                RenderControlButtons(Viewport);

                ImGui::Separator();

                // 카테고리별 섹션들
                RenderPrimitiveSection(Viewport);
                ImGui::Separator();

                RenderDebugSection(Viewport);
                ImGui::Separator();

            }
        }
        ImGui::EndChild();
        
        ImGui::PopStyleColor();
    }
}

UWorld* UShowFlagWidget::GetWorld()
{
    // UIManager를 통해 World 참조 가져오기
    return UUIManager::GetInstance().GetWorld();
}

void UShowFlagWidget::SyncWithViewport(FViewport* Viewport)
{
    if (!Viewport) return;

    EEngineShowFlags CurrentFlags = Viewport->GetShowFlags();

    // 각 플래그 상태를 로컬 변수에 동기화
    bPrimitives = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
    bStaticMeshes = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
    bWireframe = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Wireframe);
    bBillboardText = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_BillboardText);
    bBoundingBoxes = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
    bGrid = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
    bLighting = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Lighting);
}

void UShowFlagWidget::RenderShowFlagCheckbox(const char* Label, EEngineShowFlags Flag, FViewport* Viewport)
{
    if (!Viewport) return;

    bool bCurrentState = Viewport->IsShowFlagEnabled(Flag);

    // 상태에 따라 색상 변경
    ImVec4 checkboxColor = bCurrentState ? ActiveColor : InactiveColor;
    ImGui::PushStyleColor(ImGuiCol_CheckMark, checkboxColor);

    if (ImGui::Checkbox(Label, &bCurrentState))
    {
        // 체크박스 상태가 변경되면 Viewport의 Show Flag 업데이트
        if (bCurrentState)
        {
            Viewport->EnableShowFlag(Flag);
        }
        else
        {
            Viewport->DisableShowFlag(Flag);
        }
    }

    ImGui::PopStyleColor();
    
    // 툴팁 표시
    if (bShowTooltips && ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        
        // 각 플래그별 설명
        switch (Flag)
        {
        case EEngineShowFlags::SF_Primitives:
            ImGui::Text("모든 프리미티브 지오메트리 표시/숨김");
            ImGui::Text("이 옵션을 끄면 모든 3D 오브젝트가 숨겨집니다.");
            break;
        case EEngineShowFlags::SF_StaticMeshes:
            ImGui::Text("Static Mesh 액터들 표시/숨김");
            ImGui::Text("일반적인 3D 메시 오브젝트들의 가시성을 제어합니다.");
            break;
        case EEngineShowFlags::SF_Wireframe:
            ImGui::Text("와이어프레임 오버레이 표시/숨김");
            ImGui::Text("3D 모델의 와이어프레임을 표시합니다.");
            break;
        case EEngineShowFlags::SF_BillboardText:
            ImGui::Text("오브젝트 위의 UUID 텍스트 표시/숨김");
            ImGui::Text("각 오브젝트 위에 표시되는 식별자 텍스트입니다.");
            break;
        case EEngineShowFlags::SF_BoundingBoxes:
            ImGui::Text("충돌 경계(Bounding Box) 표시/숨김");
            ImGui::Text("오브젝트의 충돌 범위를 시각적으로 표시합니다.");
            break;
        case EEngineShowFlags::SF_Grid:
            ImGui::Text("월드 그리드 표시/숨김");
            ImGui::Text("3D 공간의 참조용 격자를 표시합니다.");
            break;
        case EEngineShowFlags::SF_Lighting:
            ImGui::Text("조명 효과 활성화/비활성화");
            ImGui::Text("라이팅 계산을 켜거나 끕니다.");
            break;
        default:
            ImGui::Text("Show Flag 설정");
            break;
        }
        
        ImGui::EndTooltip();
    }
}

void UShowFlagWidget::RenderPrimitiveSection(FViewport* Viewport)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Primitive Rendering"))
    {
        ImGui::PopStyleColor();

        RenderShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives, Viewport);

        // Primitives가 활성화되어 있을 때만 하위 옵션들 활성화
        bool bPrimitivesEnabled = Viewport->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
        if (!bPrimitivesEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }

        ImGui::Indent(15.0f);
        RenderShowFlagCheckbox("Static Meshes", EEngineShowFlags::SF_StaticMeshes, Viewport);
        RenderShowFlagCheckbox("Wireframe", EEngineShowFlags::SF_Wireframe, Viewport);
        ImGui::Unindent(15.0f);

        if (!bPrimitivesEnabled)
        {
            ImGui::PopStyleVar();
        }

        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderDebugSection(FViewport* Viewport)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Debug Features"))
    {
        ImGui::PopStyleColor();

        RenderShowFlagCheckbox("Billboard Text", EEngineShowFlags::SF_BillboardText, Viewport);
        RenderShowFlagCheckbox("Bounding Boxes", EEngineShowFlags::SF_BoundingBoxes, Viewport);
        RenderShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid, Viewport);

        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderLightingSection(FViewport* Viewport)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Lighting"))
    {
        ImGui::PopStyleColor();

        RenderShowFlagCheckbox("Lighting", EEngineShowFlags::SF_Lighting, Viewport);

        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderControlButtons(FViewport* Viewport)
{
    if (!Viewport) return;

    ImGui::Text("Quick Controls:");

    // 버튼들을 한 줄에 배치
    if (ImGui::Button("Show All"))
    {
        Viewport->SetShowFlags(EEngineShowFlags::SF_All);
    }

    ImGui::SameLine();

    if (ImGui::Button("Hide All"))
    {
        Viewport->SetShowFlags(EEngineShowFlags::None);
    }

    ImGui::SameLine();

    if (ImGui::Button("Reset"))
    {
        Viewport->SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
    }
}

