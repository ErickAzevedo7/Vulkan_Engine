#include <core/IDUtils.h>
#include <core/ObjectID.h>

namespace VKE {

    ObjectID ObjectID::Generate() {
        return ObjectID(GenerateID());
    }

    ObjectID ObjectID::Combine(ObjectID first, ObjectID second) {
        // The finalizer of SplitMix64, which spreads every input bit over the result.
        u64 mixed = first.m_ID * 0x9E3779B97F4A7C15ull + second.m_ID;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ull;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBull;
        mixed ^= mixed >> 31;
        return ObjectID(mixed != 0 ? mixed : 1);
    }

    std::string ObjectID::ToString() const {
        return FormatID(m_ID);
    }

    ObjectID ObjectID::Parse(const std::string& text) {
        return ObjectID(ParseID(text));
    }

} // namespace VKE
