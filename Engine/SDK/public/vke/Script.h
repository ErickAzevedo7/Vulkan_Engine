#pragma once

#include <vke/Log.h>
#include <vke/Marks.h>
#include <vke/Math.h>
#include <vke/Object.h>

namespace vke {

    namespace detail {
        struct ScriptTables;
    } // namespace detail

    // Game logic attached to an object. Derive from it, put VKE_SCRIPT first in the class and add
    // the script to objects in the Inspector:
    //
    //     class Door : public vke::Script {
    //         VKE_SCRIPT(Door)
    //
    //         VKE_PROPERTY() float m_Speed = 2.0f;
    //     };
    //
    // The class needs a constructor without arguments. Its name is how scenes refer to it, so
    // renaming the class takes the script off the objects that use it.
    class Script {
    public:
        virtual ~Script() = default;

        Script(const Script&) = delete;
        Script& operator=(const Script&) = delete;

    protected:
        Script() = default;
    };

} // namespace vke

// Goes first in the body of a script's class, with the class's name.
#define VKE_SCRIPT(Type) friend struct ::vke::detail::ScriptTables;
