#include "pch.h"
#include "HeightFogComponent.h"
#include "ObjectFactory.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "D3D11RHI.h"
#include "Actor.h"
#include "CameraActor.h"
#include "World.h"
#include "VertexData.h"

UHeightFogComponent::UHeightFogComponent()
{
    // 기본값 설정 (헤더에서 이미 초기화됨)

    // Billboard setup (Editor only)
    auto& ResourceManager = UResourceManager::GetInstance();
    BillboardQuad = ResourceManager.Get<UTextQuad>("Billboard");
    BillboardTexture = ResourceManager.Load<UTexture>("Editor/Icon/S_AtmosphericHeightFog.dds");
    BillboardMaterial = ResourceManager.Load<UMaterial>("Billboard.hlsl");
}

UHeightFogComponent::~UHeightFogComponent()
{
}

void UHeightFogComponent::TickComponent(float DeltaSeconds)
{
    Super_t::TickComponent(DeltaSeconds);

    // Height Fog는 매 프레임 업데이트가 필요 없으므로 비워둠
    // 필요시 동적 안개 효과 추가 가능
}

float UHeightFogComponent::CalculateFogDensityAtHeight(float WorldHeight) const
{
    if (!bEnabled)
        return 0.0f;

    // Exponential Height Fog 공식
    // Density(h) = GlobalDensity * exp(-HeightFalloff * (h - FogHeight))
    // 여기서 FogHeight는 컴포넌트의 World Position Z 좌표

    float FogHeight = GetWorldLocation().Z;
    float HeightDifference = WorldHeight - FogHeight;

    // 높이 차이에 따른 밀도 감소 (지수 함수)
    float DensityAtHeight = FogDensity * FMath::Exp(-FogHeightFalloff * HeightDifference);

    return FMath::Max(0.0f, DensityAtHeight);
}

float UHeightFogComponent::CalculateFogAmount(const FVector& CameraPosition, const FVector& WorldPosition) const
{
    if (!bEnabled)
        return 0.0f;

    // 카메라에서 월드 위치까지의 거리
    float Distance = FVector::Distance(CameraPosition, WorldPosition);

    // StartDistance 이전에는 안개 없음
    if (Distance < StartDistance)
        return 0.0f;

    // CutoffDistance 이후에는 최대 불투명도
    if (Distance > FogCutoffDistance)
        return FogMaxOpacity;

    // 거리 기반 안개 계산
    float DistanceFactor = (Distance - StartDistance) / (FogCutoffDistance - StartDistance);
    DistanceFactor = FMath::Clamp(DistanceFactor, 0.0f, 1.0f);

    // 높이 기반 안개 밀도
    float MidPointHeight = (CameraPosition.Z + WorldPosition.Z) * 0.5f;
    float HeightDensity = CalculateFogDensityAtHeight(MidPointHeight);

    // 최종 안개 적용량 = 거리 기반 * 높이 기반 밀도 * 최대 불투명도
    float FogAmount = DistanceFactor * HeightDensity * FogMaxOpacity;

    return FMath::Clamp(FogAmount, 0.0f, FogMaxOpacity);
}

UObject* UHeightFogComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    // 부모 클래스의 Duplicate 호출 (Transform 등 기본 속성 복사)
    UHeightFogComponent* DuplicatedObject = static_cast<UHeightFogComponent*>(Super_t::Duplicate(Parameters));

    // HeightFogComponent 전용 속성 복사
    DuplicatedObject->FogDensity = FogDensity;
    DuplicatedObject->FogHeightFalloff = FogHeightFalloff;
    DuplicatedObject->StartDistance = StartDistance;
    DuplicatedObject->FogCutoffDistance = FogCutoffDistance;
    DuplicatedObject->FogMaxOpacity = FogMaxOpacity;
    DuplicatedObject->FogInscatteringColor = FogInscatteringColor;
    DuplicatedObject->bEnabled = bEnabled;

    // Billboard 속성 복사 (에디터 전용)
    DuplicatedObject->BillboardWidth = BillboardWidth;
    DuplicatedObject->BillboardHeight = BillboardHeight;

    DuplicatedObject->DuplicateSubObjects();

    return DuplicatedObject;
}

void UHeightFogComponent::DuplicateSubObjects()
{
    // ✅ 부모 클래스의 DuplicateSubObjects 호출 (Transform 등 처리)
    Super_t::DuplicateSubObjects();

    // HeightFogComponent는 하위 객체(Mesh, Texture 등)가 없으므로 추가 작업 불필요
    // 필요시 여기에 하위 객체 복제 로직 추가
}

void UHeightFogComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!Renderer)
    {
        return;
    }

    // Editor 모드에서만 빌보드 렌더링
    if (GWorld && GWorld->WorldType == EWorldType::Editor)
    {
        RenderBillboard(Renderer, View, Proj);
    }
}

void UHeightFogComponent::CreateBillboardVertices()
{
    TArray<FBillboardVertexInfo_GPU> vertices;

    // 단일 쿼드의 4개 정점 생성 (카메라를 향하는 평면)
    float halfW = BillboardWidth * 0.5f;
    float halfH = BillboardHeight * 0.5f;

    FBillboardVertexInfo_GPU Info;

    // UV 좌표 (전체 텍스처 사용)
    float UCoord = 0.0f;
    float VCoord = 0.0f;
    float ULength = 1.0f;
    float VLength = 1.0f;

    // 정점 0: 좌상단
    Info.Position[0] = -halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    Info.CharSize[0] = BillboardWidth;
    Info.CharSize[1] = BillboardHeight;
    Info.UVRect[0] = UCoord;
    Info.UVRect[1] = VCoord;
    Info.UVRect[2] = ULength;
    Info.UVRect[3] = VLength;
    vertices.push_back(Info);

    // 정점 1: 우상단
    Info.Position[0] = halfW;
    Info.Position[1] = halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 2: 좌하단
    Info.Position[0] = -halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 정점 3: 우하단
    Info.Position[0] = halfW;
    Info.Position[1] = -halfH;
    Info.Position[2] = 0.0f;
    vertices.push_back(Info);

    // 동적 버텍스 버퍼 업데이트
    UResourceManager::GetInstance().UpdateDynamicVertexBuffer("Billboard", vertices);
}

void UHeightFogComponent::RenderBillboard(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
    if (!BillboardMaterial || !BillboardTexture || !BillboardQuad)
        return;

    // 카메라 정보 가져오기
    UWorld* World = GetOwner() ? GetOwner()->GetWorld() : nullptr;
    if (!World)
        return;

    ACameraActor* CameraActor = World->GetCameraActor();
    if (!CameraActor)
        return;

    FVector CamRight = CameraActor->GetActorRight();
    FVector CamUp = CameraActor->GetActorUp();

    // 빌보드 위치 설정 (Fog 컴포넌트의 위치)
    FVector BillboardPos = GetWorldLocation();

    // 텍스처 로드
    BillboardMaterial->Load("Editor/Icon/S_AtmosphericHeightFog.dds", Renderer->GetRHIDevice()->GetDevice());

    // 상수 버퍼 업데이트
    Renderer->UpdateBillboardConstantBuffers(BillboardPos, View, Proj, CamRight, CamUp);

    // 셰이더 준비
    UShader* CompShader = BillboardMaterial->GetShader();
    Renderer->PrepareShader(CompShader);

    ID3D11DeviceContext* DeviceContext = Renderer->GetRHIDevice()->GetDeviceContext();

    // InputLayout 설정 (중요!)
    DeviceContext->IASetInputLayout(CompShader->GetInputLayout());

    // 빌보드 정점 생성 및 버퍼 업데이트
    CreateBillboardVertices();

    // 렌더링
    Renderer->OMSetBlendState(true);
    Renderer->RSSetState(EViewModeIndex::VMI_Unlit);

    // 버퍼 및 텍스처 바인딩
    UINT Stride = sizeof(FBillboardVertexInfo_GPU);
    UINT Offset = 0;
    ID3D11Buffer* VertexBuffer = BillboardQuad->GetVertexBuffer();
    ID3D11Buffer* IndexBuffer = BillboardQuad->GetIndexBuffer();

    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // 텍스처 바인딩 (중요!)
    UTexture* CompTexture = BillboardMaterial->GetTexture();
    if (CompTexture && CompTexture->GetShaderResourceView())
    {
        ID3D11ShaderResourceView* TextureSRV = CompTexture->GetShaderResourceView();
        Renderer->GetRHIDevice()->PSSetDefaultSampler(0);
        DeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
    }

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->DrawIndexed(BillboardQuad->GetIndexCount(), 0, 0);

    Renderer->OMSetBlendState(false);
}