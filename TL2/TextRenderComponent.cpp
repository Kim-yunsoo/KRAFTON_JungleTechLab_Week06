#include "pch.h"
#include "Enums.h"
#include "d3dtk/DDSTextureLoader.h"
#include "TextRenderComponent.h"
#include "ResourceManager.h"
#include "VertexData.h"
#include "CameraActor.h"

TMap<char, FBillboardVertexInfo> UTextRenderComponent::CharInfoMap;

UTextRenderComponent::UTextRenderComponent()
{
	SetRelativeLocation({ 0, 0, 1 });

	UResourceManager& ResourceManager = UResourceManager::GetInstance();

	// 미리 최대 글자 수만큼 인덱스를 생성한다 (최대 글자 100)
	TArray<uint32> Indices;
	for (uint32 i = 0; i < MaxQuads; i++)
	{
		Indices.push_back(i * 4 + 0);
		Indices.push_back(i * 4 + 1);
		Indices.push_back(i * 4 + 2);

		Indices.push_back(i * 4 + 2);
		Indices.push_back(i * 4 + 1);
		Indices.push_back(i * 4 + 3);
	}

	// MeshData는 지역변수로 UTextQuad를 생성하고 소멸됨
	FMeshData MeshData;
	MeshData.Indices = Indices;

	// Reserve capacity for MaxQuads (4 vertices per quad)
	MeshData.Vertices.resize(MaxQuads * 4);
	MeshData.Color.resize(MaxQuads * 4);
	MeshData.UV.resize(MaxQuads * 4);

	UTextQuad* Mesh = NewObject<UTextQuad>();
	Mesh->Load(&MeshData, ResourceManager.GetDevice());
	TextQuad = Mesh;

	/*if (auto* M = ResourceManager.Get<UMaterial>("TextBillboard"))
	{
		Material = M;
	}
	else
	{
		Material = NewObject<UMaterial>();
		ResourceManager.Add<UMaterial>("TextBillboard", Material);
	}*/
	SetMaterial("TextShader.hlsl");//여기서 자동으로 메테리얼을 세팅해줍니다. 굳이 만들 필요 없음
	Material->Load("TextBillboard.dds", ResourceManager.GetDevice());

	Text = "Text";    // 텍스트를 생성하면 설정되는 기본 텍스트

	InitCharInfoMap();
}

UTextRenderComponent::~UTextRenderComponent()
{
	// GUObjectArray 에서 자동으로 지워주는 것 같음
	//if (TextQuad)
	//{
	//    delete TextQuad;
	//    TextQuad = nullptr;
	//}
}

// ASCII 문자 코드를 UVRect로 변환하는 Map을 생성
void UTextRenderComponent::InitCharInfoMap()
{
	// 최초 1번만 초기화 한다
	if (CharInfoMap.size() != 0)
	{
		return;
	}

	const float TEXTURE_WH = 512.f;
	const float SUBTEX_WH = 32.f;
	const int COLROW = 16;

	//FTextureData* Data = new FTextureData();
	for (char c = 32; c <= 126; ++c)
	{
		int key = c - 32;
		int col = key % COLROW;
		int row = key / COLROW;

		float pixelX = col * SUBTEX_WH;
		float pixelY = row * SUBTEX_WH;

		FBillboardVertexInfo CharInfo;
		CharInfo.UVRect.X = pixelX / TEXTURE_WH;
		CharInfo.UVRect.Y = pixelY / TEXTURE_WH;
		CharInfo.UVRect.Z = SUBTEX_WH / TEXTURE_WH;
		CharInfo.UVRect.W = CharInfo.UVRect.Z;

		CharInfoMap[c] = CharInfo;
	}
}


void UTextRenderComponent::SetText(FString InText)
{
	if (MaxQuads < InText.size())
	{
		UE_LOG("%d 글자 이상 작성할 수 없습니다.", MaxQuads);
		return;
	}

	bIsDirty = true;    // 버텍스 버퍼 새로 생성 예정
	Text = InText;
}

UObject* UTextRenderComponent::Duplicate()
{
	UTextRenderComponent* DuplicatedComponent = Cast<UTextRenderComponent>(NewObject(GetClass()));
	CopyCommonProperties(DuplicatedComponent);
	DuplicatedComponent->DuplicateSubObjects();

	return DuplicatedComponent;
}
 

void UTextRenderComponent::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();
}

void UTextRenderComponent::Render(URenderer* InRenderer, const FMatrix& InView, const FMatrix& InProj)
{

	// Text가 변경되었다면 버퍼를 업데이트
	if (bIsDirty)
	{
		TArray<FBillboardVertexInfo_GPU> Vertices = CreateVerticesForString(Text, FVector());
		UResourceManager::GetInstance().UpdateDynamicVertexBuffer(TextQuad, Vertices);
	}

	// 일반 씬 컴포넌트처럼 월드 행렬로 트랜스폼 지정
	InRenderer->UpdateConstantBuffer(GetWorldMatrix(), InView, InProj);

	InRenderer->PrepareShader(Material->GetShader());

	InRenderer->OMSetBlendState(true);
	InRenderer->RSSetState(EViewModeIndex::VMI_Unlit);
	InRenderer->DrawIndexedPrimitiveComponent(this, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	InRenderer->OMSetBlendState(false);
}

// Text에 대응하는 UV를 생성해준다
TArray<FBillboardVertexInfo_GPU> UTextRenderComponent::CreateVerticesForString(const FString& InText, const FVector& InStartPos)
{
	TArray<FBillboardVertexInfo_GPU> vertices;
	auto* CharUVInfo = CharInfoMap.Find('A');
	float charWidth = (CharUVInfo->UVRect.Z) / (CharUVInfo->UVRect.W); //
	float CharHeight = 1.f;
	float CursorX = -charWidth * (InText.size() / 2);
	int vertexBaseIndex = 0;
	for (char c : InText)
	{
		auto* CharUVInfo = CharInfoMap.Find(c);
		if (!CharUVInfo) continue;

		float u = CharUVInfo->UVRect.X;
		float v = CharUVInfo->UVRect.Y;
		float w = CharUVInfo->UVRect.Z; //32 / 512
		float h = CharUVInfo->UVRect.W; //32 / 512

		//float charWidth = (w / h);

		FBillboardVertexInfo_GPU Info;
		//1
		Info.Position[0] = CursorX;
		Info.Position[1] = CharHeight;
		Info.Position[2] = 0.f;

		Info.CharSize[0] = 1.f;
		Info.CharSize[1] = 1.f;

		Info.UVRect[0] = u;
		Info.UVRect[1] = v;
		vertices.push_back(Info);
		//2
		Info.Position[0] = CursorX + charWidth;
		Info.Position[1] = CharHeight;
		Info.Position[2] = 0.f;

		Info.CharSize[0] = 1.f;
		Info.CharSize[1] = 1.f;

		Info.UVRect[0] = u + w;
		Info.UVRect[1] = v;
		vertices.push_back(Info);
		//3
		Info.Position[0] = CursorX;
		Info.Position[1] = 0.f;
		Info.Position[2] = 0.f;

		Info.CharSize[0] = 1.f;
		Info.CharSize[1] = 1.f;

		Info.UVRect[0] = u;
		Info.UVRect[1] = v + h;
		vertices.push_back(Info);
		//4
		Info.Position[0] = CursorX + charWidth;
		Info.Position[1] = 0.f;
		Info.Position[2] = 0.f;

		Info.CharSize[0] = 1.f;
		Info.CharSize[1] = 1.f;

		Info.UVRect[0] = u + w;
		Info.UVRect[1] = v + h;
		vertices.push_back(Info);

		CursorX += charWidth;
		vertexBaseIndex += 4;
	}
	return vertices;
}
