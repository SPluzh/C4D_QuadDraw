#include "mesh_builder.h"
#include "quaddraw_tag.h"
#include "description/obase.h"
#include <utility>
#include <cmath>
#include <algorithm>

namespace cinema
{

MeshBuilder::MeshBuilder()
{
}

MeshBuilder::~MeshBuilder()
{
}

PolygonObject* MeshBuilder::GetOrCreateRetopoMesh(BaseDocument* doc, BaseObject* targetMesh)
{
    if (!doc) return nullptr;

    // 1. If active object is a PolygonObject and NOT the targetMesh, use it
    BaseObject* active = doc->GetActiveObject();
    if (active && active->IsInstanceOf(Opolygon) && active != targetMesh)
    {
        PolygonObject* polyObj = static_cast<PolygonObject*>(active);
        if (!polyObj->GetTag(PLUGIN_ID_QUADDRAW_TAG))
        {
            BaseTag* tag = polyObj->MakeTag(PLUGIN_ID_QUADDRAW_TAG);
            if (tag) doc->AddUndo(UNDOTYPE::NEWOBJ, tag);
        }
        return polyObj;
    }

    // 2. Search for any object that has QuadDraw tag
    BaseObject* obj = doc->GetFirstObject();
    while (obj)
    {
        if (obj->IsInstanceOf(Opolygon) && obj->GetTag(PLUGIN_ID_QUADDRAW_TAG))
            return static_cast<PolygonObject*>(obj);

        BaseObject* next = obj->GetDown();
        if (!next) next = obj->GetNext();
        while (!next && obj->GetUp())
        {
            obj = obj->GetUp();
            next = obj->GetNext();
        }
        obj = next;
    }

    // 3. Search for existing "QuadDraw_Retopo" object in the document
    BaseObject* existing = doc->SearchObject("QuadDraw_Retopo"_s);
    if (existing && existing->IsInstanceOf(Opolygon))
    {
        // Reset any forced object display color so the object retains clean C4D shading
        existing->SetParameter(ConstDescIDLevel(ID_BASEOBJECT_USECOLOR), GeData(ID_BASEOBJECT_USECOLOR_OFF), DESCFLAGS_SET::NONE);
        PolygonObject* polyObj = static_cast<PolygonObject*>(existing);
        if (!polyObj->GetTag(PLUGIN_ID_QUADDRAW_TAG))
        {
            BaseTag* tag = polyObj->MakeTag(PLUGIN_ID_QUADDRAW_TAG);
            if (tag) doc->AddUndo(UNDOTYPE::NEWOBJ, tag);
        }
        return polyObj;
    }

    // 4. Otherwise create a brand new PolygonObject
    PolygonObject* newMesh = PolygonObject::Alloc(0, 0);
    if (!newMesh) return nullptr;

    newMesh->SetName("QuadDraw_Retopo"_s);
    newMesh->SetParameter(ConstDescIDLevel(ID_BASEOBJECT_USECOLOR), GeData(ID_BASEOBJECT_USECOLOR_OFF), DESCFLAGS_SET::NONE);

    // Add Phong tag for smooth shading
    BaseTag* phongTag = BaseTag::Alloc(Tphong);
    if (phongTag)
    {
        newMesh->InsertTag(phongTag);
    }

    // Add custom QuadDraw tag
    BaseTag* qdTag = BaseTag::Alloc(PLUGIN_ID_QUADDRAW_TAG);
    if (qdTag)
    {
        newMesh->InsertTag(qdTag);
    }

    if (targetMesh)
    {
        newMesh->SetMg(targetMesh->GetMg());
        doc->InsertObject(newMesh, nullptr, targetMesh);
    }
    else
    {
        doc->InsertObject(newMesh, nullptr, nullptr);
    }

    doc->AddUndo(UNDOTYPE::NEWOBJ, newMesh);
    doc->SetActiveObject(newMesh, SELECTION_NEW);
    return newMesh;
}

Int32 MeshBuilder::AddVertex(PolygonObject* mesh, const Vector& worldPos)
{
    if (!mesh) return NOTOK;

    Int32 oldCount = mesh->GetPointCount();
    Int32 polyCount = mesh->GetPolygonCount();

    if (!mesh->ResizeObject(oldCount + 1, polyCount))
        return NOTOK;

    Vector* points = mesh->GetPointW();
    Vector localPos = ~mesh->GetMg() * worldPos;
    points[oldCount] = localPos;

    NotifyMeshUpdated(mesh);
    return oldCount;
}

Bool MeshBuilder::AddQuad(PolygonObject* mesh, Int32 a, Int32 b, Int32 c, Int32 d, const Vector& targetNormal)
{
    if (!mesh) return false;
    Int32 ptCount = mesh->GetPointCount();
    if (a < 0 || a >= ptCount || b < 0 || b >= ptCount ||
        c < 0 || c >= ptCount || d < 0 || d >= ptCount)
        return false;

    // Avoid degenerate quads
    if (a == b || b == c || c == d || d == a || a == c || b == d)
        return false;

    Int32 polyCount = mesh->GetPolygonCount();
    const CPolygon* oldPolys = mesh->GetPolygonR();

    // Check if a polygon with these exact vertices or sharing 3 vertices already exists
    for (Int32 i = 0; i < polyCount; ++i)
    {
        const CPolygon& p = oldPolys[i];
        Int32 match = 0;
        if (p.a == a || p.a == b || p.a == c || p.a == d) match++;
        if (p.b == a || p.b == b || p.b == c || p.b == d) match++;
        if (p.c == a || p.c == b || p.c == c || p.c == d) match++;
        if (p.c != p.d)
        {
            if (p.d == a || p.d == b || p.d == c || p.d == d) match++;
            if (match >= 3) return false; // Already exists or overlapping!
        }
        else
        {
            if (match >= 3) return false;
        }
    }

    // Check manifoldness: no edge can have >= 2 polygons
    auto countEdgePolys = [&](Int32 u, Int32 v) -> Int32 {
        Int32 cnt = 0;
        for (Int32 i = 0; i < polyCount; ++i)
        {
            if (PolygonHasEdge(oldPolys[i], u, v)) cnt++;
        }
        return cnt;
    };
    if (countEdgePolys(a, b) >= 2 || countEdgePolys(b, c) >= 2 || countEdgePolys(c, d) >= 2 || countEdgePolys(d, a) >= 2)
        return false;

    // Check diagonals: if (a, c) or (b, d) is already an edge, this quad overlaps an existing division
    if (countEdgePolys(a, c) > 0 || countEdgePolys(b, d) > 0)
        return false;

    // Check orientation with targetNormal
    const Vector* pts = mesh->GetPointR();
    Vector pA = mesh->GetMg() * pts[a];
    Vector pB = mesh->GetMg() * pts[b];
    Vector pC = mesh->GetMg() * pts[c];
    Vector polyNormal = Cross(pB - pA, pC - pA).GetNormalized();

    if (Dot(polyNormal, targetNormal) < 0.0)
    {
        std::swap(b, d);
    }

    if (!mesh->ResizeObject(ptCount, polyCount + 1))
        return false;

    CPolygon* polys = mesh->GetPolygonW();
    polys[polyCount] = CPolygon(a, b, c, d);

    NotifyMeshUpdated(mesh);
    return true;
}

Bool MeshBuilder::AddTriangle(PolygonObject* mesh, Int32 a, Int32 b, Int32 c, const Vector& targetNormal)
{
    if (!mesh) return false;
    Int32 ptCount = mesh->GetPointCount();
    if (a < 0 || a >= ptCount || b < 0 || b >= ptCount || c < 0 || c >= ptCount)
        return false;

    if (a == b || b == c || c == a)
        return false;

    const Vector* pts = mesh->GetPointR();
    Vector pA = mesh->GetMg() * pts[a];
    Vector pB = mesh->GetMg() * pts[b];
    Vector pC = mesh->GetMg() * pts[c];
    Vector polyNormal = Cross(pB - pA, pC - pA).GetNormalized();

    if (Dot(polyNormal, targetNormal) < 0.0)
    {
        std::swap(b, c);
    }

    Int32 polyCount = mesh->GetPolygonCount();
    if (!mesh->ResizeObject(ptCount, polyCount + 1))
        return false;

    CPolygon* polys = mesh->GetPolygonW();
    polys[polyCount] = CPolygon(a, b, c, c); // Triangle in C4D has c == d

    NotifyMeshUpdated(mesh);
    return true;
}

Bool MeshBuilder::SetVertexPosition(PolygonObject* mesh, Int32 index, const Vector& worldPos)
{
    if (!mesh || index < 0 || index >= mesh->GetPointCount())
        return false;

    Vector* pts = mesh->GetPointW();
    pts[index] = ~mesh->GetMg() * worldPos;

    NotifyMeshUpdated(mesh);
    return true;
}

Bool MeshBuilder::WeldVertices(PolygonObject* mesh, Int32 vSource, Int32 vTarget)
{
    if (!mesh || vSource == vTarget) return false;
    Int32 ptCount = mesh->GetPointCount();
    Int32 polyCount = mesh->GetPolygonCount();
    if (vSource < 0 || vSource >= ptCount || vTarget < 0 || vTarget >= ptCount)
        return false;

    // 1. Remap all polygon vertex indices from vSource to vTarget
    CPolygon* polys = mesh->GetPolygonW();
    for (Int32 i = 0; i < polyCount; ++i)
    {
        CPolygon& p = polys[i];
        if (p.a == vSource) p.a = vTarget;
        if (p.b == vSource) p.b = vTarget;
        if (p.c == vSource) p.c = vTarget;
        if (p.d == vSource) p.d = vTarget;
    }

    // 2. Clean up any degenerate polygons
    maxon::BaseArray<CPolygon> validPolys;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        CPolygon p = polys[i];
        Bool isQuad = (p.c != p.d);

        if (isQuad)
        {
            if (p.a == p.b)
                p = CPolygon(p.a, p.c, p.d, p.d);
            else if (p.b == p.c)
                p = CPolygon(p.a, p.b, p.d, p.d);
            else if (p.c == p.d)
                p = CPolygon(p.a, p.b, p.c, p.c);
            else if (p.d == p.a)
                p = CPolygon(p.a, p.b, p.c, p.c);
        }

        if (p.c == p.d)
        {
            if (p.a == p.b || p.b == p.c || p.a == p.c)
                continue;
        }

        Bool isDuplicate = false;
        for (Int32 j = 0; j < (Int32)validPolys.GetCount(); ++j)
        {
            const CPolygon& existing = validPolys[j];
            if (existing.a == p.a && existing.b == p.b && existing.c == p.c && existing.d == p.d)
            {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate)
        {
            validPolys.Append(p) iferr_ignore("Append valid poly");
        }
    }

    if ((Int32)validPolys.GetCount() != polyCount)
    {
        mesh->ResizeObject(ptCount, (Int32)validPolys.GetCount());
        CPolygon* newPolys = mesh->GetPolygonW();
        for (Int32 i = 0; i < (Int32)validPolys.GetCount(); ++i)
            newPolys[i] = validPolys[i];
    }

    return DeleteVertex(mesh, vSource);
}

Bool MeshBuilder::DeleteVertex(PolygonObject* mesh, Int32 ptIndex)
{
    if (!mesh) return false;
    Int32 ptCount = mesh->GetPointCount();
    Int32 polyCount = mesh->GetPolygonCount();
    if (ptIndex < 0 || ptIndex >= ptCount) return false;

    const Vector* oldPoints = mesh->GetPointR();
    const CPolygon* oldPolys = mesh->GetPolygonR();

    maxon::BaseArray<CPolygon> remainingPolys;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        const CPolygon& p = oldPolys[i];
        if (p.a == ptIndex || p.b == ptIndex || p.c == ptIndex || (p.c != p.d && p.d == ptIndex))
            continue;

        remainingPolys.Append(p) iferr_ignore("Append poly");
    }

    maxon::BaseArray<Int32> pointUseCount;
    pointUseCount.Resize(ptCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < ptCount; ++i) pointUseCount[i] = 0;

    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        if (p.a >= 0 && p.a < ptCount) pointUseCount[p.a]++;
        if (p.b >= 0 && p.b < ptCount) pointUseCount[p.b]++;
        if (p.c >= 0 && p.c < ptCount) pointUseCount[p.c]++;
        if (p.c != p.d && p.d >= 0 && p.d < ptCount) pointUseCount[p.d]++;
    }

    maxon::BaseArray<Vector> newPoints;
    maxon::BaseArray<Int32> oldToNew;
    oldToNew.Resize(ptCount) iferr_ignore("Resize");

    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (i == ptIndex)
        {
            oldToNew[i] = NOTOK;
        }
        else if (pointUseCount[i] > 0)
        {
            oldToNew[i] = (Int32)newPoints.GetCount();
            newPoints.Append(oldPoints[i]) iferr_ignore("Append point");
        }
        else
        {
            oldToNew[i] = NOTOK;
        }
    }

    maxon::BaseArray<CPolygon> finalPolys;
    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        Bool isTri = (p.c == p.d);

        Int32 nA = (p.a >= 0 && p.a < ptCount) ? oldToNew[p.a] : NOTOK;
        Int32 nB = (p.b >= 0 && p.b < ptCount) ? oldToNew[p.b] : NOTOK;
        Int32 nC = (p.c >= 0 && p.c < ptCount) ? oldToNew[p.c] : NOTOK;
        Int32 nD = isTri ? nC : ((p.d >= 0 && p.d < ptCount) ? oldToNew[p.d] : NOTOK);

        if (nA == NOTOK || nB == NOTOK || nC == NOTOK || nD == NOTOK)
            continue;

        finalPolys.Append(CPolygon(nA, nB, nC, nD)) iferr_ignore("Append poly");
    }

    mesh->ResizeObject((Int32)newPoints.GetCount(), (Int32)finalPolys.GetCount());
    Vector* ptsW = mesh->GetPointW();
    for (Int32 i = 0; i < (Int32)newPoints.GetCount(); ++i)
        ptsW[i] = newPoints[i];

    CPolygon* polysW = mesh->GetPolygonW();
    for (Int32 i = 0; i < (Int32)finalPolys.GetCount(); ++i)
        polysW[i] = finalPolys[i];

    NotifyMeshUpdated(mesh);
    return true;
}

Bool MeshBuilder::DeletePolygon(PolygonObject* mesh, Int32 polyIndex)
{
    if (!mesh) return false;
    Int32 ptCount = mesh->GetPointCount();
    Int32 polyCount = mesh->GetPolygonCount();
    if (polyIndex < 0 || polyIndex >= polyCount) return false;

    const CPolygon* oldPolys = mesh->GetPolygonR();
    maxon::BaseArray<CPolygon> remainingPolys;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        if (i != polyIndex)
            remainingPolys.Append(oldPolys[i]) iferr_ignore("Append poly");
    }

    // Count usage of each point in the remaining polygons
    const Vector* oldPts = mesh->GetPointR();
    maxon::BaseArray<Int32> pointUseCount;
    pointUseCount.Resize(ptCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < ptCount; ++i) pointUseCount[i] = 0;

    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        if (p.a >= 0 && p.a < ptCount) pointUseCount[p.a]++;
        if (p.b >= 0 && p.b < ptCount) pointUseCount[p.b]++;
        if (p.c >= 0 && p.c < ptCount) pointUseCount[p.c]++;
        if (p.c != p.d && p.d >= 0 && p.d < ptCount) pointUseCount[p.d]++;
    }

    // Strip points that have 0 remaining references so no hanging vertices remain
    maxon::BaseArray<Vector> newPoints;
    maxon::BaseArray<Int32> oldToNew;
    oldToNew.Resize(ptCount) iferr_ignore("Resize");

    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (pointUseCount[i] > 0)
        {
            oldToNew[i] = (Int32)newPoints.GetCount();
            newPoints.Append(oldPts[i]) iferr_ignore("Append point");
        }
        else
        {
            oldToNew[i] = NOTOK;
        }
    }

    // Remap remaining polygons to the compacted point array
    maxon::BaseArray<CPolygon> finalPolys;
    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        Bool isTri = (p.c == p.d);

        Int32 nA = (p.a >= 0 && p.a < ptCount) ? oldToNew[p.a] : NOTOK;
        Int32 nB = (p.b >= 0 && p.b < ptCount) ? oldToNew[p.b] : NOTOK;
        Int32 nC = (p.c >= 0 && p.c < ptCount) ? oldToNew[p.c] : NOTOK;
        Int32 nD = isTri ? nC : ((p.d >= 0 && p.d < ptCount) ? oldToNew[p.d] : NOTOK);

        if (nA == NOTOK || nB == NOTOK || nC == NOTOK || nD == NOTOK)
            continue;

        finalPolys.Append(CPolygon(nA, nB, nC, nD)) iferr_ignore("Append poly");
    }

    mesh->ResizeObject((Int32)newPoints.GetCount(), (Int32)finalPolys.GetCount());
    Vector* ptsW = mesh->GetPointW();
    for (Int32 i = 0; i < (Int32)newPoints.GetCount(); ++i)
        ptsW[i] = newPoints[i];

    CPolygon* polysW = mesh->GetPolygonW();
    for (Int32 i = 0; i < (Int32)finalPolys.GetCount(); ++i)
        polysW[i] = finalPolys[i];

    NotifyMeshUpdated(mesh);
    return true;
}



static Int32 GetNextLoopStep(const PolygonObject* mesh, Int32 prevVertex, Int32 currVertex, Bool isBoundary)
{
    if (!mesh) return NOTOK;
    Int32 polyCount = mesh->GetPolygonCount();
    const CPolygon* polys = mesh->GetPolygonR();

    maxon::BaseArray<Int32> polysAtCurr;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        if (PolygonHasVertex(polys[i], currVertex))
            polysAtCurr.Append(i) iferr_ignore("Append poly");
    }

    if (isBoundary)
    {
        Int32 bestCand = NOTOK;
        Float bestCosAngle = -2.0;

        const Vector* pts = mesh->GetPointR();
        Matrix mg = mesh->GetMg();
        Vector posPrev = mg * pts[prevVertex];
        Vector posCurr = mg * pts[currVertex];
        Vector vIn = posCurr - posPrev;
        Float lenIn = vIn.GetLength();

        // Border loop: find best boundary edge continuation in a straight line
        for (Int32 i = 0; i < (Int32)polysAtCurr.GetCount(); ++i)
        {
            const CPolygon& p = polys[polysAtCurr[i]];
            Int32 q[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
            Int32 count = (p.c != p.d) ? 4 : 3;

            for (Int32 k = 0; k < count; ++k)
            {
                if (q[k] == currVertex)
                {
                    Int32 cand1 = q[(k + 1) % count];
                    Int32 cand2 = q[(k + count - 1) % count];

                    for (Int32 c = 0; c < 2; ++c)
                    {
                        Int32 cand = (c == 0) ? cand1 : cand2;
                        if (cand != prevVertex && cand != NOTOK)
                        {
                            Int32 sharing = 0;
                            for (Int32 j = 0; j < polyCount; ++j)
                            {
                                if (PolygonHasEdge(polys[j], currVertex, cand))
                                    sharing++;
                            }
                            if (sharing == 1)
                            {
                                Vector posCand = mg * pts[cand];
                                Vector vOut = posCand - posCurr;
                                Float lenOut = vOut.GetLength();
                                Float cosAngle = 1.0;
                                if (lenIn > 1e-4 && lenOut > 1e-4)
                                    cosAngle = Dot(vIn, vOut) / (lenIn * lenOut);

                                if (cosAngle > bestCosAngle)
                                {
                                    bestCosAngle = cosAngle;
                                    bestCand = cand;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Only continue along boundary if roughly straight (no sharp turn > 45 degrees)
        if (bestCand != NOTOK && bestCosAngle >= 0.707)
            return bestCand;

        return NOTOK;
    }
    else
    {
        // Interior loop: currVertex must be regular valence 4 (4 quads)
        if (polysAtCurr.GetCount() != 4)
            return NOTOK;

        for (Int32 i = 0; i < 4; ++i)
        {
            if (polys[polysAtCurr[i]].c == polys[polysAtCurr[i]].d)
                return NOTOK;
        }

        Int32 pIn0 = NOTOK, pIn1 = NOTOK;
        Int32 pOut0 = NOTOK, pOut1 = NOTOK;

        for (Int32 i = 0; i < 4; ++i)
        {
            Int32 idx = polysAtCurr[i];
            if (PolygonHasEdge(polys[idx], prevVertex, currVertex))
            {
                if (pIn0 == NOTOK) pIn0 = idx;
                else pIn1 = idx;
            }
            else
            {
                if (pOut0 == NOTOK) pOut0 = idx;
                else pOut1 = idx;
            }
        }

        if (pIn0 == NOTOK || pIn1 == NOTOK || pOut0 == NOTOK || pOut1 == NOTOK)
            return NOTOK;

        const CPolygon& polyOut0 = polys[pOut0];
        const CPolygon& polyOut1 = polys[pOut1];

        Int32 nextVertex = NOTOK;
        Int32 q0[4] = { polyOut0.a, polyOut0.b, polyOut0.c, polyOut0.d };
        for (Int32 k = 0; k < 4; ++k)
        {
            if (q0[k] == currVertex)
            {
                Int32 cand1 = q0[(k + 1) % 4];
                Int32 cand2 = q0[(k + 3) % 4];

                if (PolygonHasVertex(polyOut1, cand1))
                {
                    nextVertex = cand1;
                    break;
                }
                if (PolygonHasVertex(polyOut1, cand2))
                {
                    nextVertex = cand2;
                    break;
                }
            }
        }

        if (nextVertex == NOTOK)
            return NOTOK;

        // For regular interior valence-4 vertex, the opposite edge is topologically unique.
        // Only stop if the edge folds completely back on itself (cosAngle < -0.2)
        const Vector* pts = mesh->GetPointR();
        Matrix mg = mesh->GetMg();
        Vector posPrev = mg * pts[prevVertex];
        Vector posCurr = mg * pts[currVertex];
        Vector posNext = mg * pts[nextVertex];

        Vector vIn = posCurr - posPrev;
        Vector vOut = posNext - posCurr;
        Float lenIn = vIn.GetLength();
        Float lenOut = vOut.GetLength();
        if (lenIn > 1e-4 && lenOut > 1e-4)
        {
            Float cosAngle = Dot(vIn, vOut) / (lenIn * lenOut);
            if (cosAngle < -0.2)
                return NOTOK;
        }

        return nextVertex;
    }
}

EdgeLoopResult MeshBuilder::FindEdgeLoop(PolygonObject* mesh, Int32 startV0, Int32 startV1)
{
    EdgeLoopResult res;
    if (!mesh || startV0 == startV1) return res;
    Int32 polyCount = mesh->GetPolygonCount();
    Int32 ptCount = mesh->GetPointCount();
    if (polyCount == 0 || startV0 < 0 || startV0 >= ptCount || startV1 < 0 || startV1 >= ptCount)
        return res;

    const CPolygon* polys = mesh->GetPolygonR();
    const Vector* pts = mesh->GetPointR();
    Matrix mg = mesh->GetMg();

    Int32 startSharing = 0;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        if (PolygonHasEdge(polys[i], startV0, startV1))
            startSharing++;
    }
    if (startSharing == 0) return res;
    Bool isBoundary = (startSharing == 1);

    // Forward traversal starting from startV1 (moving away from startV0)
    maxon::BaseArray<Int32> forwardChain;
    forwardChain.Append(startV0) iferr_ignore("Append");
    forwardChain.Append(startV1) iferr_ignore("Append");

    Int32 prev = startV0;
    Int32 curr = startV1;
    Bool isClosed = false;

    for (Int32 step = 0; step < 1000; ++step)
    {
        Int32 next = GetNextLoopStep(mesh, prev, curr, isBoundary);
        if (next == NOTOK)
            break;

        if (next == startV0)
        {
            isClosed = true;
            break;
        }

        Bool visited = false;
        for (Int32 i = 0; i < (Int32)forwardChain.GetCount(); ++i)
        {
            if (forwardChain[i] == next) { visited = true; break; }
        }
        if (visited) break;

        forwardChain.Append(next) iferr_ignore("Append");
        prev = curr;
        curr = next;
    }

    // Backward traversal starting from startV0 (moving away from startV1)
    maxon::BaseArray<Int32> backwardChain;
    if (!isClosed)
    {
        prev = startV1;
        curr = startV0;
        for (Int32 step = 0; step < 1000; ++step)
        {
            Int32 next = GetNextLoopStep(mesh, prev, curr, isBoundary);
            if (next == NOTOK)
                break;

            Bool visited = false;
            for (Int32 i = 0; i < (Int32)forwardChain.GetCount(); ++i)
            {
                if (forwardChain[i] == next) { visited = true; break; }
            }
            for (Int32 i = 0; i < (Int32)backwardChain.GetCount(); ++i)
            {
                if (backwardChain[i] == next) { visited = true; break; }
            }
            if (visited) break;

            backwardChain.Append(next) iferr_ignore("Append");
            prev = curr;
            curr = next;
        }
    }

    // Assemble full ordered vertex list
    maxon::BaseArray<Int32> fullVertices;
    for (Int32 i = (Int32)backwardChain.GetCount() - 1; i >= 0; --i)
    {
        fullVertices.Append(backwardChain[i]) iferr_ignore("Append");
    }
    for (Int32 i = 0; i < (Int32)forwardChain.GetCount(); ++i)
    {
        fullVertices.Append(forwardChain[i]) iferr_ignore("Append");
    }

    // Construct LoopEdges
    for (Int32 i = 0; i + 1 < (Int32)fullVertices.GetCount(); ++i)
    {
        LoopEdge le;
        le.v0 = fullVertices[i];
        le.v1 = fullVertices[i + 1];
        le.worldPos0 = mg * pts[le.v0];
        le.worldPos1 = mg * pts[le.v1];
        res.edges.Append(le) iferr_ignore("Append loop edge");
    }

    if (isClosed && fullVertices.GetCount() >= 3)
    {
        LoopEdge le;
        le.v0 = fullVertices[fullVertices.GetCount() - 1];
        le.v1 = fullVertices[0];
        le.worldPos0 = mg * pts[le.v0];
        le.worldPos1 = mg * pts[le.v1];
        res.edges.Append(le) iferr_ignore("Append closing edge");
        res.isClosed = true;
    }

    return res;
}

Bool MeshBuilder::DeleteEdgeLoop(PolygonObject* mesh, const maxon::BaseArray<LoopEdge>& loopEdges)
{
    if (!mesh || loopEdges.GetCount() == 0) return false;
    Int32 polyCount = mesh->GetPolygonCount();
    Int32 ptCount = mesh->GetPointCount();
    if (polyCount == 0 || ptCount == 0) return false;

    const CPolygon* oldPolys = mesh->GetPolygonR();
    maxon::BaseArray<CPolygon> currentPolys;
    currentPolys.Resize(polyCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < polyCount; ++i)
        currentPolys[i] = oldPolys[i];

    maxon::BaseArray<Bool> polyDeleted;
    polyDeleted.Resize(polyCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < polyCount; ++i)
        polyDeleted[i] = false;

    const Vector* pts = mesh->GetPointR();
    Matrix mg = mesh->GetMg();

    auto getQuadOpposites = [](const CPolygon& p, Int32 u, Int32 v, Int32& outU, Int32& outV) -> Bool {
        if (p.c == p.d) return false;
        Int32 q[4] = { p.a, p.b, p.c, p.d };
        Int32 iu = -1, iv = -1;
        for (Int32 i = 0; i < 4; ++i)
        {
            if (q[i] == u) iu = i;
            if (q[i] == v) iv = i;
        }
        if (iu == -1 || iv == -1) return false;

        if ((iu + 1) % 4 == iv)
        {
            outU = q[(iu + 3) % 4];
            outV = q[(iv + 1) % 4];
            return true;
        }
        else if ((iv + 1) % 4 == iu)
        {
            outV = q[(iv + 3) % 4];
            outU = q[(iu + 1) % 4];
            return true;
        }
        return false;
    };

    // Merges p0 and p1 across shared edge (u, v) into outPoly.
    auto mergePolygonsAcrossEdge = [&](const CPolygon& p0, const CPolygon& p1, Int32 u, Int32 v, CPolygon& outPoly) -> Bool
    {
        Bool isTri0 = (p0.c == p0.d);
        Bool isTri1 = (p1.c == p1.d);

        // Compute original normal from p0 (or p1)
        Vector p0A = mg * pts[p0.a];
        Vector p0B = mg * pts[p0.b];
        Vector p0C = mg * pts[p0.c];
        Vector oldNorm;
        if (isTri0)
            oldNorm = Cross(p0B - p0A, p0C - p0A);
        else
        {
            Vector p0D = mg * pts[p0.d];
            oldNorm = Cross(p0C - p0A, p0D - p0B);
        }
        if (Dot(oldNorm, oldNorm) < 1e-8)
        {
            Vector p1A = mg * pts[p1.a];
            Vector p1B = mg * pts[p1.b];
            Vector p1C = mg * pts[p1.c];
            if (isTri1)
                oldNorm = Cross(p1B - p1A, p1C - p1A);
            else
            {
                Vector p1D = mg * pts[p1.d];
                oldNorm = Cross(p1C - p1A, p1D - p1B);
            }
        }

        // CASE 1: Both are triangles -> merge into Quad
        if (isTri0 && isTri1)
        {
            Int32 w0 = NOTOK;
            if (p0.a != u && p0.a != v) w0 = p0.a;
            else if (p0.b != u && p0.b != v) w0 = p0.b;
            else if (p0.c != u && p0.c != v) w0 = p0.c;

            Int32 w1 = NOTOK;
            if (p1.a != u && p1.a != v) w1 = p1.a;
            else if (p1.b != u && p1.b != v) w1 = p1.b;
            else if (p1.c != u && p1.c != v) w1 = p1.c;

            if (w0 == NOTOK || w1 == NOTOK || w0 == w1) return false;

            Int32 t0[3] = { p0.a, p0.b, p0.c };
            Int32 iu0 = -1, iv0 = -1;
            for (Int32 k = 0; k < 3; ++k)
            {
                if (t0[k] == u) iu0 = k;
                if (t0[k] == v) iv0 = k;
            }
            if (iu0 == -1 || iv0 == -1) return false;

            if ((iu0 + 1) % 3 == iv0)
            {
                outPoly = CPolygon(w0, u, w1, v);
            }
            else
            {
                outPoly = CPolygon(w0, v, w1, u);
            }

            Vector nA = mg * pts[outPoly.a];
            Vector nB = mg * pts[outPoly.b];
            Vector nC = mg * pts[outPoly.c];
            Vector nD = mg * pts[outPoly.d];
            Vector newNorm = Cross(nC - nA, nD - nB);
            if (Dot(newNorm, oldNorm) < 0.0)
            {
                std::swap(outPoly.b, outPoly.d);
            }
            return true;
        }

        // CASE 2: Both are quads -> merge into Quad or Triangle
        if (!isTri0 && !isTri1)
        {
            Int32 o0a = NOTOK, o0b = NOTOK;
            Int32 o1a = NOTOK, o1b = NOTOK;

            if (!getQuadOpposites(p0, u, v, o0a, o0b) || !getQuadOpposites(p1, u, v, o1a, o1b))
                return false;

            if (o0a == NOTOK || o0b == NOTOK || o1a == NOTOK || o1b == NOTOK)
                return false;

            // Subcase 2A: 4 distinct outer vertices -> Quad
            if (o0a != o0b && o0b != o1b && o1b != o1a && o1a != o0a && o0a != o1b && o0b != o1a)
            {
                Int32 q0[4] = { p0.a, p0.b, p0.c, p0.d };
                Int32 iu0 = -1, iv0 = -1;
                for (Int32 k = 0; k < 4; ++k)
                {
                    if (q0[k] == u) iu0 = k;
                    if (q0[k] == v) iv0 = k;
                }

                if ((iu0 + 1) % 4 == iv0)
                {
                    outPoly = CPolygon(o0a, o1a, o1b, o0b);
                }
                else
                {
                    outPoly = CPolygon(o0a, o0b, o1b, o1a);
                }

                Vector nA = mg * pts[outPoly.a];
                Vector nB = mg * pts[outPoly.b];
                Vector nC = mg * pts[outPoly.c];
                Vector nD = mg * pts[outPoly.d];
                Vector newNorm = Cross(nC - nA, nD - nB);
                if (Dot(newNorm, oldNorm) < 0.0)
                {
                    std::swap(outPoly.b, outPoly.d);
                }
                return true;
            }

            // Subcase 2B: 3 distinct outer vertices -> Triangle
            if (o0a == o1a && o0b != o1b && o0a != o0b && o0a != o1b)
            {
                outPoly = CPolygon(o0a, o1b, o0b, o0b);
                Vector nA = mg * pts[outPoly.a];
                Vector nB = mg * pts[outPoly.b];
                Vector nC = mg * pts[outPoly.c];
                Vector newNorm = Cross(nB - nA, nC - nA);
                if (Dot(newNorm, oldNorm) < 0.0)
                {
                    std::swap(outPoly.b, outPoly.c);
                    outPoly.d = outPoly.c;
                }
                return true;
            }
            if (o0b == o1b && o0a != o1a && o0b != o0a && o0b != o1a)
            {
                outPoly = CPolygon(o0b, o0a, o1a, o1a);
                Vector nA = mg * pts[outPoly.a];
                Vector nB = mg * pts[outPoly.b];
                Vector nC = mg * pts[outPoly.c];
                Vector newNorm = Cross(nB - nA, nC - nA);
                if (Dot(newNorm, oldNorm) < 0.0)
                {
                    std::swap(outPoly.b, outPoly.c);
                    outPoly.d = outPoly.c;
                }
                return true;
            }

            return false;
        }

        // CASE 3: One quad, one triangle -> collapse into Triangle
        const CPolygon& pQuad = isTri0 ? p1 : p0;
        const CPolygon& pTri  = isTri0 ? p0 : p1;

        Int32 triW = NOTOK;
        if (pTri.a != u && pTri.a != v) triW = pTri.a;
        else if (pTri.b != u && pTri.b != v) triW = pTri.b;
        else if (pTri.c != u && pTri.c != v) triW = pTri.c;

        Int32 qoA = NOTOK, qoB = NOTOK;
        if (!getQuadOpposites(pQuad, u, v, qoA, qoB)) return false;

        if (triW == qoA && qoA != qoB)
        {
            outPoly = CPolygon(triW, qoB, v, v);
        }
        else if (triW == qoB && qoA != qoB)
        {
            outPoly = CPolygon(triW, u, qoA, qoA);
        }
        else
        {
            outPoly = CPolygon(qoA, qoB, v, v);
        }

        Vector nA = mg * pts[outPoly.a];
        Vector nB = mg * pts[outPoly.b];
        Vector nC = mg * pts[outPoly.c];
        Vector newNorm = Cross(nB - nA, nC - nA);
        if (Dot(newNorm, oldNorm) < 0.0)
        {
            std::swap(outPoly.b, outPoly.c);
            outPoly.d = outPoly.c;
        }
        return true;
    };

    // Process each edge in the loop
    for (Int32 e = 0; e < (Int32)loopEdges.GetCount(); ++e)
    {
        Int32 u = loopEdges[e].v0;
        Int32 v = loopEdges[e].v1;

        maxon::BaseArray<Int32> sharing;
        for (Int32 i = 0; i < (Int32)currentPolys.GetCount(); ++i)
        {
            if (!polyDeleted[i] && PolygonHasEdge(currentPolys[i], u, v))
                sharing.Append(i) iferr_ignore("Append sharing");
        }

        if (sharing.GetCount() == 2)
        {
            Int32 idx0 = sharing[0];
            Int32 idx1 = sharing[1];
            CPolygon newPoly;
            if (mergePolygonsAcrossEdge(currentPolys[idx0], currentPolys[idx1], u, v, newPoly))
            {
                currentPolys[idx0] = newPoly;
                polyDeleted[idx1] = true;
                continue;
            }

            polyDeleted[idx0] = true;
            polyDeleted[idx1] = true;
        }
        else if (sharing.GetCount() == 1)
        {
            polyDeleted[sharing[0]] = true;
        }
        else if (sharing.GetCount() > 2)
        {
            for (Int32 k = 0; k < (Int32)sharing.GetCount(); ++k)
                polyDeleted[sharing[k]] = true;
        }
    }

    maxon::BaseArray<CPolygon> remainingPolys;
    for (Int32 i = 0; i < (Int32)currentPolys.GetCount(); ++i)
    {
        if (!polyDeleted[i])
        {
            CPolygon p = currentPolys[i];
            Bool isTri = (p.c == p.d);
            if (isTri)
            {
                if (p.a != p.b && p.b != p.c && p.a != p.c)
                    remainingPolys.Append(p) iferr_ignore("Append tri");
            }
            else
            {
                if (p.a == p.b) p = CPolygon(p.a, p.c, p.d, p.d);
                else if (p.b == p.c) p = CPolygon(p.a, p.b, p.d, p.d);
                else if (p.c == p.d) p = CPolygon(p.a, p.b, p.c, p.c);
                else if (p.d == p.a) p = CPolygon(p.a, p.b, p.c, p.c);

                if (p.c == p.d)
                {
                    if (p.a != p.b && p.b != p.c && p.a != p.c)
                        remainingPolys.Append(p) iferr_ignore("Append collapsed tri");
                }
                else
                {
                    remainingPolys.Append(p) iferr_ignore("Append quad");
                }
            }
        }
    }

    // Strip unreferenced points
    maxon::BaseArray<Int32> pointUseCount;
    pointUseCount.Resize(ptCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < ptCount; ++i) pointUseCount[i] = 0;

    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        if (p.a >= 0 && p.a < ptCount) pointUseCount[p.a]++;
        if (p.b >= 0 && p.b < ptCount) pointUseCount[p.b]++;
        if (p.c >= 0 && p.c < ptCount) pointUseCount[p.c]++;
        if (p.c != p.d && p.d >= 0 && p.d < ptCount) pointUseCount[p.d]++;
    }

    maxon::BaseArray<Vector> newPoints;
    maxon::BaseArray<Int32> oldToNew;
    oldToNew.Resize(ptCount) iferr_ignore("Resize");

    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (pointUseCount[i] > 0)
        {
            oldToNew[i] = (Int32)newPoints.GetCount();
            newPoints.Append(pts[i]) iferr_ignore("Append point");
        }
        else
        {
            oldToNew[i] = NOTOK;
        }
    }

    maxon::BaseArray<CPolygon> finalPolys;
    for (Int32 i = 0; i < (Int32)remainingPolys.GetCount(); ++i)
    {
        const CPolygon& p = remainingPolys[i];
        Bool isTri = (p.c == p.d);

        Int32 nA = (p.a >= 0 && p.a < ptCount) ? oldToNew[p.a] : NOTOK;
        Int32 nB = (p.b >= 0 && p.b < ptCount) ? oldToNew[p.b] : NOTOK;
        Int32 nC = (p.c >= 0 && p.c < ptCount) ? oldToNew[p.c] : NOTOK;
        Int32 nD = isTri ? nC : ((p.d >= 0 && p.d < ptCount) ? oldToNew[p.d] : NOTOK);

        if (nA == NOTOK || nB == NOTOK || nC == NOTOK || nD == NOTOK)
            continue;

        CPolygon np(nA, nB, nC, nD);
        if (isTri)
        {
            if (np.a == np.b || np.b == np.c || np.a == np.c)
                continue;
        }
        else
        {
            if (np.a == np.b) np = CPolygon(np.a, np.c, np.d, np.d);
            else if (np.b == np.c) np = CPolygon(np.a, np.b, np.d, np.d);
            else if (np.c == np.d) np = CPolygon(np.a, np.b, np.c, np.c);
            else if (np.d == np.a) np = CPolygon(np.a, np.b, np.c, np.c);

            if (np.c == np.d)
            {
                if (np.a == np.b || np.b == np.c || np.a == np.c)
                    continue;
            }
        }

        finalPolys.Append(np) iferr_ignore("Append final poly");
    }

    mesh->ResizeObject((Int32)newPoints.GetCount(), (Int32)finalPolys.GetCount());
    Vector* ptsW = mesh->GetPointW();
    for (Int32 i = 0; i < (Int32)newPoints.GetCount(); ++i)
        ptsW[i] = newPoints[i];

    CPolygon* polysW = mesh->GetPolygonW();
    for (Int32 i = 0; i < (Int32)finalPolys.GetCount(); ++i)
        polysW[i] = finalPolys[i];

    NotifyMeshUpdated(mesh);
    return true;
}

Bool MeshBuilder::DeleteEdge(PolygonObject* mesh, Int32 v0, Int32 v1)
{
    EdgeLoopResult loop = FindEdgeLoop(mesh, v0, v1);
    if (loop.edges.GetCount() > 0)
        return DeleteEdgeLoop(mesh, loop.edges);

    maxon::BaseArray<LoopEdge> singleEdge;
    LoopEdge le;
    le.v0 = v0;
    le.v1 = v1;
    singleEdge.Append(le) iferr_ignore("Append single edge");
    return DeleteEdgeLoop(mesh, singleEdge);
}

static Bool PointInTriangle2D(const Vector& p, const Vector& a, const Vector& b, const Vector& c)
{
    auto sign = [](const Vector& p1, const Vector& p2, const Vector& p3) {
        return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
    };
    Float d1 = sign(p, a, b);
    Float d2 = sign(p, b, c);
    Float d3 = sign(p, c, a);
    Bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    Bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
    return !(hasNeg && hasPos);
}

static Bool SegmentsIntersect2D(const Vector& a, const Vector& b, const Vector& c, const Vector& d)
{
    auto ccw = [](const Vector& p1, const Vector& p2, const Vector& p3) -> Float {
        return (p2.x - p1.x) * (p3.y - p1.y) - (p2.y - p1.y) * (p3.x - p1.x);
    };
    Float cp1 = ccw(a, b, c);
    Float cp2 = ccw(a, b, d);
    Float cp3 = ccw(c, d, a);
    Float cp4 = ccw(c, d, b);
    Bool abOpposite = ((cp1 > 0.05 && cp2 < -0.05) || (cp1 < -0.05 && cp2 > 0.05));
    Bool cdOpposite = ((cp3 > 0.05 && cp4 < -0.05) || (cp3 < -0.05 && cp4 > 0.05));
    return abOpposite && cdOpposite;
}

Int32 MeshBuilder::FindPolygonUnderScreen(BaseDraw* bd, PolygonObject* mesh, Float screenX, Float screenY, PolygonObject* targetMesh, SurfaceSnapper* snapper, Float* outAvgZ)
{
    if (!bd || !mesh || mesh->GetPolygonCount() == 0)
        return NOTOK;

    Int32 polyCount = mesh->GetPolygonCount();
    const CPolygon* polys = mesh->GetPolygonR();
    const Vector* pts = mesh->GetPointR();
    Matrix mg = mesh->GetMg();

    Vector pScreen(screenX, screenY, 0.0);
    Int32 closestPoly = NOTOK;
    Float minZ = 1e30;

    Float targetZAtCursor = 1e30;
    Bool hasTargetHit = false;
    if (targetMesh && snapper && targetMesh->GetPolygonCount() > 0)
    {
        SnapResult tSnap = snapper->RaycastSurface(bd, targetMesh, screenX, screenY);
        if (tSnap.valid)
        {
            targetZAtCursor = bd->WS(tSnap.worldPos).z;
            hasTargetHit = true;
        }
    }
    Float targetTol = maxon::Max(Float(2.5), Float(targetZAtCursor * 0.005));

    Vector pNear = bd->SW(Vector(screenX, screenY, 0.0));
    Vector pFar  = bd->SW(Vector(screenX, screenY, 1000.0));
    Vector toCam = (pNear - pFar).GetNormalized();

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

    for (Int32 i = 0; i < polyCount; ++i)
    {
        const CPolygon& p = polys[i];
        Vector wa = mg * pts[p.a];
        Vector wb = mg * pts[p.b];
        Vector wc = mg * pts[p.c];

        Vector sa = bd->WS(wa);
        Vector sb = bd->WS(wb);
        Vector sc = bd->WS(wc);

        if (sa.z <= 0.0 || sb.z <= 0.0 || sc.z <= 0.0)
            continue;

        Float exactZ = 1e30;
        Bool hit = false;

        if (PointInTriangle2D(pScreen, sa, sb, sc))
        {
            hit = true;
            exactZ = calcDepth2D(pScreen, sa, sb, sc);
        }
        else if (p.c != p.d)
        {
            Vector wd = mg * pts[p.d];
            Vector sd = bd->WS(wd);
            if (sd.z > 0.0 && PointInTriangle2D(pScreen, sa, sc, sd))
            {
                hit = true;
                exactZ = calcDepth2D(pScreen, sa, sc, sd);
            }
        }

        if (hit)
        {
            if (hasTargetHit && exactZ > targetZAtCursor + targetTol)
                continue; // Behind target mesh!

            if (exactZ < minZ)
            {
                minZ = exactZ;
                closestPoly = i;
            }
        }
    }

    if (closestPoly != NOTOK && outAvgZ)
        *outAvgZ = minZ;

    return closestPoly;
}

QuadPreview MeshBuilder::FindPotentialQuad(BaseDraw* bd, PolygonObject* retopo, const Vector& targetNormal, Float screenX, Float screenY)
{
    QuadPreview result;
    result.valid = false;
    if (!bd || !retopo) return result;

    // Rule 1: Never create or preview a quad if cursor is already over an existing polygon!
    if (FindPolygonUnderScreen(bd, retopo, screenX, screenY) != NOTOK)
        return result;

    Int32 ptCount = retopo->GetPointCount();
    if (ptCount < 4) return result;

    const Vector* pts = retopo->GetPointR();
    Matrix mg = retopo->GetMg();

    struct CandidatePt
    {
        Int32 index;
        Vector worldPos;
        Vector screenPos;
        Float distSq;
    };

    maxon::BaseArray<CandidatePt> candidates;
    const Float maxSearchRadius = 350.0;
    const Float maxRadiusSq = maxSearchRadius * maxSearchRadius;

    for (Int32 i = 0; i < ptCount; ++i)
    {
        Vector wPos = mg * pts[i];
        Vector sPos = bd->WS(wPos);

        if (sPos.z <= 0.0) continue;

        Float dx = sPos.x - screenX;
        Float dy = sPos.y - screenY;
        Float distSq = dx * dx + dy * dy;

        if (distSq <= maxRadiusSq)
        {
            CandidatePt cp;
            cp.index = i;
            cp.worldPos = wPos;
            cp.screenPos = sPos;
            cp.distSq = distSq;
            candidates.Append(cp) iferr_ignore("Append candidate");
        }
    }

    if (candidates.GetCount() < 4) return result;

    std::sort(&candidates[0], &candidates[0] + candidates.GetCount(), [](const CandidatePt& a, const CandidatePt& b) {
        return a.distSq < b.distSq;
    });

    Int32 numCand = (Int32)candidates.GetCount();
    if (numCand > 16) numCand = 16;

    Int32 polyCount = retopo->GetPolygonCount();
    const CPolygon* polys = retopo->GetPolygonR();

    auto hasExistingPolygon4 = [&](Int32 a, Int32 b, Int32 c, Int32 d) -> Bool {
        for (Int32 p = 0; p < polyCount; ++p)
        {
            const CPolygon& poly = polys[p];
            Int32 matchCount = 0;
            if (poly.a == a || poly.a == b || poly.a == c || poly.a == d) matchCount++;
            if (poly.b == a || poly.b == b || poly.b == c || poly.b == d) matchCount++;
            if (poly.c == a || poly.c == b || poly.c == c || poly.c == d) matchCount++;
            if (poly.c != poly.d)
            {
                if (poly.d == a || poly.d == b || poly.d == c || poly.d == d) matchCount++;
                if (matchCount == 4) return true;
            }
            else
            {
                if (matchCount == 3) return true;
            }
        }
        return false;
    };

    auto sharesThreeOrMoreWithExisting = [&](Int32 a, Int32 b, Int32 c, Int32 d) -> Bool {
        for (Int32 p = 0; p < polyCount; ++p)
        {
            const CPolygon& poly = polys[p];
            Int32 matchCount = 0;
            if (poly.a == a || poly.a == b || poly.a == c || poly.a == d) matchCount++;
            if (poly.b == a || poly.b == b || poly.b == c || poly.b == d) matchCount++;
            if (poly.c == a || poly.c == b || poly.c == c || poly.c == d) matchCount++;
            if (poly.c != poly.d)
            {
                if (poly.d == a || poly.d == b || poly.d == c || poly.d == d) matchCount++;
                if (matchCount >= 3) return true;
            }
            else
            {
                if (matchCount >= 3) return true;
            }
        }
        return false;
    };

    auto countEdgePolys = [&](Int32 u, Int32 v) -> Int32 {
        Int32 count = 0;
        for (Int32 p = 0; p < polyCount; ++p)
        {
            if (PolygonHasEdge(polys[p], u, v))
                count++;
        }
        return count;
    };

    auto isExistingEdge = [&](Int32 u, Int32 v) -> Bool {
        return countEdgePolys(u, v) > 0;
    };

    Float bestScore = 1e30;
    QuadPreview bestQuad;

    auto cross2D = [](const Vector& a, const Vector& b, const Vector& c) -> Float {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    };

    Vector cursor(screenX, screenY, 0.0);

    for (Int32 i0 = 0; i0 < numCand - 3; ++i0)
    {
        for (Int32 i1 = i0 + 1; i1 < numCand - 2; ++i1)
        {
            for (Int32 i2 = i1 + 1; i2 < numCand - 1; ++i2)
            {
                for (Int32 i3 = i2 + 1; i3 < numCand; ++i3)
                {
                    const CandidatePt& c0 = candidates[i0];
                    const CandidatePt& c1 = candidates[i1];
                    const CandidatePt& c2 = candidates[i2];
                    const CandidatePt& c3 = candidates[i3];

                    if (hasExistingPolygon4(c0.index, c1.index, c2.index, c3.index))
                        continue;

                    if (sharesThreeOrMoreWithExisting(c0.index, c1.index, c2.index, c3.index))
                        continue;

                    CandidatePt quadPts[4] = { c0, c1, c2, c3 };
                    Vector centroid = (c0.screenPos + c1.screenPos + c2.screenPos + c3.screenPos) * 0.25;

                    // Centroid must not be inside any existing polygon!
                    if (FindPolygonUnderScreen(bd, retopo, centroid.x, centroid.y) != NOTOK)
                        continue;

                    Float angles[4];
                    for (Int32 k = 0; k < 4; ++k)
                    {
                        angles[k] = std::atan2(quadPts[k].screenPos.y - centroid.y, quadPts[k].screenPos.x - centroid.x);
                    }

                    for (Int32 a = 0; a < 3; ++a)
                    {
                        for (Int32 b = a + 1; b < 4; ++b)
                        {
                            if (angles[a] > angles[b])
                            {
                                std::swap(angles[a], angles[b]);
                                std::swap(quadPts[a], quadPts[b]);
                            }
                        }
                    }

                    Float cp0 = cross2D(quadPts[0].screenPos, quadPts[1].screenPos, quadPts[2].screenPos);
                    Float cp1 = cross2D(quadPts[1].screenPos, quadPts[2].screenPos, quadPts[3].screenPos);
                    Float cp2 = cross2D(quadPts[2].screenPos, quadPts[3].screenPos, quadPts[0].screenPos);
                    Float cp3 = cross2D(quadPts[3].screenPos, quadPts[0].screenPos, quadPts[1].screenPos);

                    Bool allPos = (cp0 > 1.0 && cp1 > 1.0 && cp2 > 1.0 && cp3 > 1.0);
                    Bool allNeg = (cp0 < -1.0 && cp1 < -1.0 && cp2 < -1.0 && cp3 < -1.0);
                    if (!allPos && !allNeg)
                        continue;

                    Float sign = allPos ? 1.0 : -1.0;
                    Float d0 = cross2D(quadPts[0].screenPos, quadPts[1].screenPos, cursor) * sign;
                    Float d1 = cross2D(quadPts[1].screenPos, quadPts[2].screenPos, cursor) * sign;
                    Float d2 = cross2D(quadPts[2].screenPos, quadPts[3].screenPos, cursor) * sign;
                    Float d3 = cross2D(quadPts[3].screenPos, quadPts[0].screenPos, cursor) * sign;

                    if (d0 < 0.0 || d1 < 0.0 || d2 < 0.0 || d3 < 0.0)
                        continue;

                    // Ensure no other retopo vertex is inside this quad
                    Bool hasOtherInside = false;
                    for (Int32 m = 0; m < ptCount; ++m)
                    {
                        if (m == quadPts[0].index || m == quadPts[1].index ||
                            m == quadPts[2].index || m == quadPts[3].index)
                            continue;

                        Vector sPt = bd->WS(mg * pts[m]);
                        if (sPt.z <= 0.0) continue;

                        Float t0 = cross2D(quadPts[0].screenPos, quadPts[1].screenPos, sPt) * sign;
                        Float t1 = cross2D(quadPts[1].screenPos, quadPts[2].screenPos, sPt) * sign;
                        Float t2 = cross2D(quadPts[2].screenPos, quadPts[3].screenPos, sPt) * sign;
                        Float t3 = cross2D(quadPts[3].screenPos, quadPts[0].screenPos, sPt) * sign;

                        if (t0 > 0.5 && t1 > 0.5 && t2 > 0.5 && t3 > 0.5)
                        {
                            hasOtherInside = true;
                            break;
                        }
                    }
                    if (hasOtherInside)
                        continue;

                    // Ensure no existing polygon centroid is inside this quad
                    Bool hasPolyCentroidInside = false;
                    for (Int32 p = 0; p < polyCount; ++p)
                    {
                        const CPolygon& poly = polys[p];
                        Vector sa = bd->WS(mg * pts[poly.a]);
                        Vector sb = bd->WS(mg * pts[poly.b]);
                        Vector sc = bd->WS(mg * pts[poly.c]);
                        Vector polyCenter = (sa + sb + sc) * (1.0 / 3.0);
                        if (poly.c != poly.d)
                        {
                            Vector sd = bd->WS(mg * pts[poly.d]);
                            polyCenter = (sa + sb + sc + sd) * 0.25;
                        }

                        Float t0 = cross2D(quadPts[0].screenPos, quadPts[1].screenPos, polyCenter) * sign;
                        Float t1 = cross2D(quadPts[1].screenPos, quadPts[2].screenPos, polyCenter) * sign;
                        Float t2 = cross2D(quadPts[2].screenPos, quadPts[3].screenPos, polyCenter) * sign;
                        Float t3 = cross2D(quadPts[3].screenPos, quadPts[0].screenPos, polyCenter) * sign;

                        if (t0 > 0.5 && t1 > 0.5 && t2 > 0.5 && t3 > 0.5)
                        {
                            hasPolyCentroidInside = true;
                            break;
                        }
                    }
                    if (hasPolyCentroidInside)
                        continue;

                    if (isExistingEdge(quadPts[0].index, quadPts[2].index) ||
                        isExistingEdge(quadPts[1].index, quadPts[3].index))
                        continue;

                    if (countEdgePolys(quadPts[0].index, quadPts[1].index) >= 2 ||
                        countEdgePolys(quadPts[1].index, quadPts[2].index) >= 2 ||
                        countEdgePolys(quadPts[2].index, quadPts[3].index) >= 2 ||
                        countEdgePolys(quadPts[3].index, quadPts[0].index) >= 2)
                        continue;

                    // Ensure none of the candidate quad edges cross any existing edge of any polygon
                    Bool edgeCrosses = false;
                    for (Int32 qe = 0; qe < 4; ++qe)
                    {
                        Int32 qA = quadPts[qe].index;
                        Int32 qB = quadPts[(qe + 1) % 4].index;
                        Vector sA = quadPts[qe].screenPos;
                        Vector sB = quadPts[(qe + 1) % 4].screenPos;

                        for (Int32 p = 0; p < polyCount; ++p)
                        {
                            const CPolygon& poly = polys[p];
                            Int32 pEdges[4][2] = { {poly.a, poly.b}, {poly.b, poly.c}, {poly.c, poly.d}, {poly.d, poly.a} };
                            Int32 numPolyEdges = (poly.c != poly.d) ? 4 : 3;
                            if (numPolyEdges == 3)
                            {
                                pEdges[2][0] = poly.c; pEdges[2][1] = poly.a;
                            }

                            for (Int32 pe = 0; pe < numPolyEdges; ++pe)
                            {
                                Int32 pU = pEdges[pe][0];
                                Int32 pV = pEdges[pe][1];

                                if (qA == pU || qA == pV || qB == pU || qB == pV)
                                    continue;

                                Vector sU = bd->WS(mg * pts[pU]);
                                Vector sV = bd->WS(mg * pts[pV]);
                                if (sU.z <= 0.0 || sV.z <= 0.0) continue;

                                if (SegmentsIntersect2D(sA, sB, sU, sV))
                                {
                                    edgeCrosses = true;
                                    break;
                                }
                            }
                            if (edgeCrosses) break;
                        }
                        if (edgeCrosses) break;
                    }
                    if (edgeCrosses)
                        continue;

                    Vector v01 = quadPts[1].worldPos - quadPts[0].worldPos;
                    Vector v02 = quadPts[2].worldPos - quadPts[0].worldPos;
                    Vector geomNormal = Cross(v01, v02).GetNormalized();

                    if (Dot(geomNormal, targetNormal) < 0.0)
                    {
                        std::swap(quadPts[1], quadPts[3]);
                        geomNormal = -geomNormal;
                    }

                    if (Dot(geomNormal, targetNormal) < 0.05)
                        continue;

                    Float perimeter = (quadPts[1].screenPos - quadPts[0].screenPos).GetLength() +
                                      (quadPts[2].screenPos - quadPts[1].screenPos).GetLength() +
                                      (quadPts[3].screenPos - quadPts[2].screenPos).GetLength() +
                                      (quadPts[0].screenPos - quadPts[3].screenPos).GetLength();
                    Float distToCentroid = (centroid - cursor).GetLength();
                    Float score = perimeter + 2.0 * distToCentroid;

                    if (score < bestScore)
                    {
                        bestScore = score;
                        bestQuad.valid = true;
                        bestQuad.v[0] = quadPts[0].index;
                        bestQuad.v[1] = quadPts[1].index;
                        bestQuad.v[2] = quadPts[2].index;
                        bestQuad.v[3] = quadPts[3].index;
                        bestQuad.worldPositions[0] = quadPts[0].worldPos;
                        bestQuad.worldPositions[1] = quadPts[1].worldPos;
                        bestQuad.worldPositions[2] = quadPts[2].worldPos;
                        bestQuad.worldPositions[3] = quadPts[3].worldPos;
                        bestQuad.screenPositions[0] = quadPts[0].screenPos;
                        bestQuad.screenPositions[1] = quadPts[1].screenPos;
                        bestQuad.screenPositions[2] = quadPts[2].screenPos;
                        bestQuad.screenPositions[3] = quadPts[3].screenPos;
                        bestQuad.normal = geomNormal;
                    }
                }
            }
        }
    }

    return bestQuad;
}

Float MeshBuilder::ComputeEdgeParam(BaseDraw* bd, PolygonObject* mesh, Int32 v0, Int32 v1, Float screenX, Float screenY)
{
    if (!bd || !mesh || v0 == v1 || v0 < 0 || v1 < 0) return 0.5;
    Int32 ptCount = mesh->GetPointCount();
    if (v0 >= ptCount || v1 >= ptCount) return 0.5;

    const Vector* pts = mesh->GetPointR();
    Matrix mg = mesh->GetMg();

    Vector wA = mg * pts[v0];
    Vector wB = mg * pts[v1];
    Vector sA = bd->WS(wA);
    Vector sB = bd->WS(wB);
    if (sA.z <= 0.0 || sB.z <= 0.0) return 0.5;

    Vector ab = sB - sA;
    Float lenSq = ab.x * ab.x + ab.y * ab.y;
    Float t = 0.5;
    if (lenSq > 0.001)
    {
        t = ((screenX - sA.x) * ab.x + (screenY - sA.y) * ab.y) / lenSq;
        if (t < 0.02) t = 0.02;
        else if (t > 0.98) t = 0.98;
    }
    return t;
}

EdgeHit MeshBuilder::FindClosestEdgeOfPolygon(BaseDraw* bd, PolygonObject* mesh, Int32 polyIdx, Float screenX, Float screenY)
{
    EdgeHit hit;
    if (!bd || !mesh || polyIdx < 0 || polyIdx >= mesh->GetPolygonCount())
        return hit;

    const CPolygon* polys = mesh->GetPolygonR();
    const Vector* pts = mesh->GetPointR();
    Matrix mg = mesh->GetMg();
    const CPolygon& p = polys[polyIdx];
    Bool isQuad = (p.c != p.d);

    Vector cursor(screenX, screenY, 0.0);
    Float bestDist = 1e30;

    auto checkEdge = [&](Int32 u, Int32 v) {
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

    checkEdge(p.a, p.b);
    checkEdge(p.b, p.c);
    if (isQuad)
    {
        checkEdge(p.c, p.d);
        checkEdge(p.d, p.a);
    }
    else
    {
        checkEdge(p.c, p.a);
    }

    return hit;
}

EdgeCutResult MeshBuilder::FindEdgeLoopCut(PolygonObject* retopo, PolygonObject* targetMesh, SurfaceSnapper& snapper, BaseDraw* bd, Int32 startV0, Int32 startV1, Float startT, Int32 hintPoly)
{
    EdgeCutResult res;
    if (!retopo || startV0 == startV1 || startV0 < 0 || startV1 < 0) return res;

    Int32 polyCount = retopo->GetPolygonCount();
    Int32 ptCount = retopo->GetPointCount();
    if (polyCount == 0 || ptCount == 0) return res;
    if (startV0 >= ptCount || startV1 >= ptCount) return res;

    const CPolygon* polys = retopo->GetPolygonR();
    const Vector* pts = retopo->GetPointR();
    Matrix mg = retopo->GetMg();

    res.primaryV0 = startV0;
    res.primaryV1 = startV1;
    res.paramT = startT;
    res.primaryPoly = hintPoly;

    Neighbor neighbor;
    if (!neighbor.Init(ptCount, polys, polyCount, nullptr))
        return res;

    auto getOrCreateCutPoint = [&](Int32 u, Int32 v, Float paramUtoV) -> Int32 {
        Int32 uMin = (u < v) ? u : v;
        Int32 vMax = (u < v) ? v : u;
        Float tNorm = (u < v) ? paramUtoV : (1.0 - paramUtoV);
        if (tNorm < 0.02) tNorm = 0.02;
        if (tNorm > 0.98) tNorm = 0.98;

        for (Int32 i = 0; i < (Int32)res.cutPoints.GetCount(); ++i)
        {
            if (res.cutPoints[i].v0 == uMin && res.cutPoints[i].v1 == vMax)
                return i;
        }

        CutPoint cp;
        cp.v0 = uMin;
        cp.v1 = vMax;
        cp.t = tNorm;

        Vector posA = mg * pts[uMin];
        Vector posB = mg * pts[vMax];
        Vector linearPos = (1.0 - tNorm) * posA + tNorm * posB;
        cp.worldPos = linearPos;

        if (targetMesh)
        {
            // Compute surface normal along edge (uMin, vMax) at parameter tNorm
            Vector edgeNormal(0.0);

            auto addFaceNormal = [&](Int32 pi) {
                if (pi == NOTOK || pi < 0 || pi >= polyCount) return;
                const CPolygon& poly = polys[pi];
                Vector pa = mg * pts[poly.a];
                Vector pb = mg * pts[poly.b];
                Vector pc = mg * pts[poly.c];
                Vector fn = Cross(pb - pa, pc - pa);
                Float fnLen = fn.GetLength();
                if (fnLen > 1e-4)
                    edgeNormal += fn / fnLen;
            };

            // Accumulate face normals of polygons sharing edge (uMin, vMax) in O(1)
            Int32 epA = NOTOK, epB = NOTOK;
            neighbor.GetEdgePolys(uMin, vMax, &epA, &epB);
            addFaceNormal(epA);
            addFaceNormal(epB);

            // If edge normal is zero, accumulate face normals of polygons sharing uMin or vMax in O(1)
            if (edgeNormal.GetLength() < 1e-4)
            {
                Int32* dadr = nullptr;
                Int32 dcnt = 0;
                neighbor.GetPointPolys(uMin, &dadr, &dcnt);
                for (Int32 i = 0; i < dcnt; ++i)
                    addFaceNormal(dadr[i]);

                neighbor.GetPointPolys(vMax, &dadr, &dcnt);
                for (Int32 i = 0; i < dcnt; ++i)
                    addFaceNormal(dadr[i]);
            }

            Float normLen = edgeNormal.GetLength();
            if (normLen > 1e-4)
            {
                edgeNormal = edgeNormal / normLen;
                Float edgeLen = (posB - posA).GetLength();
                Float searchDist = maxon::Max(Float(50.0), Float(edgeLen * 3.0));

                SnapResult sr = snapper.ProjectPointAlongNormal(targetMesh, linearPos, edgeNormal, searchDist);
                if (sr.valid)
                {
                    cp.worldPos = sr.worldPos;
                }
            }
        }

        Int32 idx = (Int32)res.cutPoints.GetCount();
        res.cutPoints.Append(cp) iferr_ignore("Append cut point");
        return idx;
    };

    // Find polygons containing edge (startV0, startV1) in O(1)
    Int32 polyA = NOTOK, polyB = NOTOK;
    neighbor.GetEdgePolys(startV0, startV1, &polyA, &polyB);

    if (polyA == NOTOK && polyB == NOTOK)
    {
        // Standalone edge: just cut this edge
        Int32 cp = getOrCreateCutPoint(startV0, startV1, startT);
        if (cp != NOTOK)
        {
            res.valid = true;
        }
        return res;
    }

    if (polyA == NOTOK)
    {
        polyA = polyB;
        polyB = NOTOK;
    }

    if (hintPoly != NOTOK)
    {
        if (polyB == hintPoly)
        {
            polyB = polyA;
            polyA = hintPoly;
        }
    }

    maxon::BaseArray<Int32> visitedPolys;

    auto traverseDirection = [&](Int32 initialPoly, Int32 initU, Int32 initV, Float initT) {
        Int32 currPoly = initialPoly;
        Int32 currU = initU;
        Int32 currV = initV;
        Float currT = initT;

        for (Int32 step = 0; step < 1000; ++step)
        {
            if (currPoly < 0 || currPoly >= polyCount) break;

            Bool alreadyVisited = false;
            for (Int32 vi = 0; vi < (Int32)visitedPolys.GetCount(); ++vi)
            {
                if (visitedPolys[vi] == currPoly) { alreadyVisited = true; break; }
            }
            if (alreadyVisited)
            {
                res.isClosed = true;
                break;
            }

            visitedPolys.Append(currPoly) iferr_ignore("Append visited");

            const CPolygon& p = polys[currPoly];
            Bool isQuad = (p.c != p.d);

            if (isQuad)
            {
                Int32 Q[4] = { p.a, p.b, p.c, p.d };
                Int32 k = -1;
                Bool forward = true;

                for (Int32 i = 0; i < 4; ++i)
                {
                    Int32 iNext = (i + 1) % 4;
                    if (Q[i] == currU && Q[iNext] == currV)
                    {
                        k = i;
                        forward = true;
                        break;
                    }
                    else if (Q[i] == currV && Q[iNext] == currU)
                    {
                        k = i;
                        forward = false;
                        break;
                    }
                }

                if (k == -1) break;

                Float tA = forward ? currT : (1.0 - currT);
                if (tA < 0.02) tA = 0.02;
                if (tA > 0.98) tA = 0.98;

                Int32 q0 = Q[k];
                Int32 q1 = Q[(k + 1) % 4];
                Int32 q2 = Q[(k + 2) % 4];
                Int32 q3 = Q[(k + 3) % 4];

                Int32 cpA = getOrCreateCutPoint(q0, q1, tA);
                Int32 cpB = getOrCreateCutPoint(q3, q2, tA);

                QuadSplitInfo qs;
                qs.polyIndex = currPoly;
                qs.q[0] = q0;
                qs.q[1] = q1;
                qs.q[2] = q2;
                qs.q[3] = q3;
                qs.cutPtA = cpA;
                qs.cutPtB = cpB;
                res.quadSplits.Append(qs) iferr_ignore("Append quad split");

                CutSegment seg;
                seg.p0 = res.cutPoints[cpA].worldPos;
                seg.p1 = res.cutPoints[cpB].worldPos;
                seg.polyIndex = currPoly;
                res.cutSegments.Append(seg) iferr_ignore("Append cut segment");

                // Next edge across the quad is (q3, q2)
                Int32 nextU = q3;
                Int32 nextV = q2;
                Float nextT = tA;

                // Find next neighbor sharing edge {nextU, nextV} in O(1)
                Int32 pFirst = NOTOK, pSecond = NOTOK;
                neighbor.GetEdgePolys(nextU, nextV, &pFirst, &pSecond);
                Int32 nextPoly = (pFirst != currPoly) ? pFirst : pSecond;

                if (nextPoly == NOTOK || nextPoly < 0 || nextPoly >= polyCount) break; // Reached boundary

                currPoly = nextPoly;
                currU = nextU;
                currV = nextV;
                currT = nextT;
            }
            else
            {
                // Triangle
                Int32 T[3] = { p.a, p.b, p.c };
                Int32 k = -1;
                Bool forward = true;
                for (Int32 i = 0; i < 3; ++i)
                {
                    Int32 iNext = (i + 1) % 3;
                    if (T[i] == currU && T[iNext] == currV)
                    {
                        k = i;
                        forward = true;
                        break;
                    }
                    else if (T[i] == currV && T[iNext] == currU)
                    {
                        k = i;
                        forward = false;
                        break;
                    }
                }

                if (k == -1) break;

                Float tA = forward ? currT : (1.0 - currT);
                Int32 t0 = T[k];
                Int32 t1 = T[(k + 1) % 3];
                Int32 t2 = T[(k + 2) % 3];

                Int32 cp = getOrCreateCutPoint(t0, t1, tA);

                TriangleSplitInfo ts;
                ts.polyIndex = currPoly;
                ts.t[0] = t0;
                ts.t[1] = t1;
                ts.t[2] = t2;
                ts.cutPt = cp;
                res.triSplits.Append(ts) iferr_ignore("Append tri split");

                CutSegment seg;
                seg.p0 = res.cutPoints[cp].worldPos;
                seg.p1 = mg * pts[t2];
                seg.polyIndex = currPoly;
                res.cutSegments.Append(seg) iferr_ignore("Append cut segment");

                break; // Stop at triangles
            }
        }
    };

    // Traverse Direction 1
    traverseDirection(polyA, startV0, startV1, startT);

    // Traverse Direction 2 (if not closed and polyB exists)
    if (!res.isClosed && polyB != NOTOK)
    {
        traverseDirection(polyB, startV1, startV0, 1.0 - startT);
    }

    if (res.quadSplits.GetCount() > 0 || res.triSplits.GetCount() > 0 || res.cutPoints.GetCount() > 0)
    {
        res.valid = true;
    }

    return res;
}

Bool MeshBuilder::ApplyEdgeLoopCut(PolygonObject* retopo, const EdgeCutResult& cutResult)
{
    if (!retopo || !cutResult.valid) return false;
    Int32 oldPtCount = retopo->GetPointCount();
    Int32 oldPolyCount = retopo->GetPolygonCount();
    if (oldPtCount == 0) return false;

    Int32 addPoints = (Int32)cutResult.cutPoints.GetCount();
    if (addPoints == 0) return false;

    Int32 newPtCount = oldPtCount + addPoints;

    // Make local copy of cutPoints with allocated vertex indices
    maxon::BaseArray<CutPoint> pointsCopy;
    pointsCopy.CopyFrom(cutResult.cutPoints) iferr_ignore("Copy cut points");

    for (Int32 i = 0; i < addPoints; ++i)
    {
        pointsCopy[i].newVertexIdx = oldPtCount + i;
    }

    const Vector* oldPts = retopo->GetPointR();
    const CPolygon* oldPolys = retopo->GetPolygonR();
    Matrix invMg = ~retopo->GetMg();

    maxon::BaseArray<Vector> allPoints;
    allPoints.Resize(newPtCount) iferr_ignore("Resize allPoints");
    for (Int32 i = 0; i < oldPtCount; ++i)
        allPoints[i] = oldPts[i];
    for (Int32 i = 0; i < addPoints; ++i)
        allPoints[oldPtCount + i] = invMg * pointsCopy[i].worldPos;

    maxon::BaseArray<Bool> isSplit;
    isSplit.Resize(oldPolyCount) iferr_ignore("Resize isSplit");
    for (Int32 i = 0; i < oldPolyCount; ++i)
        isSplit[i] = false;

    for (Int32 i = 0; i < (Int32)cutResult.quadSplits.GetCount(); ++i)
    {
        Int32 pi = cutResult.quadSplits[i].polyIndex;
        if (pi >= 0 && pi < oldPolyCount) isSplit[pi] = true;
    }
    for (Int32 i = 0; i < (Int32)cutResult.triSplits.GetCount(); ++i)
    {
        Int32 pi = cutResult.triSplits[i].polyIndex;
        if (pi >= 0 && pi < oldPolyCount) isSplit[pi] = true;
    }

    maxon::BaseArray<CPolygon> allPolys;
    // 1. Copy un-split polygons
    for (Int32 i = 0; i < oldPolyCount; ++i)
    {
        if (!isSplit[i])
            allPolys.Append(oldPolys[i]) iferr_ignore("Append poly");
    }

    // 2. Add split quads
    for (Int32 i = 0; i < (Int32)cutResult.quadSplits.GetCount(); ++i)
    {
        const QuadSplitInfo& qs = cutResult.quadSplits[i];
        Int32 vA = pointsCopy[qs.cutPtA].newVertexIdx;
        Int32 vB = pointsCopy[qs.cutPtB].newVertexIdx;

        CPolygon q1(qs.q[0], vA, vB, qs.q[3]);
        CPolygon q2(vA, qs.q[1], qs.q[2], vB);
        allPolys.Append(q1) iferr_ignore("Append quad1");
        allPolys.Append(q2) iferr_ignore("Append quad2");
    }

    // 3. Add split triangles
    for (Int32 i = 0; i < (Int32)cutResult.triSplits.GetCount(); ++i)
    {
        const TriangleSplitInfo& ts = cutResult.triSplits[i];
        Int32 vCut = pointsCopy[ts.cutPt].newVertexIdx;

        CPolygon t1(ts.t[0], vCut, ts.t[2], ts.t[2]);
        CPolygon t2(vCut, ts.t[1], ts.t[2], ts.t[2]);
        allPolys.Append(t1) iferr_ignore("Append tri1");
        allPolys.Append(t2) iferr_ignore("Append tri2");
    }

    // Resize and write back to mesh
    if (!retopo->ResizeObject((Int32)allPoints.GetCount(), (Int32)allPolys.GetCount()))
        return false;

    Vector* ptsW = retopo->GetPointW();
    for (Int32 i = 0; i < (Int32)allPoints.GetCount(); ++i)
        ptsW[i] = allPoints[i];

    CPolygon* polysW = retopo->GetPolygonW();
    for (Int32 i = 0; i < (Int32)allPolys.GetCount(); ++i)
        polysW[i] = allPolys[i];

    NotifyMeshUpdated(retopo);
    return true;
}

static Float DistToSegment2D(Float px, Float py, Float ax, Float ay, Float bx, Float by)
{
    Float abx = bx - ax;
    Float aby = by - ay;
    Float lenSq = abx * abx + aby * aby;
    if (lenSq < 1e-6)
    {
        Float dx = px - ax;
        Float dy = py - ay;
        return std::sqrt(dx * dx + dy * dy);
    }
    Float apx = px - ax;
    Float apy = py - ay;
    Float t = (apx * abx + apy * aby) / lenSq;
    t = std::max(0.0, std::min(1.0, t));
    Float qx = ax + t * abx;
    Float qy = ay + t * aby;
    Float dx = px - qx;
    Float dy = py - qy;
    return std::sqrt(dx * dx + dy * dy);
}

Bool MeshBuilder::IsCursorNearBorder(PolygonObject* retopo, BaseDraw* bd, Float screenX, Float screenY, Float brushRadius)
{
    if (!retopo || !bd)
        return false;

    Int32 ptCount = retopo->GetPointCount();
    Int32 polyCount = retopo->GetPolygonCount();
    if (ptCount < 3 || polyCount == 0)
        return false;

    const Vector* pts = retopo->GetPointR();
    const CPolygon* polys = retopo->GetPolygonR();
    Matrix mg = retopo->GetMg();

    struct EdgeEntry
    {
        Int32 u;
        Int32 v;
        Int32 count;
    };
    maxon::BaseArray<EdgeEntry> edges;

    auto addOrIncEdge = [&](Int32 a, Int32 b) {
        Int32 u = std::min(a, b);
        Int32 v = std::max(a, b);
        for (Int32 k = 0; k < (Int32)edges.GetCount(); ++k)
        {
            if (edges[k].u == u && edges[k].v == v)
            {
                edges[k].count++;
                return;
            }
        }
        EdgeEntry ee;
        ee.u = u;
        ee.v = v;
        ee.count = 1;
        edges.Append(ee) iferr_ignore("Append edge");
    };

    for (Int32 p = 0; p < polyCount; ++p)
    {
        const CPolygon& poly = polys[p];
        addOrIncEdge(poly.a, poly.b);
        addOrIncEdge(poly.b, poly.c);
        if (poly.c != poly.d)
        {
            addOrIncEdge(poly.c, poly.d);
            addOrIncEdge(poly.d, poly.a);
        }
        else
        {
            addOrIncEdge(poly.c, poly.a);
        }
    }

    Float minBorderDist = 1e30;
    Float minInteriorDist = 1e30;

    for (Int32 k = 0; k < (Int32)edges.GetCount(); ++k)
    {
        const EdgeEntry& ee = edges[k];
        if (ee.u < 0 || ee.u >= ptCount || ee.v < 0 || ee.v >= ptCount) continue;

        Vector sA = bd->WS(mg * pts[ee.u]);
        Vector sB = bd->WS(mg * pts[ee.v]);
        if (sA.z <= 0.0 || sB.z <= 0.0) continue;

        Float d = DistToSegment2D(screenX, screenY, sA.x, sA.y, sB.x, sB.y);
        if (ee.count == 1)
        {
            if (d < minBorderDist) minBorderDist = d;
        }
        else
        {
            if (d < minInteriorDist) minInteriorDist = d;
        }
    }

    if (minBorderDist > brushRadius && minInteriorDist > brushRadius)
        return false;

    if (minInteriorDist >= 1e29)
        return (minBorderDist <= brushRadius);

    if (minBorderDist <= 10.0)
        return true;

    return (minBorderDist <= minInteriorDist);
}

Bool MeshBuilder::RelaxVertices(PolygonObject* retopo, PolygonObject* target, SurfaceSnapper& snapper, BaseDraw* bd, Float screenX, Float screenY, Float brushRadius, Float strength, Bool lockBorder, Bool lockInterior)
{
    if (!retopo || !bd || brushRadius <= 0.0 || strength <= 0.0)
        return false;

    Int32 ptCount = retopo->GetPointCount();
    Int32 polyCount = retopo->GetPolygonCount();
    if (ptCount < 3 || polyCount == 0)
        return false;

    const Vector* pts = retopo->GetPointR();
    const CPolygon* polys = retopo->GetPolygonR();
    Matrix retopoMg = retopo->GetMg();
    Matrix invRetopoMg = ~retopoMg;

    // 1. Find all vertices within brush radius in screen space
    maxon::BaseArray<Int32> affectedVertices;
    maxon::BaseArray<Float> vertexWeights;

    for (Int32 i = 0; i < ptCount; ++i)
    {
        Vector worldPos = retopoMg * pts[i];
        Vector screenPos = bd->WS(worldPos);
        if (screenPos.z <= 0.0) continue; // Behind camera

        Float dx = screenPos.x - screenX;
        Float dy = screenPos.y - screenY;
        Float dist = sqrt(dx * dx + dy * dy);
        if (dist <= brushRadius)
        {
            Float t = 1.0 - (dist / brushRadius);
            Float falloff = t * t * (3.0 - 2.0 * t); // Smoothstep falloff
            affectedVertices.Append(i) iferr_ignore("Append");
            vertexWeights.Append(falloff * strength) iferr_ignore("Append");
        }
    }

    if (affectedVertices.GetCount() == 0)
        return false;

    // 2. Build vertex adjacency (neighbors) and boundary neighbors
    struct EdgeEntry
    {
        Int32 u;
        Int32 v;
        Int32 count;
    };
    maxon::BaseArray<EdgeEntry> edges;

    auto addOrIncEdge = [&](Int32 a, Int32 b) {
        Int32 u = std::min(a, b);
        Int32 v = std::max(a, b);
        for (Int32 k = 0; k < (Int32)edges.GetCount(); ++k)
        {
            if (edges[k].u == u && edges[k].v == v)
            {
                edges[k].count++;
                return;
            }
        }
        EdgeEntry ee;
        ee.u = u;
        ee.v = v;
        ee.count = 1;
        edges.Append(ee) iferr_ignore("Append edge");
    };

    for (Int32 p = 0; p < polyCount; ++p)
    {
        const CPolygon& poly = polys[p];
        addOrIncEdge(poly.a, poly.b);
        addOrIncEdge(poly.b, poly.c);
        if (poly.c != poly.d)
        {
            addOrIncEdge(poly.c, poly.d);
            addOrIncEdge(poly.d, poly.a);
        }
        else
        {
            addOrIncEdge(poly.c, poly.a);
        }
    }

    maxon::BaseArray<maxon::BaseArray<Int32>> allNeighbors;
    maxon::BaseArray<maxon::BaseArray<Int32>> boundaryNeighbors;
    allNeighbors.Resize(ptCount) iferr_ignore("Resize");
    boundaryNeighbors.Resize(ptCount) iferr_ignore("Resize");

    for (Int32 k = 0; k < (Int32)edges.GetCount(); ++k)
    {
        const EdgeEntry& ee = edges[k];
        if (ee.u >= 0 && ee.u < ptCount && ee.v >= 0 && ee.v < ptCount)
        {
            allNeighbors[ee.u].Append(ee.v) iferr_ignore("Append");
            allNeighbors[ee.v].Append(ee.u) iferr_ignore("Append");

            if (ee.count == 1) // Boundary edge
            {
                boundaryNeighbors[ee.u].Append(ee.v) iferr_ignore("Append");
                boundaryNeighbors[ee.v].Append(ee.u) iferr_ignore("Append");
            }
        }
    }

    // 3. Compute vertex normals for projection onto target
    maxon::BaseArray<Vector> pointNormals;
    pointNormals.Resize(ptCount) iferr_ignore("Resize");
    for (Int32 i = 0; i < ptCount; ++i) pointNormals[i] = Vector(0.0);

    for (Int32 p = 0; p < polyCount; ++p)
    {
        const CPolygon& poly = polys[p];
        Vector pA = retopoMg * pts[poly.a];
        Vector pB = retopoMg * pts[poly.b];
        Vector pC = retopoMg * pts[poly.c];
        Vector fn = Cross(pB - pA, pC - pA).GetNormalized();
        pointNormals[poly.a] += fn;
        pointNormals[poly.b] += fn;
        pointNormals[poly.c] += fn;
        if (poly.c != poly.d)
            pointNormals[poly.d] += fn;
    }
    for (Int32 i = 0; i < ptCount; ++i)
    {
        if (Dot(pointNormals[i], pointNormals[i]) > 1e-6)
            pointNormals[i] = pointNormals[i].GetNormalized();
        else
            pointNormals[i] = Vector(0.0, 1.0, 0.0);
    }

    // 4. Compute relaxed positions for all affected vertices
    Vector* ptsW = retopo->GetPointW();

    for (Int32 ai = 0; ai < (Int32)affectedVertices.GetCount(); ++ai)
    {
        Int32 idx = affectedVertices[ai];
        Float w = vertexWeights[ai];
        if (w <= 0.001) continue;

        const maxon::BaseArray<Int32>& bNeighbors = boundaryNeighbors[idx];
        const maxon::BaseArray<Int32>& nNeighbors = allNeighbors[idx];

        Bool isBoundary = (bNeighbors.GetCount() > 0);

        if (lockBorder && isBoundary)
            continue;

        if (lockInterior && !isBoundary)
            continue;

        Vector targetLocalPos = pts[idx];

        if (isBoundary)
        {
            if (bNeighbors.GetCount() == 2)
            {
                // Boundary vertex: smooth along boundary contour
                targetLocalPos = (pts[bNeighbors[0]] + pts[bNeighbors[1]]) * 0.5;
            }
            else
            {
                // Corner or non-manifold on boundary: keep fixed to preserve silhouette
                continue;
            }
        }
        else
        {
            if (nNeighbors.GetCount() > 0)
            {
                // Interior vertex: smooth towards centroid of all topological neighbors
                Vector sum(0.0);
                for (Int32 ni = 0; ni < (Int32)nNeighbors.GetCount(); ++ni)
                {
                    sum += pts[nNeighbors[ni]];
                }
                targetLocalPos = sum / (Float)nNeighbors.GetCount();
            }
            else
            {
                continue;
            }
        }

        Vector newLocalPos = pts[idx] + (targetLocalPos - pts[idx]) * w;
        Vector newWorldPos = retopoMg * newLocalPos;

        // 5. Project onto target mesh surface (if target mesh exists)
        if (target)
        {
            SnapResult snap = snapper.ProjectPointAlongNormal(target, newWorldPos, pointNormals[idx], 100.0);
            if (snap.valid)
            {
                newWorldPos = snap.worldPos;
            }
        }

        ptsW[idx] = invRetopoMg * newWorldPos;
    }

    NotifyMeshUpdated(retopo);
    return true;
}

void MeshBuilder::NotifyMeshUpdated(PolygonObject* mesh)
{
    if (!mesh) return;
    mesh->Message(MSG_UPDATE);
}

} // namespace cinema
