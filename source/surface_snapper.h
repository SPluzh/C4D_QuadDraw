#ifndef SURFACE_SNAPPER_H__
#define SURFACE_SNAPPER_H__

#include "c4d.h"
#include "c4d_basedraw.h"
#include "lib_collider.h"

namespace cinema
{

enum class SnapMode
{
    None = 0,
    RetopoVertex,  // Near existing retopo vertex (for weld / tweak)
    TargetSurface  // Smooth continuous position on target polygon surface
};

struct SnapResult
{
    Bool     valid = false;
    SnapMode mode = SnapMode::None;
    Vector   worldPos = Vector(0.0);
    Vector   normal = Vector(0.0, 1.0, 0.0);
    Int32    elementIndex = NOTOK;
    Int32    retopoVertexIndex = NOTOK;
};

struct EdgeHit
{
    Bool   valid = false;
    Int32  v0 = NOTOK;
    Int32  v1 = NOTOK;
    Vector worldPos0 = Vector(0.0);
    Vector worldPos1 = Vector(0.0);
    Float  t = 0.5;
    Float  dist = 1e30;
    Int32  polyIndex = NOTOK;
};

class SurfaceSnapper
{
public:
    SurfaceSnapper();
    ~SurfaceSnapper();

    // Smooth continuous raycast directly to target mesh surface
    SnapResult RaycastSurface(BaseDraw* bd, PolygonObject* targetMesh, Float screenX, Float screenY);

    // Project a 3D point onto target mesh surface strictly along the given normal direction (both +normal and -normal)
    SnapResult ProjectPointAlongNormal(PolygonObject* targetMesh, const Vector& worldPoint, const Vector& worldNormal, Float maxDist = 500.0);

    // Find nearest retopo vertex in screen space (with target occlusion and depth sorting)
    Int32 FindNearestRetopoVertex(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels = 10.0, Int32 excludeIndex = NOTOK, PolygonObject* targetMesh = nullptr, const SnapResult* precomputedTargetSnap = nullptr);

    // Find nearest retopo edge in screen space within pixel distance (with target occlusion and depth sorting)
    EdgeHit FindNearestRetopoEdge(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels = 8.0, PolygonObject* targetMesh = nullptr, const SnapResult* precomputedTargetSnap = nullptr);

    // Combined snap: checks retopo vertex first (for dragging/welding), then raycasts to target surface
    SnapResult Snap(BaseDocument* doc, BaseDraw* bd, PolygonObject* targetMesh, PolygonObject* retopoMesh, Float screenX, Float screenY, Float snapRadiusPixels = 10.0);

    // Invalidate cached raycast result
    void ClearRaycastCache();

private:
    struct CachedRaycast
    {
        BaseDraw*      bd = nullptr;
        PolygonObject* target = nullptr;
        Float          screenX = -1e30;
        Float          screenY = -1e30;
        Int32          dirty = -1;
        SnapResult     result;
    };

    GeRayCollider* m_collider = nullptr;
    PolygonObject* m_cachedTarget = nullptr;
    Int32          m_cachedDirty = -1;
    CachedRaycast  m_cachedRaycast;
};

} // namespace cinema

#endif // SURFACE_SNAPPER_H__
