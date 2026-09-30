// SPDX-License-Identifier: 0BSD
#pragma once
#include "CoreMinimal.h"
#include "ProductionStatsHistory.h"
#include "ProductionStatsSave.generated.h"

// Native struct serializer lets SaveGame write the bounded bytes without a
// second TArray copy. No UObject/resource pointers enter the payload.
USTRUCT()
struct FProductionStatsSavePayload
{
    GENERATED_BODY()
    std::vector<std::uint8_t> Bytes;
    bool WriteFailed = false;
    bool Serialize(FArchive& Ar)
    {
        if (Ar.IsSaving() && (WriteFailed || Bytes.size() > FactoryProductionStats::History::SaveByteLimit))
        { Ar.SetError(); return true; }
        int32 Size = static_cast<int32>(Bytes.size());
        Ar << Size;
        if (Ar.IsLoading())
        {
            if (Size < 0 || static_cast<std::size_t>(Size) > FactoryProductionStats::History::SaveByteLimit)
            { Ar.SetError(); return true; }
            Bytes.resize(static_cast<std::size_t>(Size));
        }
        if (Size > 0 && !Ar.IsError()) Ar.Serialize(Bytes.data(), Size);
        return true;
    }
};
template<> struct TStructOpsTypeTraits<FProductionStatsSavePayload> : TStructOpsTypeTraitsBase2<FProductionStatsSavePayload>
{
    enum { WithSerializer = true, WithCopy = true };
};
