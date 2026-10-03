#ifndef MESH_BUILDER_H__
#define MESH_BUILDER_H__

#include "c4d.h"
#include "c4d_basedraw.h"
#include "c4d_baseobject.h"
#include "surface_snapper.h"

namespace cinema
{

inline Bool PolygonHasEdge(const CPolygon& p, Int32 u, Int32 v)
{
    Bool isQuad = (p.c != p.d);
    if ((p.a == u && p.b == v) || (p.a == v && p.b == u)) return true;
    if ((p.b == u && p.c == v) || (p.b == v && p.c == u)) return true;
    if (isQuad)
    {
        if ((p.c == u && p.d == v) || (p.c == v && p.d == u)) return true;
        if ((p.d == u && p.a == v) || (p.d == v && p.a == u)) return true;
    }
    else
    {
        if ((p.c == u && p.a == v) || (p.c == v && p.a == u)) return true;
    }
    return false;
}

inline Bool PolygonHasVertex(const CPolygon& p, Int32 v)
{
    if (p.a == v || p.b == v || p.c == v) return true;
    if (p.c != p.d && p.d == v) return true;
    return false;
}

struct QuadPreview
{
    Bool   valid = false;
    Int32  v[4] = { NOTOK, NOTOK, NOTOK, NOTOK };
    Vector worldPositions[4];
    Vector screenPositions[4];
    Vector normal = Vector(0.0, 1.0, 0.0);
};

struct LoopEdge
{
    Int32  v0 = NOTOK;
    Int32  v1 = NOTOK;
    Vector worldPos0 = Vector(0.0);
    Vector worldPos1 = Vector(0.0);
};

struct EdgeLoopResult
{
    maxon::BaseArray<LoopEdge> edges;
    Bool isClosed = false;
};

struct CutPoint
{
    Int32  v0 = NOTOK;          // undirected edge: v0 < v1
    Int32  v1 = NOTOK;
    Float  t = 0.5;             // parameter from v0 to v1 [0..1]
    Vector worldPos = Vector(0.0);
    Int32  newVertexIdx = NOTOK;
};

struct CutSegment
{
    Vector p0 = Vector(0.0);
    Vector p1 = Vector(0.0);
    Int32  polyIndex = NOTOK;
};

struct QuadSplitInfo
{
    Int32 polyIndex = NOTOK;
    Int32 cutPtA = NOTOK; // index into cutPoints for edge (q0, q1)
    Int32 cutPtB = NOTOK; // index into cutPoints for edge (q3, q2)
    Int32 q[4] = { NOTOK, NOTOK, NOTOK, NOTOK }; // ordered vertices such that (q0, q1) and (q3, q2) are cut
};

struct TriangleSplitInfo
{
    Int32 polyIndex = NOTOK;
    Int32 cutPt = NOTOK; // index into cutPoints
    Int32 t[3] = { NOTOK, NOTOK, NOTOK }; // edge (t0, t1) is cut
};

struct EdgeCutResult
{
    Bool                         valid = false;
    Float                        paramT = 0.5;
    Int32                        primaryV0 = NOTOK;
    Int32                        primaryV1 = NOTOK;
    Int32                        primaryPoly = NOTOK;
    maxon::BaseArray<CutPoint>   cutPoints;
    maxon::BaseArray<CutSegment> cutSegments;
    maxon::BaseArray<QuadSplitInfo> quadSplits;
    maxon::BaseArray<TriangleSplitInfo> triSplits;
    Bool                         isClosed = false;
};

class MeshBuilder
{
public:
    MeshBuilder();
    ~MeshBuilder();

    // Get existing retopo mesh or create a new one
    PolygonObject* GetOrCreateRetopoMesh(BaseDocument* doc, BaseObject* targetMesh);

    // Add a vertex to the mesh, returns the new vertex index
    Int32 AddVertex(PolygonObject* mesh, const Vector& worldPos);

    // Add a quad polygon (a, b, c, d) with correct normal direction
    Bool AddQuad(PolygonObject* mesh, Int32 a, Int32 b, Int32 c, Int32 d, const Vector& targetNormal);

    // Add a triangle polygon (a, b, c)
    Bool AddTriangle(PolygonObject* mesh, Int32 a, Int32 b, Int32 c, const Vector& targetNormal);

    // Update vertex position
    Bool SetVertexPosition(PolygonObject* mesh, Int32 index, const Vector& worldPos);

    // Weld vSource into vTarget and delete vSource
    Bool WeldVertices(PolygonObject* mesh, Int32 vSource, Int32 vTarget);

    // Delete a vertex and any polygon that uses it
    Bool DeleteVertex(PolygonObject* mesh, Int32 ptIndex);

    // Delete an edge and handle adjacent polygons (dissolve edge between quads or remove border polygon)
    Bool DeleteEdge(PolygonObject* mesh, Int32 v0, Int32 v1);

    // Trace an edge loop (strip of edges) starting from edge (startV0, startV1)
    EdgeLoopResult FindEdgeLoop(PolygonObject* mesh, Int32 startV0, Int32 startV1);

    // Delete an entire edge loop (strip of edges), cleanly dissolving quads and removing unreferenced vertices
    Bool DeleteEdgeLoop(PolygonObject* mesh, const maxon::BaseArray<LoopEdge>& loopEdges);

    // Delete a polygon by index
    Bool DeletePolygon(PolygonObject* mesh, Int32 polyIndex);

    // Find nearest front-facing polygon under screen point, optionally checking target occlusion
    Int32 FindPolygonUnderScreen(BaseDraw* bd, PolygonObject* mesh, Float screenX, Float screenY, PolygonObject* targetMesh = nullptr, SurfaceSnapper* snapper = nullptr, Float* outAvgZ = nullptr, const SnapResult* precomputedTargetSnap = nullptr);

    // Find 4 surrounding vertices that can form a valid convex quad around (screenX, screenY)
    QuadPreview FindPotentialQuad(BaseDraw* bd, PolygonObject* retopo, const Vector& targetNormal, Float screenX, Float screenY);

    // Compute interpolation factor t in [0..1] along edge (v0 -> v1) for screen position (screenX, screenY)
    Float ComputeEdgeParam(BaseDraw* bd, PolygonObject* mesh, Int32 v0, Int32 v1, Float screenX, Float screenY);

    // Find the edge of polyIndex closest to (screenX, screenY) in screen space
    EdgeHit FindClosestEdgeOfPolygon(BaseDraw* bd, PolygonObject* mesh, Int32 polyIdx, Float screenX, Float screenY);

    // Trace an edge loop cut across quad rings perpendicular to start edge
    EdgeCutResult FindEdgeLoopCut(PolygonObject* retopo, PolygonObject* targetMesh, SurfaceSnapper& snapper, BaseDraw* bd, Int32 startV0, Int32 startV1, Float startT, Int32 hintPoly = NOTOK);

    // Apply the edge loop cut to the retopo mesh, splitting quads and inserting new vertices
    Bool ApplyEdgeLoopCut(PolygonObject* retopo, const EdgeCutResult& cutResult);

    // Check if a screen position is near the outer boundary of the retopo mesh
    Bool IsCursorNearBorder(PolygonObject* retopo, BaseDraw* bd, Float screenX, Float screenY, Float brushRadius);

    // Relax vertices within brush radius in screen space (Laplacian smoothing constrained to target surface)
    Bool RelaxVertices(PolygonObject* retopo, PolygonObject* target, SurfaceSnapper& snapper, BaseDraw* bd, Float screenX, Float screenY, Float brushRadius, Float strength, Bool lockBorder = false, Bool lockInterior = false);

    // Topological edge and neighbor cache
    struct EdgeCacheEntry
    {
        Int32 u;
        Int32 v;
        Int32 count;
    };

    struct RetopoEdgeCache
    {
        maxon::BaseArray<EdgeCacheEntry>          edges;
        maxon::BaseArray<maxon::BaseArray<Int32>> allNeighbors;
        maxon::BaseArray<maxon::BaseArray<Int32>> boundaryNeighbors;
        PolygonObject*                            mesh = nullptr;
        Int32                                     ptCount = 0;
        Int32                                     polyCount = 0;
        Int32                                     dirty = -1;
        Bool                                      valid = false;
    };

    void InvalidateEdgeCache() { m_edgeCache.valid = false; }
    void EnsureEdgeCache(PolygonObject* retopo);

    // Commit changes and notify C4D
    void NotifyMeshUpdated(PolygonObject* mesh);

private:
    RetopoEdgeCache m_edgeCache;
};

} // namespace cinema

#endif // MESH_BUILDER_H__
