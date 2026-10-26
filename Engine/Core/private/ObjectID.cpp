#include <core/IDUtils.h>
#include <core/ObjectID.h>

namespace VKE {

    ObjectID ObjectID::Generate() {
        return ObjectID(GenerateID());
    }

    std::string ObjectID::ToString() const {
        return FormatID(m_ID);
    }

    ObjectID ObjectID::Parse(const std::string& text) {
        return ObjectID(ParseID(text));
    }

} // namespace VKE
