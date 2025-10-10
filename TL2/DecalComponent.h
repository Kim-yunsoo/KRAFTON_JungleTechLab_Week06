// DecalComponent.h
#pragma once
#include "PrimitiveComponent.h"
#include "StaticMesh.h"

class UDecalComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UDecalComponent, UPrimitiveComponent)

    UDecalComponent();
    virtual ~UDecalComponent() override;
    
    //void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;

    void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport) ;
    
    // 데칼 크기 설정 (박스 볼륨의 크기)
    void SetDecalSize(const FVector& InSize) { DecalSize = InSize; }
    FVector GetDecalSize() const { return DecalSize; }

    // 데칼 텍스처 설정
    void SetDecalTexture(const FString& TexturePath);

    UObject* Duplicate() override;
    void DuplicateSubObjects() override;

    UStaticMesh* GetDecalBoxMesh() const { return DecalBoxMesh; }
protected:
    // 데칼 박스 메쉬 (큐브)
    UStaticMesh* DecalBoxMesh = nullptr;

    // 데칼 크기
    FVector DecalSize = FVector(1.0f, 1.0f, 1.0f);

    // 데칼 블렌드 모드
    enum class EDecalBlendMode
    {
        Translucent,    // 반투명 블렌딩
        Stain,          // 곱셈 블렌딩
        Normal,         // 노멀맵
        Emissive        // 발광
    };

    EDecalBlendMode BlendMode = EDecalBlendMode::Translucent;
};
