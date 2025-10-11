#include "pch.h"

const FName& FName::GetNone()
{
    // Use an interned empty-string FName as the canonical None
    static const FName NonInstance("");
    return NonInstance;
}
