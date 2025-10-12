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
    UBillboardComponent* DuplicatedComponent = Cast<UBillboardComponent>(NewObject(GetClass()));

    // 공통 속성 복사 (Transform, AttachChildren)
    CopyCommonProperties(DuplicatedComponent);

    // BillboardComponent 전용 속성 복사
    DuplicatedComponent->BillboardQuad = this->BillboardQuad;
    DuplicatedComponent->BillboardWidth = this->BillboardWidth;
    DuplicatedComponent->BillboardHeight = this->BillboardHeight;
    DuplicatedComponent->TexturePath = this->TexturePath;
    DuplicatedComponent->UCoord = this->UCoord;
    DuplicatedComponent->VCoord = this->VCoord;
    DuplicatedComponent->ULength = this->ULength;
    DuplicatedComponent->VLength = this->VLength;
    DuplicatedComponent->bIsScreenSizeScaled = this->bIsScreenSizeScaled;
    DuplicatedComponent->ScreenSize = this->ScreenSize;

    DuplicatedComponent->DuplicateSubObjects();

    return DuplicatedComponent;
}

UObject* UBillboardComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    auto DubObject = static_cast<UBillboardComponent*>(Super_t::Duplicate(Parameters));
   
    DubObject->BillboardQuad = this->BillboardQuad;
    DubObject->BillboardWidth = this->BillboardWidth;
    DubObject->BillboardHeight = this->BillboardHeight;
    DubObject->TexturePath = this->TexturePath;
    DubObject->UCoord = this->UCoord;
    DubObject->VCoord = this->VCoord;
    DubObject->ULength = this->ULength;
    DubObject->VLength = this->VLength;
    DubObject->bIsScreenSizeScaled = this->bIsScreenSizeScaled;
    DubObject->ScreenSize = this->ScreenSize;

    return DubObject;
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

void UBillboardComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj, FViewport* Viewport)
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
