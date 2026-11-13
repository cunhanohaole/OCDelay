#pragma once

#include "EnvelopeCurve.h"
#include "TripleBuffer.h"
#include <array>
#include <span>

enum class CurveTarget
{
    volume,
    speed
};

// One editable curve plus the lookup table the audio thread reads from it.
// update() runs on the message thread and hands a fresh table over lock-free; read() runs on
// the audio thread and never touches the editable curve.
class CurveSlot
{
public:
    static constexpr size_t kTableSize = 2048;
    using Table = std::array<float, kTableSize>;

    explicit CurveSlot (const EnvelopeCurve& initialCurve)
        : curve (initialCurve), tables (makeTable (initialCurve))
    {
    }

    const EnvelopeCurve& getCurve() const { return curve; }

    void update (const EnvelopeCurve& newCurve)
    {
        curve = newCurve;
        curve.fillTable (tables.getWriteBuffer());
        tables.publish();
    }

    std::span<const float> read() { return tables.read(); }

private:
    static Table makeTable (const EnvelopeCurve& source)
    {
        Table table {};
        source.fillTable (table);
        return table;
    }

    EnvelopeCurve curve;
    TripleBuffer<Table> tables;
};
