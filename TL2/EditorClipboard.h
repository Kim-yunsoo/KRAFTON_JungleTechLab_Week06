#pragma once
#include "Object.h"
#include "UEContainer.h"

class AActor;
class UWorld;

/**
 * EditorClipboard
 * - 에디터에서 Copy/Paste 기능을 담당하는 싱글톤 클래스
 * - Ctrl+C로 선택된 액터들을 복사
 * - Ctrl+V로 복사된 액터들을 붙여넣기
 */
class UEditorClipboard
{
public:
    static UEditorClipboard& GetInstance();

    /** 선택된 액터들을 클립보드에 복사 */
    void Copy(const TArray<AActor*>& Actors);

    /** 클립보드의 액터들을 월드에 붙여넣기 */
    TArray<AActor*> Paste(UWorld* World, const FVector& SpawnOffset = FVector(0, 0, 0));

    /** 클립보드에 복사된 액터가 있는지 확인 */
    bool HasCopiedActors() const;

    /** 클립보드 클리어 */
    void Clear();

    /** 복사된 액터 개수 반환 */
    int32 GetCopiedActorCount() const { return static_cast<int32>(CopiedActors.size()); }

private:
    UEditorClipboard() = default;
    ~UEditorClipboard() = default;

    UEditorClipboard(const UEditorClipboard&) = delete;
    UEditorClipboard& operator=(const UEditorClipboard&) = delete;

    TArray<AActor*> CopiedActors; // 복사된 액터 참조
};
