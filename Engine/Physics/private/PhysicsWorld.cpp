#include "physics/PhysicsWorld.h"
#include "Broadphase.h"
#include "Narrowphase.h"
#include "TriangleTree.h"

#include <core/Log.h>
#include <core/SlotMap.h>

#include <algorithm>
#include <unordered_map>

namespace VKE::Physics {

    struct MeshShape {
        TriangleTree Tree;
    };

    struct Body {
        BodyDesc Desc;
        // Updated at the start of each step.
        AABB Bounds;
        bool HasShape = false;
        // Mesh bodies: the mesh's space to world, and back.
        Mat4 MeshToWorld{1.0f};
        Mat4 WorldToMesh{1.0f};
    };

    namespace {
        // Bodies this close count as touching, so a body pushed out exactly to a surface keeps
        // its contact instead of flickering between Begin and End.
        constexpr f32 kContactMargin = 0.005f;
        // Pushing a body out of one neighbor can push it into another; a few rounds settle that.
        constexpr u32 kPushRounds = 4;

        struct TouchingPair {
            // Ordered by slot index, so the same pair always reports the same A and B.
            BodyID A;
            BodyID B;
            bool IsTrigger = false;
            Vec3 Normal{0.0f};
            u64 UserDataA = 0;
            u64 UserDataB = 0;

            ContactEvent ToEvent(ContactPhase phase) const {
                Vec3 normal = phase == ContactPhase::End ? Vec3(0.0f) : Normal;
                return {A, B, phase, IsTrigger, normal, UserDataA, UserDataB};
            }
        };

        u64 MakePairKey(BodyID a, BodyID b) {
            u32 low = std::min(a.Index, b.Index);
            u32 high = std::max(a.Index, b.Index);
            return (static_cast<u64>(low) << 32) | high;
        }

        bool IsStatic(const BodyDesc& desc) {
            return desc.Motion == BodyMotion::Static || desc.Shape == ShapeType::Mesh;
        }

        OrientedBox MakeBox(const BodyDesc& desc) {
            Mat3 rotation = glm::mat3_cast(desc.Pose.Rotation);
            OrientedBox box;
            for (int i = 0; i < 3; ++i) {
                box.Axes[i] = rotation[i];
            }
            box.HalfExtents = glm::abs(desc.HalfExtents * desc.Pose.Scale);
            box.Center = desc.Pose.Position + rotation * (desc.Center * desc.Pose.Scale);
            return box;
        }

        AABB TransformBounds(const AABB& bounds, const Mat4& matrix) {
            Vec3 first(matrix * Vec4(bounds.Min, 1.0f));
            AABB result(first, first);
            for (int corner = 1; corner < 8; ++corner) {
                Vec3 local((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
                           (corner & 4) ? bounds.Max.z : bounds.Min.z);
                Vec3 world(matrix * Vec4(local, 1.0f));
                result.Min = glm::min(result.Min, world);
                result.Max = glm::max(result.Max, world);
            }
            return result;
        }

        Triangle TransformTriangle(const Triangle& triangle, const Mat4& matrix) {
            return {Vec3(matrix * Vec4(triangle.A, 1.0f)), Vec3(matrix * Vec4(triangle.B, 1.0f)),
                    Vec3(matrix * Vec4(triangle.C, 1.0f))};
        }

        AABB Expand(const AABB& bounds, f32 amount) {
            return AABB(bounds.Min - Vec3(amount), bounds.Max + Vec3(amount));
        }
    } // namespace

    struct PhysicsWorld::Storage {
        SlotMap<MeshShape> Shapes;
        SlotMap<Body> Bodies;

        Broadphase Pairs;
        std::vector<Broadphase::Entry> Entries;
        std::vector<Vec3> StartPositions;

        // Pairs that were touching at the end of the last step.
        std::unordered_map<u64, TouchingPair> Touching;
        std::unordered_map<u64, TouchingPair> NowTouching;
        // End events for destroyed bodies, reported by the next step.
        std::vector<ContactEvent> PendingEnds;

        std::vector<BodyID> Moved;
        std::vector<ContactEvent> Events;
        // Reused by each push out of a mesh.
        std::vector<Triangle> NearbyTriangles;
        bool MovableMeshWarned = false;

        Body& BodyAt(u32 denseIndex) { return *Bodies.Get(Bodies.HandleAt(denseIndex)); }

        void UpdateShape(Body& body) {
            const BodyDesc& desc = body.Desc;
            if (desc.Shape == ShapeType::Box) {
                body.HasShape = true;
                body.Bounds = GetBounds(MakeBox(desc));
                return;
            }

            const MeshShape* shape = Shapes.Get(desc.Mesh);
            body.HasShape = shape && !shape->Tree.IsEmpty();
            if (!body.HasShape) return;

            body.MeshToWorld = desc.Pose.GetMatrix();
            body.WorldToMesh = glm::inverse(body.MeshToWorld);
            body.Bounds = TransformBounds(shape->Tree.GetBounds(), body.MeshToWorld);
        }

        // Calls `visit(const Triangle&)` with each triangle of `mesh`, in world space, near `region`.
        template<typename Visitor>
        void VisitTriangles(const Body& mesh, const AABB& region, Visitor&& visit) {
            const MeshShape* shape = Shapes.Get(mesh.Desc.Mesh);
            shape->Tree.Query(TransformBounds(region, mesh.WorldToMesh), [&](const Triangle& triangle) {
                visit(TransformTriangle(triangle, mesh.MeshToWorld));
            });
        }

        // Moves the movable bodies of a solid pair apart. Returns true if anything moved.
        bool Push(Body& a, Body& b) {
            if (a.Desc.Shape == ShapeType::Mesh) return PushOutOfMesh(b, a);
            if (b.Desc.Shape == ShapeType::Mesh) return PushOutOfMesh(a, b);

            Penetration penetration;
            if (!TestBoxBox(MakeBox(a.Desc), MakeBox(b.Desc), 0.0f, penetration) || penetration.Depth <= 0.0f) {
                return false;
            }

            Vec3 push = penetration.Normal * penetration.Depth;
            bool aMoves = !IsStatic(a.Desc);
            bool bMoves = !IsStatic(b.Desc);
            if (aMoves && bMoves) {
                a.Desc.Pose.Position += 0.5f * push;
                b.Desc.Pose.Position -= 0.5f * push;
            } else if (aMoves) {
                a.Desc.Pose.Position += push;
            } else {
                b.Desc.Pose.Position -= push;
            }
            return true;
        }

        bool PushOutOfMesh(Body& box, const Body& mesh) {
            if (IsStatic(box.Desc)) return false;

            OrientedBox shape = MakeBox(box.Desc);
            NearbyTriangles.clear();
            VisitTriangles(mesh, GetBounds(shape),
                           [&](const Triangle& triangle) { NearbyTriangles.push_back(triangle); });

            // Triangles one after another, each against where the earlier ones left the box, so a
            // floor and a wall both hold it at once. The ones the box's center is over go first,
            // pushing along their normals: they are the surfaces it stands on or walks into. The
            // ones it only grazes, like the edge of a molding, then get the smallest push, which
            // their normal would overshoot.
            // Sorted by where the box was before any push, so each triangle is visited once.
            const Vec3 center = shape.Center;
            bool pushed = false;
            for (bool over : {true, false}) {
                for (const Triangle& triangle : NearbyTriangles) {
                    if (IsOverTriangle(center, triangle) != over) continue;

                    Penetration penetration;
                    if (!TestBoxTriangle(shape, triangle, 0.0f, over, penetration) || penetration.Depth <= 0.0f) {
                        continue;
                    }

                    Vec3 push = penetration.Normal * penetration.Depth;
                    shape.Center += push;
                    box.Desc.Pose.Position += push;
                    pushed = true;
                }
            }
            return pushed;
        }

        // Whether the pair is touching after the pushes, and the normal that moves `a` away from `b`.
        bool FindContact(const Body& a, const Body& b, Vec3& outNormal) {
            if (a.Desc.Shape == ShapeType::Mesh || b.Desc.Shape == ShapeType::Mesh) {
                bool meshIsA = a.Desc.Shape == ShapeType::Mesh;
                const Body& box = meshIsA ? b : a;
                const Body& mesh = meshIsA ? a : b;

                OrientedBox shape = MakeBox(box.Desc);
                bool touching = false;
                f32 deepest = -kContactMargin;
                VisitTriangles(mesh, Expand(GetBounds(shape), kContactMargin), [&](const Triangle& triangle) {
                    Penetration penetration;
                    bool over = IsOverTriangle(shape.Center, triangle);
                    if (!TestBoxTriangle(shape, triangle, kContactMargin, over, penetration)) return;
                    if (!touching || penetration.Depth > deepest) {
                        deepest = penetration.Depth;
                        outNormal = meshIsA ? -penetration.Normal : penetration.Normal;
                    }
                    touching = true;
                });
                return touching;
            }

            Penetration penetration;
            if (!TestBoxBox(MakeBox(a.Desc), MakeBox(b.Desc), kContactMargin, penetration)) return false;
            outNormal = penetration.Normal;
            return true;
        }

        void EndContactsOf(BodyID id) {
            for (auto it = Touching.begin(); it != Touching.end();) {
                const TouchingPair& pair = it->second;
                if (pair.A == id || pair.B == id) {
                    PendingEnds.push_back(pair.ToEvent(ContactPhase::End));
                    it = Touching.erase(it);
                } else {
                    ++it;
                }
            }
        }
    };

    PhysicsWorld::PhysicsWorld()
        : m_Storage(MakeIunique<Storage>()) {
    }

    PhysicsWorld::~PhysicsWorld() = default;

    MeshShapeID PhysicsWorld::CreateMeshShape(const MeshShapeDesc& desc) {
        MeshShape shape;
        shape.Tree.Build(desc.Positions, desc.Indices);
        if (shape.Tree.IsEmpty()) VKE_CORE_WARN("PhysicsWorld: mesh shape has no triangles");
        return m_Storage->Shapes.Emplace(std::move(shape));
    }

    void PhysicsWorld::DestroyMeshShape(MeshShapeID id) {
        m_Storage->Shapes.Remove(id);
    }

    BodyID PhysicsWorld::CreateBody(const BodyDesc& desc) {
        Body body;
        body.Desc = desc;
        return m_Storage->Bodies.Emplace(std::move(body));
    }

    void PhysicsWorld::SetBody(BodyID id, const BodyDesc& desc) {
        if (Body* body = m_Storage->Bodies.Get(id)) body->Desc = desc;
    }

    void PhysicsWorld::SetPose(BodyID id, const Transform& pose) {
        if (Body* body = m_Storage->Bodies.Get(id)) body->Desc.Pose = pose;
    }

    void PhysicsWorld::DestroyBody(BodyID id) {
        if (!m_Storage->Bodies.Remove(id)) return;
        m_Storage->EndContactsOf(id);
    }

    const BodyDesc* PhysicsWorld::GetBody(BodyID id) const {
        const Body* body = m_Storage->Bodies.Get(id);
        return body ? &body->Desc : nullptr;
    }

    void PhysicsWorld::Step() {
        Storage& s = *m_Storage;
        s.Moved.clear();
        s.Events.swap(s.PendingEnds);
        s.PendingEnds.clear();

        const u32 bodyCount = static_cast<u32>(s.Bodies.Size());
        s.Entries.clear();
        s.StartPositions.resize(bodyCount);
        for (u32 i = 0; i < bodyCount; ++i) {
            Body& body = s.BodyAt(i);
            if (body.Desc.Shape == ShapeType::Mesh && body.Desc.Motion == BodyMotion::Movable && !s.MovableMeshWarned) {
                VKE_CORE_WARN("PhysicsWorld: mesh bodies can't be movable and are treated as static");
                s.MovableMeshWarned = true;
            }

            s.UpdateShape(body);
            s.StartPositions[i] = body.Desc.Pose.Position;
            // Grown by the margin, so pairs that only come close are still checked for contact.
            if (body.HasShape) s.Entries.push_back({Expand(body.Bounds, kContactMargin), i, IsStatic(body.Desc)});
        }

        // A mesh is never pushed, so two meshes never need comparing.
        std::vector<std::pair<u32, u32>> pairs;
        for (const auto& [a, b] : s.Pairs.FindPairs(s.Entries)) {
            if (s.BodyAt(a).Desc.Shape == ShapeType::Mesh && s.BodyAt(b).Desc.Shape == ShapeType::Mesh) continue;
            pairs.emplace_back(a, b);
        }

        for (u32 round = 0; round < kPushRounds; ++round) {
            bool pushed = false;
            for (const auto& [a, b] : pairs) {
                Body& bodyA = s.BodyAt(a);
                Body& bodyB = s.BodyAt(b);
                if (bodyA.Desc.IsTrigger || bodyB.Desc.IsTrigger) continue;
                pushed |= s.Push(bodyA, bodyB);
            }
            if (!pushed) break;
        }

        for (u32 i = 0; i < bodyCount; ++i) {
            if (s.BodyAt(i).Desc.Pose.Position != s.StartPositions[i]) s.Moved.push_back(s.Bodies.HandleAt(i));
        }

        s.NowTouching.clear();
        for (const auto& [a, b] : pairs) {
            BodyID idA = s.Bodies.HandleAt(a);
            BodyID idB = s.Bodies.HandleAt(b);
            u32 first = a;
            u32 second = b;
            if (idB.Index < idA.Index) {
                std::swap(idA, idB);
                std::swap(first, second);
            }

            const Body& bodyA = s.BodyAt(first);
            const Body& bodyB = s.BodyAt(second);
            TouchingPair pair{idA, idB, bodyA.Desc.IsTrigger || bodyB.Desc.IsTrigger, Vec3(0.0f),
                              bodyA.Desc.UserData, bodyB.Desc.UserData};
            Vec3 normal;
            if (!s.FindContact(bodyA, bodyB, normal)) continue;
            if (!pair.IsTrigger) pair.Normal = normal;
            s.NowTouching.emplace(MakePairKey(idA, idB), pair);
        }

        for (const auto& [key, pair] : s.Touching) {
            auto now = s.NowTouching.find(key);
            // A pair that turned into a trigger pair, or back, ends and begins again.
            if (now == s.NowTouching.end() || now->second.IsTrigger != pair.IsTrigger) {
                s.Events.push_back(pair.ToEvent(ContactPhase::End));
            }
        }
        for (const auto& [key, pair] : s.NowTouching) {
            auto before = s.Touching.find(key);
            bool continues = before != s.Touching.end() && before->second.IsTrigger == pair.IsTrigger;
            s.Events.push_back(pair.ToEvent(continues ? ContactPhase::Stay : ContactPhase::Begin));
        }
        s.Touching.swap(s.NowTouching);
    }

    const std::vector<BodyID>& PhysicsWorld::GetMovedBodies() const {
        return m_Storage->Moved;
    }

    const std::vector<ContactEvent>& PhysicsWorld::GetContactEvents() const {
        return m_Storage->Events;
    }

} // namespace VKE::Physics
