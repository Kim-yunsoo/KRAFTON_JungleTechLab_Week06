#pragma once
#include "SWindow.h"
#include "SSplitterV.h"
#include "SSplitterH.h"
#include "SViewportWindow.h"

class SSceneIOWindow; // 새로 추가할 UI
class SDetailsWindow;
class UMenuBarWidget;
class SMultiViewportWindow : public SWindow
{
public:
    void SaveSplitterConfig();
    void LoadSplitterConfig();
    SMultiViewportWindow();
    virtual ~SMultiViewportWindow();

    void Initialize(ID3D11Device* Device, UWorld* World, const FRect& InRect, SViewportWindow* MainViewport);
    void SwitchLayout(EViewportLayoutMode NewMode);

    void SwitchPanel(SWindow* SwitchPanel);


    virtual void OnRender() override;
    virtual void OnUpdate(float deltaSecond) override;
    virtual void OnMouseMove(FVector2D MousePos) override;
    virtual void OnMouseDown(FVector2D MousePos, uint32 Button) override;
    virtual void OnMouseUp(FVector2D MousePos, uint32 Button) override;

    void SetMainViewPort();
    EViewportLayoutMode GetCurrentLayoutMode() { return CurrentMode; }
    SViewportWindow** GetViewports() { return Viewports; }
    SViewportWindow* GetMainViewport() { return MainViewport; }
    void OnShutdown();

    static SViewportWindow* ActiveViewport; // 현재 드래그 중인 뷰포트
private:
    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    SSplitterH* RootSplitter = nullptr;

    // 두 가지 레이아웃을 미리 생성해둠
    SSplitter* FourSplitLayout = nullptr;
    SSplitter* SingleLayout = nullptr;

    // 뷰포트
    SViewportWindow* Viewports[4];
    SViewportWindow* MainViewport;
 
    SSplitterH* LeftTop;
    SSplitterH* LeftBottom ;
    // 오른쪽 고정 UI
    SWindow* SceneIOPanel = nullptr;
    // 아래쪽 UI
    SWindow* ControlPanel = nullptr;
    SWindow* DetailPanel = nullptr;

    SSplitterV* TopPanel = nullptr;
    SSplitterV* LeftPanel = nullptr;

    SSplitterV* BottomPanel ;
    
   

    // 현재 모드
    EViewportLayoutMode CurrentMode = EViewportLayoutMode::FourSplit;

    // 애니메이션 관련 변수
    bool bIsAnimating = false;
    float AnimationProgress = 0.0f;
    float AnimationDuration = 0.5f; // 애니메이션 지속 시간 (초)

    // 애니메이션 방향 (true: 4분할->전체화면, false: 전체화면->4분할)
    bool bExpandingToSingle = false;

    // 애니메이션 대상 뷰포트
    SWindow* AnimTargetViewport = nullptr;

    // 저장된 스플리터 비율 (애니메이션 전 상태로 복귀하기 위해)
    float SavedLeftTopRatio = 0.5f;
    float SavedLeftBottomRatio = 0.5f;
    float SavedLeftPanelRatio = 0.5f;

    // 애니메이션 업데이트 함수
    void UpdateAnimation(float DeltaSeconds);
    void StartExpandAnimation(SWindow* TargetViewport);
    void StartCollapseAnimation();

    // 메뉴바 관련
    void OnFileMenuAction(const char* action);
    void OnEditMenuAction(const char* action);
    void OnWindowMenuAction(const char* action);
    void OnHelpMenuAction(const char* action);

    UMenuBarWidget* MenuBar;
};
