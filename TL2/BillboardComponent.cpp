#include "pch.h"
#include "BillboardComponent.h"
#include "ResourceManager.h"
#include "VertexData.h"
#include "CameraActor.h"

UBillboardComponent::UBillboardComponent()
{
    SetRelativeLocation({ 0, 0, 1 });

    auto& ResourceManager = UResourceManager::GetInstance();

    // 빌보드용 메시 가져오기 (단일 쿼드)
    BillboardQuad = ResourceManager.Get<UTextQuad>("Billboard");

    SetMaterial("Billboard.hlsl");//메테리얼 자동 매칭
}

UBillboardComponent::~UBillboardComponent()
{
}

void UBillboardComponent::SetTexture(const FString& InTexturePath)
{
    TexturePath = InTexturePath;
}

void UBillboardComponent::SetUVCoords(float U, float V, float UL, float VL)
{
    UCoord = U;
    VCoord = V;
    ULength = UL;
    VLength = VL;
}

UObject* UBillboardComponent::Duplicate()
{
    UBillboardComponent* DuplicatedComponent = NewObject<UBillboardComponent>(*this);
    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}

void UBillboardComponent::DuplicateSubObjects()
{
    Super_t::DuplicateSubObjects();

}

void UBillboardComponent::CreateBillboardVertices()
{
    TArray<FBillboardVertexInfo_GPU> vertices;

    // 단일 쿼드의 4개 정점 생성 (카메라를 향하는 평면)
    // 중심이 (0,0,0)이고 크기가 BillboardWidth x BillboardHeight인 사각형
    float halfW = BillboardWidth * 0.5f;
    float halfH = BillboardHeight * 0.5f;

    FBillboardVertexInfo_GPU Info;

    // 정점 0: 좌상단 (-halfW, +halfH)
    Info.Position[0] = -halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    Info.CharSize[0] = BillboardWidth;
    Info.CharSize[1] = BillboardHeight;
    Info.UVRect[0] = UCoord;   // u start
    Info.UVRect[1] = VCoord;   // v start
    Info.UVRect[2] = ULength;  // u length (UL)
    Info.UVRect[3] = VLength;  // v length (VL)
    vertices.push_back(Info);

    // 정점 1: 우상단 (+halfW, +halfH)
    Info.Position[0] = halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 2: 좌하단 (-halfW, -halfH)
    Info.Position[0] = -halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 3: 우하단 (+halfW, -halfH)
    Info.Position[0] = halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 동적 버텍스 버퍼 업데이트
    UResourceManager::GetInstance().UpdateDynamicVertexBuffer("Billboard", vertices);
}

void UBillboardComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    // 텍스처 로드
    Material->Load(TexturePath, Renderer->GetRHIDevice()->GetDevice());

    // 카메라 정보 가져오기
    ACameraActor* CameraActor = GetOwner()->GetWorld()->GetCameraActor();
    FVector CamRight = CameraActor->GetActorRight();
    FVector CamUp = CameraActor->GetActorUp();

    // 빌보드 위치 설정
    FVector BillboardPos = GetWorldLocation();

    // 상수 버퍼 업데이트
    Renderer->UpdateBillboardConstantBuffers(BillboardPos, View, Proj, CamRight, CamUp);

    // 셰이더 준비
    Renderer->PrepareShader(Material->GetShader());

    // 빌보드 정점 생성 및 버퍼 업데이트
    CreateBillboardVertices();

    // 렌더링
    Renderer->OMSetBlendState(true);
    Renderer->RSSetState(EViewModeIndex::VMI_Unlit);
    Renderer->DrawIndexedPrimitiveComponent(this, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Renderer->OMSetBlendState(false);
}
