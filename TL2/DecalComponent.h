#pragma once
#include "SceneComponent.h"
#include "Texture.h"
#include "OrientedBox.h"

class URenderer;
class FViewport;
class UWorld;
class UStaticMeshComponent;
class AStaticMeshActor;

// Decal Fade State
namespace EDecalFadeState
{
    enum Type
    {
        None,
        FadingIn,
        FadingOut
    };
}

// UDecalComponent
//  - Forward Projection Decal 렌더링
//  - X축 Forward Vector 기준으로 Projection
//  - Fade In / Out 지원
class UDecalComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UDecalComponent, USceneComponent)

    UDecalComponent();

protected:
    ~UDecalComponent() override;

public:
    // Rendering
    virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport);

    // Decal Box와 충돌하는 Static Mesh 컴포넌트 찾기
    TArray<UStaticMeshComponent*> FindAffectedMeshes(UWorld* World);

    // Texture
    void SetDecalTexture(const FString& TexturePath);
    UTexture* GetDecalTexture() const { return DecalTexture; }

    // Decal Size (Box Volume)
    // X = Projection Depth (Forward)
    // Y = Width
    // Z = Height
    void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }
    FVector GetDecalSize() const { return DecalSize; }

    // Opacity
    void SetBaseOpacity(float InOpacity) { BaseOpacity = FMath::Clamp(InOpacity, 0.0f, 1.0f); }
    float GetBaseOpacity() const { return BaseOpacity; }

    // Fade 적용된 최종 Opacity
    float GetCurrentOpacity() const;

    // Fade System
    void StartFadeIn(float Duration);
    void StartFadeOut(float Duration);
    void UpdateFade(float DeltaTime);

    EDecalFadeState::Type GetFadeState() const { return FadeState; }
    bool IsFading() const { return FadeState != EDecalFadeState::None; }

    // Rendering Options
    void SetSortOrder(int32 InOrder) { SortOrder = InOrder; }
    int32 GetSortOrder() const { return SortOrder; }

    void SetProjectOnBackfaces(bool bInProject) { bProjectOnBackfaces = bInProject; }
    bool GetProjectOnBackfaces() const { return bProjectOnBackfaces; }

    // Transform Matrices
    FMatrix GetDecalToWorldMatrix() const;
    FMatrix GetWorldToDecalMatrix() const;

    // Decal Bounding Box (AABB for culling)
    FBound GetDecalBoundingBox() const;

    // Decal OBB (정밀 충돌 검사용)
    FOrientedBox GetDecalOrientedBox() const;

    // Duplicate Support
    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

private:
    // Actor에서 OBB 생성
    FOrientedBox GetActorOrientedBox(AStaticMeshActor* Actor) const;

protected:
    // Texture
    // [PIE] 주소 복사
    UTexture* DecalTexture = nullptr;

    // Decal Properties
    // [PIE] 값 복사
    FVector DecalSize = FVector(10.0f, 10.0f, 10.0f); // X=Depth, Y=Width, Z=Height
    float BaseOpacity = 1.0f; // 기본 투명도 (0.0 ~ 1.0)
    int32 SortOrder = 0; // 렌더링 순서 (높을수록 나중에)
    bool bProjectOnBackfaces = false; // 뒷면에도 투영할지 여부

    // Fade System
    // [PIE] 값 복사
    EDecalFadeState::Type FadeState = EDecalFadeState::None;
    float FadeDuration = 1.0f; // Fade 지속 시간 (초)
    float FadeCurrentTime = 0.0f; // 현재 경과 시간
    float FadeStartOpacity = 0.0f; // Fade 시작 시 투명도
    float FadeTargetOpacity = 1.0f; // Fade 목표 투명도

    // Blend Mode (추후 확장)
    // 0 = Translucent, 1 = Multiply, 2 = Additive
    int32 BlendMode = 0;
};
