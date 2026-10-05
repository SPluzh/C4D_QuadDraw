#ifndef QUADDRAW_TOOL_H__
#define QUADDRAW_TOOL_H__

#include "c4d.h"
#include "c4d_descriptiondialog.h"
#include "surface_snapper.h"
#include "mesh_builder.h"
#include "quaddraw_tag.h"

#define PLUGIN_ID_QUADDRAW 1067828

namespace cinema
{

class QuadDrawToolData : public cinema::DescriptionToolData
{
public:
    virtual Int32 GetToolPluginId() const override { return PLUGIN_ID_QUADDRAW; }
    virtual const String GetResourceSymbol() const override { return "toolquaddraw"_s; }

    virtual Int32 GetState(BaseDocument* doc) override;
    virtual Bool GetCursorInfo(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, Float x, Float y, BaseContainer& bc) override;
    virtual Bool MouseInput(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win, const BaseContainer& msg) override;
    virtual TOOLDRAW Draw(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, BaseDrawHelp* bh, BaseThread* bt, TOOLDRAWFLAGS flags) override;
    virtual Bool KeyboardInput(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win, const BaseContainer& msg) override;
    virtual Bool InitTool(BaseDocument* doc, BaseContainer& data, BaseThread* bt) override;
    virtual void FreeTool(BaseDocument* doc, BaseContainer& data) override;
    virtual void InitDefaultSettings(BaseDocument* doc, BaseContainer& data) override;
    virtual Bool GetDDescription(const BaseDocument* doc, const BaseContainer& data, Description* description, DESCFLAGS_DESC& flags) const override;
    virtual Bool GetDEnabling(const BaseDocument* doc, const BaseContainer& data, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) const override;
    virtual Bool Message(BaseDocument* doc, BaseContainer& data, Int32 type, void* t_data) override;

private:
    PolygonObject* GetEditableMesh(BaseDocument* doc, Bool createIfNone = false);
    PolygonObject* GetTargetMesh(BaseDocument* doc, PolygonObject* retopoMesh);
    BaseTag*       EnsureQuadDrawTag(BaseDocument* doc, PolygonObject* mesh);
    PolygonObject* CreateNewRetopoMesh(BaseDocument* doc);
    PolygonObject* FindExistingRetopoMesh(BaseDocument* doc);
    Bool           DoExtrudeEdgeDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                     PolygonObject* retopo, PolygonObject* target, Int32 v0, Int32 v1,
                                     Float mx, Float my, Int32 dragButton);
    Bool           DoExtrudeEdgeLoopDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                         PolygonObject* retopo, PolygonObject* target,
                                         const maxon::BaseArray<LoopEdge>& loopEdges,
                                         Float mx, Float my, Int32 dragButton);
    Bool           DoMoveComponentLoopDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                           PolygonObject* retopo, PolygonObject* target,
                                           Float mx, Float my, Int32 dragButton);

    SurfaceSnapper m_snapper;
    MeshBuilder    m_builder;

    // Hover state
    SnapResult     m_hoverSnap;
    Float          m_cursorX = 0.0;
    Float          m_cursorY = 0.0;

    // Shift Quad Preview
    Bool           m_shiftHeld = false;
    Bool           m_ctrlHeld = false;
    QuadPreview    m_shiftQuadPreview;
    Bool           m_isRelaxDragging = false;
    Bool           m_relaxLockBorder = false;
    Bool           m_relaxLockInterior = false;
    Bool           m_isResizingBrush = false;
    Float          m_brushResizeCenterX = 0.0;
    Float          m_brushResizeCenterY = 0.0;

    // Shift hover throttling and border check caching
    Float          m_lastShiftHoverX = -1e30;
    Float          m_lastShiftHoverY = -1e30;
    Float          m_lastBorderCheckX = -1e30;
    Float          m_lastBorderCheckY = -1e30;
    Float          m_lastBorderCheckRadius = -1.0;
    Bool           m_lastBorderResult = false;

    // Ctrl Cut / Insert Edge Loop Preview (Maya style)
    EdgeCutResult  m_edgeCutPreview;
    Int32          m_cachedCutV0 = NOTOK;
    Int32          m_cachedCutV1 = NOTOK;
    Float          m_cachedCutT = -1.0;
    Int32          m_cachedCutPoly = NOTOK;

    // Delete Highlight (Ctrl+Shift / Ctrl mode)
    enum class DeleteTargetType
    {
        None = 0,
        Vertex,
        Edge,
        Polygon
    };

    struct DeleteHighlight
    {
        DeleteTargetType type = DeleteTargetType::None;
        Int32            index = NOTOK;
        Int32            edgeV0 = NOTOK;
        Int32            edgeV1 = NOTOK;
        Vector           worldPos0 = Vector(0.0);
        Vector           worldPos1 = Vector(0.0);
        maxon::BaseArray<LoopEdge> loopEdges;
        maxon::BaseArray<Int32>    loopPolygons;
        maxon::BaseArray<Int32>    loopVertices;
        Vector           polyPts[4];
        Bool             polyIsQuad = true;
        Bool             isLoop = false;

        void Reset()
        {
            type = DeleteTargetType::None;
            index = NOTOK;
            edgeV0 = NOTOK;
            edgeV1 = NOTOK;
            worldPos0 = Vector(0.0);
            worldPos1 = Vector(0.0);
            loopEdges.Reset();
            loopPolygons.Reset();
            loopVertices.Reset();
            polyIsQuad = true;
            isLoop = false;
        }
    };

    DeleteHighlight m_deleteHighlight;

    // Component Loop Highlight (Ctrl hover: Vertex Loop, Edge Loop, Polygon Loop)
    enum class ComponentLoopType
    {
        None = 0,
        Vertex,
        Edge,
        Polygon
    };

    struct ComponentLoopHighlight
    {
        ComponentLoopType type = ComponentLoopType::None;
        Int32             sourceIndex = NOTOK;
        maxon::BaseArray<Int32> vertices;
        maxon::BaseArray<LoopEdge> edges;
        maxon::BaseArray<Int32> polygons;

        void Reset()
        {
            type = ComponentLoopType::None;
            sourceIndex = NOTOK;
            vertices.Reset();
            edges.Reset();
            polygons.Reset();
        }
    };

    ComponentLoopHighlight m_componentLoop;

    // Normal Tweak Mode (Vertex, Edge, Polygon hover & drag)
    enum class TweakMode
    {
        None = 0,
        Vertex,
        Edge,
        Polygon,
        LoopExtrude,
        LoopMove
    };

    struct TweakHover
    {
        TweakMode mode = TweakMode::None;
        Int32     index = NOTOK; // Vertex index or Polygon index
        Int32     edgeV0 = NOTOK;
        Int32     edgeV1 = NOTOK;
        Vector    edgeWorld0 = Vector(0.0);
        Vector    edgeWorld1 = Vector(0.0);
        Int32     polyPts[4] = { NOTOK, NOTOK, NOTOK, NOTOK };
        Vector    polyWorld[4];
        Bool      polyIsQuad = true;

        void Reset() { *this = TweakHover(); }
    };

    TweakHover    m_hoverTweak;
    TweakMode     m_activeDragMode = TweakMode::None;
    Int32         m_dragVertexIdx = NOTOK;
    Int32         m_dragEdgeV0 = NOTOK;
    Int32         m_dragEdgeV1 = NOTOK;
    Int32         m_dragPolyIdx = NOTOK;
    Int32         m_dragPolyPts[4] = { NOTOK, NOTOK, NOTOK, NOTOK };
    Int32         m_dragPolyNumPts = 0;
    Int32         m_weldTargetIdx = NOTOK;
    Int32         m_weldTargetIdx2 = NOTOK;
    maxon::BaseArray<Int32> m_loopWeldTargets;

    // Multi-Cut state (Maya Multi-Cut Tool)
    MultiCutPoint                  m_multiCutHover;
    maxon::BaseArray<MultiCutPoint> m_multiCutPoints;
    MultiCutResult                 m_multiCutPreview;

    struct SliceDrag
    {
        Bool   active = false;
        Float  startX = 0.0;
        Float  startY = 0.0;
        Float  currX = 0.0;
        Float  currY = 0.0;
        MultiCutResult result;
    };
    SliceDrag m_sliceDrag;

    Bool CommitMultiCut(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, PolygonObject* retopo, PolygonObject* target);
};

Bool RegisterQuadDraw();

} // namespace cinema

#endif // QUADDRAW_TOOL_H__
