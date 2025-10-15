#pragma once

#include "SceneComponent.h"

/**
 * UHeightFogComponent
 * - Unreal Engine의 Exponential Height Fog를 단순화하여 구현
 * - Basic Inscattering만 지원 (Volumetric/Directional Inscattering 제외)
 */
class UHeightFogComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UHeightFogComponent, USceneComponent)

    UHeightFogComponent();
    virtual ~UHeightFogComponent() override;

    // Component Tick
    virtual void TickComponent(float DeltaSeconds) override;

    // Rendering
    void Render(class URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);

    // Duplicate
    virtual UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    virtual void DuplicateSubObjects() override;

    // ============================================================
    // Fog Density Settings
    // ============================================================

    /** 안개의 전역 밀도 계수 (값이 클수록 짙은 안개) */
    void SetFogDensity(float InDensity) { FogDensity = InDensity; }
    float GetFogDensity() const { return FogDensity; }

    /** 높이에 따른 안개 밀도 감소율 (값이 클수록 급격히 감소) */
    void SetFogHeightFalloff(float InFalloff) { FogHeightFalloff = InFalloff; }
    float GetFogHeightFalloff() const { return FogHeightFalloff; }

    // ============================================================
    // Fog Distance Settings
    // ============================================================

    /** 안개가 시작되는 카메라로부터의 거리 */
    void SetStartDistance(float InDistance) { StartDistance = InDistance; }
    float GetStartDistance() const { return StartDistance; }

    /** 안개가 완전히 불투명해지는 최대 거리 */
    void SetFogCutoffDistance(float InDistance) { FogCutoffDistance = InDistance; }
    float GetFogCutoffDistance() const { return FogCutoffDistance; }

    // ============================================================
    // Fog Appearance Settings
    // ============================================================

    /** 안개의 최대 불투명도 [0, 1] */
    void SetFogMaxOpacity(float InOpacity) { FogMaxOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f); }
    float GetFogMaxOpacity() const { return FogMaxOpacity; }

    /** 안개의 산란 색상 (안개가 빛을 산란시킬 때의 색상) */
    void SetFogInscatteringColor(const FVector4& InColor) { FogInscatteringColor = InColor; }
    FVector4 GetFogInscatteringColor() const { return FogInscatteringColor; }

    // ============================================================
    // Utility Methods
    // ============================================================

    /** 특정 위치에서의 안개 밀도 계산 (높이 기반) */
    float CalculateFogDensityAtHeight(float WorldHeight) const;

    /** 카메라에서 특정 지점까지의 안개 적용량 계산 */
    float CalculateFogAmount(const FVector& CameraPosition, const FVector& WorldPosition) const;

    /** 컴포넌트 활성화 여부 */
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
    bool IsEnabled() const { return bEnabled; }

protected:
    // ============================================================
    // Fog Properties
    // ============================================================

    /** 안개의 전역 밀도 계수 */
    float FogDensity = 0.02f;

    /** 높이에 따른 안개 밀도 감소율 */
    float FogHeightFalloff = 0.2f;

    /** 안개가 시작되는 거리 (카메라로부터) */
    float StartDistance = 0.0f;

    /** 안개가 최대 불투명도에 도달하는 거리 */
    float FogCutoffDistance = 10000.0f;

    /** 안개의 최대 불투명도 [0, 1] */
    float FogMaxOpacity = 1.0f;

    /** 안개의 산란 색상 */
    FVector4 FogInscatteringColor{ 0.447f, 0.639f, 1.0f, 1.0f }; // 기본: 하늘색

    /** 컴포넌트 활성화 여부 */
    bool bEnabled = true;

    // ============================================================
    // Billboard for Editor
    // ============================================================

    /** 빌보드 렌더링 (Editor 전용) */
    void RenderBillboard(class URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);

    /** 빌보드 정점 생성 */
    void CreateBillboardVertices();

    /** 빌보드 쿼드 메시 */
    class UTextQuad* BillboardQuad = nullptr;

    /** 빌보드 텍스처 */
    class UTexture* BillboardTexture = nullptr;

    /** 빌보드 머티리얼 */
    class UMaterial* BillboardMaterial = nullptr;

    /** 빌보드 크기 */
    float BillboardWidth = 1.0f;
    float BillboardHeight = 1.0f;
};