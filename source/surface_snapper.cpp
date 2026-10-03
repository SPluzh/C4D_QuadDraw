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

Int32 SurfaceSnapper::FindNearestRetopoVertex(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels, Int32 excludeIndex)
{
    if (!bd || !retopoMesh || retopoMesh->GetPointCount() == 0)
        return NOTOK;

    Int32 ptCount = retopoMesh->GetPointCount();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix rMg = retopoMesh->GetMg();

    Float bestDistSq = maxRadiusPixels * maxRadiusPixels;
    Int32 bestIdx = NOTOK;

    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (i == excludeIndex) continue;

        Vector wPos = rMg * pts[i];
        Vector sPos = bd->WS(wPos);
        if (sPos.z <= 0.0) continue; // Behind camera

        Float dx = sPos.x - screenX;
        Float dy = sPos.y - screenY;
        Float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestIdx = i;
        }
    }

    return bestIdx;
}

EdgeHit SurfaceSnapper::FindNearestRetopoEdge(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels)
{
    EdgeHit hit;
    if (!bd || !retopoMesh || retopoMesh->GetPolygonCount() == 0)
        return hit;

    Int32 polyCount = retopoMesh->GetPolygonCount();
    const CPolygon* polys = retopoMesh->GetPolygonR();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix mg = retopoMesh->GetMg();

    Vector cursor(screenX, screenY, 0.0);
    Float bestDist = maxRadiusPixels;

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

        if (d < bestDist)
        {
            bestDist = d;
            hit.valid = true;
            hit.v0 = u;
            hit.v1 = v;
            hit.worldPos0 = wA;
            hit.worldPos1 = wB;
            hit.t = t;
            hit.dist = d;
            hit.polyIndex = polyIdx;
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

    return hit;
}

SnapResult SurfaceSnapper::Snap(BaseDocument* doc, BaseDraw* bd, PolygonObject* targetMesh, PolygonObject* retopoMesh, Float screenX, Float screenY, Float snapRadiusPixels)
{
    // 1. Check if hovering over an existing retopo vertex (for grabbing / dragging)
    if (retopoMesh)
    {
        Int32 nearRetopo = FindNearestRetopoVertex(bd, retopoMesh, screenX, screenY, snapRadiusPixels, NOTOK);
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
