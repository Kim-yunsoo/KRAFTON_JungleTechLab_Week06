// DecalComponent.h
#pragma once
#include "PrimitiveComponent.h"
#include "StaticMesh.h"
 
enum class EDecalState : uint8
{
   FadeIn = 0,
   Delay,
   FadingOut,
   Finished,
   Count,
};
 
class UDecalComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

    UDecalComponent();
    virtual ~UDecalComponent() override;
    
    //void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    void RenderOnActor(URenderer* Renderer, AActor* TargetActor, const FMatrix& View, const FMatrix& Proj);
    
    // 데칼 크기 설정 (박스 볼륨의 크기)
    void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }
    FVector GetDecalSize() const { return DecalSize; }

    // 데칼 텍스처 설정
    void SetDecalTexture(const FString& TexturePath);
     
    void DecalAnimTick(float DeltaTime);
    
    // Start fade-in → delay → fade-out sequence
    void StartFade();
   
    // Fade 효과를 처음부터 시작시키는 함수
    void ActivateFadeEffect();

    // 현재 계산된 투명도 값을 반환하는 함수
    float GetCurrentAlpha() const { return CurrentAlpha; }

    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

    UStaticMesh* GetDecalBoxMesh() const { return DecalBoxMesh; }

    float GetFadeInDuration() { return FadeInDuration; }
    float GetFadeStartDelay() { return FadeStartDelay; }
    float GetFadeDuration() { return FadeDuration; }

    void SetFadeInDuration(float Value) { FadeInDuration = Value; }
    void SetFadeStartDelay(float Value) { FadeStartDelay = Value; }
    void SetFadeDuration(float Value) { FadeDuration = Value; }
     
protected:
    // 데칼 박스 메쉬 (큐브)
    UStaticMesh* DecalBoxMesh = nullptr;

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

    // 데칼 블렌드 모드
    //enum class EDecalBlendMode
    //{
    //    Translucent,    // 반투명 블렌딩
    //    Stain,          // 곱셈 블렌딩
    //    Normal,         // 노멀맵
    //    Emissive        // 발광
    //};
};
