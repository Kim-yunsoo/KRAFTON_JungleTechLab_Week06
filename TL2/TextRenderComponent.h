#pragma once
#include "MeshComponent.h"

class UTextRenderComponent : public UPrimitiveComponent
{
public:
	DECLARE_CLASS(UTextRenderComponent, UPrimitiveComponent)
	UTextRenderComponent();

protected:
	~UTextRenderComponent() override;

public:
	void InitCharInfoMap();
	TArray<FBillboardVertexInfo_GPU> CreateVerticesForString(const FString& text,const FVector& StartPos);
	virtual void Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) override;
	
	const FString GetText() { return Text; }
	void SetText(FString InText);

	UTextQuad* GetStaticMesh() const { return TextQuad; }

	UObject* Duplicate() override;
	//UObject* Duplicate(FObjectDuplicationParameters Parameter) override;
	void DuplicateSubObjects() override;

private:
	static const uint32 MaxQuads = 100; // capacity
	static TMap<char, FBillboardVertexInfo> CharInfoMap;	// ASCII 문자 코드를 UVRect로 변환하는 Map

private:
	// [PIE] 값 복사
	FString Text;

	// [PIE] 주소 복사 / NOTE: 만약 복사 후에도 GPU 버퍼 내용을 다르게 갖고 싶은 경우 깊은 복사를 해서 버퍼를 2개 생성하는 방법도 고려
	UTextQuad* TextQuad = nullptr;

	// [PIE] 값 복사
	bool bIsDirty = true;	// Text 가 변경된 경우 true 후 TextQuad 업데이트
};