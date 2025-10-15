#include "pch.h"
#include "HeightFogComponent.h"
#include "ObjectFactory.h"

UHeightFogComponent::UHeightFogComponent()
{
    // 기본값 설정 (헤더에서 이미 초기화됨)
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

UObject* UHeightFogComponent::Duplicate()
{
    // ✅ 얕은 복사: NewObject로 기본 생성 후 *this로 복사
    UHeightFogComponent* NewComp = NewObject<UHeightFogComponent>(*this);

    // ✅ 명시적으로 속성 복사 (얕은 복사 보완)
    NewComp->FogDensity = FogDensity;
    NewComp->FogHeightFalloff = FogHeightFalloff;
    NewComp->StartDistance = StartDistance;
    NewComp->FogCutoffDistance = FogCutoffDistance;
    NewComp->FogMaxOpacity = FogMaxOpacity;
    NewComp->FogInscatteringColor = FogInscatteringColor;
    NewComp->bEnabled = bEnabled;

    return NewComp;
}

UObject* UHeightFogComponent::Duplicate(FObjectDuplicationParameters Parameters)
{
    // ✅ 1. DuplicationSeed에서 이미 복제된 객체 찾기
    auto It = Parameters.DuplicationSeed.find(this);
    if (It != Parameters.DuplicationSeed.end())
    {
        return It->second;
    }

    // ✅ 2. 새로운 인스턴스 생성 (얕은 복사)
    UHeightFogComponent* NewComp = NewObject<UHeightFogComponent>(*this);

    // ✅ 3. Outer 설정
    if (Parameters.DestOuter)
    {
        NewComp->SetOuter(Parameters.DestOuter);
    }

    // ✅ 4. 속성 복사
    NewComp->FogDensity = FogDensity;
    NewComp->FogHeightFalloff = FogHeightFalloff;
    NewComp->StartDistance = StartDistance;
    NewComp->FogCutoffDistance = FogCutoffDistance;
    NewComp->FogMaxOpacity = FogMaxOpacity;
    NewComp->FogInscatteringColor = FogInscatteringColor;
    NewComp->bEnabled = bEnabled;

    // ✅ 5. CreatedObjects 맵에 등록
    Parameters.CreatedObjects.emplace(this, NewComp);

    // ✅ 6. DuplicateSubObjects 호출
    NewComp->DuplicateSubObjects();

    return NewComp;
}

void UHeightFogComponent::DuplicateSubObjects()
{
    // ✅ 부모 클래스의 DuplicateSubObjects 호출 (Transform 등 처리)
    Super_t::DuplicateSubObjects();

    // HeightFogComponent는 하위 객체(Mesh, Texture 등)가 없으므로 추가 작업 불필요
    // 필요시 여기에 하위 객체 복제 로직 추가
}