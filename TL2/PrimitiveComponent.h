#pragma once
#include "SceneComponent.h"
#include "Material.h"

// 전방 선언
struct FPrimitiveData;

class URenderer;
class FViewport;

class UPrimitiveComponent :public USceneComponent
{
public:
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

    UPrimitiveComponent() = default;
    virtual ~UPrimitiveComponent() = default;

    virtual void SetMaterial(const FString& FilePath);
    virtual UMaterial* GetMaterial() { return Material; }

    // 트랜스폼 직렬화/역직렬화 (월드 트랜스폼 기준)
    virtual void Serialize(bool bIsLoading, FPrimitiveData& InOut);

    virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport = nullptr) {}

    UObject* Duplicate() override;
    UObject* Duplicate(FObjectDuplicationParameters Parameters) override;
    void DuplicateSubObjects() override;
protected:
    // [PIE] 주소 복사 / NOTE: 만약 복사 후에도 GPU 버퍼 내용을 다르게 갖고 싶은 경우 깊은 복사를 해서 버퍼를 2개 생성하는 방법도 고려
    UMaterial* Material = nullptr;
};