// DecalComponent.h
#pragma once
#include "SceneComponent.h"
#include "Texture.h"
#include "OrientedBox.h"

class URenderer;
class FViewport;
class UWorld;
class UStaticMeshComponent;
class AStaticMeshActor;
class UTextQuad;
class UMaterial;
 
enum class EDecalState : uint8
{
   FadeIn = 0,
   Delay,
   FadingOut,
   Finished,
   Count,
};
 
class UDecalComponent : public USceneComponent
{
public:
    DECLARE_CLASS(UDecalComponent, USceneComponent)

    UDecalComponent();
    virtual ~UDecalComponent() override;

    virtual void TickComponent(float DeltaSeconds) override;
    
	// 전체 렌더링 (Editor + 실제 데칼 투영)
    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);

    // Editor 비주얼만 렌더링
    void RenderEditorVisuals(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);

    // 실제 Decal 투영만 렌더링
    void RenderDecalProjection(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);

    // Decal Box와 충돌하는 Static Mesh 컴포넌트 찾기
    TArray<UStaticMeshComponent*> FindAffectedMeshes(UWorld* World);
    
    // 데칼 크기 설정 (박스 볼륨의 크기)
    void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }
    FVector GetDecalSize() const { return DecalSize; }

    // Texture
    void SetDecalTexture(const FString& TexturePath);
    UTexture* GetDecalTexture() const { return DecalTexture; }
     
    void DecalAnimTick(float DeltaTime);
    
    // Start fade-in → delay → fade-out sequence
    void StartFade();
   
    // Fade 효과를 처음부터 시작시키는 함수
    void ActivateFadeEffect();

    // 현재 계산된 투명도 값을 반환하는 함수
    float GetCurrentAlpha() const { return CurrentAlpha; }

    // Decal Bounding Box (AABB for culling)
    FBound GetDecalBoundingBox() const;

    // Decal OBB (정밀 충돌 검사용)
    FOrientedBox GetDecalOrientedBox() const;

    // Duplicate
    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters ) override;
    void DuplicateSubObjects() override;

    float GetFadeInDuration() { return FadeInDuration; }
    float GetFadeStartDelay() { return FadeStartDelay; }
    float GetFadeDuration() { return FadeDuration; }

    void SetFadeInDuration(float Value) { FadeInDuration = Value; }
    void SetFadeStartDelay(float Value) { FadeStartDelay = Value; }
    void SetFadeDuration(float Value) { FadeDuration = Value; }

private:
    void RenderBillboard(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj);
    void RenderOBB(URenderer* Renderer);
    void CreateBillboardVertices();

protected:
    // Texture
    // [PIE] 주소 복사
    UTexture* DecalTexture = nullptr;

    // 데칼 크기
    FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);

    /** Fade State */
    EDecalState DecalCurrentState;
    
    /** Decal이 그려지는 순서, 값이 클 수록 나중에 그려진다. */
    int32 SortOrder;

    /** Decal이 나타나는데 걸리는 시간 (투명 -> 불투명) */
    float FadeInDuration;

    /** Decal 지속 시간 */
    float FadeStartDelay;

    /** Decal이 Fadeout으로 사라지는데 걸리는 시간 */
    float FadeDuration;
     
    float CurrentAlpha;
    float CurrentStateElapsedTime[4];

    // Billboard properties
    UTextQuad* BillboardQuad = nullptr;
    UTexture* BillboardTexture = nullptr;
    UMaterial* BillboardMaterial = nullptr;
    float BillboardWidth = 1.0f;
    float BillboardHeight = 1.0f;
};
