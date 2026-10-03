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
    virtual Bool Message(BaseDocument* doc, BaseContainer& data, Int32 type, void* t_data) override;

private:
    PolygonObject* GetEditableMesh(BaseDocument* doc, Bool createIfNone = false);
    PolygonObject* GetTargetMesh(BaseDocument* doc, PolygonObject* retopoMesh);
    BaseTag*       EnsureQuadDrawTag(BaseDocument* doc, PolygonObject* mesh);
    PolygonObject* CreateNewRetopoMesh(BaseDocument* doc);
    PolygonObject* FindExistingRetopoMesh(BaseDocument* doc);

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
        Vector           polyPts[4];
        Bool             polyIsQuad = true;
    };

    DeleteHighlight m_deleteHighlight;

    // Normal Tweak Mode (Vertex, Edge, Polygon hover & drag)
    enum class TweakMode
    {
        None = 0,
        Vertex,
        Edge,
        Polygon
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
};

Bool RegisterQuadDraw();

} // namespace cinema

#endif // QUADDRAW_TOOL_H__
