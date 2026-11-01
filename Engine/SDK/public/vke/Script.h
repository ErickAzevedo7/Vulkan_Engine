#pragma once

#include <vke/Log.h>
#include <vke/Marks.h>
#include <vke/Math.h>
#include <vke/Object.h>

namespace vke {

    namespace detail {
        struct ScriptTables;
    } // namespace detail

    // Why a script is stopping, in OnStop.
    enum class StopReason {
        // The object was destroyed, or the script was taken off it.
        Destroyed,
        // The game stopped.
        GameEnded,
    };

    // Two objects touching, as a script sees it.
    struct Collision {
        // The other object. In OnCollisionEnd it may already be destroyed: check IsValid.
        Object other;
        // The direction that moves this object away from the other. Zero in OnCollisionEnd.
        Vec3 normal;
    };

    // Game logic attached to an object. Derive from it, put VKE_SCRIPT first in the class and add
    // the script to objects in the Inspector:
    //
    //     class Door : public vke::Script {
    //         VKE_SCRIPT(Door)
    //
    //         VKE_PROPERTY() float m_Speed = 2.0f;
    //
    //         void OnUpdate(float deltaTime) override { ... }
    //     };
    //
    // The class needs a constructor without arguments. Its name is how scenes refer to it, so
    // renaming the class takes the script off the objects that use it.
    //
    // The functions below are called only while the game runs. Override the ones you need. A
    // script on an inactive object gets none of them, and starts when the object first turns on.
    class Script {
    public:
        virtual ~Script() = default;

        Script(const Script&) = delete;
        Script& operator=(const Script&) = delete;

        // The object this script is on. Not set yet in the constructor: use OnStart.
        Object GetOwner() const { return m_Owner; }

    protected:
        Script() = default;

        // Once, before the first OnUpdate.
        virtual void OnStart() {}
        // Every frame, before objects collide. `deltaTime` is the seconds since the last frame.
        virtual void OnUpdate(float deltaTime) {}
        // Every frame, after objects collide, like for a camera following what moved.
        virtual void OnLateUpdate(float deltaTime) {}
        // Once, when the script is done, if OnStart ran.
        virtual void OnStop(StopReason reason) {}

        // While this object touches a solid collider: the first frame, every frame after it, and
        // when they part. The object needs a collider too, and one of the two must be movable.
        virtual void OnCollisionStart(const Collision& collision) {}
        virtual void OnCollisionStay(const Collision& collision) {}
        virtual void OnCollisionEnd(const Collision& collision) {}

        // The same when one of the two colliders is a trigger, which nothing bumps into.
        virtual void OnTriggerStart(Object other) {}
        virtual void OnTriggerStay(Object other) {}
        virtual void OnTriggerEnd(Object other) {}

    private:
        friend struct detail::ScriptTables;

        Object m_Owner;
    };

} // namespace vke

// Goes first in the body of a script's class, with the class's name.
#define VKE_SCRIPT(Type)                                                                                               \
    friend struct ::vke::detail::ScriptTables;                                                                         \
                                                                                                                       \
public:                                                                                                                \
    static constexpr const char* kScriptName = #Type;                                                                  \
                                                                                                                       \
private:
