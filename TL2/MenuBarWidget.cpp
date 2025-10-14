#include "pch.h"
#include "MenuBarWidget.h"
#include "SMultiViewportWindow.h"
#include "ImGui/imgui.h"
#include "UI/UIManager.h"
#include "World.h"
#include "Renderer.h"

// 필요하다면 외부 free 함수 사용 가능 (동일 TU가 아닐 경우 extern 선언이 필요)
// extern void LoadSplitterConfig(SSplitter* RootSplitter);

UMenuBarWidget::UMenuBarWidget() {}
UMenuBarWidget::UMenuBarWidget(SMultiViewportWindow* InOwner) : Owner(InOwner) {}

void UMenuBarWidget::Initialize() {}
void UMenuBarWidget::Update() {}

void UMenuBarWidget::RenderWidget()
{
    if (!ImGui::BeginMainMenuBar())
        return;

    // ===== File =====
    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene", "Ctrl+N"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("new_scene");

        if (ImGui::MenuItem("Open Scene", "Ctrl+O"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("open_scene");

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("save_scene");

        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("save_scene_as");

        ImGui::Separator();

        if (ImGui::BeginMenu("Recent Scenes"))
        {
            ImGui::MenuItem("Scene1.scene");
            ImGui::MenuItem("Scene2.scene");
            ImGui::MenuItem("Scene3.scene");
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit", "Alt+F4"))
            (FileAction ? FileAction : [this](auto a) { OnFileMenuAction(a); })("exit");

        ImGui::EndMenu();
    }

    // ===== Edit =====
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("undo");
        if (ImGui::MenuItem("Redo", "Ctrl+Y"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("redo");

        ImGui::Separator();

        if (ImGui::MenuItem("Cut", "Ctrl+X"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("cut");
        if (ImGui::MenuItem("Copy", "Ctrl+C"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("copy");
        if (ImGui::MenuItem("Paste", "Ctrl+V"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("paste");
        if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("duplicate");
        if (ImGui::MenuItem("Delete", "Delete"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("delete");

        ImGui::Separator();

        if (ImGui::MenuItem("Select All", "Ctrl+A"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("select_all");

        ImGui::Separator();

        if (ImGui::MenuItem("Project Settings"))
            (EditAction ? EditAction : [this](auto a) { OnEditMenuAction(a); })("project_settings");

        ImGui::EndMenu();
    }

    // ===== Window =====
    if (ImGui::BeginMenu("Window"))
    {
        if (ImGui::BeginMenu("Viewport Layout"))
        {
            if (ImGui::MenuItem("Single Viewport"))
                (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("single_viewport");

            if (ImGui::MenuItem("Four Split"))
                (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("four_split");

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Details Panel"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("details_panel");

        if (ImGui::MenuItem("Scene Manager"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("scene_manager");

        if (ImGui::MenuItem("Console"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("console");

        if (ImGui::MenuItem("Control Panel"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("control_panel");

        ImGui::Separator();

        if (ImGui::MenuItem("Reset Layout"))
            (WindowAction ? WindowAction : [this](auto a) { OnWindowMenuAction(a); })("reset_layout");

        ImGui::EndMenu();
    }

    // ===== AA (Anti-Aliasing) =====
    // ===== AA (Anti-Aliasing) =====
    if (ImGui::BeginMenu("AA"))
    {
        // Acquire renderer from current world
        URenderer* Renderer = nullptr;
        if (UWorld* World = UUIManager::GetInstance().GetWorld())
        {
            Renderer = World->GetRenderer();
        }

        if (!Renderer)
        {
            ImGui::TextDisabled("Renderer unavailable");
            ImGui::EndMenu();
            return;
        }

        // --- FXAA On/Off ---
        bool fxaaEnabled = Renderer->IsFXAAEnabled();
        const char* modeLabel = fxaaEnabled ? "FXAA" : "Off";
        if (ImGui::BeginCombo("Mode", modeLabel))
        {
            const bool selOff = !fxaaEnabled;
            const bool selFxaa = fxaaEnabled;

            if (ImGui::Selectable("Off", selOff))
            {
                Renderer->SetFXAAEnabled(false);
                fxaaEnabled = false;
            }
            if (ImGui::Selectable("FXAA", selFxaa))
            {
                Renderer->SetFXAAEnabled(true);
                fxaaEnabled = true;
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::Text("Options");

        // Checkbox toggle (동일 기능, 즉각 토글)
        bool fxaaBox = fxaaEnabled;
        if (ImGui::Checkbox("FXAA", &fxaaBox))
        {
            Renderer->SetFXAAEnabled(fxaaBox);
            fxaaEnabled = fxaaBox;
        }

        // --- FXAA 파라미터 ---
        if (ImGui::CollapsingHeader("FXAA Params"))
        {
            // Defaults from NVIDIA FXAA 3.11
            static float fxaa_SpanMax = 8.0f;
            static float fxaa_ReduceMul = 1.0f / 8.0f;
            static float fxaa_ReduceMin = 1.0f / 128.0f;

            if (fxaaEnabled)
            {
                bool changed = false;
                changed |= ImGui::SliderFloat("Span Max", &fxaa_SpanMax, 1.0f, 16.0f, "%.1f");
                changed |= ImGui::SliderFloat("Reduce Mul", &fxaa_ReduceMul, 1.0f / 32.0f, 1.0f / 4.0f, "%.4f");
                changed |= ImGui::SliderFloat("Reduce Min", &fxaa_ReduceMin, 1.0f / 256.0f, 1.0f / 64.0f, "%.5f");

                if (ImGui::Button("Reset FXAA"))
                {
                    fxaa_SpanMax = 8.0f;
                    fxaa_ReduceMul = 1.0f / 8.0f;
                    fxaa_ReduceMin = 1.0f / 128.0f;
                    changed = true;
                }

                if (changed)
                {
                    Renderer->SetFXAAParams(fxaa_SpanMax, fxaa_ReduceMul, fxaa_ReduceMin);
                }
            }
            else
            {
                ImGui::TextDisabled("FXAA disabled");
            }
        }

        ImGui::EndMenu();
    }
    // ===== Help =====
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Documentation"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("documentation");

        if (ImGui::MenuItem("Tutorials"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("tutorials");

        ImGui::Separator();

        if (ImGui::MenuItem("Keyboard Shortcuts"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("shortcuts");

        ImGui::Separator();

        if (ImGui::MenuItem("About"))
            (HelpAction ? HelpAction : [this](auto a) { OnHelpMenuAction(a); })("about");

        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// ---------------- 기본 동작 ----------------

void UMenuBarWidget::OnFileMenuAction(const char* action)
{
    UE_LOG("File menu action: %s", action);

    if (strcmp(action, "new_scene") == 0)
    {
        // TODO: 새 씬 생성
    }
    else if (strcmp(action, "open_scene") == 0)
    {
        // TODO: 씬 열기
    }
    else if (strcmp(action, "save_scene") == 0)
    {
        // TODO: 저장
    }
    else if (strcmp(action, "save_scene_as") == 0)
    {
        // TODO: 다른 이름으로 저장
    }
    else if (strcmp(action, "exit") == 0)
    {
        PostQuitMessage(0);
    }
}

void UMenuBarWidget::OnEditMenuAction(const char* action)
{
    UE_LOG("Edit menu action: %s", action);
    // TODO: Undo/Redo/Cut/Copy/Paste 등 구현 혹은 외부 콜백으로 처리
}

void UMenuBarWidget::OnWindowMenuAction(const char* action)
{
    UE_LOG("Window menu action: %s", action);

    if (!Owner) return;

    if (strcmp(action, "single_viewport") == 0)
    {
        Owner->SwitchLayout(EViewportLayoutMode::SingleMain);
    }
    else if (strcmp(action, "four_split") == 0)
    {
        Owner->SwitchLayout(EViewportLayoutMode::FourSplit);
    }
    else if (strcmp(action, "reset_layout") == 0)
    {
        // 추천: SMultiViewportWindow에 ResetLayout() 같은 래퍼 함수 하나 만들어 호출
        // Owner->ResetLayout();

        // 임시: 이미 공개 범위라면 외부 free 함수를 사용해도 됨
        // LoadSplitterConfig(Owner->GetRootSplitter());  // GetRootSplitter() 제공 필요
    }
    else
    {
        // "details_panel", "scene_manager", "console", "control_panel" 토글은
        // Owner 쪽에 Show/Hide API를 추가하여 호출하는 걸 권장
        // 예: Owner->ToggleDetailsPanel();
    }
}

void UMenuBarWidget::OnHelpMenuAction(const char* action)
{
    UE_LOG("Help menu action: %s", action);
    // TODO: 문서/튜토리얼/단축키/어바웃 처리
}
