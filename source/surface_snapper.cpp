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

    Int32 dirty = targetMesh->GetDirty(DIRTYFLAGS::DATA | DIRTYFLAGS::MATRIX);
    if (m_cachedRaycast.bd == bd &&
        m_cachedRaycast.target == targetMesh &&
        m_cachedRaycast.dirty == dirty &&
        std::abs(m_cachedRaycast.screenX - screenX) < 0.001 &&
        std::abs(m_cachedRaycast.screenY - screenY) < 0.001)
    {
        return m_cachedRaycast.result;
    }

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
        }
    }

    m_cachedRaycast.bd = bd;
    m_cachedRaycast.target = targetMesh;
    m_cachedRaycast.dirty = dirty;
    m_cachedRaycast.screenX = screenX;
    m_cachedRaycast.screenY = screenY;
    m_cachedRaycast.result = result;

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

Int32 SurfaceSnapper::FindNearestRetopoVertex(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels, Int32 excludeIndex, PolygonObject* targetMesh, const SnapResult* precomputedTargetSnap)
{
    if (!bd || !retopoMesh || retopoMesh->GetPointCount() == 0)
        return NOTOK;

    Int32 ptCount = retopoMesh->GetPointCount();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix rMg = retopoMesh->GetMg();

    Vector pScreen(screenX, screenY, 0.0);

    // 1. Check depth against Target Mesh surface under cursor (if targetMesh or precomputedTargetSnap is provided)
    Float targetDepthAtCursor = 1e30;
    Bool hasTargetHit = false;
    if (precomputedTargetSnap && precomputedTargetSnap->valid)
    {
        targetDepthAtCursor = bd->WS(precomputedTargetSnap->worldPos).z;
        hasTargetHit = true;
    }
    else if (targetMesh && targetMesh->GetPolygonCount() > 0)
    {
        SnapResult tSnap = RaycastSurface(bd, targetMesh, screenX, screenY);
        if (tSnap.valid)
        {
            targetDepthAtCursor = bd->WS(tSnap.worldPos).z;
            hasTargetHit = true;
        }
    }
    Float targetTolerance = maxon::Max(Float(2.5), Float(targetDepthAtCursor * 0.005));

    // 2. Find front-most retopo polygon depth under cursor (to prevent picking through polygons)
    Int32 polyCount = retopoMesh->GetPolygonCount();
    const CPolygon* polys = retopoMesh->GetPolygonR();
    Float retopoDepthUnderCursor = 1e30;
    Bool hasRetopoHit = false;

    auto pointInTri2D = [](const Vector& p, const Vector& a, const Vector& b, const Vector& c) -> Bool {
        auto sign = [](const Vector& p1, const Vector& p2, const Vector& p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };
        Float d1 = sign(p, a, b);
        Float d2 = sign(p, b, c);
        Float d3 = sign(p, c, a);
        Bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
        Bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
        return !(hasNeg && hasPos);
    };

    auto calcDepth2D = [](const Vector& p, const Vector& a, const Vector& b, const Vector& c) -> Float {
        Float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (std::abs(denom) < 1e-9)
            return (a.z + b.z + c.z) / 3.0;
        Float invDenom = 1.0 / denom;
        Float u = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) * invDenom;
        Float v = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) * invDenom;
        Float w = 1.0 - u - v;
        return u * a.z + v * b.z + w * c.z;
    };

    Vector camPos = bd->GetMg().off;
    Bool isOrtho = (bd->GetProjection() != Pperspective);
    Vector orthoLook = -bd->GetMg().sqmat.v3.GetNormalized();

    for (Int32 pi = 0; pi < polyCount; ++pi)
    {
        const CPolygon& p = polys[pi];
        Vector wa = rMg * pts[p.a];
        Vector wb = rMg * pts[p.b];
        Vector wc = rMg * pts[p.c];

        Vector fn = Cross(wb - wa, wc - wa);
        Vector polyCenter = (wa + wb + wc) * (1.0 / 3.0);
        Vector toCam = isOrtho ? orthoLook : (camPos - polyCenter).GetNormalized();
        if (Dot(fn, toCam) <= 0.0)
            continue; // Skip backface!

        Vector sa = bd->WS(wa);
        Vector sb = bd->WS(wb);
        Vector sc = bd->WS(wc);
        if (sa.z > 0.0 && sb.z > 0.0 && sc.z > 0.0)
        {
            if (pointInTri2D(pScreen, sa, sb, sc))
            {
                Float z = calcDepth2D(pScreen, sa, sb, sc);
                if (z < retopoDepthUnderCursor)
                {
                    retopoDepthUnderCursor = z;
                    hasRetopoHit = true;
                }
            }
            else if (p.c != p.d)
            {
                Vector wd = rMg * pts[p.d];
                Vector sd = bd->WS(wd);
                if (sd.z > 0.0 && pointInTri2D(pScreen, sa, sc, sd))
                {
                    Float z = calcDepth2D(pScreen, sa, sc, sd);
                    if (z < retopoDepthUnderCursor)
                    {
                        retopoDepthUnderCursor = z;
                        hasRetopoHit = true;
                    }
                }
            }
        }
    }
    Float retopoTolerance = maxon::Max(Float(3.0), Float(retopoDepthUnderCursor * 0.01));

    // 3. Gather candidates within radius
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

        // Target Mesh occlusion
        if (hasTargetHit && sPos.z > targetDepthAtCursor + targetTolerance)
            continue;

        // Front Retopo Mesh occlusion (behind polygon covering cursor)
        if (hasRetopoHit && sPos.z > retopoDepthUnderCursor + retopoTolerance)
            continue;

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

    // 4. Select closest candidate in front layer (strict depth tolerance)
    Float layerTol = maxon::Max(Float(1.5), Float(minZ * 0.005));
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

EdgeHit SurfaceSnapper::FindNearestRetopoEdge(BaseDraw* bd, PolygonObject* retopoMesh, Float screenX, Float screenY, Float maxRadiusPixels, PolygonObject* targetMesh, const SnapResult* precomputedTargetSnap)
{
    EdgeHit hit;
    if (!bd || !retopoMesh || retopoMesh->GetPolygonCount() == 0)
        return hit;

    Int32 polyCount = retopoMesh->GetPolygonCount();
    const CPolygon* polys = retopoMesh->GetPolygonR();
    const Vector* pts = retopoMesh->GetPointR();
    Matrix mg = retopoMesh->GetMg();

    Vector cursor(screenX, screenY, 0.0);

    // 1. Check depth against Target Mesh surface under cursor (if targetMesh or precomputedTargetSnap is provided)
    Float targetDepthAtCursor = 1e30;
    Bool hasTargetHit = false;
    if (precomputedTargetSnap && precomputedTargetSnap->valid)
    {
        targetDepthAtCursor = bd->WS(precomputedTargetSnap->worldPos).z;
        hasTargetHit = true;
    }
    else if (targetMesh && targetMesh->GetPolygonCount() > 0)
    {
        SnapResult tSnap = RaycastSurface(bd, targetMesh, screenX, screenY);
        if (tSnap.valid)
        {
            targetDepthAtCursor = bd->WS(tSnap.worldPos).z;
            hasTargetHit = true;
        }
    }
    Float targetTolerance = maxon::Max(Float(2.5), Float(targetDepthAtCursor * 0.005));

    // 2. Find front-most retopo polygon depth under cursor
    Float retopoDepthUnderCursor = 1e30;
    Bool hasRetopoHit = false;

    auto pointInTri2D = [](const Vector& p, const Vector& a, const Vector& b, const Vector& c) -> Bool {
        auto sign = [](const Vector& p1, const Vector& p2, const Vector& p3) {
            return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
        };
        Float d1 = sign(p, a, b);
        Float d2 = sign(p, b, c);
        Float d3 = sign(p, c, a);
        Bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
        Bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
        return !(hasNeg && hasPos);
    };

    auto calcDepth2D = [](const Vector& p, const Vector& a, const Vector& b, const Vector& c) -> Float {
        Float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (std::abs(denom) < 1e-9)
            return (a.z + b.z + c.z) / 3.0;
        Float invDenom = 1.0 / denom;
        Float u = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) * invDenom;
        Float v = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) * invDenom;
        Float w = 1.0 - u - v;
        return u * a.z + v * b.z + w * c.z;
    };

    Int32 ptCount = retopoMesh->GetPointCount();
    maxon::BaseArray<Vector> sPts;
    sPts.Resize(ptCount) iferr_ignore("Resize sPts");
    for (Int32 i = 0; i < ptCount; ++i)
        sPts[i] = bd->WS(mg * pts[i]);

    Vector camPos = bd->GetMg().off;
    Bool isOrtho = (bd->GetProjection() != Pperspective);
    Vector orthoLook = -bd->GetMg().sqmat.v3.GetNormalized();

    for (Int32 pi = 0; pi < polyCount; ++pi)
    {
        const CPolygon& p = polys[pi];
        const Vector& sa = sPts[p.a];
        const Vector& sb = sPts[p.b];
        const Vector& sc = sPts[p.c];
        if (sa.z <= 0.0 || sb.z <= 0.0 || sc.z <= 0.0)
            continue;

        Vector wa = mg * pts[p.a];
        Vector wb = mg * pts[p.b];
        Vector wc = mg * pts[p.c];
        Vector fn = Cross(wb - wa, wc - wa);
        Vector polyCenter = (wa + wb + wc) * (1.0 / 3.0);
        Vector toCam = isOrtho ? orthoLook : (camPos - polyCenter).GetNormalized();
        if (Dot(fn, toCam) <= 0.0)
            continue; // Skip backface!

        Float pMinX = std::min(std::min(sa.x, sb.x), sc.x);
        Float pMaxX = std::max(std::max(sa.x, sb.x), sc.x);
        if (p.c != p.d)
        {
            const Vector& sd = sPts[p.d];
            if (sd.z <= 0.0) continue;
            pMinX = std::min(pMinX, sd.x);
            pMaxX = std::max(pMaxX, sd.x);
        }
        if (cursor.x < pMinX || cursor.x > pMaxX)
            continue;

        Float pMinY = std::min(std::min(sa.y, sb.y), sc.y);
        Float pMaxY = std::max(std::max(sa.y, sb.y), sc.y);
        if (p.c != p.d)
        {
            const Vector& sd = sPts[p.d];
            pMinY = std::min(pMinY, sd.y);
            pMaxY = std::max(pMaxY, sd.y);
        }
        if (cursor.y < pMinY || cursor.y > pMaxY)
            continue;

        if (pointInTri2D(cursor, sa, sb, sc))
        {
            Float z = calcDepth2D(cursor, sa, sb, sc);
            if (z < retopoDepthUnderCursor)
            {
                retopoDepthUnderCursor = z;
                hasRetopoHit = true;
            }
        }
        else if (p.c != p.d)
        {
            const Vector& sd = sPts[p.d];
            if (pointInTri2D(cursor, sa, sc, sd))
            {
                Float z = calcDepth2D(cursor, sa, sc, sd);
                if (z < retopoDepthUnderCursor)
                {
                    retopoDepthUnderCursor = z;
                    hasRetopoHit = true;
                }
            }
        }
    }
    Float retopoTolerance = maxon::Max(Float(3.0), Float(retopoDepthUnderCursor * 0.01));

    // 3. Gather candidates
    struct EdgeCandidate
    {
        EdgeHit hit;
        Float z;
    };
    maxon::BaseArray<EdgeCandidate> candidates;
    Float minZ = 1e30;

    auto checkEdge = [&](Int32 u, Int32 v, Int32 polyIdx) {
        if (u < 0 || v < 0 || u == v || u >= ptCount || v >= ptCount) return;

        const Vector& sA = sPts[u];
        const Vector& sB = sPts[v];
        if (sA.z <= 0.0 || sB.z <= 0.0) return;

        Float minX = std::min(sA.x, sB.x) - maxRadiusPixels;
        Float maxX = std::max(sA.x, sB.x) + maxRadiusPixels;
        if (cursor.x < minX || cursor.x > maxX) return;

        Float minY = std::min(sA.y, sB.y) - maxRadiusPixels;
        Float maxY = std::max(sA.y, sB.y) + maxRadiusPixels;
        if (cursor.y < minY || cursor.y > maxY) return;

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

            // Target mesh occlusion
            if (hasTargetHit && projZ > targetDepthAtCursor + targetTolerance)
                return;

            // Retopo mesh occlusion
            if (hasRetopoHit && projZ > retopoDepthUnderCursor + retopoTolerance)
                return;

            EdgeCandidate ec;
            ec.hit.valid = true;
            ec.hit.v0 = u;
            ec.hit.v1 = v;
            ec.hit.worldPos0 = mg * pts[u];
            ec.hit.worldPos1 = mg * pts[v];
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

    // 4. Select closest candidate in front layer (strict depth tolerance)
    Float layerTol = maxon::Max(Float(1.5), Float(minZ * 0.005));
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

void SurfaceSnapper::ClearRaycastCache()
{
    m_cachedRaycast = CachedRaycast();
}

} // namespace cinema
