#include "surface_snapper.h"
#include "c4d_baseobject.h"
#include <cmath>

namespace cinema
{

SurfaceSnapper::SurfaceSnapper()
    : m_collider(nullptr), m_cachedTarget(nullptr), m_cachedDirty(-1)
{
}

SurfaceSnapper::~SurfaceSnapper()
{
    if (m_collider)
    {
        GeRayCollider::Free(m_collider);
        m_collider = nullptr;
    }
}

SnapResult SurfaceSnapper::RaycastSurface(BaseDraw* bd, PolygonObject* targetMesh, Float screenX, Float screenY)
{
    SnapResult result;
    if (!bd || !targetMesh || targetMesh->GetPolygonCount() == 0)
        return result;

    // Convert screen coordinates to world ray
    Vector pNear = bd->SW(Vector(screenX, screenY, 0.0));
    Vector pFar  = bd->SW(Vector(screenX, screenY, 1000000.0));

    Vector worldRayDir = pFar - pNear;
    Float worldRayLen = worldRayDir.GetLength();
    if (worldRayLen < 0.0001)
        return result;
    worldRayDir = worldRayDir / worldRayLen;

    // Transform ray to local object space of targetMesh
    Matrix invMg = ~targetMesh->GetMg();
    Vector localOrigin = invMg * pNear;
    Vector localFar    = invMg * pFar;
    Vector localDir    = localFar - localOrigin;
    Float localLen     = localDir.GetLength();
    if (localLen < 0.0001)
        return result;
    localDir = localDir / localLen;

    // Initialize or re-use GeRayCollider
    if (!m_collider)
    {
        m_collider = GeRayCollider::Alloc();
        if (!m_collider) return result;
    }

    Int32 dirty = targetMesh->GetDirty(DIRTYFLAGS::DATA | DIRTYFLAGS::MATRIX);
    if (m_cachedTarget != targetMesh || m_cachedDirty != dirty)
    {
        if (!m_collider->Init(targetMesh, false))
            return result;
        m_cachedTarget = targetMesh;
        m_cachedDirty = dirty;
    }

    if (m_collider->Intersect(localOrigin, localDir, localLen, false))
    {
        GeRayColResult col;
        if (m_collider->GetNearestIntersection(&col))
        {
            Vector worldHit = targetMesh->GetMg() * col.hitpos;
            Vector localNorm = col.f_normal.GetNormalized();
            Vector worldNorm = (targetMesh->GetMg() * (col.hitpos + localNorm) - worldHit).GetNormalized();
            if (col.backface)
                worldNorm = -worldNorm;

            result.valid = true;
            result.mode = SnapMode::TargetSurface;
            result.worldPos = worldHit;
            result.normal = worldNorm;
            result.elementIndex = col.face_id;
            return result;
        }
    }

    return result;
}

SnapResult SurfaceSnapper::ProjectPointAlongNormal(PolygonObject* targetMesh, const Vector& worldPoint, const Vector& worldNormal, Float maxDist)
{
    SnapResult result;
    if (!targetMesh || targetMesh->GetPolygonCount() == 0)
        return result;

    Float normLen = worldNormal.GetLength();
    if (normLen < 0.0001)
        return result;
    Vector norm = worldNormal / normLen;

    // Transform to local space of targetMesh
    Matrix invMg = ~targetMesh->GetMg();
    Vector localPoint = invMg * worldPoint;
    Vector localNormal = (invMg * (worldPoint + norm) - localPoint);
    Float localNormLen = localNormal.GetLength();
    if (localNormLen < 0.0001)
        return result;
    localNormal = localNormal / localNormLen;

    Float scaleEst = (invMg * Vector(1.0, 0.0, 0.0) - invMg * Vector(0.0, 0.0, 0.0)).GetLength();
    if (scaleEst < 1e-5) scaleEst = 1.0;
    Float localMaxDist = maxDist * scaleEst;

    // Initialize or re-use GeRayCollider
    if (!m_collider)
    {
        m_collider = GeRayCollider::Alloc();
        if (!m_collider) return result;
    }

    Int32 dirty = targetMesh->GetDirty(DIRTYFLAGS::DATA | DIRTYFLAGS::MATRIX);
    if (m_cachedTarget != targetMesh || m_cachedDirty != dirty)
    {
        if (!m_collider->Init(targetMesh, false))
            return result;
        m_cachedTarget = targetMesh;
        m_cachedDirty = dirty;
    }

    Float bestDist = 1e30;
    GeRayColResult bestCol;
    Bool foundHit = false;

    // Ray 1: Pass-through ray along normal line through localPoint
    Float offset = maxon::Min(Float(100.0 * scaleEst), Float(localMaxDist * 0.5));
    if (offset < 1.0) offset = 1.0;
    Vector rayStartOut = localPoint + localNormal * offset;
    if (m_collider->Intersect(rayStartOut, -localNormal, offset * 2.0, false))
    {
        GeRayColResult col;
        if (m_collider->GetNearestIntersection(&col))
        {
            Float d = (col.hitpos - localPoint).GetLength();
            if (d < bestDist && d <= localMaxDist)
            {
                bestDist = d;
                bestCol = col;
                foundHit = true;
            }
        }
    }

    // Ray 2: Ray from localPoint along +localNormal
    if (m_collider->Intersect(localPoint, localNormal, localMaxDist, false))
    {
        GeRayColResult col;
        if (m_collider->GetNearestIntersection(&col))
        {
            Float d = (col.hitpos - localPoint).GetLength();
            if (d < bestDist)
            {
                bestDist = d;
                bestCol = col;
                foundHit = true;
            }
        }
    }

    // Ray 3: Ray from localPoint along -localNormal
    if (m_collider->Intersect(localPoint, -localNormal, localMaxDist, false))
    {
        GeRayColResult col;
        if (m_collider->GetNearestIntersection(&col))
        {
            Float d = (col.hitpos - localPoint).GetLength();
            if (d < bestDist)
            {
                bestDist = d;
                bestCol = col;
                foundHit = true;
            }
        }
    }

    if (foundHit)
    {
        Vector worldHit = targetMesh->GetMg() * bestCol.hitpos;
        Vector localHitNorm = bestCol.f_normal.GetNormalized();
        Vector worldHitNorm = (targetMesh->GetMg() * (bestCol.hitpos + localHitNorm) - worldHit).GetNormalized();
        if (bestCol.backface)
            worldHitNorm = -worldHitNorm;

        result.valid = true;
        result.mode = SnapMode::TargetSurface;
        result.worldPos = worldHit;
        result.normal = worldHitNorm;
        result.elementIndex = bestCol.face_id;
        return result;
    }

    return result;
}

Int32 SurfaceSnapper::FindNearestRetopoVertex(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels, Int32 excludeIndex, PolygonObject* targetMesh)
{
    if (!bd || !retopoMesh || retopoMesh->GetPointCount() == 0)
        return NOTOK;

    Int32 ptCount = retopoMesh->GetPointCount();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix rMg = retopoMesh->GetMg();

    // 1. Check depth against Target Mesh surface under cursor (if targetMesh is provided)
    Float targetDepthAtCursor = 1e30;
    Bool hasTargetHit = false;
    if (targetMesh && targetMesh->GetPolygonCount() > 0)
    {
        SnapResult tSnap = RaycastSurface(bd, targetMesh, screenX, screenY);
        if (tSnap.valid)
        {
            targetDepthAtCursor = bd->WS(tSnap.worldPos).z;
            hasTargetHit = true;
        }
    }
    Float targetTolerance = maxon::Max(Float(15.0), Float(targetDepthAtCursor * 0.05));

    // 2. Gather candidates within radius
    struct VertCandidate
    {
        Int32 index;
        Float dist2D;
        Float z;
    };
    maxon::BaseArray<VertCandidate> candidates;
    Float minZ = 1e30;

    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (i == excludeIndex) continue;

        Vector wPos = rMg * pts[i];
        Vector sPos = bd->WS(wPos);
        if (sPos.z <= 0.0) continue; // Behind camera

        if (hasTargetHit && sPos.z > targetDepthAtCursor + targetTolerance)
            continue; // Occluded by target mesh!

        Float dx = sPos.x - screenX;
        Float dy = sPos.y - screenY;
        Float dist = std::sqrt(dx * dx + dy * dy);
        if (dist <= maxRadiusPixels)
        {
            VertCandidate c;
            c.index = i;
            c.dist2D = dist;
            c.z = sPos.z;
            candidates.Append(c) iferr_ignore("Append");
            if (sPos.z < minZ) minZ = sPos.z;
        }
    }

    if (candidates.GetCount() == 0) return NOTOK;
    if (candidates.GetCount() == 1) return candidates[0].index;

    // 3. Select closest candidate in front layer (strict depth tolerance)
    Float layerTol = maxon::Max(Float(1.0), Float(minZ * 0.01));
    Int32 bestIdx = NOTOK;
    Float bestDist = 1e30;

    for (Int32 k = 0; k < (Int32)candidates.GetCount(); ++k)
    {
        const VertCandidate& c = candidates[k];
        if (c.z <= minZ + layerTol)
        {
            if (c.dist2D < bestDist)
            {
                bestDist = c.dist2D;
                bestIdx = c.index;
            }
        }
    }

    return bestIdx;
}

EdgeHit SurfaceSnapper::FindNearestRetopoEdge(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels, PolygonObject* targetMesh)
{
    EdgeHit hit;
    if (!bd || !retopoMesh || retopoMesh->GetPolygonCount() == 0)
        return hit;

    Int32 polyCount = retopoMesh->GetPolygonCount();
    const CPolygon* polys = retopoMesh->GetPolygonR();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix mg = retopoMesh->GetMg();

    // 1. Check depth against Target Mesh surface under cursor (if targetMesh is provided)
    Float targetDepthAtCursor = 1e30;
    Bool hasTargetHit = false;
    if (targetMesh && targetMesh->GetPolygonCount() > 0)
    {
        SnapResult tSnap = RaycastSurface(bd, targetMesh, screenX, screenY);
        if (tSnap.valid)
        {
            targetDepthAtCursor = bd->WS(tSnap.worldPos).z;
            hasTargetHit = true;
        }
    }
    Float targetTolerance = maxon::Max(Float(15.0), Float(targetDepthAtCursor * 0.05));

    // 2. Gather candidates
    struct EdgeCandidate
    {
        EdgeHit hit;
        Float z;
    };
    maxon::BaseArray<EdgeCandidate> candidates;
    Float minZ = 1e30;
    Vector cursor(screenX, screenY, 0.0);

    auto checkEdge = [&](Int32 u, Int32 v, Int32 polyIdx) {
        if (u < 0 || v < 0 || u == v) return;

        Vector wA = mg * pts[u];
        Vector wB = mg * pts[v];
        Vector sA = bd->WS(wA);
        Vector sB = bd->WS(wB);
        if (sA.z <= 0.0 || sB.z <= 0.0) return;

        Vector ab = sB - sA;
        Float lenSq = ab.x * ab.x + ab.y * ab.y;
        Float t = 0.5;
        if (lenSq > 0.001)
        {
            t = ((cursor.x - sA.x) * ab.x + (cursor.y - sA.y) * ab.y) / lenSq;
            if (t < 0.02) t = 0.02;
            else if (t > 0.98) t = 0.98;
        }
        Vector proj = sA + ab * t;
        Float dx = cursor.x - proj.x;
        Float dy = cursor.y - proj.y;
        Float d = std::sqrt(dx * dx + dy * dy);

        if (d <= maxRadiusPixels)
        {
            Float projZ = sA.z + (sB.z - sA.z) * t;
            if (hasTargetHit && projZ > targetDepthAtCursor + targetTolerance)
                return; // Occluded by target mesh!

            EdgeCandidate ec;
            ec.hit.valid = true;
            ec.hit.v0 = u;
            ec.hit.v1 = v;
            ec.hit.worldPos0 = wA;
            ec.hit.worldPos1 = wB;
            ec.hit.t = t;
            ec.hit.dist = d;
            ec.hit.polyIndex = polyIdx;
            ec.z = projZ;
            candidates.Append(ec) iferr_ignore("Append");
            if (projZ < minZ) minZ = projZ;
        }
    };

    for (Int32 i = 0; i < polyCount; ++i)
    {
        const CPolygon& p = polys[i];
        checkEdge(p.a, p.b, i);
        checkEdge(p.b, p.c, i);
        if (p.c != p.d)
        {
            checkEdge(p.c, p.d, i);
            checkEdge(p.d, p.a, i);
        }
        else
        {
            checkEdge(p.c, p.a, i);
        }
    }

    if (candidates.GetCount() == 0) return hit;
    if (candidates.GetCount() == 1) return candidates[0].hit;

    // 3. Select closest candidate in front layer (strict depth tolerance)
    Float layerTol = maxon::Max(Float(1.0), Float(minZ * 0.01));
    Float bestDist = 1e30;

    for (Int32 k = 0; k < (Int32)candidates.GetCount(); ++k)
    {
        const EdgeCandidate& ec = candidates[k];
        if (ec.z <= minZ + layerTol)
        {
            if (ec.hit.dist < bestDist)
            {
                bestDist = ec.hit.dist;
                hit = ec.hit;
            }
        }
    }

    return hit;
}

SnapResult SurfaceSnapper::Snap(BaseDocument* doc, BaseDraw* bd, PolygonObject* targetMesh, PolygonObject* retopoMesh, Float screenX, Float screenY, Float snapRadiusPixels)
{
    // 1. Check if hovering over an existing retopo vertex (for grabbing / dragging)
    if (retopoMesh)
    {
        Int32 nearRetopo = FindNearestRetopoVertex(bd, retopoMesh, screenX, screenY, snapRadiusPixels, NOTOK, targetMesh);
        if (nearRetopo != NOTOK)
        {
            SnapResult res;
            res.valid = true;
            res.mode = SnapMode::RetopoVertex;
            res.retopoVertexIndex = nearRetopo;
            res.worldPos = retopoMesh->GetMg() * retopoMesh->GetPointR()[nearRetopo];
            return res;
        }
    }

    // 2. Smooth continuous surface raycast on target mesh
    return RaycastSurface(bd, targetMesh, screenX, screenY);
}

} // namespace cinema
