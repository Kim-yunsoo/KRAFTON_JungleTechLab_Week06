#include "pch.h"
#include "AABoundingBoxComponent.h"
#include "StaticMeshActor.h"
#include "ObjectFactory.h"

AStaticMeshActor::AStaticMeshActor()
{
    // TODO(KJC): AActor의 생성자에서 USceneComponent를 Root로 넣어주고 있지만
    // 현재 StaticMeshActor 에서는 Root가 StaticMeshComponent가 되어야 하므로 기존 Root를 제거 후 새로 할당함
    // 필요에 따라서, Root를 Default로 생성하지 않고 인자로 처리하는 함수를 제공하면 좋음
    // 현재는 생성 직후 소멸로 이어지기 때문에 GUObectArray를 계속 counting 처리하는 방향이라 좋지 않음
    if (RootComponent)
    {
        USceneComponent* TempRootComponent = RootComponent;
        RootComponent = nullptr;
        DeleteComponent(TempRootComponent);
    }

    Name = "Static Mesh Actor";
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
    RootComponent = StaticMeshComponent;
    AddComponent(StaticMeshComponent);

    CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
	CollisionComponent->SetupAttachment(RootComponent);
}

void AStaticMeshActor::Tick(float DeltaTime)
{
    static float times;
    times += DeltaTime;

    //TODO: bTickEditor를 추가해서 Tick안에서 분기문을 없애야됨
    if (World->WorldType == EWorldType::PIE) {
        //RootComponent->AddLocalRotation({ 0.01f, 0.0f,0.0f });
        //RootComponent->AddLocalOffset({ sin(times)/100, sin(times)/100,sin(times)/100 });
    }

    if(bIsPicked&& CollisionComponent)
    CollisionComponent->SetFromVertices(StaticMeshComponent->GetStaticMesh()->GetStaticMeshAsset()->Vertices);
}

AStaticMeshActor::~AStaticMeshActor()
{
    if (StaticMeshComponent)
    {
        ObjectFactory::DeleteObject(StaticMeshComponent);
    }
    StaticMeshComponent = nullptr;
}

void AStaticMeshActor::SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent)
{
    StaticMeshComponent = InStaticMeshComponent;
}

void AStaticMeshActor::SetCollisionComponent(EPrimitiveType InType)
{
    if (!CollisionComponent) {
        return;
    }
    CollisionComponent->SetFromVertices(StaticMeshComponent->GetStaticMesh()->GetStaticMeshAsset()->Vertices);
    CollisionComponent->SetPrimitiveType(InType);
}

UObject* AStaticMeshActor::Duplicate()
{
    // 원본(this)의 컴포넌트들 저장
    USceneComponent* OriginalRoot = this->RootComponent;

    // 얕은 복사 수행 (생성자 실행됨 - StaticMeshComponent, CollisionComponent 생성)
    AStaticMeshActor* DuplicatedActor = NewObject<AStaticMeshActor>(*this);

    // 생성자가 만든 컴포넌트들 삭제
    if (DuplicatedActor->StaticMeshComponent)
    {
        DuplicatedActor->OwnedComponents.Remove(DuplicatedActor->StaticMeshComponent);
        ObjectFactory::DeleteObject(DuplicatedActor->StaticMeshComponent);
        DuplicatedActor->StaticMeshComponent = nullptr;
    }

    if (DuplicatedActor->CollisionComponent)
    {
        DuplicatedActor->OwnedComponents.Remove(DuplicatedActor->CollisionComponent);
        ObjectFactory::DeleteObject(DuplicatedActor->CollisionComponent);
        DuplicatedActor->CollisionComponent = nullptr;
    }

    DuplicatedActor->RootComponent = nullptr;
    DuplicatedActor->OwnedComponents.clear();

    // 원본의 RootComponent(StaticMeshComponent) 복제
    if (OriginalRoot)
    {
        DuplicatedActor->RootComponent = Cast<USceneComponent>(OriginalRoot->Duplicate());
    }

    // OwnedComponents 재구성 및 타입별 포인터 재설정
    DuplicatedActor->DuplicateSubObjects();

    return DuplicatedActor;
}

void AStaticMeshActor::DuplicateSubObjects()
{
    // Duplicate()에서 이미 RootComponent를 복제했으므로
    // 부모 클래스가 OwnedComponents를 재구성
    Super_t::DuplicateSubObjects();

    // 타입별 포인터 재설정
    StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);

    // CollisionComponent 찾기
    for (UActorComponent* Comp : OwnedComponents)
    {
        if (UAABoundingBoxComponent* BBoxComp = Cast<UAABoundingBoxComponent>(Comp))
        {
            CollisionComponent = BBoxComp;
            break;
        }
    }
}

// 특화된 멤버 컴포넌트 CollisionComponent, StaticMeshComponent 는 삭제 시 포인터를 초기화합니다.
bool AStaticMeshActor::DeleteComponent(USceneComponent* ComponentToDelete)
{
    // 1. [자식 클래스의 추가 처리] 삭제 대상이 나의 특정 컴포넌트인지 확인합니다.
    if (ComponentToDelete == CollisionComponent)
    {
        // 맞다면, 나의 멤버 포인터를 nullptr로 설정합니다.
        CollisionComponent = nullptr;
    }
    else if (ComponentToDelete == StaticMeshComponent)
    {
        // AStaticMeshActor는 StaticMeshComponent가 Root 이기 때문에 삭제할 수 없음
        UE_LOG("루트 컴포넌트는 직접 삭제할 수 없습니다.");
        return false;
    }

    // 2. [부모 클래스의 원래 기능 호출]
    // 기본적인 삭제 로직(소유 목록 제거, 메모리 해제 등)은 부모에게 위임합니다.
    // Super:: 키워드를 사용합니다.
    return AActor::DeleteComponent(ComponentToDelete);
}
