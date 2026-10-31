#pragma once

namespace vke {

    // A field that points at an object of the scene. In the Inspector, drag the object onto it.
    class ObjectRef {
    public:
        // Whether the field points at something. The object can still have been destroyed.
        bool IsSet() const { return m_ID != 0; }

    private:
        unsigned long long m_ID = 0;
    };

    // A field that points at a prefab. In the Inspector, drag the prefab onto it.
    class PrefabRef {
    public:
        bool IsSet() const { return m_ID != 0; }

    private:
        unsigned long long m_ID = 0;
    };

} // namespace vke
