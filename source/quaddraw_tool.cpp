#include "quaddraw_tool.h"
#include <cmath>
#include "description/toolquaddraw.h"
#include "c4d_basecontainer.h"
#include "c4d_baseobject.h"
#include "c4d_general.h"
#include "c4d_gui.h"
#include "gui.h"

namespace cinema
{
using namespace maxon;

Int32 QuadDrawToolData::GetState(BaseDocument* doc)
{
    return CMD_ENABLED;
}

Bool QuadDrawToolData::InitTool(BaseDocument* doc, BaseContainer& data, BaseThread* bt)
{
    if (!DescriptionToolData::InitTool(doc, data, bt))
        return false;

    m_shiftHeld = false;
    m_ctrlHeld = false;
    m_shiftQuadPreview.valid = false;
    m_edgeCutPreview.valid = false;
    m_cachedCutV0 = NOTOK;
    m_cachedCutV1 = NOTOK;
    m_cachedCutT = -1.0;
    m_cachedCutPoly = NOTOK;
    m_deleteHighlight.type = DeleteTargetType::None;
    m_hoverTweak.mode = TweakMode::None;
    m_activeDragMode = TweakMode::None;
    m_dragVertexIdx = NOTOK;
    m_dragEdgeV0 = NOTOK;
    m_dragEdgeV1 = NOTOK;
    m_dragPolyIdx = NOTOK;
    m_dragPolyNumPts = 0;
    m_weldTargetIdx = NOTOK;
    m_weldTargetIdx2 = NOTOK;

    if (doc)
    {
        PolygonObject* editMesh = GetEditableMesh(doc, true);
        if (editMesh)
        {
            doc->SetActiveObject(editMesh, SELECTION_NEW);
            PolygonObject* target = GetTargetMesh(doc, editMesh);
            String targetName = target ? target->GetName() : "None (No Snapping)"_s;
            StatusSetText(FormatString("QuadDraw: Editing '@' | Target: @"_s, editMesh->GetName(), targetName));
        }
    }

    if (data.FindIndex(QUADDRAW_MESH_COLOR) == NOTOK)
    {
        InitDefaultSettings(doc, data);
    }
    else
    {
        // Ensure all settings (including newly added ones) are initialized
        if (data.FindIndex(QUADDRAW_DISABLE_CUSTOM_SHADING) == NOTOK)
            data.SetBool(QUADDRAW_DISABLE_CUSTOM_SHADING, true);
        if (data.FindIndex(QUADDRAW_DISABLE_XRAY) == NOTOK)
            data.SetBool(QUADDRAW_DISABLE_XRAY, true);
        if (data.FindIndex(QUADDRAW_FACE_OPACITY) == NOTOK)
            data.SetFloat(QUADDRAW_FACE_OPACITY, 0.35);
        if (data.FindIndex(QUADDRAW_WIRE_COLOR) == NOTOK)
            data.SetVector(QUADDRAW_WIRE_COLOR, Vector(0.0, 0.0, 0.0));
        if (data.FindIndex(QUADDRAW_LINE_WIDTH) == NOTOK)
            data.SetFloat(QUADDRAW_LINE_WIDTH, 1.0);
        else if (Abs(data.GetFloat(QUADDRAW_LINE_WIDTH) - 1.7) < 0.05)
            data.SetFloat(QUADDRAW_LINE_WIDTH, 1.0);

        if (data.FindIndex(QUADDRAW_POINT_SIZE) == NOTOK)
            data.SetFloat(QUADDRAW_POINT_SIZE, 6.0);
        else if (Abs(data.GetFloat(QUADDRAW_POINT_SIZE) - 3.0) < 0.05)
            data.SetFloat(QUADDRAW_POINT_SIZE, 6.0);

        if (data.FindIndex(QUADDRAW_PREVIEW_COLOR) == NOTOK)
            data.SetVector(QUADDRAW_PREVIEW_COLOR, Vector(0.15, 0.85, 0.45));
        if (data.FindIndex(QUADDRAW_CUT_COLOR) == NOTOK)
            data.SetVector(QUADDRAW_CUT_COLOR, Vector(0.2, 1.0, 0.4));
        if (data.FindIndex(QUADDRAW_HIGHLIGHT_COLOR) == NOTOK)
            data.SetVector(QUADDRAW_HIGHLIGHT_COLOR, Vector(1.0, 1.0, 1.0));
        else
        {
            // If it still had the old green default (0.3, 1.0, 0.4), update to white
            Vector curHl = data.GetVector(QUADDRAW_HIGHLIGHT_COLOR);
            if (Abs(curHl.x - 0.3) < 0.05 && Abs(curHl.y - 1.0) < 0.05 && Abs(curHl.z - 0.4) < 0.05)
                data.SetVector(QUADDRAW_HIGHLIGHT_COLOR, Vector(1.0, 1.0, 1.0));
        }

        if (data.FindIndex(QUADDRAW_HOVER_LINE_WIDTH) == NOTOK)
            data.SetFloat(QUADDRAW_HOVER_LINE_WIDTH, 1.4);
        else if (Abs(data.GetFloat(QUADDRAW_HOVER_LINE_WIDTH) - 2.8) < 0.05)
            data.SetFloat(QUADDRAW_HOVER_LINE_WIDTH, 1.4);

        if (data.FindIndex(QUADDRAW_ACTIVE_TOOL) == NOTOK)
            data.SetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);

        if (data.FindIndex(QUADDRAW_RELAX_MODE) == NOTOK)
            data.SetInt32(QUADDRAW_RELAX_MODE, QUADDRAW_RELAX_MODE_AUTOLOCK);
        if (data.FindIndex(QUADDRAW_RELAX_RADIUS) == NOTOK)
            data.SetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        if (data.FindIndex(QUADDRAW_RELAX_STRENGTH) == NOTOK)
            data.SetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
        if (data.FindIndex(QUADDRAW_RELAX_VISIBLE_ONLY) == NOTOK)
            data.SetBool(QUADDRAW_RELAX_VISIBLE_ONLY, true);
    }

    return true;
}

void QuadDrawToolData::InitDefaultSettings(BaseDocument* doc, BaseContainer& data)
{
    const Vector defaultFaceColor(0.0, 150.0 / 255.0, 1.0); // 0 150 255
    const Vector defaultWireColor(0.0, 0.0, 0.0);           // Black
    const Vector defaultHighlightColor(1.0, 1.0, 1.0);      // White

    data.SetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);

    data.SetBool(QUADDRAW_DISABLE_CUSTOM_SHADING, true);
    data.SetBool(QUADDRAW_DISABLE_XRAY, true);
    data.SetVector(QUADDRAW_MESH_COLOR, defaultFaceColor);
    data.SetFloat(QUADDRAW_FACE_OPACITY, 0.35); // 35% opacity
    data.SetVector(QUADDRAW_WIRE_COLOR, defaultWireColor);
    data.SetFloat(QUADDRAW_LINE_WIDTH, 1.0);
    data.SetFloat(QUADDRAW_POINT_SIZE, 6.0);

    data.SetBool(QUADDRAW_BORDER_EXTRUDE_LMB, true);
    data.SetVector(QUADDRAW_PREVIEW_COLOR, Vector(0.15, 0.85, 0.45));
    data.SetVector(QUADDRAW_CUT_COLOR, Vector(0.2, 1.0, 0.4));
    data.SetVector(QUADDRAW_HIGHLIGHT_COLOR, defaultHighlightColor);
    data.SetFloat(QUADDRAW_HOVER_LINE_WIDTH, 1.4);

    data.SetInt32(QUADDRAW_RELAX_MODE, QUADDRAW_RELAX_MODE_AUTOLOCK);
    data.SetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
    data.SetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
    data.SetBool(QUADDRAW_RELAX_VISIBLE_ONLY, true);

    DescriptionToolData::InitDefaultSettings(doc, data);
    data.SetBool(MDATA_INTERACTIVE, false);
}

void QuadDrawToolData::FreeTool(BaseDocument* doc, BaseContainer& data)
{
    DescriptionToolData::FreeTool(doc, data);

    m_shiftHeld = false;
    m_ctrlHeld = false;
    m_shiftQuadPreview.valid = false;
    m_edgeCutPreview.valid = false;
    m_deleteHighlight.type = DeleteTargetType::None;
    m_componentLoop.Reset();
    m_hoverTweak.mode = TweakMode::None;
    m_activeDragMode = TweakMode::None;
    m_dragVertexIdx = NOTOK;
    m_dragEdgeV0 = NOTOK;
    m_dragEdgeV1 = NOTOK;
    m_dragPolyIdx = NOTOK;
    m_dragPolyNumPts = 0;
    m_weldTargetIdx = NOTOK;
    m_weldTargetIdx2 = NOTOK;
    m_multiCutPoints.Reset();
    m_multiCutPreview.valid = false;
    m_multiCutPreview.cuts.Reset();
    m_multiCutPreview.previewSegments.Reset();
    m_multiCutHover = MultiCutPoint();
    m_sliceDrag.active = false;
    m_sliceDrag.result.valid = false;
    m_sliceDrag.result.cuts.Reset();
    m_sliceDrag.result.previewSegments.Reset();
}

Bool QuadDrawToolData::GetDDescription(const BaseDocument* doc, const BaseContainer& data, Description* description, DESCFLAGS_DESC& flags) const
{
    if (!description)
        return false;

    if (!DescriptionToolData::GetDDescription(doc, data, description, flags))
    {
        if (description->LoadDescription(PLUGIN_ID_QUADDRAW))
        {
            flags |= DESCFLAGS_DESC::LOADED;
        }
        else if (description->LoadDescription("toolquaddraw"_s))
        {
            flags |= DESCFLAGS_DESC::LOADED;
        }
    }

    if (flags & DESCFLAGS_DESC::LOADED)
    {
        BaseContainer* bc = description->GetParameterI(ConstDescIDLevel(MDATA_COMMANDGROUP), nullptr);
        if (bc)
            bc->SetBool(DESC_HIDE, true);
        return true;
    }

    return false;
}

Bool QuadDrawToolData::GetDEnabling(const BaseDocument* doc, const BaseContainer& data, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) const
{
    Int32 paramId = (Int32)id[0].id;
    if (paramId == QUADDRAW_MESH_COLOR || paramId == QUADDRAW_FACE_OPACITY ||
        paramId == QUADDRAW_WIRE_COLOR || paramId == QUADDRAW_LINE_WIDTH)
    {
        Bool disabled = data.GetBool(QUADDRAW_DISABLE_CUSTOM_SHADING, true);
        return !disabled;
    }
    return DescriptionToolData::GetDEnabling(doc, data, id, t_data, flags, itemdesc);
}

Bool QuadDrawToolData::Message(BaseDocument* doc, BaseContainer& data, Int32 type, void* t_data)
{
    switch (type)
    {
        case MSG_DESCRIPTION_CHECKUPDATE:
        {
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }

        case MSG_DESCRIPTION_COMMAND:
        {
            DescriptionCommand* dc = (DescriptionCommand*)t_data;
            if (dc && dc->_descId[0].id == MDATA_DEFAULTVALUES)
            {
                InitDefaultSettings(doc, data);
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
            break;
        }

        case MSG_TOOL_ASK:
        {
            ToolAskMsgData* ask = static_cast<ToolAskMsgData*>(t_data);
            if (ask)
            {
                ask->use_middlemouse = true;
                ask->resize_allowed = true;
            }
            return true;
        }

        case MSG_TOOL_RESIZE:
        {
            ToolResizeData* d = static_cast<ToolResizeData*>(t_data);
            if (!d || !d->data)
                return false;

            switch (d->pass)
            {
                case ToolResizeData::RESIZE_PASS_INIT:
                {
                    d->cross_type = true;
                    d->falloff.show = true;
                    d->falloff.size = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
                    d->falloff.opacity = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
                    d->falloff.color = Vector(1.0, 1.0, 1.0);
                    d->falloff.position.off = Vector(m_cursorX, m_cursorY, 0.0);
                    m_isResizingBrush = true;
                    m_brushResizeCenterX = m_cursorX;
                    m_brushResizeCenterY = m_cursorY;
                    return true;
                }

                case ToolResizeData::RESIZE_PASS_RESIZE:
                {
                    if (d->horizontal)
                    {
                        Float radius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
                        radius += (Float)d->delta;
                        radius = maxon::ClampValue(radius, Float(5.0), Float(500.0));
                        data.SetFloat(QUADDRAW_RELAX_RADIUS, radius);
                        d->falloff.size = radius;
                        d->cursor_text = FormatString("Radius: @ px"_s, (Int32)(radius + 0.5));
                        StatusSetText(FormatString("QuadDraw [RESIZE BRUSH] | Radius: @ px"_s, (Int32)(radius + 0.5)));
                    }
                    else
                    {
                        Float strength = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
                        strength += (Float)d->delta * 0.005;
                        strength = maxon::ClampValue(strength, Float(0.01), Float(1.0));
                        data.SetFloat(QUADDRAW_RELAX_STRENGTH, strength);
                        d->falloff.opacity = strength;
                        d->cursor_text = FormatString("Strength: @"_s, strength);
                        StatusSetText(FormatString("QuadDraw [RESIZE BRUSH] | Strength: @"_s, strength));
                    }
                    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                    return true;
                }

                case ToolResizeData::RESIZE_PASS_END:
                case ToolResizeData::RESIZE_PASS_RESET:
                {
                    m_isResizingBrush = false;
                    EventAdd();
                    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                    return true;
                }

                default:
                    break;
            }
            return true;
        }
    }

    return DescriptionToolData::Message(doc, data, type, t_data);
}

BaseTag* QuadDrawToolData::EnsureQuadDrawTag(BaseDocument* doc, PolygonObject* mesh)
{
    if (!mesh) return nullptr;
    BaseTag* tag = mesh->GetTag(PLUGIN_ID_QUADDRAW_TAG);
    if (!tag)
    {
        tag = mesh->MakeTag(PLUGIN_ID_QUADDRAW_TAG);
        if (tag && doc)
        {
            doc->AddUndo(UNDOTYPE::NEWOBJ, tag);
        }
    }
    return tag;
}

PolygonObject* QuadDrawToolData::FindExistingRetopoMesh(BaseDocument* doc)
{
    if (!doc) return nullptr;

    // 1. Search for any object that has the QuadDraw tag
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

    // 2. Search by default name "QuadDraw_Retopo"
    BaseObject* named = doc->SearchObject("QuadDraw_Retopo"_s);
    if (named && named->IsInstanceOf(Opolygon))
        return static_cast<PolygonObject*>(named);

    return nullptr;
}

PolygonObject* QuadDrawToolData::CreateNewRetopoMesh(BaseDocument* doc)
{
    if (!doc) return nullptr;

    PolygonObject* newMesh = PolygonObject::Alloc(0, 0);
    if (!newMesh) return nullptr;

    newMesh->SetName("QuadDraw_Retopo"_s);
    newMesh->SetParameter(ConstDescIDLevel(ID_BASEOBJECT_USECOLOR), GeData(ID_BASEOBJECT_USECOLOR_OFF), DESCFLAGS_SET::NONE);

    // Add Phong tag
    BaseTag* phong = BaseTag::Alloc(Tphong);
    if (phong) newMesh->InsertTag(phong);

    // Add QuadDraw tag
    BaseTag* tag = BaseTag::Alloc(PLUGIN_ID_QUADDRAW_TAG);
    if (tag) newMesh->InsertTag(tag);

    doc->InsertObject(newMesh, nullptr, nullptr);
    doc->AddUndo(UNDOTYPE::NEWOBJ, newMesh);
    doc->SetActiveObject(newMesh, SELECTION_NEW);
    return newMesh;
}

PolygonObject* QuadDrawToolData::GetEditableMesh(BaseDocument* doc, Bool createIfNone)
{
    if (!doc) return nullptr;

    // Rule 1: If an active object is selected and is a PolygonObject, it IS the editable mesh!
    BaseObject* active = doc->GetActiveObject();
    if (active && active->IsInstanceOf(Opolygon))
    {
        PolygonObject* polyObj = static_cast<PolygonObject*>(active);
        EnsureQuadDrawTag(doc, polyObj);
        return polyObj;
    }

    // Rule 2: If no active object (or not a polygon object), find an existing mesh with QuadDraw tag
    PolygonObject* existing = FindExistingRetopoMesh(doc);
    if (existing)
    {
        EnsureQuadDrawTag(doc, existing);
        return existing;
    }

    // Rule 3: If createIfNone requested, create brand new mesh
    if (createIfNone)
    {
        return CreateNewRetopoMesh(doc);
    }

    return nullptr;
}

PolygonObject* QuadDrawToolData::GetTargetMesh(BaseDocument* doc, PolygonObject* retopoMesh)
{
    if (!doc || !retopoMesh) return nullptr;

    BaseTag* tag = retopoMesh->GetTag(PLUGIN_ID_QUADDRAW_TAG);
    if (!tag) return nullptr;

    GeData d;
    if (tag->GetParameter(ConstDescIDLevel(QUADDRAW_TAG_TARGET), d, DESCFLAGS_GET::NONE))
    {
        const BaseLink* bl = d.GetBaseLink();
        if (bl)
        {
            BaseList2D* linked = bl->GetLink(doc, 0);
            if (linked && linked->IsInstanceOf(Obase))
            {
                BaseObject* targetObj = static_cast<BaseObject*>(linked);
                if (targetObj != retopoMesh)
                {
                    if (targetObj->IsInstanceOf(Opolygon))
                        return static_cast<PolygonObject*>(targetObj);
                    if (targetObj->GetDeformCache() && targetObj->GetDeformCache()->IsInstanceOf(Opolygon))
                        return static_cast<PolygonObject*>(targetObj->GetDeformCache());
                    if (targetObj->GetCache() && targetObj->GetCache()->IsInstanceOf(Opolygon))
                        return static_cast<PolygonObject*>(targetObj->GetCache());
                }
            }
        }
    }

    return nullptr;
}

Bool QuadDrawToolData::GetCursorInfo(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, Float x, Float y, BaseContainer& bc)
{
    if (bc.GetId() == BFM_CURSORINFO_REMOVE)
        return true;

    m_cursorX = x;
    m_cursorY = y;

    PolygonObject* retopo = GetEditableMesh(doc, false);
    PolygonObject* target = GetTargetMesh(doc, retopo);

    // Detect keyboard qualifiers
    BaseContainer keyState;
    if (GetInputState(BFM_INPUT_KEYBOARD, BFM_INPUT_QUALIFIER, keyState))
    {
        Int32 qual = keyState.GetInt32(BFM_INPUT_QUALIFIER);
        m_shiftHeld = (qual & QSHIFT) != 0;
        m_ctrlHeld  = (qual & QCTRL)  != 0;
    }
    else
    {
        m_shiftHeld = false;
        m_ctrlHeld  = false;
    }

    String targetName = target ? target->GetName() : "None (No Snapping)"_s;
    Int32 retopoPolys = retopo ? retopo->GetPolygonCount() : 0;
    Int32 retopoPts = retopo ? retopo->GetPointCount() : 0;
    Int32 activeTool = data.GetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);

    // =========================================================================
    // MODE 1: CTRL + SHIFT HELD (DELETE MODE - Maya style)
    // =========================================================================
    if (m_ctrlHeld && m_shiftHeld)
    {
        m_shiftQuadPreview.valid = false;
        m_deleteHighlight.type = DeleteTargetType::None;

        if (retopo)
        {
            Float polyZ = 1e30;
            Int32 underPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper, &polyZ);

            if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
            {
                // When cursor is over a polygon, only its components can be deleted!
                const CPolygon& p = retopo->GetPolygonR()[underPoly];
                const Vector* pts = retopo->GetPointR();
                Matrix rMg = retopo->GetMg();

                Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
                Int32 vertCount = (p.c != p.d) ? 4 : 3;

                // Priority 1: Vertex of underPoly
                Int32 bestPolyV = NOTOK;
                Float bestVertDist = 10.0;
                for (Int32 vi = 0; vi < vertCount; ++vi)
                {
                    Int32 vIdx = polyVerts[vi];
                    if (vIdx == NOTOK) continue;
                    Vector sPos = bd->WS(rMg * pts[vIdx]);
                    if (sPos.z <= 0.0) continue;
                    Float dx = sPos.x - x, dy = sPos.y - y;
                    Float d = std::sqrt(dx * dx + dy * dy);
                    if (d <= bestVertDist)
                    {
                        bestVertDist = d;
                        bestPolyV = vIdx;
                    }
                }

                if (bestPolyV != NOTOK)
                {
                    m_deleteHighlight.type = DeleteTargetType::Vertex;
                    m_deleteHighlight.index = bestPolyV;
                    m_deleteHighlight.worldPos0 = rMg * pts[bestPolyV];
                }
                else
                {
                    // Priority 2: Edge of underPoly
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, x, y);
                    if (polyEdge.valid && polyEdge.dist <= 8.0)
                    {
                        m_deleteHighlight.type = DeleteTargetType::Edge;
                        m_deleteHighlight.edgeV0 = polyEdge.v0;
                        m_deleteHighlight.edgeV1 = polyEdge.v1;
                        m_deleteHighlight.worldPos0 = polyEdge.worldPos0;
                        m_deleteHighlight.worldPos1 = polyEdge.worldPos1;

                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, polyEdge.v0, polyEdge.v1);
                        m_deleteHighlight.loopEdges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                    else
                    {
                        // Priority 3: underPoly itself
                        m_deleteHighlight.type = DeleteTargetType::Polygon;
                        m_deleteHighlight.index = underPoly;
                        m_deleteHighlight.polyIsQuad = (p.c != p.d);
                        m_deleteHighlight.polyPts[0] = rMg * pts[p.a];
                        m_deleteHighlight.polyPts[1] = rMg * pts[p.b];
                        m_deleteHighlight.polyPts[2] = rMg * pts[p.c];
                        m_deleteHighlight.polyPts[3] = rMg * pts[p.d];
                    }
                }
            }
            else
            {
                // Cursor in empty space outside polygons
                Int32 nearVertex = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 10.0, NOTOK, target);
                if (nearVertex != NOTOK)
                {
                    m_deleteHighlight.type = DeleteTargetType::Vertex;
                    m_deleteHighlight.index = nearVertex;
                    m_deleteHighlight.worldPos0 = retopo->GetMg() * retopo->GetPointR()[nearVertex];
                }
                else
                {
                    EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 8.0, target);
                    if (edgeHit.valid)
                    {
                        m_deleteHighlight.type = DeleteTargetType::Edge;
                        m_deleteHighlight.edgeV0 = edgeHit.v0;
                        m_deleteHighlight.edgeV1 = edgeHit.v1;
                        m_deleteHighlight.worldPos0 = edgeHit.worldPos0;
                        m_deleteHighlight.worldPos1 = edgeHit.worldPos1;

                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, edgeHit.v0, edgeHit.v1);
                        m_deleteHighlight.loopEdges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                }
            }
        }

        bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);

        String status;
        if (m_deleteHighlight.type == DeleteTargetType::Vertex)
            status = FormatString("QuadDraw [DELETE] | Click to Delete Vertex #@"_s, m_deleteHighlight.index);
        else if (m_deleteHighlight.type == DeleteTargetType::Edge)
            status = FormatString("QuadDraw [DELETE] | Click to Delete Edge Loop (@ edges)"_s, (Int32)m_deleteHighlight.loopEdges.GetCount());
        else if (m_deleteHighlight.type == DeleteTargetType::Polygon)
            status = FormatString("QuadDraw [DELETE] | Click to Delete Polygon #@"_s, m_deleteHighlight.index);
        else
            status = "QuadDraw [DELETE] | Hover over Point, Edge, or Polygon to delete (Ctrl+Shift+LMB)"_s;

        StatusSetText(status);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Clear delete highlight when not in Ctrl+Shift mode
    m_deleteHighlight.type = DeleteTargetType::None;
    m_deleteHighlight.loopEdges.Reset();

    // =========================================================================
    // MODE 2: CTRL HELD ALONE (COMPONENT LOOP HIGHLIGHT - Vertex, Edge, Polygon Loop)
    // =========================================================================
    if (m_ctrlHeld && !m_shiftHeld && activeTool != QUADDRAW_TOOL_MULTICUT && activeTool != QUADDRAW_TOOL_KNIFE)
    {
        m_shiftQuadPreview.valid = false;
        m_edgeCutPreview.valid = false;
        m_componentLoop.Reset();

        if (retopo && retopo->GetPointCount() > 0)
        {
            Float polyZ = 1e30;
            Int32 underPoly = (retopo->GetPolygonCount() > 0)
                ? m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper, &polyZ) : NOTOK;

            if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
            {
                // When cursor is over a polygon:
                const CPolygon& p = retopo->GetPolygonR()[underPoly];
                const Vector* pts = retopo->GetPointR();
                Matrix rMg = retopo->GetMg();

                Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
                Int32 vertCount = (p.c != p.d) ? 4 : 3;

                // Priority 1: Vertex of underPoly (threshold 10 px)
                Int32 bestPolyV = NOTOK;
                Float bestVertDist = 10.0;
                for (Int32 vi = 0; vi < vertCount; ++vi)
                {
                    Int32 vIdx = polyVerts[vi];
                    if (vIdx == NOTOK) continue;
                    Vector sPos = bd->WS(rMg * pts[vIdx]);
                    if (sPos.z <= 0.0) continue;
                    Float dx = sPos.x - x, dy = sPos.y - y;
                    Float d = std::sqrt(dx * dx + dy * dy);
                    if (d <= bestVertDist)
                    {
                        bestVertDist = d;
                        bestPolyV = vIdx;
                    }
                }

                if (bestPolyV != NOTOK)
                {
                    // Vertex loop
                    m_componentLoop.type = ComponentLoopType::Vertex;
                    m_componentLoop.sourceIndex = bestPolyV;
                    m_componentLoop.vertices = m_builder.FindVertexLoop(bd, retopo, bestPolyV, x, y, &m_componentLoop.edges);
                }
                else
                {
                    // Priority 2: Edge of underPoly (threshold 10 px)
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, x, y);
                    if (polyEdge.valid && polyEdge.dist <= 10.0)
                    {
                        // Edge loop
                        m_componentLoop.type = ComponentLoopType::Edge;
                        m_componentLoop.sourceIndex = polyEdge.v0;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, polyEdge.v0, polyEdge.v1);
                        m_componentLoop.edges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                    else
                    {
                        // Priority 3: underPoly face loop (Polygon loop)
                        EdgeHit pe = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, x, y);
                        Int32 enterV0 = pe.valid ? pe.v0 : p.a;
                        Int32 enterV1 = pe.valid ? pe.v1 : p.b;

                        m_componentLoop.type = ComponentLoopType::Polygon;
                        m_componentLoop.sourceIndex = underPoly;
                        m_componentLoop.polygons = m_builder.FindPolygonLoop(retopo, underPoly, enterV0, enterV1);
                    }
                }
            }
            else
            {
                // Cursor outside polygons (near mesh boundary)
                Int32 nearVertex = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 10.0, NOTOK, target);
                if (nearVertex != NOTOK)
                {
                    m_componentLoop.type = ComponentLoopType::Vertex;
                    m_componentLoop.sourceIndex = nearVertex;
                    m_componentLoop.vertices = m_builder.FindVertexLoop(bd, retopo, nearVertex, x, y, &m_componentLoop.edges);
                }
                else
                {
                    EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 10.0, target);
                    if (edgeHit.valid)
                    {
                        m_componentLoop.type = ComponentLoopType::Edge;
                        m_componentLoop.sourceIndex = edgeHit.v0;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, edgeHit.v0, edgeHit.v1);
                        m_componentLoop.edges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                }
            }
        }

        bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);

        String status;
        if (m_componentLoop.type == ComponentLoopType::Vertex)
            status = FormatString("QuadDraw [LOOP] | Vertex Loop (@ vertices) | Ctrl+LMB: Select / Drag: Move Loop"_s, (Int32)m_componentLoop.vertices.GetCount());
        else if (m_componentLoop.type == ComponentLoopType::Edge)
            status = FormatString("QuadDraw [LOOP] | Edge Loop (@ edges) | Ctrl+LMB: Select / Drag: Extrude or Move Loop"_s, (Int32)m_componentLoop.edges.GetCount());
        else if (m_componentLoop.type == ComponentLoopType::Polygon)
            status = FormatString("QuadDraw [LOOP] | Polygon Loop (@ polygons) | Ctrl+LMB: Select / Drag: Move Loop"_s, (Int32)m_componentLoop.polygons.GetCount());
        else
            status = "QuadDraw [LOOP] | Hover over Point, Edge, or Polygon to highlight loop (Ctrl+LMB to select / drag)"_s;

        StatusSetText(status);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Clear component loop highlight when not holding Ctrl
    m_componentLoop.Reset();

    // =========================================================================
    // =========================================================================
    // MODE 3: SHIFT HELD (QUAD CREATION PREVIEW OR MAYA RELAX BRUSH)
    // =========================================================================
    if (m_shiftHeld)
    {
        Float brushRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float brushStrength = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);

        // Check quad creation preview only if retopo has at least 4 vertices and not in Move mode
        if (activeTool != QUADDRAW_TOOL_MOVE && retopo && retopo->GetPointCount() >= 4)
        {
            Vector viewNormal = bd ? -bd->GetMg().sqmat.v3 : Vector(0.0, 1.0, 0.0);
            m_shiftQuadPreview = m_builder.FindPotentialQuad(bd, retopo, viewNormal, x, y, target, &m_snapper);
        }
        else
        {
            m_shiftQuadPreview.valid = false;
        }

        if (m_shiftQuadPreview.valid)
        {
            bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);
            StatusSetText(FormatString("QuadDraw | Shift+LMB: Create Quad! (Vertices: @, @, @, @) | Shift+MMB Drag: Adjust Brush (@ px, @) | Target: @"_s,
                m_shiftQuadPreview.v[0], m_shiftQuadPreview.v[1], m_shiftQuadPreview.v[2], m_shiftQuadPreview.v[3], (Int32)(brushRadius + 0.5), brushStrength, targetName));
        }
        else
        {
            // Maya-style Relax brush (fast, responsive hover exactly like C4D_RelaxTool)
            bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
            StatusSetText(FormatString("QuadDraw [RELAX] | Shift+LMB Drag: Relax Brush (@ px, @) | Shift+MMB Drag: Adjust Radius & Strength | Target: @"_s,
                (Int32)(brushRadius + 0.5), brushStrength, targetName));
        }

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // =========================================================================
    // MODE 4A: MULTI-CUT TOOL MODE (ACTIVE TOOL == MULTI-CUT - Maya Multi-Cut)
    // =========================================================================
    if (activeTool == QUADDRAW_TOOL_MULTICUT)
    {
        m_shiftQuadPreview.valid = false;
        m_hoverTweak.mode = TweakMode::None;
        m_componentLoop.Reset();

        if (m_ctrlHeld && retopo && retopo->GetPolygonCount() > 0)
        {
            // Maya Multi-Cut + Ctrl: Edge Loop Cut Preview
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper);
            if (nearPoly != NOTOK)
            {
                EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, nearPoly, x, y);
                if (polyEdge.valid)
                {
                    hitV0 = polyEdge.v0;
                    hitV1 = polyEdge.v1;
                    hitT  = polyEdge.t;
                    hitPoly = nearPoly;
                }
            }
            else
            {
                EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 12.0, target);
                if (edgeHit.valid)
                {
                    hitV0 = edgeHit.v0;
                    hitV1 = edgeHit.v1;
                    hitT  = edgeHit.t;
                    hitPoly = edgeHit.polyIndex;
                }
            }

            if (hitV0 != NOTOK && hitV1 != NOTOK)
            {
                if (std::abs(hitT - 0.5) < 0.05) hitT = 0.5;
                Bool sameEdge = (hitV0 == m_cachedCutV0 && hitV1 == m_cachedCutV1 && hitPoly == m_cachedCutPoly);
                Bool sameT = sameEdge && (std::abs(hitT - m_cachedCutT) < 0.005);
                if (!sameT || !m_edgeCutPreview.valid)
                {
                    m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, hitV0, hitV1, hitT, hitPoly);
                    m_cachedCutV0 = hitV0;
                    m_cachedCutV1 = hitV1;
                    m_cachedCutT = hitT;
                    m_cachedCutPoly = hitPoly;
                }
            }
            else
            {
                m_edgeCutPreview.valid = false;
                m_edgeCutPreview.cutPoints.Reset();
                m_edgeCutPreview.cutSegments.Reset();
                m_edgeCutPreview.quadSplits.Reset();
                m_edgeCutPreview.triSplits.Reset();
                m_cachedCutV0 = NOTOK;
                m_cachedCutV1 = NOTOK;
                m_cachedCutT = -1.0;
                m_cachedCutPoly = NOTOK;
            }

            m_multiCutHover = MultiCutPoint();
            bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);
            if (m_edgeCutPreview.valid)
            {
                Int32 pct = (Int32)(m_edgeCutPreview.paramT * 100.0 + 0.5);
                Int32 count = (Int32)m_edgeCutPreview.quadSplits.GetCount();
                StatusSetText(FormatString("QuadDraw [LOOP CUT (Ctrl)] | LMB: Insert Edge Loop (@% across @ quads) | Drag to Slide | Esc to Cancel"_s, pct, count));
            }
            else
            {
                StatusSetText("QuadDraw [LOOP CUT (Ctrl)] | Hover over Edge/Quad to Insert Edge Loop"_s);
            }
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }

        m_edgeCutPreview.valid = false;
        m_edgeCutPreview.cutPoints.Reset();
        m_edgeCutPreview.cutSegments.Reset();
        m_edgeCutPreview.quadSplits.Reset();
        m_edgeCutPreview.triSplits.Reset();
        m_cachedCutV0 = NOTOK;
        m_cachedCutV1 = NOTOK;
        m_cachedCutT = -1.0;
        m_cachedCutPoly = NOTOK;

        // Normal Multi-Cut Hover: Snapping to Vertex or Edge
        m_multiCutHover.type = MultiCutSnapType::None;
        m_multiCutHover.vertexIdx = NOTOK;
        m_multiCutHover.edgeV0 = NOTOK;
        m_multiCutHover.edgeV1 = NOTOK;
        m_multiCutHover.polyIndex = NOTOK;

        if (retopo && retopo->GetPointCount() > 0)
        {
            Matrix rMg = retopo->GetMg();
            const Vector* pts = retopo->GetPointR();
            Int32 ptCount = retopo->GetPointCount();

            // 1. Check Vertex Snap (12px)
            Int32 nearV = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 12.0, NOTOK, target);
            if (nearV != NOTOK && nearV < ptCount)
            {
                m_multiCutHover.type = MultiCutSnapType::Vertex;
                m_multiCutHover.vertexIdx = nearV;
                m_multiCutHover.worldPos = rMg * pts[nearV];
            }
            else
            {
                // 2. Check Edge Snap
                Int32 hitV0 = NOTOK, hitV1 = NOTOK, hitPoly = NOTOK;
                Float hitT = 0.5;

                Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper);
                if (nearPoly != NOTOK)
                {
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, nearPoly, x, y);
                    if (polyEdge.valid)
                    {
                        hitV0 = polyEdge.v0;
                        hitV1 = polyEdge.v1;
                        hitT  = polyEdge.t;
                        hitPoly = nearPoly;
                    }
                }
                else
                {
                    EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 14.0, target);
                    if (edgeHit.valid)
                    {
                        hitV0 = edgeHit.v0;
                        hitV1 = edgeHit.v1;
                        hitT  = edgeHit.t;
                        hitPoly = edgeHit.polyIndex;
                    }
                }

                if (hitV0 != NOTOK && hitV1 != NOTOK)
                {
                    if (m_shiftHeld)
                    {
                        Float steps[] = { 0.0, 0.1, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.7, 0.75, 0.8, 0.9, 1.0 };
                        Float bestDist = 1e30;
                        Float bestS = hitT;
                        for (Float s : steps)
                        {
                            Float d = std::abs(hitT - s);
                            if (d < bestDist) { bestDist = d; bestS = s; }
                        }
                        hitT = bestS;
                    }
                    else
                    {
                        if (std::abs(hitT - 0.5) < 0.04) hitT = 0.5;
                    }

                    Vector w0 = rMg * pts[hitV0];
                    Vector w1 = rMg * pts[hitV1];
                    Vector midPos = (1.0 - hitT) * w0 + hitT * w1;
                    if (target)
                    {
                        Vector approxN = Cross(w1 - w0, Vector(0.0, 1.0, 0.0)).GetNormalized();
                        if (Dot(approxN, approxN) < 0.01) approxN = Vector(0.0, 1.0, 0.0);
                        SnapResult snap = m_snapper.ProjectPointAlongNormal(target, midPos, approxN, 50.0);
                        if (snap.valid) midPos = snap.worldPos;
                    }

                    m_multiCutHover.type = MultiCutSnapType::Edge;
                    m_multiCutHover.edgeV0 = hitV0;
                    m_multiCutHover.edgeV1 = hitV1;
                    m_multiCutHover.edgeT = hitT;
                    m_multiCutHover.polyIndex = hitPoly;
                    m_multiCutHover.worldPos = midPos;
                }
            }
        }

        // Update preview if points are placed
        if (m_multiCutPoints.GetCount() > 0)
        {
            m_multiCutPreview = m_builder.BuildMultiCutFromPoints(bd, retopo, target, m_snapper, m_multiCutPoints, &m_multiCutHover);
            Int32 ptCount = (Int32)m_multiCutPoints.GetCount();
            StatusSetText(FormatString("QuadDraw [MULTI-CUT] | Point @ placed | LMB: Place Next Point | Shift: Snap 50%/25% | Enter/RMB: Commit Cut | Backspace: Undo | Esc: Cancel"_s, ptCount));
        }
        else
        {
            m_multiCutPreview.valid = false;
            m_multiCutPreview.cuts.Reset();
            m_multiCutPreview.previewSegments.Reset();

            if (m_multiCutHover.type == MultiCutSnapType::Edge)
            {
                Int32 pct = (Int32)(m_multiCutHover.edgeT * 100.0 + 0.5);
                StatusSetText(FormatString("QuadDraw [MULTI-CUT] | LMB: Start Cut on Edge (@%) | Shift: Snap 50%/25% | Drag: Slice Cut across faces"_s, pct));
            }
            else if (m_multiCutHover.type == MultiCutSnapType::Vertex)
            {
                StatusSetText("QuadDraw [MULTI-CUT] | LMB: Start Cut on Vertex | Drag: Slice Cut across faces | Shift: Snap"_s);
            }
            else
            {
                StatusSetText("QuadDraw [MULTI-CUT] | LMB: Click Edge or Vertex to start Cut | Drag: Slice Cut across faces | Ctrl: Edge Loop"_s);
            }
        }

        bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    m_multiCutHover = MultiCutPoint();
    m_multiCutPoints.Reset();
    m_multiCutPreview.valid = false;
    m_multiCutPreview.cuts.Reset();
    m_multiCutPreview.previewSegments.Reset();

    // =========================================================================
    // MODE 4B: KNIFE TOOL MODE (ACTIVE TOOL == KNIFE)
    // =========================================================================
    if (activeTool == QUADDRAW_TOOL_KNIFE)
    {
        m_shiftQuadPreview.valid = false;
        m_hoverTweak.mode = TweakMode::None;

        if (retopo && retopo->GetPolygonCount() > 0)
        {
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper);
            if (nearPoly != NOTOK)
            {
                EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, nearPoly, x, y);
                if (polyEdge.valid)
                {
                    hitV0 = polyEdge.v0;
                    hitV1 = polyEdge.v1;
                    hitT  = polyEdge.t;
                    hitPoly = nearPoly;
                }
            }
            else
            {
                EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 12.0, target);
                if (edgeHit.valid)
                {
                    hitV0 = edgeHit.v0;
                    hitV1 = edgeHit.v1;
                    hitT  = edgeHit.t;
                    hitPoly = edgeHit.polyIndex;
                }
            }

            if (hitV0 != NOTOK && hitV1 != NOTOK)
            {
                if (std::abs(hitT - 0.5) < 0.05)
                    hitT = 0.5;

                Bool sameEdge = (hitV0 == m_cachedCutV0 && hitV1 == m_cachedCutV1 && hitPoly == m_cachedCutPoly);
                Bool sameT = sameEdge && (std::abs(hitT - m_cachedCutT) < 0.005);

                if (!sameT || !m_edgeCutPreview.valid)
                {
                    m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, hitV0, hitV1, hitT, hitPoly);
                    m_cachedCutV0 = hitV0;
                    m_cachedCutV1 = hitV1;
                    m_cachedCutT = hitT;
                    m_cachedCutPoly = hitPoly;
                }
            }
            else
            {
                m_edgeCutPreview.valid = false;
                m_cachedCutV0 = NOTOK;
                m_cachedCutV1 = NOTOK;
                m_cachedCutT = -1.0;
                m_cachedCutPoly = NOTOK;
            }
        }
        else
        {
            m_edgeCutPreview.valid = false;
            m_cachedCutV0 = NOTOK;
            m_cachedCutV1 = NOTOK;
            m_cachedCutT = -1.0;
            m_cachedCutPoly = NOTOK;
        }

        bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);

        if (m_edgeCutPreview.valid)
        {
            Int32 pct = (Int32)(m_edgeCutPreview.paramT * 100.0 + 0.5);
            Int32 count = (Int32)m_edgeCutPreview.quadSplits.GetCount();
            StatusSetText(FormatString("QuadDraw [KNIFE] | LMB: Insert Edge Loop (@% across @ quads) | Drag to Slide | Esc to Cancel"_s, pct, count));
        }
        else
        {
            StatusSetText("QuadDraw [KNIFE] | Hover over Edge or Quad to Insert Edge Loop (LMB) | Drag to Slide"_s);
        }

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Clear cut preview when not in Knife mode
    m_edgeCutPreview.valid = false;
    m_edgeCutPreview.cutPoints.Reset();
    m_edgeCutPreview.cutSegments.Reset();
    m_edgeCutPreview.quadSplits.Reset();
    m_edgeCutPreview.triSplits.Reset();
    m_cachedCutV0 = NOTOK;
    m_cachedCutV1 = NOTOK;
    m_cachedCutT = -1.0;
    m_cachedCutPoly = NOTOK;

    // =========================================================================
    // MODE 4C: NORMAL QUADDRAW (DOT PLACEMENT / TWEAK DRAG: VERTEX, EDGE, POLYGON)
    // =========================================================================
    m_shiftQuadPreview.valid = false;
    m_hoverTweak.mode = TweakMode::None;

    if (retopo && retopo->GetPointCount() > 0)
    {
        Float polyZ = 1e30;
        Int32 underPoly = (retopo->GetPolygonCount() > 0)
            ? m_builder.FindPolygonUnderScreen(bd, retopo, x, y, target, &m_snapper, &polyZ) : NOTOK;

        if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
        {
            // -------------------------------------------------------------
            // SITUATION A: CURSOR IS OVER A POLYGON
            // -------------------------------------------------------------
            // Background components BEHIND underPoly are completely occluded!
            const CPolygon& p = retopo->GetPolygonR()[underPoly];
            const Vector* pts = retopo->GetPointR();
            Matrix rMg = retopo->GetMg();

            Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
            Int32 vertCount = (p.c != p.d) ? 4 : 3;

            // 1. Check vertices of underPoly
            Int32 bestPolyV = NOTOK;
            Float bestVertDist = 10.0; // 10 px radius
            for (Int32 vi = 0; vi < vertCount; ++vi)
            {
                Int32 vIdx = polyVerts[vi];
                if (vIdx == NOTOK) continue;
                Vector sPos = bd->WS(rMg * pts[vIdx]);
                if (sPos.z <= 0.0) continue;
                Float dx = sPos.x - x, dy = sPos.y - y;
                Float d = std::sqrt(dx * dx + dy * dy);
                if (d <= bestVertDist)
                {
                    bestVertDist = d;
                    bestPolyV = vIdx;
                }
            }

            if (bestPolyV != NOTOK)
            {
                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = bestPolyV;
                m_hoverSnap.valid = true;
                m_hoverSnap.mode = SnapMode::RetopoVertex;
                m_hoverSnap.retopoVertexIndex = bestPolyV;

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s : "QuadDraw | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s,
                    bestPolyV, targetName));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }

            // 2. Check edges of underPoly
            EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, x, y);
            if (polyEdge.valid && polyEdge.dist <= 8.0)
            {
                m_hoverTweak.mode = TweakMode::Edge;
                m_hoverTweak.edgeV0 = polyEdge.v0;
                m_hoverTweak.edgeV1 = polyEdge.v1;
                m_hoverTweak.edgeWorld0 = polyEdge.worldPos0;
                m_hoverTweak.edgeWorld1 = polyEdge.worldPos1;

                Bool extrudeLMB = (activeTool != QUADDRAW_TOOL_MOVE) && data.GetBool(QUADDRAW_BORDER_EXTRUDE_LMB, true);
                Int32 edgePolyCount = 0;
                Int32 pCount = retopo->GetPolygonCount();
                const CPolygon* rPolys = retopo->GetPolygonR();
                for (Int32 i = 0; i < pCount; ++i)
                {
                    if (PolygonHasEdge(rPolys[i], polyEdge.v0, polyEdge.v1))
                        edgePolyCount++;
                }

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                if (extrudeLMB && edgePolyCount < 2)
                {
                    StatusSetText(FormatString("QuadDraw | LMB Drag: Extrude Edge (#@ - #@) | Target: @"_s,
                        polyEdge.v0, polyEdge.v1, targetName));
                }
                else if (edgePolyCount >= 2)
                {
                    StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Interior Edge (#@ - #@) | Target: @"_s : "QuadDraw | LMB Drag: Move Interior Edge (#@ - #@) | Target: @"_s,
                        polyEdge.v0, polyEdge.v1, targetName));
                }
                else
                {
                    StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Edge (#@ - #@) | Target: @"_s : "QuadDraw | LMB Drag: Move Edge (#@ - #@) | Target: @"_s,
                        polyEdge.v0, polyEdge.v1, targetName));
                }
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }

            // 3. Hover over the polygon itself!
            m_hoverTweak.mode = TweakMode::Polygon;
            m_hoverTweak.index = underPoly;
            m_hoverTweak.polyIsQuad = (p.c != p.d);
            m_hoverTweak.polyPts[0] = p.a;
            m_hoverTweak.polyPts[1] = p.b;
            m_hoverTweak.polyPts[2] = p.c;
            m_hoverTweak.polyPts[3] = p.d;
            m_hoverTweak.polyWorld[0] = rMg * pts[p.a];
            m_hoverTweak.polyWorld[1] = rMg * pts[p.b];
            m_hoverTweak.polyWorld[2] = rMg * pts[p.c];
            m_hoverTweak.polyWorld[3] = rMg * pts[p.d];

            bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
            StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Polygon #@ | Target: @"_s : "QuadDraw | LMB Drag: Move Polygon #@ | Target: @"_s,
                underPoly, targetName));
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }
        else
        {
            // -------------------------------------------------------------
            // SITUATION B: CURSOR IN EMPTY SPACE (OUTSIDE POLYGONS)
            // -------------------------------------------------------------
            // 1. Check Vertex hover in empty space (boundary vertices, isolated dots)
            Int32 nearV = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 10.0, NOTOK, target);
            if (nearV != NOTOK)
            {
                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = nearV;
                m_hoverSnap.valid = true;
                m_hoverSnap.mode = SnapMode::RetopoVertex;
                m_hoverSnap.retopoVertexIndex = nearV;

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s : "QuadDraw | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s,
                    nearV, targetName));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }

            // 2. Check Edge hover in empty space (boundary edges)
            EdgeHit nearEdge = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 8.0, target);
            if (nearEdge.valid)
            {
                m_hoverTweak.mode = TweakMode::Edge;
                m_hoverTweak.edgeV0 = nearEdge.v0;
                m_hoverTweak.edgeV1 = nearEdge.v1;
                m_hoverTweak.edgeWorld0 = nearEdge.worldPos0;
                m_hoverTweak.edgeWorld1 = nearEdge.worldPos1;

                Bool extrudeLMB = (activeTool != QUADDRAW_TOOL_MOVE) && data.GetBool(QUADDRAW_BORDER_EXTRUDE_LMB, true);
                Int32 edgePolyCount = 0;
                Int32 pCount = retopo->GetPolygonCount();
                const CPolygon* rPolys = retopo->GetPolygonR();
                for (Int32 i = 0; i < pCount; ++i)
                {
                    if (PolygonHasEdge(rPolys[i], nearEdge.v0, nearEdge.v1))
                        edgePolyCount++;
                }

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                if (extrudeLMB && edgePolyCount < 2)
                {
                    StatusSetText(FormatString("QuadDraw | LMB Drag: Extrude Edge (#@ - #@) | Target: @"_s,
                        nearEdge.v0, nearEdge.v1, targetName));
                }
                else if (edgePolyCount >= 2)
                {
                    StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Interior Edge (#@ - #@) | Target: @"_s : "QuadDraw | LMB Drag: Move Interior Edge (#@ - #@) | Target: @"_s,
                        nearEdge.v0, nearEdge.v1, targetName));
                }
                else
                {
                    StatusSetText(FormatString(activeTool == QUADDRAW_TOOL_MOVE ? "QuadDraw [MOVE] | LMB Drag: Move Edge (#@ - #@) | Target: @"_s : "QuadDraw | LMB Drag: Move Edge (#@ - #@) | Target: @"_s,
                        nearEdge.v0, nearEdge.v1, targetName));
                }
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }
    }

    // 4. Empty surface hover -> ready to place point (only if not Move tool)
    if (activeTool == QUADDRAW_TOOL_MOVE)
    {
        m_hoverSnap.valid = false;
        bc.SetInt32(RESULT_CURSOR, MOUSE_NORMAL);
        StatusSetText(FormatString("QuadDraw [MOVE] | Drag Vertices, Edges, or Polygons to Move / Tweak | Mesh: @ | Target: @"_s,
            retopo ? retopo->GetName() : "None"_s, targetName));
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }
    if (target)
    {
        m_hoverSnap = m_snapper.RaycastSurface(bd, target, x, y);
    }
    else if (bd && retopo)
    {
        Vector refPt = retopo->GetMg().off;
        if (retopo->GetPointCount() > 0)
            refPt = retopo->GetMg() * retopo->GetPointR()[0];
        m_hoverSnap.worldPos = bd->SW_Reference(x, y, refPt);
        m_hoverSnap.normal = -bd->GetMg().sqmat.v3;
        m_hoverSnap.valid = true;
        m_hoverSnap.mode = SnapMode::None;
    }
    else
    {
        m_hoverSnap.valid = false;
    }

    if (m_hoverSnap.valid)
    {
        bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);
        StatusSetText(FormatString("QuadDraw | LMB: Place Point | Shift+Hover: Preview Quad | Mesh: @ | Target: @"_s,
            retopo ? retopo->GetName() : "None"_s, targetName));
    }
    else
    {
        bc.SetInt32(RESULT_CURSOR, MOUSE_NORMAL);
        StatusSetText(FormatString("QuadDraw | Mesh: @ | Target: @ | @ pts, @ polys"_s,
            retopo ? retopo->GetName() : "None"_s, targetName, retopoPts, retopoPolys));
    }

    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    return true;
}

Bool QuadDrawToolData::DoExtrudeEdgeDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                        PolygonObject* retopo, PolygonObject* target, Int32 v0, Int32 v1,
                                        Float mx, Float my, Int32 dragButton)
{
    if (!doc || !bd || !win || !retopo) return false;

    Int32 polyCount = retopo->GetPolygonCount();
    const CPolygon* oldPolys = retopo->GetPolygonR();

    auto countPolysForEdge = [&](Int32 u, Int32 v) -> Int32 {
        Int32 cnt = 0;
        for (Int32 i = 0; i < polyCount; ++i)
        {
            if (PolygonHasEdge(oldPolys[i], u, v)) cnt++;
        }
        return cnt;
    };

    Int32 edgePolyCount = countPolysForEdge(v0, v1);
    if (edgePolyCount >= 2)
    {
        StatusSetText("QuadDraw: Cannot extrude interior edge (only border edges can be extruded)."_s);
        return false;
    }

    Int32 adjPolyIdx = NOTOK;
    for (Int32 i = 0; i < polyCount; ++i)
    {
        if (PolygonHasEdge(oldPolys[i], v0, v1))
        {
            adjPolyIdx = i;
            break;
        }
    }

    Vector initP0 = retopo->GetMg() * retopo->GetPointR()[v0];
    Vector initP1 = retopo->GetMg() * retopo->GetPointR()[v1];
    Vector s0 = bd->WS(initP0);
    Vector s1 = bd->WS(initP1);
    if (s0.z <= 0.0 || s1.z <= 0.0) return false;

    // Determine initial normal orientation
    Vector targetNorm(0.0, 1.0, 0.0);
    Bool hasNorm = false;
    if (adjPolyIdx != NOTOK)
    {
        const Vector* pts = retopo->GetPointR();
        const CPolygon& p = oldPolys[adjPolyIdx];
        Vector pA = retopo->GetMg() * pts[p.a];
        Vector pB = retopo->GetMg() * pts[p.b];
        Vector pC = retopo->GetMg() * pts[p.c];
        Vector n = Cross(pB - pA, pC - pA);
        if (n.GetSquaredLength() > 1e-6)
        {
            targetNorm = n.GetNormalized();
            hasNorm = true;
        }
    }
    if (target)
    {
        SnapResult snap = m_snapper.RaycastSurface(bd, target, mx, my);
        if (snap.valid)
        {
            if (hasNorm && Dot(targetNorm, snap.normal) < 0.0)
                targetNorm = -targetNorm;
            else if (!hasNorm)
                targetNorm = snap.normal;
        }
    }
    else if (!hasNorm && bd)
    {
        targetNorm = -bd->GetMg().sqmat.v3;
    }

    BaseContainer device;
    win->MouseDragStart(dragButton, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

    Float dx, dy;
    Float totalDx = 0.0, totalDy = 0.0;
    Bool isDragging = false;
    Vector lastP0 = initP0, lastP1 = initP1;
    Vector lastNorm0(0.0, 1.0, 0.0), lastNorm1(0.0, 1.0, 0.0);
    const Float DRAG_THRESHOLD = 3.0;

    Int32 newV0 = NOTOK;
    Int32 newV1 = NOTOK;
    Int32 newPoly = NOTOK;
    Int32 weldTarget0 = NOTOK;
    Int32 weldTarget1 = NOTOK;

    while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
    {
        if (dx == 0.0 && dy == 0.0) continue;
        totalDx += dx;
        totalDy += dy;

        Float currS0x = s0.x + totalDx;
        Float currS0y = s0.y + totalDy;
        Float currS1x = s1.x + totalDx;
        Float currS1y = s1.y + totalDy;

        Vector p0 = initP0, p1 = initP1;
        Bool hasP0 = false, hasP1 = false;

        if (target)
        {
            SnapResult h0 = m_snapper.RaycastSurface(bd, target, currS0x, currS0y);
            if (h0.valid)
            {
                p0 = h0.worldPos;
                lastP0 = p0;
                lastNorm0 = h0.normal;
                hasP0 = true;
            }
            else
            {
                Vector cand0 = bd->SW_Reference(currS0x, currS0y, lastP0);
                SnapResult pr0 = m_snapper.ProjectPointAlongNormal(target, cand0, lastNorm0, 500.0);
                if (pr0.valid) { p0 = pr0.worldPos; hasP0 = true; }
            }

            SnapResult h1 = m_snapper.RaycastSurface(bd, target, currS1x, currS1y);
            if (h1.valid)
            {
                p1 = h1.worldPos;
                lastP1 = p1;
                lastNorm1 = h1.normal;
                hasP1 = true;
            }
            else
            {
                Vector cand1 = bd->SW_Reference(currS1x, currS1y, lastP1);
                SnapResult pr1 = m_snapper.ProjectPointAlongNormal(target, cand1, lastNorm1, 500.0);
                if (pr1.valid) { p1 = pr1.worldPos; hasP1 = true; }
            }
        }

        if (!hasP0 && bd)
        {
            p0 = bd->SW_Reference(currS0x, currS0y, initP0);
            hasP0 = true;
        }
        if (!hasP1 && bd)
        {
            p1 = bd->SW_Reference(currS1x, currS1y, initP1);
            hasP1 = true;
        }

        if (!isDragging)
        {
            Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
            if (distSoFar < DRAG_THRESHOLD)
                continue;

            isDragging = true;
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);

            ExtrudeEdgeResult extRes = m_builder.ExtrudeEdge(retopo, v0, v1, p0, p1, targetNorm);
            if (!extRes.valid)
            {
                doc->EndUndo();
                doc->DoUndo(true);
                break;
            }

            newV0 = extRes.newV0;
            newV1 = extRes.newV1;
            newPoly = extRes.newPoly;

            m_activeDragMode = TweakMode::Edge;
            m_dragEdgeV0 = newV0;
            m_dragEdgeV1 = newV1;
        }

        if (isDragging && newV0 != NOTOK && newV1 != NOTOK)
        {
            weldTarget0 = NOTOK;
            weldTarget1 = NOTOK;
            m_weldTargetIdx = NOTOK;
            m_weldTargetIdx2 = NOTOK;

            // Check weld target for newV0
            Vector curScreen0 = bd->WS(p0);
            Int32 candWeld0 = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen0.x, curScreen0.y, 12.0, newV0, target);
            if (candWeld0 != NOTOK && candWeld0 != v0 && candWeld0 != v1 && candWeld0 != newV1 && candWeld0 != newV0 &&
                m_builder.IsBoundaryOrIsolatedVertex(retopo, candWeld0))
            {
                Vector targetPos0 = retopo->GetMg() * retopo->GetPointR()[candWeld0];
                Vector targetScreen0 = bd->WS(targetPos0);
                Float maxDepthDiff0 = maxon::Max(Float(6.0), Float(curScreen0.z * 0.015));
                if (std::abs(targetScreen0.z - curScreen0.z) <= maxDepthDiff0)
                {
                    weldTarget0 = candWeld0;
                    p0 = targetPos0;
                }
            }

            // Check weld target for newV1
            Vector curScreen1 = bd->WS(p1);
            Int32 candWeld1 = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen1.x, curScreen1.y, 12.0, newV1, target);
            if (candWeld1 != NOTOK && candWeld1 != v0 && candWeld1 != v1 && candWeld1 != newV0 && candWeld1 != newV1 && candWeld1 != weldTarget0 &&
                m_builder.IsBoundaryOrIsolatedVertex(retopo, candWeld1))
            {
                Vector targetPos1 = retopo->GetMg() * retopo->GetPointR()[candWeld1];
                Vector targetScreen1 = bd->WS(targetPos1);
                Float maxDepthDiff1 = maxon::Max(Float(6.0), Float(curScreen1.z * 0.015));
                if (std::abs(targetScreen1.z - curScreen1.z) <= maxDepthDiff1)
                {
                    weldTarget1 = candWeld1;
                    p1 = targetPos1;
                }
            }

            m_builder.SetVertexPosition(retopo, newV0, p0);
            m_builder.SetVertexPosition(retopo, newV1, p1);

            m_weldTargetIdx = weldTarget0;
            m_weldTargetIdx2 = weldTarget1;

            if (weldTarget0 != NOTOK || weldTarget1 != NOTOK)
                StatusSetText("QuadDraw: Extruding edge (Release to weld into target vertex)"_s);
            else
                StatusSetText(FormatString("QuadDraw: Extruding edge (#@ - #@)"_s, v0, v1));

            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }
    }

    MOUSEDRAGRESULT dragResult = win->MouseDragEnd();

    if (isDragging)
    {
        if (dragResult == MOUSEDRAGRESULT::ESCAPE)
        {
            doc->DoUndo(true);
        }
        else
        {
            Int32 finalV0 = newV0;
            Int32 finalV1 = newV1;

            // Weld newV1 first (since newV1 > newV0, deleting newV1 doesn't shift newV0)
            if (weldTarget1 != NOTOK && weldTarget1 != newV1)
            {
                m_builder.WeldVertices(retopo, newV1, weldTarget1);
                finalV1 = (weldTarget1 > newV1) ? (weldTarget1 - 1) : weldTarget1;
                if (weldTarget0 > newV1)
                    weldTarget0--;
                if (finalV0 > newV1)
                    finalV0--;
            }

            // Weld newV0 second
            if (weldTarget0 != NOTOK && weldTarget0 != finalV0)
            {
                m_builder.WeldVertices(retopo, finalV0, weldTarget0);
                finalV0 = (weldTarget0 > finalV0) ? (weldTarget0 - 1) : weldTarget0;
                if (finalV1 > finalV0)
                    finalV1--;
            }

            StatusSetText("QuadDraw: Edge extruded."_s);
            doc->EndUndo();
            EventAdd();

            // Keep the new outer edge highlighted
            if (finalV0 >= 0 && finalV0 < retopo->GetPointCount() &&
                finalV1 >= 0 && finalV1 < retopo->GetPointCount() && finalV0 != finalV1)
            {
                m_hoverTweak.mode = TweakMode::Edge;
                m_hoverTweak.edgeV0 = finalV0;
                m_hoverTweak.edgeV1 = finalV1;
                m_hoverTweak.edgeWorld0 = retopo->GetMg() * retopo->GetPointR()[finalV0];
                m_hoverTweak.edgeWorld1 = retopo->GetMg() * retopo->GetPointR()[finalV1];
            }
            else
            {
                m_hoverTweak.mode = TweakMode::None;
            }
        }
    }

    m_cursorX = mx + totalDx;
    m_cursorY = my + totalDy;
    m_dragEdgeV0 = NOTOK;
    m_dragEdgeV1 = NOTOK;
    m_weldTargetIdx = NOTOK;
    m_weldTargetIdx2 = NOTOK;
    m_activeDragMode = TweakMode::None;
    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    return true;
}

Bool QuadDrawToolData::DoExtrudeEdgeLoopDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                            PolygonObject* retopo, PolygonObject* target,
                                            const maxon::BaseArray<LoopEdge>& loopEdges,
                                            Float mx, Float my, Int32 dragButton)
{
    if (!doc || !bd || !win || !retopo || loopEdges.GetCount() == 0) return false;

    Int32 polyCount = retopo->GetPolygonCount();
    const CPolygon* oldPolys = retopo->GetPolygonR();
    const Vector* pts = retopo->GetPointR();
    Matrix rMg = retopo->GetMg();

    struct LoopExtrudeEdgeInfo
    {
        Int32 v0 = NOTOK;
        Int32 v1 = NOTOK;
        Int32 adjPoly = NOTOK;
        Bool adjReversed = false;
        Vector targetNorm = Vector(0.0, 1.0, 0.0);
    };

    maxon::BaseArray<LoopExtrudeEdgeInfo> edgeInfos;
    edgeInfos.Resize(loopEdges.GetCount()) iferr_ignore("Resize");

    for (Int32 i = 0; i < (Int32)loopEdges.GetCount(); ++i)
    {
        Int32 u = loopEdges[i].v0;
        Int32 v = loopEdges[i].v1;
        Int32 edgePolyCount = 0;
        Int32 adjPolyIdx = NOTOK;
        Bool adjReversed = false;

        for (Int32 p = 0; p < polyCount; ++p)
        {
            const CPolygon& poly = oldPolys[p];
            if (PolygonHasEdge(poly, u, v))
            {
                edgePolyCount++;
                adjPolyIdx = p;
                Bool isQuad = (poly.c != poly.d);
                if ((poly.a == v && poly.b == u) || (poly.b == v && poly.c == u) ||
                    (isQuad && poly.c == v && poly.d == u) || (isQuad && poly.d == v && poly.a == u) ||
                    (!isQuad && poly.c == v && poly.a == u))
                {
                    adjReversed = true;
                }
            }
        }

        if (edgePolyCount != 1)
        {
            // Not a pure border edge loop! Fallback to moving the loop.
            return false;
        }

        edgeInfos[i].v0 = u;
        edgeInfos[i].v1 = v;
        edgeInfos[i].adjPoly = adjPolyIdx;
        edgeInfos[i].adjReversed = adjReversed;

        Vector n(0.0, 1.0, 0.0);
        if (adjPolyIdx != NOTOK)
        {
            const CPolygon& p = oldPolys[adjPolyIdx];
            Vector pA = rMg * pts[p.a];
            Vector pB = rMg * pts[p.b];
            Vector pC = rMg * pts[p.c];
            Vector crossN = Cross(pB - pA, pC - pA);
            if (crossN.GetSquaredLength() > 1e-6)
                n = crossN.GetNormalized();
        }
        edgeInfos[i].targetNorm = n;
    }

    // Collect all unique vertices from loopEdges
    maxon::BaseArray<Int32> uniqueVerts;
    for (Int32 i = 0; i < (Int32)loopEdges.GetCount(); ++i)
    {
        auto addU = [&](Int32 v) {
            for (Int32 k = 0; k < (Int32)uniqueVerts.GetCount(); ++k)
                if (uniqueVerts[k] == v) return;
            uniqueVerts.Append(v) iferr_ignore("Append");
        };
        addU(loopEdges[i].v0);
        addU(loopEdges[i].v1);
    }

    Int32 numUnique = (Int32)uniqueVerts.GetCount();
    maxon::BaseArray<Vector> initPts;
    maxon::BaseArray<Vector> sPts;
    maxon::BaseArray<Vector> lastPts;
    maxon::BaseArray<Vector> lastNorms;
    initPts.Resize(numUnique) iferr_ignore("Resize");
    sPts.Resize(numUnique) iferr_ignore("Resize");
    lastPts.Resize(numUnique) iferr_ignore("Resize");
    lastNorms.Resize(numUnique) iferr_ignore("Resize");

    for (Int32 k = 0; k < numUnique; ++k)
    {
        initPts[k] = rMg * pts[uniqueVerts[k]];
        sPts[k] = bd->WS(initPts[k]);
        lastPts[k] = initPts[k];
        lastNorms[k] = Vector(0.0, 1.0, 0.0);
    }

    BaseContainer device;
    win->MouseDragStart(dragButton, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

    Float dx, dy;
    Float totalDx = 0.0, totalDy = 0.0;
    Bool isDragging = false;
    const Float DRAG_THRESHOLD = 3.0;

    maxon::BaseArray<Int32> newVerts;
    newVerts.Resize(numUnique) iferr_ignore("Resize");
    for (Int32 k = 0; k < numUnique; ++k) newVerts[k] = NOTOK;

    auto getNewV = [&](Int32 origV) -> Int32 {
        for (Int32 k = 0; k < numUnique; ++k)
            if (uniqueVerts[k] == origV) return newVerts[k];
        return NOTOK;
    };

    while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
    {
        if (dx == 0.0 && dy == 0.0) continue;
        totalDx += dx;
        totalDy += dy;

        if (!isDragging)
        {
            Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
            if (distSoFar < DRAG_THRESHOLD)
                continue;

            isDragging = true;
            m_activeDragMode = TweakMode::LoopExtrude;
            m_loopWeldTargets.Resize(numUnique) iferr_ignore("Resize");
            for (Int32 k = 0; k < numUnique; ++k) m_loopWeldTargets[k] = NOTOK;

            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);

            // Allocate new vertices
            for (Int32 k = 0; k < numUnique; ++k)
            {
                newVerts[k] = m_builder.AddVertex(retopo, initPts[k]);
            }

            // Create quads for each border edge
            for (Int32 i = 0; i < (Int32)edgeInfos.GetCount(); ++i)
            {
                Int32 u = edgeInfos[i].v0;
                Int32 v = edgeInfos[i].v1;
                Int32 nu = getNewV(u);
                Int32 nv = getNewV(v);
                Int32 q0, q1, q2, q3;
                if (edgeInfos[i].adjReversed)
                {
                    q0 = u; q1 = v; q2 = nv; q3 = nu;
                }
                else
                {
                    q0 = v; q1 = u; q2 = nu; q3 = nv;
                }
                m_builder.AddQuad(retopo, q0, q1, q2, q3, edgeInfos[i].targetNorm);
            }

            // Immediately retarget m_componentLoop to highlight the new outer leading edges
            m_componentLoop.edges.Reset();
            for (Int32 i = 0; i < (Int32)edgeInfos.GetCount(); ++i)
            {
                Int32 nu = getNewV(edgeInfos[i].v0);
                Int32 nv = getNewV(edgeInfos[i].v1);
                LoopEdge le;
                le.v0 = nu;
                le.v1 = nv;
                le.worldPos0 = initPts[i < numUnique ? i : 0];
                le.worldPos1 = initPts[i < numUnique ? i : 0];
                m_componentLoop.edges.Append(le) iferr_ignore("Append");
            }
        }

        // Update positions during drag in batch
        Vector* ptsW = retopo->GetPointW();
        Matrix invMg = ~retopo->GetMg();
        Int32 curPtCount = retopo->GetPointCount();
        Bool hasWeldTarget = false;

        for (Int32 k = 0; k < numUnique; ++k)
        {
            m_loopWeldTargets[k] = NOTOK;
            Float currSx = sPts[k].x + totalDx;
            Float currSy = sPts[k].y + totalDy;
            Vector newPos = initPts[k];
            Bool gotPos = false;

            if (target)
            {
                SnapResult h = m_snapper.RaycastSurface(bd, target, currSx, currSy);
                if (h.valid)
                {
                    newPos = h.worldPos;
                    lastPts[k] = newPos;
                    lastNorms[k] = h.normal;
                    gotPos = true;
                }
                else
                {
                    Vector cand = bd->SW_Reference(currSx, currSy, lastPts[k]);
                    SnapResult pr = m_snapper.ProjectPointAlongNormal(target, cand, lastNorms[k], 500.0);
                    if (pr.valid)
                    {
                        newPos = pr.worldPos;
                        gotPos = true;
                    }
                }
            }

            if (!gotPos && bd)
            {
                newPos = bd->SW_Reference(currSx, currSy, initPts[k]);
                gotPos = true;
            }

            // Real-time magnetic snap / sticking to nearby boundary vertices
            Int32 nv = newVerts[k];
            if (nv != NOTOK)
            {
                Vector curScreen = bd->WS(newPos);
                Int32 candWeld = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen.x, curScreen.y, 14.0, nv, target);
                if (candWeld != NOTOK && m_builder.IsBoundaryOrIsolatedVertex(retopo, candWeld))
                {
                    Bool isSelf = false;
                    for (Int32 j = 0; j < numUnique; ++j)
                    {
                        if (newVerts[j] == candWeld || uniqueVerts[j] == candWeld)
                        {
                            isSelf = true;
                            break;
                        }
                    }
                    if (!isSelf)
                    {
                        Vector targetPos = retopo->GetMg() * retopo->GetPointR()[candWeld];
                        Vector targetScreen = bd->WS(targetPos);
                        Float maxDepthDiff = maxon::Max(Float(6.0), Float(curScreen.z * 0.015));
                        if (std::abs(targetScreen.z - curScreen.z) <= maxDepthDiff)
                        {
                            newPos = targetPos; // Magnetize / snap!
                            m_loopWeldTargets[k] = candWeld;
                            hasWeldTarget = true;
                        }
                    }
                }
            }

            if (nv != NOTOK && ptsW && nv >= 0 && nv < curPtCount)
            {
                ptsW[nv] = invMg * newPos;
            }
        }

        retopo->Message(MSG_UPDATE);

        // Update live world positions of leading edges
        Matrix rMgLive = retopo->GetMg();
        const Vector* ptsLive = retopo->GetPointR();
        for (Int32 i = 0; i < (Int32)m_componentLoop.edges.GetCount(); ++i)
        {
            Int32 u = m_componentLoop.edges[i].v0;
            Int32 v = m_componentLoop.edges[i].v1;
            if (u >= 0 && u < curPtCount && v >= 0 && v < curPtCount && ptsLive)
            {
                m_componentLoop.edges[i].worldPos0 = rMgLive * ptsLive[u];
                m_componentLoop.edges[i].worldPos1 = rMgLive * ptsLive[v];
            }
        }

        if (hasWeldTarget)
            StatusSetText("QuadDraw: Extruding Border Loop (Release to weld into target vertices)"_s);
        else
            StatusSetText(FormatString("QuadDraw: Extruding Border Loop (@ quads)..."_s, (Int32)edgeInfos.GetCount()));

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    }

    MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
    m_activeDragMode = TweakMode::None;

    if (!isDragging)
    {
        // Click without drag: Perform Selection!
        EdgeBaseSelect* edgeSel = retopo->GetWritableEdgeS();
        if (edgeSel)
        {
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
            Neighbor neighbor;
            neighbor.Init(retopo->GetPointCount(), retopo->GetPolygonR(), retopo->GetPolygonCount(), nullptr);
            for (Int32 i = 0; i < (Int32)loopEdges.GetCount(); ++i)
            {
                Int32 u = loopEdges[i].v0;
                Int32 v = loopEdges[i].v1;
                Int32 pA = NOTOK, pB = NOTOK;
                neighbor.GetEdgePolys(u, v, &pA, &pB);
                auto selectPolyEdge = [&](Int32 pIdx) {
                    if (pIdx == NOTOK || pIdx >= retopo->GetPolygonCount()) return;
                    const CPolygon& poly = retopo->GetPolygonR()[pIdx];
                    Int32 vArr[4] = { poly.a, poly.b, poly.c, poly.d };
                    Int32 numE = (poly.c != poly.d) ? 4 : 3;
                    for (Int32 e = 0; e < numE; ++e)
                    {
                        Int32 ea = vArr[e];
                        Int32 eb = vArr[(e + 1) % numE];
                        if ((ea == u && eb == v) || (ea == v && eb == u))
                        {
                            edgeSel->Select(pIdx * 4 + e);
                            break;
                        }
                    }
                };
                selectPolyEdge(pA);
                selectPolyEdge(pB);
            }
            doc->EndUndo();
            EventAdd();
            StatusSetText(FormatString("QuadDraw: Selected Edge Loop (@ edges)"_s, (Int32)loopEdges.GetCount()));
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }
        return true;
    }

    if (dragResult == MOUSEDRAGRESULT::ESCAPE)
    {
        doc->DoUndo(true);
        m_loopWeldTargets.Reset();
        StatusSetText("QuadDraw: Extrude canceled."_s);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Weld magnetized vertices from highest index to lowest
    for (Int32 k = numUnique - 1; k >= 0; --k)
    {
        Int32 nv = newVerts[k];
        Int32 tw = (k < (Int32)m_loopWeldTargets.GetCount()) ? m_loopWeldTargets[k] : NOTOK;
        if (nv != NOTOK && tw != NOTOK && nv != tw && nv < retopo->GetPointCount() && tw < retopo->GetPointCount())
        {
            m_builder.WeldVertices(retopo, nv, tw);
            newVerts[k] = tw;
        }
    }
    m_loopWeldTargets.Reset();

    // Additional auto-weld check for any newly created vertices on drop
    for (Int32 k = numUnique - 1; k >= 0; --k)
    {
        Int32 nv = newVerts[k];
        if (nv == NOTOK || nv >= retopo->GetPointCount()) continue;
        Vector wPos = retopo->GetMg() * retopo->GetPointR()[nv];
        Vector sPos = bd->WS(wPos);
        if (sPos.z <= 0.0) continue;

        Int32 targetV = m_snapper.FindNearestRetopoVertex(bd, retopo, sPos.x, sPos.y, 14.0, nv, target);
        if (targetV != NOTOK)
        {
            Bool isNewVert = false;
            for (Int32 j = 0; j < numUnique; ++j)
                if (newVerts[j] == targetV) { isNewVert = true; break; }

            if (!isNewVert && targetV < retopo->GetPointCount())
            {
                m_builder.WeldVertices(retopo, nv, targetV);
                newVerts[k] = targetV;
            }
        }
    }

    doc->EndUndo();
    EventAdd();
    StatusSetText(FormatString("QuadDraw: Extruded Border Loop (@ quads)."_s, (Int32)edgeInfos.GetCount()));

    // Update m_componentLoop with the newly created outer border edges
    m_componentLoop.edges.Reset();
    Matrix mg = retopo->GetMg();
    const Vector* finalPts = retopo->GetPointR();
    for (Int32 i = 0; i < (Int32)edgeInfos.GetCount(); ++i)
    {
        Int32 nu = getNewV(edgeInfos[i].v0);
        Int32 nv = getNewV(edgeInfos[i].v1);
        if (nu != NOTOK && nv != NOTOK && nu != nv && nu < retopo->GetPointCount() && nv < retopo->GetPointCount())
        {
            LoopEdge le;
            le.v0 = nu;
            le.v1 = nv;
            le.worldPos0 = mg * finalPts[nu];
            le.worldPos1 = mg * finalPts[nv];
            m_componentLoop.edges.Append(le) iferr_ignore("Append");
        }
    }

    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    return true;
}

Bool QuadDrawToolData::DoMoveComponentLoopDrag(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win,
                                              PolygonObject* retopo, PolygonObject* target,
                                              Float mx, Float my, Int32 dragButton)
{
    if (!doc || !bd || !win || !retopo || m_componentLoop.type == ComponentLoopType::None) return false;

    Int32 polyCount = retopo->GetPolygonCount();
    Int32 ptCount = retopo->GetPointCount();
    if (ptCount == 0) return false;

    const CPolygon* oldPolys = retopo->GetPolygonR();
    const Vector* pts = retopo->GetPointR();
    Matrix rMg = retopo->GetMg();

    maxon::BaseArray<Int32> loopVerts;
    if (m_componentLoop.type == ComponentLoopType::Edge)
    {
        for (Int32 i = 0; i < (Int32)m_componentLoop.edges.GetCount(); ++i)
        {
            auto addV = [&](Int32 v) {
                if (v < 0 || v >= ptCount) return;
                for (Int32 k = 0; k < (Int32)loopVerts.GetCount(); ++k)
                    if (loopVerts[k] == v) return;
                loopVerts.Append(v) iferr_ignore("Append");
            };
            addV(m_componentLoop.edges[i].v0);
            addV(m_componentLoop.edges[i].v1);
        }
    }
    else if (m_componentLoop.type == ComponentLoopType::Polygon)
    {
        for (Int32 i = 0; i < (Int32)m_componentLoop.polygons.GetCount(); ++i)
        {
            Int32 pIdx = m_componentLoop.polygons[i];
            if (pIdx < 0 || pIdx >= polyCount) continue;
            const CPolygon& p = oldPolys[pIdx];
            Int32 numPts = (p.c != p.d) ? 4 : 3;
            Int32 pVerts[4] = { p.a, p.b, p.c, p.d };
            for (Int32 j = 0; j < numPts; ++j)
            {
                Int32 v = pVerts[j];
                if (v < 0 || v >= ptCount) continue;
                Bool found = false;
                for (Int32 k = 0; k < (Int32)loopVerts.GetCount(); ++k)
                    if (loopVerts[k] == v) { found = true; break; }
                if (!found) loopVerts.Append(v) iferr_ignore("Append");
            }
        }
    }
    else if (m_componentLoop.type == ComponentLoopType::Vertex)
    {
        for (Int32 i = 0; i < (Int32)m_componentLoop.vertices.GetCount(); ++i)
        {
            Int32 v = m_componentLoop.vertices[i];
            if (v >= 0 && v < ptCount)
                loopVerts.Append(v) iferr_ignore("Append");
        }
    }

    Int32 numVerts = (Int32)loopVerts.GetCount();
    if (numVerts == 0) return false;

    maxon::BaseArray<Vector> initPts;
    maxon::BaseArray<Vector> sPts;
    maxon::BaseArray<Vector> lastPts;
    maxon::BaseArray<Vector> lastNorms;
    initPts.Resize(numVerts) iferr_ignore("Resize");
    sPts.Resize(numVerts) iferr_ignore("Resize");
    lastPts.Resize(numVerts) iferr_ignore("Resize");
    lastNorms.Resize(numVerts) iferr_ignore("Resize");

    for (Int32 k = 0; k < numVerts; ++k)
    {
        initPts[k] = rMg * pts[loopVerts[k]];
        sPts[k] = bd->WS(initPts[k]);
        lastPts[k] = initPts[k];
        lastNorms[k] = Vector(0.0, 1.0, 0.0);
    }

    BaseContainer device;
    win->MouseDragStart(dragButton, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

    Float dx, dy;
    Float totalDx = 0.0, totalDy = 0.0;
    Bool isDragging = false;
    const Float DRAG_THRESHOLD = 3.0;

    while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
    {
        if (dx == 0.0 && dy == 0.0) continue;
        totalDx += dx;
        totalDy += dy;

        if (!isDragging)
        {
            Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
            if (distSoFar < DRAG_THRESHOLD)
                continue;

            isDragging = true;
            m_activeDragMode = TweakMode::LoopMove;
            m_loopWeldTargets.Resize(numVerts) iferr_ignore("Resize");
            for (Int32 k = 0; k < numVerts; ++k) m_loopWeldTargets[k] = NOTOK;

            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
        }

        Vector* ptsW = retopo->GetPointW();
        Matrix invMg = ~retopo->GetMg();
        Bool hasWeldTarget = false;

        for (Int32 k = 0; k < numVerts; ++k)
        {
            m_loopWeldTargets[k] = NOTOK;
            Float currSx = sPts[k].x + totalDx;
            Float currSy = sPts[k].y + totalDy;
            Vector newPos = initPts[k];
            Bool gotPos = false;

            if (target)
            {
                SnapResult h = m_snapper.RaycastSurface(bd, target, currSx, currSy);
                if (h.valid)
                {
                    newPos = h.worldPos;
                    lastPts[k] = newPos;
                    lastNorms[k] = h.normal;
                    gotPos = true;
                }
                else
                {
                    Vector cand = bd->SW_Reference(currSx, currSy, lastPts[k]);
                    SnapResult pr = m_snapper.ProjectPointAlongNormal(target, cand, lastNorms[k], 500.0);
                    if (pr.valid)
                    {
                        newPos = pr.worldPos;
                        gotPos = true;
                    }
                }
            }

            if (!gotPos && bd)
            {
                newPos = bd->SW_Reference(currSx, currSy, initPts[k]);
                gotPos = true;
            }

            // Real-time magnetic snap for boundary vertices
            Int32 lv = loopVerts[k];
            if (lv >= 0 && lv < ptCount && m_builder.IsBoundaryOrIsolatedVertex(retopo, lv))
            {
                Vector curScreen = bd->WS(newPos);
                Int32 candWeld = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen.x, curScreen.y, 14.0, lv, target);
                if (candWeld != NOTOK && m_builder.IsBoundaryOrIsolatedVertex(retopo, candWeld))
                {
                    Bool isSelf = false;
                    for (Int32 j = 0; j < numVerts; ++j)
                    {
                        if (loopVerts[j] == candWeld) { isSelf = true; break; }
                    }
                    if (!isSelf)
                    {
                        Vector targetPos = retopo->GetMg() * retopo->GetPointR()[candWeld];
                        Vector targetScreen = bd->WS(targetPos);
                        Float maxDepthDiff = maxon::Max(Float(6.0), Float(curScreen.z * 0.015));
                        if (std::abs(targetScreen.z - curScreen.z) <= maxDepthDiff)
                        {
                            newPos = targetPos; // Magnetize / snap!
                            m_loopWeldTargets[k] = candWeld;
                            hasWeldTarget = true;
                        }
                    }
                }
            }

            if (ptsW && loopVerts[k] >= 0 && loopVerts[k] < ptCount)
            {
                ptsW[loopVerts[k]] = invMg * newPos;
            }
        }

        retopo->Message(MSG_UPDATE);

        // Update live world positions of component loop edges so overlay follows seamlessly
        Matrix rMgLive = retopo->GetMg();
        const Vector* ptsLive = retopo->GetPointR();
        for (Int32 i = 0; i < (Int32)m_componentLoop.edges.GetCount(); ++i)
        {
            Int32 u = m_componentLoop.edges[i].v0;
            Int32 v = m_componentLoop.edges[i].v1;
            if (u >= 0 && u < ptCount && v >= 0 && v < ptCount && ptsLive)
            {
                m_componentLoop.edges[i].worldPos0 = rMgLive * ptsLive[u];
                m_componentLoop.edges[i].worldPos1 = rMgLive * ptsLive[v];
            }
        }

        if (hasWeldTarget)
            StatusSetText("QuadDraw: Moving loop (Release to weld into target vertices)"_s);
        else
            StatusSetText(FormatString("QuadDraw: Moving loop (@ vertices)..."_s, numVerts));

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    }

    MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
    m_activeDragMode = TweakMode::None;

    if (!isDragging)
    {
        // Click without drag: Perform Selection!
        if (m_componentLoop.type == ComponentLoopType::Edge)
        {
            EdgeBaseSelect* edgeSel = retopo->GetWritableEdgeS();
            if (edgeSel)
            {
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                Neighbor neighbor;
                neighbor.Init(ptCount, oldPolys, polyCount, nullptr);
                for (Int32 i = 0; i < (Int32)m_componentLoop.edges.GetCount(); ++i)
                {
                    Int32 u = m_componentLoop.edges[i].v0;
                    Int32 v = m_componentLoop.edges[i].v1;
                    Int32 pA = NOTOK, pB = NOTOK;
                    neighbor.GetEdgePolys(u, v, &pA, &pB);
                    auto selectPolyEdge = [&](Int32 pIdx) {
                        if (pIdx == NOTOK || pIdx >= polyCount) return;
                        const CPolygon& poly = oldPolys[pIdx];
                        Int32 vArr[4] = { poly.a, poly.b, poly.c, poly.d };
                        Int32 numE = (poly.c != poly.d) ? 4 : 3;
                        for (Int32 e = 0; e < numE; ++e)
                        {
                            Int32 ea = vArr[e];
                            Int32 eb = vArr[(e + 1) % numE];
                            if ((ea == u && eb == v) || (ea == v && eb == u))
                            {
                                edgeSel->Select(pIdx * 4 + e);
                                break;
                            }
                        }
                    };
                    selectPolyEdge(pA);
                    selectPolyEdge(pB);
                }
                doc->EndUndo();
                EventAdd();
                StatusSetText(FormatString("QuadDraw: Selected Edge Loop (@ edges)"_s, (Int32)m_componentLoop.edges.GetCount()));
            }
        }
        else if (m_componentLoop.type == ComponentLoopType::Polygon)
        {
            BaseSelect* polySel = retopo->GetWritablePolygonS();
            if (polySel)
            {
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                for (Int32 i = 0; i < (Int32)m_componentLoop.polygons.GetCount(); ++i)
                {
                    polySel->Select(m_componentLoop.polygons[i]);
                }
                doc->EndUndo();
                EventAdd();
                StatusSetText(FormatString("QuadDraw: Selected Polygon Loop (@ polygons)"_s, (Int32)m_componentLoop.polygons.GetCount()));
            }
        }
        else if (m_componentLoop.type == ComponentLoopType::Vertex)
        {
            BaseSelect* ptSel = retopo->GetWritablePointS();
            if (ptSel)
            {
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                for (Int32 i = 0; i < (Int32)m_componentLoop.vertices.GetCount(); ++i)
                {
                    ptSel->Select(m_componentLoop.vertices[i]);
                }
                doc->EndUndo();
                EventAdd();
                StatusSetText(FormatString("QuadDraw: Selected Vertex Loop (@ vertices)"_s, (Int32)m_componentLoop.vertices.GetCount()));
            }
        }

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    if (dragResult == MOUSEDRAGRESULT::ESCAPE)
    {
        doc->DoUndo(true);
        m_loopWeldTargets.Reset();
        StatusSetText("QuadDraw: Move canceled."_s);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Weld magnetized vertices from highest index to lowest
    for (Int32 k = numVerts - 1; k >= 0; --k)
    {
        Int32 lv = loopVerts[k];
        Int32 tw = (k < (Int32)m_loopWeldTargets.GetCount()) ? m_loopWeldTargets[k] : NOTOK;
        if (lv != NOTOK && tw != NOTOK && lv != tw && lv < retopo->GetPointCount() && tw < retopo->GetPointCount())
        {
            m_builder.WeldVertices(retopo, lv, tw);
            loopVerts[k] = tw;
        }
    }
    m_loopWeldTargets.Reset();

    doc->EndUndo();
    EventAdd();
    StatusSetText(FormatString("QuadDraw: Loop moved (@ vertices)."_s, numVerts));

    // Update world positions in m_componentLoop
    Matrix mg = retopo->GetMg();
    const Vector* finalPts = retopo->GetPointR();
    for (Int32 i = 0; i < (Int32)m_componentLoop.edges.GetCount(); ++i)
    {
        Int32 u = m_componentLoop.edges[i].v0;
        Int32 v = m_componentLoop.edges[i].v1;
        if (u < retopo->GetPointCount() && v < retopo->GetPointCount())
        {
            m_componentLoop.edges[i].worldPos0 = mg * finalPts[u];
            m_componentLoop.edges[i].worldPos1 = mg * finalPts[v];
        }
    }

    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    return true;
}

Bool QuadDrawToolData::CommitMultiCut(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, PolygonObject* retopo, PolygonObject* target)
{
    if (!retopo || m_multiCutPoints.GetCount() < 2)
    {
        m_multiCutPoints.Reset();
        m_multiCutPreview.valid = false;
        m_multiCutPreview.cuts.Reset();
        m_multiCutPreview.previewSegments.Reset();
        m_multiCutHover = MultiCutPoint();
        return false;
    }

    MultiCutResult cutRes = m_builder.BuildMultiCutFromPoints(bd, retopo, target, m_snapper, m_multiCutPoints, nullptr);
    if (!cutRes.valid || cutRes.cuts.GetCount() == 0)
    {
        m_multiCutPoints.Reset();
        m_multiCutPreview.valid = false;
        m_multiCutPreview.cuts.Reset();
        m_multiCutPreview.previewSegments.Reset();
        m_multiCutHover = MultiCutPoint();
        return false;
    }

    doc->StartUndo();
    doc->AddUndo(UNDOTYPE::CHANGE, retopo);

    Int32 count = (Int32)cutRes.cuts.GetCount();
    Bool success = m_builder.ApplyPolygonCuts(retopo, target, m_snapper, cutRes.cuts);
    if (success)
    {
        StatusSetText(FormatString("QuadDraw [MULTI-CUT]: Cut completed (@ polygons split)."_s, count));
    }

    doc->EndUndo();
    EventAdd();

    m_multiCutPoints.Reset();
    m_multiCutPreview.valid = false;
    m_multiCutPreview.cuts.Reset();
    m_multiCutPreview.previewSegments.Reset();
    m_multiCutHover = MultiCutPoint();
    return success;
}

Bool QuadDrawToolData::MouseInput(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win, const BaseContainer& msg)
{
    if (!doc || !bd || !win) return false;

    Int32 channel = msg.GetInt32(BFM_INPUT_CHANNEL);
    Float mx = msg.GetFloat(BFM_INPUT_X);
    Float my = msg.GetFloat(BFM_INPUT_Y);
    Int32 qualifier = msg.GetInt32(BFM_INPUT_QUALIFIER);
    Bool shiftPressed = ((qualifier & QSHIFT) != 0) || m_shiftHeld;

    // Handle RMB for Multi-Cut commit
    if (channel == BFM_INPUT_MOUSERIGHT)
    {
        Int32 activeTool = data.GetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);
        if (activeTool == QUADDRAW_TOOL_MULTICUT)
        {
            PolygonObject* retopo = GetEditableMesh(doc, false);
            PolygonObject* target = GetTargetMesh(doc, retopo);
            if (m_multiCutPoints.GetCount() >= 2 && retopo)
            {
                CommitMultiCut(doc, data, bd, retopo, target);
                m_multiCutPoints.Reset();
                m_multiCutPreview.valid = false;
                m_multiCutPreview.cuts.Reset();
                m_multiCutPreview.previewSegments.Reset();
                m_multiCutHover = MultiCutPoint();
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
            else if (m_multiCutPoints.GetCount() >= 1)
            {
                m_multiCutPoints.Reset();
                m_multiCutPreview.valid = false;
                m_multiCutPreview.cuts.Reset();
                m_multiCutPreview.previewSegments.Reset();
                m_multiCutHover = MultiCutPoint();
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }
        return false;
    }

    // =========================================================================
    // ACTION 0: SHIFT + MMB DRAG -> RESIZE RELAX BRUSH (RADIUS & STRENGTH)
    // =========================================================================
    if (channel == BFM_INPUT_MOUSEMIDDLE && shiftPressed)
    {
        Float initialRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float currentRadius = initialRadius;
        Float initialStrength = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
        Float currentStrength = initialStrength;

        m_isResizingBrush = true;
        m_brushResizeCenterX = mx;
        m_brushResizeCenterY = my;
        m_cursorX = mx;
        m_cursorY = my;

        BaseContainer device;
        win->MouseDragStart(KEY_MMIDDLE, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        Float totalDx = 0.0;
        Float totalDy = 0.0;
        // Direction lock like cross_type: 0 = uncommitted, 1 = horizontal (radius), 2 = vertical (strength)
        Int32 dragAxis = 0;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;

            totalDx += dx;
            totalDy += dy;

            if (dragAxis == 0)
            {
                if (std::abs(totalDx) >= 3.0 || std::abs(totalDy) >= 3.0)
                {
                    if (std::abs(totalDx) >= std::abs(totalDy))
                        dragAxis = 1;
                    else
                        dragAxis = 2;
                }
            }

            if (dragAxis == 1 || dragAxis == 0)
            {
                currentRadius += dx;
                currentRadius = maxon::ClampValue(currentRadius, Float(5.0), Float(500.0));
                data.SetFloat(QUADDRAW_RELAX_RADIUS, currentRadius);
                StatusSetText(FormatString("QuadDraw [RESIZE BRUSH] | Relax Radius: @ px (Drag Left/Right to adjust)"_s, (Int32)(currentRadius + 0.5)));
            }
            if (dragAxis == 2 || (dragAxis == 0 && std::abs(totalDy) > std::abs(totalDx)))
            {
                currentStrength -= dy * 0.005;
                currentStrength = maxon::ClampValue(currentStrength, Float(0.01), Float(1.0));
                data.SetFloat(QUADDRAW_RELAX_STRENGTH, currentStrength);
                StatusSetText(FormatString("QuadDraw [RESIZE BRUSH] | Relax Strength: @ (Drag Up/Down to adjust)"_s, currentStrength));
            }

            m_cursorX += dx;
            m_cursorY += dy;

            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }

        win->MouseDragEnd();
        m_isResizingBrush = false;

        data.SetFloat(QUADDRAW_RELAX_RADIUS, currentRadius);
        data.SetFloat(QUADDRAW_RELAX_STRENGTH, currentStrength);
        StatusSetText(FormatString("QuadDraw: Relax Radius: @ px | Strength: @"_s, (Int32)(currentRadius + 0.5), currentStrength));

        EventAdd();
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // =========================================================================
    // ACTION 0B: MMB -> EXTRUDE HIGHLIGHTED/SELECTED BORDER EDGE OR EDGE LOOP
    // =========================================================================
    if (channel == BFM_INPUT_MOUSEMIDDLE && !shiftPressed)
    {
        PolygonObject* retopo = GetEditableMesh(doc, true);
        if (!retopo) return false;
        PolygonObject* target = GetTargetMesh(doc, retopo);

        if ((qualifier & QCTRL) && m_componentLoop.edges.GetCount() > 0)
        {
            if (DoExtrudeEdgeLoopDrag(doc, data, bd, win, retopo, target, m_componentLoop.edges, mx, my, KEY_MMIDDLE))
                return true;
        }

        Int32 edgeV0 = NOTOK;
        Int32 edgeV1 = NOTOK;

        if (m_hoverTweak.mode == TweakMode::Edge)
        {
            edgeV0 = m_hoverTweak.edgeV0;
            edgeV1 = m_hoverTweak.edgeV1;
        }

        if (edgeV0 == NOTOK || edgeV1 == NOTOK)
        {
            EdgeHit nearEdge = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 30.0, target);
            if (nearEdge.valid)
            {
                edgeV0 = nearEdge.v0;
                edgeV1 = nearEdge.v1;
            }
        }

        if (edgeV0 != NOTOK && edgeV1 != NOTOK)
        {
            return DoExtrudeEdgeDrag(doc, data, bd, win, retopo, target, edgeV0, edgeV1, mx, my, KEY_MMIDDLE);
        }

        BaseContainer device;
        win->MouseDragStart(KEY_MMIDDLE, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);
        Float dx, dy;
        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE) {}
        win->MouseDragEnd();
        return true;
    }

    if (channel != BFM_INPUT_MOUSELEFT)
        return false;

    PolygonObject* retopo = GetEditableMesh(doc, true);
    if (!retopo) return false;

    PolygonObject* target = GetTargetMesh(doc, retopo);
    Int32 activeTool = data.GetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);

    // ==========================================
    // ACTION 1: CTRL + SHIFT + LMB -> DELETE ELEMENT
    // ==========================================
    if ((qualifier & QCTRL) && (qualifier & QSHIFT))
    {
        DeleteHighlight& del = m_deleteHighlight;

        // If not already detected by GetCursorInfo, run on-the-spot detection
        if (del.type == DeleteTargetType::None)
        {
            Float polyZ = 1e30;
            Int32 underPoly = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper, &polyZ);

            if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
            {
                const CPolygon& p = retopo->GetPolygonR()[underPoly];
                const Vector* pts = retopo->GetPointR();
                Matrix rMg = retopo->GetMg();

                Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
                Int32 vertCount = (p.c != p.d) ? 4 : 3;

                Int32 bestPolyV = NOTOK;
                Float bestVertDist = 10.0;
                for (Int32 vi = 0; vi < vertCount; ++vi)
                {
                    Int32 vIdx = polyVerts[vi];
                    if (vIdx == NOTOK) continue;
                    Vector sPos = bd->WS(rMg * pts[vIdx]);
                    if (sPos.z <= 0.0) continue;
                    Float dx = sPos.x - mx, dy = sPos.y - my;
                    Float d = std::sqrt(dx * dx + dy * dy);
                    if (d <= bestVertDist)
                    {
                        bestVertDist = d;
                        bestPolyV = vIdx;
                    }
                }

                if (bestPolyV != NOTOK)
                {
                    del.type = DeleteTargetType::Vertex;
                    del.index = bestPolyV;
                }
                else
                {
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, mx, my);
                    if (polyEdge.valid && polyEdge.dist <= 8.0)
                    {
                        del.type = DeleteTargetType::Edge;
                        del.edgeV0 = polyEdge.v0;
                        del.edgeV1 = polyEdge.v1;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, polyEdge.v0, polyEdge.v1);
                        del.loopEdges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                    else
                    {
                        del.type = DeleteTargetType::Polygon;
                        del.index = underPoly;
                    }
                }
            }
            else
            {
                Int32 nv = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 10.0, NOTOK, target);
                if (nv != NOTOK)
                {
                    del.type = DeleteTargetType::Vertex;
                    del.index = nv;
                }
                else
                {
                    EdgeHit eh = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 8.0, target);
                    if (eh.valid)
                    {
                        del.type = DeleteTargetType::Edge;
                        del.edgeV0 = eh.v0;
                        del.edgeV1 = eh.v1;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, eh.v0, eh.v1);
                        del.loopEdges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                }
            }
        }

        if (del.type == DeleteTargetType::Vertex && del.index != NOTOK)
        {
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
            m_builder.DeleteVertex(retopo, del.index);
            m_deleteHighlight.type = DeleteTargetType::None;
            m_deleteHighlight.loopEdges.Reset();
            m_shiftQuadPreview.valid = false;
            doc->EndUndo();
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            StatusSetText("QuadDraw: Deleted vertex"_s);
            return true;
        }
        else if (del.type == DeleteTargetType::Edge && (del.loopEdges.GetCount() > 0 || (del.edgeV0 != NOTOK && del.edgeV1 != NOTOK)))
        {
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
            if (del.loopEdges.GetCount() > 0)
                m_builder.DeleteEdgeLoop(retopo, del.loopEdges);
            else
                m_builder.DeleteEdge(retopo, del.edgeV0, del.edgeV1);
            m_deleteHighlight.type = DeleteTargetType::None;
            m_deleteHighlight.loopEdges.Reset();
            m_shiftQuadPreview.valid = false;
            doc->EndUndo();
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            StatusSetText("QuadDraw: Deleted edge loop"_s);
            return true;
        }
        else if (del.type == DeleteTargetType::Polygon && del.index != NOTOK)
        {
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
            m_builder.DeletePolygon(retopo, del.index);
            m_deleteHighlight.type = DeleteTargetType::None;
            m_deleteHighlight.loopEdges.Reset();
            m_shiftQuadPreview.valid = false;
            doc->EndUndo();
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            StatusSetText("QuadDraw: Deleted polygon"_s);
            return true;
        }

        return true;
    }

    // ==========================================
    // ACTION 2: CTRL (alone) + LMB -> COMPONENT LOOP (SELECT / EXTRUDE / MOVE)
    // ==========================================
    if ((qualifier & QCTRL) && !(qualifier & QSHIFT) && activeTool != QUADDRAW_TOOL_MULTICUT && activeTool != QUADDRAW_TOOL_KNIFE)
    {
        // If m_componentLoop was not detected yet (e.g. rapid click), detect on the spot
        if (m_componentLoop.type == ComponentLoopType::None)
        {
            Float polyZ = 1e30;
            Int32 underPoly = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper, &polyZ);
            if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
            {
                const CPolygon& p = retopo->GetPolygonR()[underPoly];
                const Vector* rPts = retopo->GetPointR();
                Matrix rMg = retopo->GetMg();

                Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
                Int32 vertCount = (p.c != p.d) ? 4 : 3;

                Int32 bestPolyV = NOTOK;
                Float bestVertDist = 12.0;
                for (Int32 vi = 0; vi < vertCount; ++vi)
                {
                    Int32 vIdx = polyVerts[vi];
                    if (vIdx == NOTOK) continue;
                    Vector sPos = bd->WS(rMg * rPts[vIdx]);
                    if (sPos.z <= 0.0) continue;
                    Float dx = sPos.x - mx, dy = sPos.y - my;
                    Float d = std::sqrt(dx * dx + dy * dy);
                    if (d <= bestVertDist)
                    {
                        bestVertDist = d;
                        bestPolyV = vIdx;
                    }
                }

                if (bestPolyV != NOTOK)
                {
                    m_componentLoop.type = ComponentLoopType::Vertex;
                    m_componentLoop.sourceIndex = bestPolyV;
                    m_componentLoop.vertices = m_builder.FindVertexLoop(bd, retopo, bestPolyV, mx, my, &m_componentLoop.edges);
                }
                else
                {
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, mx, my);
                    if (polyEdge.valid && polyEdge.dist <= 10.0)
                    {
                        m_componentLoop.type = ComponentLoopType::Edge;
                        m_componentLoop.sourceIndex = polyEdge.v0;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, polyEdge.v0, polyEdge.v1);
                        m_componentLoop.edges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                    else
                    {
                        EdgeHit pe = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, mx, my);
                        Int32 enterV0 = pe.valid ? pe.v0 : p.a;
                        Int32 enterV1 = pe.valid ? pe.v1 : p.b;

                        m_componentLoop.type = ComponentLoopType::Polygon;
                        m_componentLoop.sourceIndex = underPoly;
                        m_componentLoop.polygons = m_builder.FindPolygonLoop(retopo, underPoly, enterV0, enterV1);
                    }
                }
            }
            else
            {
                Int32 nearVertex = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 10.0, NOTOK, target);
                if (nearVertex != NOTOK)
                {
                    m_componentLoop.type = ComponentLoopType::Vertex;
                    m_componentLoop.sourceIndex = nearVertex;
                    m_componentLoop.vertices = m_builder.FindVertexLoop(bd, retopo, nearVertex, mx, my, &m_componentLoop.edges);
                }
                else
                {
                    EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 10.0, target);
                    if (edgeHit.valid)
                    {
                        m_componentLoop.type = ComponentLoopType::Edge;
                        m_componentLoop.sourceIndex = edgeHit.v0;
                        EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, edgeHit.v0, edgeHit.v1);
                        m_componentLoop.edges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                    }
                }
            }
        }

        if (m_componentLoop.type == ComponentLoopType::Edge && m_componentLoop.edges.GetCount() > 0)
        {
            Bool extrudeEnabled = (activeTool != QUADDRAW_TOOL_MOVE) && data.GetBool(QUADDRAW_BORDER_EXTRUDE_LMB, true);
            if (extrudeEnabled)
            {
                // If it is a border loop, DoExtrudeEdgeLoopDrag executes extrude (or selects on click <3px) and returns true.
                // If it is an interior edge loop, DoExtrudeEdgeLoopDrag returns false before initiating drag.
                if (DoExtrudeEdgeLoopDrag(doc, data, bd, win, retopo, target, m_componentLoop.edges, mx, my, KEY_MLEFT))
                    return true;
            }
            return DoMoveComponentLoopDrag(doc, data, bd, win, retopo, target, mx, my, KEY_MLEFT);
        }
        else if (m_componentLoop.type != ComponentLoopType::None)
        {
            return DoMoveComponentLoopDrag(doc, data, bd, win, retopo, target, mx, my, KEY_MLEFT);
        }

        return true;
    }

    // ==========================================
    // ACTION 2B: KNIFE TOOL MODE (ACTIVE TOOL == KNIFE) + LMB -> CUT / INSERT EDGE LOOP
    // ==========================================
    if (activeTool == QUADDRAW_TOOL_KNIFE && !(qualifier & QSHIFT) && !(qualifier & QCTRL))
    {
        if (!m_edgeCutPreview.valid)
        {
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            // Priority 1: Check front polygon under cursor
            Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper);
            if (nearPoly != NOTOK)
            {
                EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, nearPoly, mx, my);
                if (polyEdge.valid)
                {
                    hitV0 = polyEdge.v0;
                    hitV1 = polyEdge.v1;
                    hitT  = polyEdge.t;
                    hitPoly = nearPoly;
                }
            }
            else
            {
                // Priority 2: Check nearby visible boundary edge
                EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 12.0, target);
                if (edgeHit.valid)
                {
                    hitV0 = edgeHit.v0;
                    hitV1 = edgeHit.v1;
                    hitT  = edgeHit.t;
                    hitPoly = edgeHit.polyIndex;
                }
            }

            if (hitV0 != NOTOK && hitV1 != NOTOK)
            {
                if (std::abs(hitT - 0.5) < 0.05) hitT = 0.5;
                m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, hitV0, hitV1, hitT, hitPoly);
            }
        }

        if (m_edgeCutPreview.valid)
        {
            BaseContainer device;
            win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

            Float dx, dy;
            while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
            {
                if (dx == 0.0 && dy == 0.0) continue;
                mx += dx;
                my += dy;

                if (m_edgeCutPreview.primaryV0 != NOTOK && m_edgeCutPreview.primaryV1 != NOTOK)
                {
                    Float newT = m_builder.ComputeEdgeParam(bd, retopo, m_edgeCutPreview.primaryV0, m_edgeCutPreview.primaryV1, mx, my);
                    if (std::abs(newT - 0.5) < 0.04) newT = 0.5;
                    if (std::abs(newT - m_edgeCutPreview.paramT) > 0.003)
                    {
                        m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, m_edgeCutPreview.primaryV0, m_edgeCutPreview.primaryV1, newT, m_edgeCutPreview.primaryPoly);
                        Int32 pct = (Int32)(m_edgeCutPreview.paramT * 100.0 + 0.5);
                        StatusSetText(FormatString("QuadDraw [KNIFE] | Sliding Edge Loop (@%)"_s, pct));
                        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                    }
                }
            }

            MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                m_edgeCutPreview.valid = false;
                m_edgeCutPreview.cutPoints.Reset();
                m_edgeCutPreview.cutSegments.Reset();
                m_edgeCutPreview.quadSplits.Reset();
                m_edgeCutPreview.triSplits.Reset();
                m_cachedCutV0 = NOTOK;
                m_cachedCutV1 = NOTOK;
                m_cachedCutT = -1.0;
                m_cachedCutPoly = NOTOK;
                StatusSetText("QuadDraw [KNIFE]: Cut canceled."_s);
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }

            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);

            Int32 quadsSplit = (Int32)m_edgeCutPreview.quadSplits.GetCount();
            if (m_builder.ApplyEdgeLoopCut(retopo, m_edgeCutPreview))
            {
                StatusSetText(FormatString("QuadDraw [KNIFE]: Inserted Edge Loop (@ quads split)"_s, quadsSplit));
            }
            m_edgeCutPreview.valid = false;
            m_edgeCutPreview.cutPoints.Reset();
            m_edgeCutPreview.cutSegments.Reset();
            m_edgeCutPreview.quadSplits.Reset();
            m_edgeCutPreview.triSplits.Reset();
            m_cachedCutV0 = NOTOK;
            m_cachedCutV1 = NOTOK;
            m_cachedCutT = -1.0;
            m_cachedCutPoly = NOTOK;

            doc->EndUndo();
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }
        else
        {
            StatusSetText("QuadDraw [KNIFE]: No edge or quad loop detected under cursor to cut."_s);
            return true;
        }
    }

    // ==========================================
    // ACTION 2C: MULTI-CUT TOOL MODE (ACTIVE TOOL == MULTI-CUT - Maya Multi-Cut)
    // ==========================================
    if (activeTool == QUADDRAW_TOOL_MULTICUT)
    {
        if (m_ctrlHeld || ((qualifier & QCTRL) != 0))
        {
            // Maya Multi-Cut + Ctrl: Insert Edge Loop (Loop Cut)
            if (!m_edgeCutPreview.valid)
            {
                Int32 hitV0 = NOTOK, hitV1 = NOTOK;
                Float hitT = 0.5;
                Int32 hitPoly = NOTOK;

                Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper);
                if (nearPoly != NOTOK)
                {
                    EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, nearPoly, mx, my);
                    if (polyEdge.valid)
                    {
                        hitV0 = polyEdge.v0;
                        hitV1 = polyEdge.v1;
                        hitT  = polyEdge.t;
                        hitPoly = nearPoly;
                    }
                }
                else
                {
                    EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 12.0, target);
                    if (edgeHit.valid)
                    {
                        hitV0 = edgeHit.v0;
                        hitV1 = edgeHit.v1;
                        hitT  = edgeHit.t;
                        hitPoly = edgeHit.polyIndex;
                    }
                }

                if (hitV0 != NOTOK && hitV1 != NOTOK)
                {
                    if (std::abs(hitT - 0.5) < 0.05) hitT = 0.5;
                    m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, hitV0, hitV1, hitT, hitPoly);
                }
            }

            if (m_edgeCutPreview.valid)
            {
                BaseContainer device;
                win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

                Float dx, dy;
                while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
                {
                    if (dx == 0.0 && dy == 0.0) continue;
                    mx += dx;
                    my += dy;

                    if (m_edgeCutPreview.primaryV0 != NOTOK && m_edgeCutPreview.primaryV1 != NOTOK)
                    {
                        Float newT = m_builder.ComputeEdgeParam(bd, retopo, m_edgeCutPreview.primaryV0, m_edgeCutPreview.primaryV1, mx, my);
                        if (std::abs(newT - 0.5) < 0.04) newT = 0.5;
                        if (std::abs(newT - m_edgeCutPreview.paramT) > 0.003)
                        {
                            m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, m_snapper, bd, m_edgeCutPreview.primaryV0, m_edgeCutPreview.primaryV1, newT, m_edgeCutPreview.primaryPoly);
                            Int32 pct = (Int32)(m_edgeCutPreview.paramT * 100.0 + 0.5);
                            StatusSetText(FormatString("QuadDraw [LOOP CUT (Ctrl)] | Sliding Edge Loop (@%)"_s, pct));
                            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                        }
                    }
                }

                MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
                if (dragResult == MOUSEDRAGRESULT::ESCAPE)
                {
                    m_edgeCutPreview.valid = false;
                    m_edgeCutPreview.cutPoints.Reset();
                    m_edgeCutPreview.cutSegments.Reset();
                    m_edgeCutPreview.quadSplits.Reset();
                    m_edgeCutPreview.triSplits.Reset();
                    m_cachedCutV0 = NOTOK;
                    m_cachedCutV1 = NOTOK;
                    m_cachedCutT = -1.0;
                    m_cachedCutPoly = NOTOK;
                    m_multiCutHover = MultiCutPoint();
                    StatusSetText("QuadDraw [LOOP CUT (Ctrl)]: Cut canceled."_s);
                    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                    return true;
                }

                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                Int32 quadsSplit = (Int32)m_edgeCutPreview.quadSplits.GetCount();
                if (m_builder.ApplyEdgeLoopCut(retopo, m_edgeCutPreview))
                {
                    StatusSetText(FormatString("QuadDraw [LOOP CUT (Ctrl)]: Inserted Edge Loop (@ quads split)"_s, quadsSplit));
                }
                m_edgeCutPreview.valid = false;
                m_edgeCutPreview.cutPoints.Reset();
                m_edgeCutPreview.cutSegments.Reset();
                m_edgeCutPreview.quadSplits.Reset();
                m_edgeCutPreview.triSplits.Reset();
                m_cachedCutV0 = NOTOK;
                m_cachedCutV1 = NOTOK;
                m_cachedCutT = -1.0;
                m_cachedCutPoly = NOTOK;
                m_multiCutHover = MultiCutPoint();
                doc->EndUndo();
                EventAdd();
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
            return true;
        }

        // Double click: commit active cut
        Bool isDoubleClick = msg.GetBool(BFM_INPUT_DOUBLECLICK);
        if (isDoubleClick && m_multiCutPoints.GetCount() >= 1)
        {
            if (m_multiCutHover.type != MultiCutSnapType::None)
            {
                m_multiCutPoints.Append(m_multiCutHover) iferr_ignore("Append cut pt");
            }
            if (m_multiCutPoints.GetCount() >= 2)
            {
                CommitMultiCut(doc, data, bd, retopo, target);
                m_multiCutPoints.Reset();
                m_multiCutPreview.valid = false;
                m_multiCutPreview.cuts.Reset();
                m_multiCutPreview.previewSegments.Reset();
                m_multiCutHover = MultiCutPoint();
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }

        // Drag (Slice Cut) or Click (Place Point)
        Float startX = mx, startY = my;
        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        Bool isDrag = false;
        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            mx += dx;
            my += dy;

            Float dragDist = std::sqrt((mx - startX) * (mx - startX) + (my - startY) * (my - startY));
            if (dragDist > 6.0 && m_multiCutPoints.GetCount() == 0)
            {
                isDrag = true;
                m_sliceDrag.active = true;
                m_sliceDrag.startX = startX;
                m_sliceDrag.startY = startY;
                m_sliceDrag.currX = mx;
                m_sliceDrag.currY = my;

                m_sliceDrag.result = m_builder.BuildSliceCut(bd, retopo, target, m_snapper, Vector(startX, startY, 0.0), Vector(mx, my, 0.0));
                Int32 cutCount = (Int32)m_sliceDrag.result.cuts.GetCount();
                StatusSetText(FormatString("QuadDraw [MULTI-CUT SLICE] | Slicing @ polygons | Release LMB to cut | Esc to cancel"_s, cutCount));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
        if (dragResult == MOUSEDRAGRESULT::ESCAPE)
        {
            m_sliceDrag.active = false;
            m_sliceDrag.result.valid = false;
            m_sliceDrag.result.cuts.Reset();
            m_sliceDrag.result.previewSegments.Reset();
            m_multiCutPoints.Reset();
            m_multiCutPreview.valid = false;
            m_multiCutPreview.cuts.Reset();
            m_multiCutPreview.previewSegments.Reset();
            m_multiCutHover = MultiCutPoint();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }

        if (isDrag && m_sliceDrag.active)
        {
            if (m_sliceDrag.result.valid && m_sliceDrag.result.cuts.GetCount() > 0)
            {
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                Int32 count = (Int32)m_sliceDrag.result.cuts.GetCount();
                if (m_builder.ApplyPolygonCuts(retopo, target, m_snapper, m_sliceDrag.result.cuts))
                {
                    StatusSetText(FormatString("QuadDraw [MULTI-CUT]: Sliced @ polygons."_s, count));
                }
                doc->EndUndo();
                EventAdd();
            }
            m_sliceDrag.active = false;
            m_sliceDrag.result.valid = false;
            m_sliceDrag.result.cuts.Reset();
            m_sliceDrag.result.previewSegments.Reset();
            m_multiCutPoints.Reset();
            m_multiCutPreview.valid = false;
            m_multiCutPreview.cuts.Reset();
            m_multiCutPreview.previewSegments.Reset();
            m_multiCutHover = MultiCutPoint();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }
        else
        {
            // Click: Add point to m_multiCutPoints
            if (m_multiCutHover.type != MultiCutSnapType::None)
            {
                m_multiCutPoints.Append(m_multiCutHover) iferr_ignore("Append cut pt");
                m_multiCutPreview = m_builder.BuildMultiCutFromPoints(bd, retopo, target, m_snapper, m_multiCutPoints, nullptr);
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }
        return true;
    }

    // ==========================================
    // ACTION 3: SHIFT (alone) + LMB -> CREATE QUAD OR MAYA-STYLE RELAX BRUSH
    // ==========================================
    if ((qualifier & QSHIFT) && !(qualifier & QCTRL))
    {
        Vector norm(0.0, 1.0, 0.0);
        if (target)
        {
            SnapResult snap = m_snapper.RaycastSurface(bd, target, mx, my);
            if (snap.valid) norm = snap.normal;
        }
        else if (bd)
        {
            norm = -bd->GetMg().sqmat.v3;
        }

        QuadPreview qp = m_shiftQuadPreview.valid ? m_shiftQuadPreview : m_builder.FindPotentialQuad(bd, retopo, norm, mx, my, target, &m_snapper);

        // Sub-case 3A: Empty prospective quad under cursor -> Click to create quad
        if (qp.valid && m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper) == NOTOK)
        {
            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);
            if (m_builder.AddQuad(retopo, qp.v[0], qp.v[1], qp.v[2], qp.v[3], qp.normal))
            {
                StatusSetText("QuadDraw: Quad polygon created!"_s);
                m_shiftQuadPreview.valid = false;
            }
            else
            {
                StatusSetText("QuadDraw: Cannot create quad — polygon already exists or edge is closed."_s);
            }
            doc->EndUndo();
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }

        // Sub-case 3B: Over polygons or edges -> Maya-style Relax Brush Drag!
        Float brushRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float strength = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);
        Int32 relaxMode = data.GetInt32(QUADDRAW_RELAX_MODE, QUADDRAW_RELAX_MODE_AUTOLOCK);
        Bool visibleOnly = data.GetBool(QUADDRAW_RELAX_VISIBLE_ONLY, true);

        Bool lockBorder = false;
        Bool lockInterior = false;

        if (relaxMode == QUADDRAW_RELAX_MODE_INTERIOR)
        {
            lockBorder = true;
            lockInterior = false;
        }
        else if (relaxMode == QUADDRAW_RELAX_MODE_BORDER)
        {
            lockBorder = false;
            lockInterior = true;
        }
        else if (relaxMode == QUADDRAW_RELAX_MODE_ALL)
        {
            lockBorder = false;
            lockInterior = false;
        }
        else // QUADDRAW_RELAX_MODE_AUTOLOCK
        {
            // Auto-lock logic (Maya):
            // If stroke starts on/near border -> relax border, lock interior.
            // If stroke starts on interior -> relax interior, lock border.
            Bool startOnBorder = m_builder.IsCursorNearBorder(retopo, bd, mx, my, brushRadius, target, &m_snapper, visibleOnly);
            if (startOnBorder)
            {
                lockBorder = false;
                lockInterior = true;
            }
            else
            {
                lockBorder = true;
                lockInterior = false;
            }
        }

        doc->StartUndo();
        doc->AddUndo(UNDOTYPE::CHANGE, retopo);

        m_isRelaxDragging = true;
        m_relaxLockBorder = lockBorder;
        m_relaxLockInterior = lockInterior;
        m_cursorX = mx;
        m_cursorY = my;

        // Perform initial relaxation step at click position
        m_builder.RelaxVertices(retopo, target, m_snapper, bd, mx, my, brushRadius, strength, lockBorder, lockInterior, visibleOnly);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            mx += dx;
            my += dy;
            m_cursorX = mx;
            m_cursorY = my;

            m_builder.RelaxVertices(retopo, target, m_snapper, bd, mx, my, brushRadius, strength, lockBorder, lockInterior, visibleOnly);

            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }

        win->MouseDragEnd();
        m_isRelaxDragging = false;
        m_relaxLockBorder = false;
        m_relaxLockInterior = false;
        doc->EndUndo();
        EventAdd();
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);

        if (lockInterior)
            StatusSetText("QuadDraw: Relaxed border vertices (interior locked)."_s);
        else if (lockBorder)
            StatusSetText("QuadDraw: Relaxed interior vertices (border locked)."_s);
        else
            StatusSetText("QuadDraw: Relaxed all vertices."_s);

        return true;
    }

    // ==========================================
    // ACTION 4: NORMAL LMB -> TWEAK (DRAG VERTEX / EDGE / POLYGON) OR PLACE POINT
    // ==========================================
    Float polyZ = 1e30;
    Int32 underPoly = (retopo && retopo->GetPolygonCount() > 0)
        ? m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper, &polyZ) : NOTOK;

    Int32 hitV = NOTOK;
    EdgeHit hitEdge;
    Int32 hitPoly = NOTOK;

    if (underPoly != NOTOK && underPoly < retopo->GetPolygonCount())
    {
        // SITUATION A: Cursor over a polygon -> interact ONLY with its components!
        const CPolygon& p = retopo->GetPolygonR()[underPoly];
        const Vector* pts = retopo->GetPointR();
        Matrix rMg = retopo->GetMg();

        Int32 polyVerts[4] = { p.a, p.b, p.c, (p.c != p.d) ? p.d : NOTOK };
        Int32 vertCount = (p.c != p.d) ? 4 : 3;

        Float bestVertDist = 10.0;
        for (Int32 vi = 0; vi < vertCount; ++vi)
        {
            Int32 vIdx = polyVerts[vi];
            if (vIdx == NOTOK) continue;
            Vector sPos = bd->WS(rMg * pts[vIdx]);
            if (sPos.z <= 0.0) continue;
            Float dx = sPos.x - mx, dy = sPos.y - my;
            Float d = std::sqrt(dx * dx + dy * dy);
            if (d <= bestVertDist)
            {
                bestVertDist = d;
                hitV = vIdx;
            }
        }

        if (hitV == NOTOK)
        {
            EdgeHit polyEdge = m_builder.FindClosestEdgeOfPolygon(bd, retopo, underPoly, mx, my);
            if (polyEdge.valid && polyEdge.dist <= 8.0)
            {
                hitEdge = polyEdge;
            }
            else
            {
                hitPoly = underPoly;
            }
        }
    }
    else if (retopo && retopo->GetPointCount() > 0)
    {
        // SITUATION B: Cursor in empty space outside polygons
        hitV = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 10.0, NOTOK, target);
        if (hitV == NOTOK && retopo->GetPolygonCount() > 0)
        {
            hitEdge = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 8.0, target);
        }
    }

    // Priority 1: Vertex Drag
    if (hitV != NOTOK)
    {
        Vector initVertexPos = retopo->GetMg() * retopo->GetPointR()[hitV];
        Vector vScreen = bd->WS(initVertexPos);
        if (vScreen.z <= 0.0) return true;

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        Float totalDx = 0.0, totalDy = 0.0;
        Bool isDragging = false;
        Vector lastValidPos = initVertexPos;
        Vector lastValidNorm(0.0, 1.0, 0.0);
        const Float DRAG_THRESHOLD = 3.0;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            totalDx += dx;
            totalDy += dy;

            if (!isDragging)
            {
                Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
                if (distSoFar < DRAG_THRESHOLD)
                    continue;

                isDragging = true;
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                m_activeDragMode = TweakMode::Vertex;
                m_dragVertexIdx = hitV;
                m_weldTargetIdx = NOTOK;
            }

            Vector movePos = initVertexPos;
            Bool hasMovePos = false;

            Float currVx = vScreen.x + totalDx;
            Float currVy = vScreen.y + totalDy;

            if (target)
            {
                SnapResult surfaceHit = m_snapper.RaycastSurface(bd, target, currVx, currVy);
                if (surfaceHit.valid)
                {
                    movePos = surfaceHit.worldPos;
                    lastValidPos = movePos;
                    lastValidNorm = surfaceHit.normal;
                    hasMovePos = true;
                }
                else
                {
                    Vector candidate = bd->SW_Reference(currVx, currVy, lastValidPos);
                    SnapResult proj = m_snapper.ProjectPointAlongNormal(target, candidate, lastValidNorm, 500.0);
                    if (proj.valid)
                    {
                        movePos = proj.worldPos;
                        hasMovePos = true;
                    }
                }
            }

            if (!hasMovePos && bd)
            {
                movePos = bd->SW_Reference(currVx, currVy, initVertexPos);
                hasMovePos = true;
            }

            if (hasMovePos)
            {
                Vector curScreen = bd->WS(movePos);
                Int32 weldTarget = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen.x, curScreen.y, 12.0, hitV, target);
                if (weldTarget != NOTOK && weldTarget != hitV)
                {
                    Vector targetPos = retopo->GetMg() * retopo->GetPointR()[weldTarget];
                    Vector targetScreen = bd->WS(targetPos);

                    // 1. Depth check: must be on the same surface layer
                    Float maxDepthDiff = maxon::Max(Float(6.0), Float(curScreen.z * 0.015));
                    Bool depthOk = (std::abs(targetScreen.z - curScreen.z) <= maxDepthDiff);

                    // 2. World distance check: 12 pixels at current depth in world units
                    Vector p0 = bd->SW(Vector(curScreen.x, curScreen.y, curScreen.z));
                    Vector p1 = bd->SW(Vector(curScreen.x + 12.0, curScreen.y, curScreen.z));
                    Float maxWorldDist = (p1 - p0).GetLength() * 2.0;
                    Bool distOk = ((targetPos - movePos).GetLength() <= maxWorldDist);

                    if (depthOk && distOk)
                    {
                        m_weldTargetIdx = weldTarget;
                        m_builder.SetVertexPosition(retopo, hitV, targetPos);
                        StatusSetText(FormatString("QuadDraw: Release to Weld vertex #@ into #@"_s, hitV, m_weldTargetIdx));
                    }
                    else
                    {
                        weldTarget = NOTOK;
                    }
                }

                if (weldTarget == NOTOK)
                {
                    m_weldTargetIdx = NOTOK;
                    m_builder.SetVertexPosition(retopo, hitV, movePos);
                    StatusSetText(FormatString("QuadDraw: Moving vertex #@"_s, hitV));
                }
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();

        if (isDragging)
        {
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                doc->DoUndo(true);
            }
            else if (m_weldTargetIdx != NOTOK && m_weldTargetIdx != hitV)
            {
                Int32 targetV = m_weldTargetIdx;
                if (targetV > hitV)
                    targetV--;
                m_builder.WeldVertices(retopo, hitV, m_weldTargetIdx);
                StatusSetText("QuadDraw: Vertices welded!"_s);
                doc->EndUndo();
                EventAdd();

                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = targetV;
            }
            else
            {
                StatusSetText(FormatString("QuadDraw: Vertex #@ moved."_s, hitV));
                doc->EndUndo();
                EventAdd();

                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = hitV;
            }
        }

        m_cursorX = mx + totalDx;
        m_cursorY = my + totalDy;
        m_dragVertexIdx = NOTOK;
        m_weldTargetIdx = NOTOK;
        m_activeDragMode = TweakMode::None;
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Check if edge is hovered in m_hoverTweak if hitEdge wasn't directly found
    if (!hitEdge.valid && hitV == NOTOK && m_hoverTweak.mode == TweakMode::Edge)
    {
        hitEdge.valid = true;
        hitEdge.v0 = m_hoverTweak.edgeV0;
        hitEdge.v1 = m_hoverTweak.edgeV1;
    }

    // Priority 2: Edge Drag
    if (hitEdge.valid)
    {
        Int32 v0 = hitEdge.v0;
        Int32 v1 = hitEdge.v1;

        // Check if Extrude Border Edge on LMB Drag is enabled
        Bool extrudeLMB = (activeTool != QUADDRAW_TOOL_MOVE) && data.GetBool(QUADDRAW_BORDER_EXTRUDE_LMB, true);
        if (extrudeLMB)
        {
            Int32 polyCount = retopo->GetPolygonCount();
            const CPolygon* oldPolys = retopo->GetPolygonR();
            Int32 edgePolyCount = 0;
            for (Int32 i = 0; i < polyCount; ++i)
            {
                if (PolygonHasEdge(oldPolys[i], v0, v1))
                    edgePolyCount++;
            }

            if (edgePolyCount < 2)
            {
                return DoExtrudeEdgeDrag(doc, data, bd, win, retopo, target, v0, v1, mx, my, KEY_MLEFT);
            }
        }
        Vector initP0 = retopo->GetMg() * retopo->GetPointR()[v0];
        Vector initP1 = retopo->GetMg() * retopo->GetPointR()[v1];
        Vector s0 = bd->WS(initP0);
        Vector s1 = bd->WS(initP1);
        if (s0.z <= 0.0 || s1.z <= 0.0) return true;

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        Float totalDx = 0.0, totalDy = 0.0;
        Bool isDragging = false;
        Vector lastP0 = initP0, lastP1 = initP1;
        Vector lastNorm0(0.0, 1.0, 0.0), lastNorm1(0.0, 1.0, 0.0);
        const Float DRAG_THRESHOLD = 3.0;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            totalDx += dx;
            totalDy += dy;

            if (!isDragging)
            {
                Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
                if (distSoFar < DRAG_THRESHOLD)
                    continue;

                isDragging = true;
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                m_activeDragMode = TweakMode::Edge;
                m_dragEdgeV0 = v0;
                m_dragEdgeV1 = v1;
            }

            Vector p0 = initP0, p1 = initP1;
            Bool hasP0 = false, hasP1 = false;

            Float currS0x = s0.x + totalDx;
            Float currS0y = s0.y + totalDy;
            Float currS1x = s1.x + totalDx;
            Float currS1y = s1.y + totalDy;

            if (target)
            {
                SnapResult h0 = m_snapper.RaycastSurface(bd, target, currS0x, currS0y);
                if (h0.valid)
                {
                    p0 = h0.worldPos;
                    lastP0 = p0;
                    lastNorm0 = h0.normal;
                    hasP0 = true;
                }
                else
                {
                    Vector cand0 = bd->SW_Reference(currS0x, currS0y, lastP0);
                    SnapResult pr0 = m_snapper.ProjectPointAlongNormal(target, cand0, lastNorm0, 500.0);
                    if (pr0.valid) { p0 = pr0.worldPos; hasP0 = true; }
                }

                SnapResult h1 = m_snapper.RaycastSurface(bd, target, currS1x, currS1y);
                if (h1.valid)
                {
                    p1 = h1.worldPos;
                    lastP1 = p1;
                    lastNorm1 = h1.normal;
                    hasP1 = true;
                }
                else
                {
                    Vector cand1 = bd->SW_Reference(currS1x, currS1y, lastP1);
                    SnapResult pr1 = m_snapper.ProjectPointAlongNormal(target, cand1, lastNorm1, 500.0);
                    if (pr1.valid) { p1 = pr1.worldPos; hasP1 = true; }
                }
            }

            if (!hasP0 && bd)
            {
                p0 = bd->SW_Reference(currS0x, currS0y, initP0);
                hasP0 = true;
            }
            if (!hasP1 && bd)
            {
                p1 = bd->SW_Reference(currS1x, currS1y, initP1);
                hasP1 = true;
            }

            if (hasP0 && hasP1)
            {
                m_builder.SetVertexPosition(retopo, v0, p0);
                m_builder.SetVertexPosition(retopo, v1, p1);
                StatusSetText(FormatString("QuadDraw: Moving edge (#@ - #@)"_s, v0, v1));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();

        if (isDragging)
        {
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                doc->DoUndo(true);
            }
            else
            {
                StatusSetText(FormatString("QuadDraw: Edge (#@ - #@) moved."_s, v0, v1));
                doc->EndUndo();
                EventAdd();

                m_hoverTweak.mode = TweakMode::Edge;
                m_hoverTweak.edgeV0 = v0;
                m_hoverTweak.edgeV1 = v1;
                m_hoverTweak.edgeWorld0 = retopo->GetMg() * retopo->GetPointR()[v0];
                m_hoverTweak.edgeWorld1 = retopo->GetMg() * retopo->GetPointR()[v1];
            }
        }

        m_cursorX = mx + totalDx;
        m_cursorY = my + totalDy;
        m_dragEdgeV0 = NOTOK;
        m_dragEdgeV1 = NOTOK;
        m_activeDragMode = TweakMode::None;
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Priority 3: Polygon Drag
    if (hitPoly != NOTOK && hitPoly < retopo->GetPolygonCount())
    {
        const CPolygon& p = retopo->GetPolygonR()[hitPoly];
        Int32 numPts = (p.c != p.d) ? 4 : 3;
        Int32 polyPts[4] = { p.a, p.b, p.c, p.d };

        Vector initPts[4];
        Vector sPts[4];
        Vector lastPts[4];
        Vector lastNorms[4];
        Bool allVisible = true;
        for (Int32 k = 0; k < numPts; ++k)
        {
            initPts[k] = retopo->GetMg() * retopo->GetPointR()[polyPts[k]];
            lastPts[k] = initPts[k];
            lastNorms[k] = Vector(0.0, 1.0, 0.0);
            sPts[k] = bd->WS(initPts[k]);
            if (sPts[k].z <= 0.0) allVisible = false;
        }
        if (!allVisible) return true;

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        Float totalDx = 0.0, totalDy = 0.0;
        Bool isDragging = false;
        const Float DRAG_THRESHOLD = 3.0;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            totalDx += dx;
            totalDy += dy;

            if (!isDragging)
            {
                Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
                if (distSoFar < DRAG_THRESHOLD)
                    continue;

                isDragging = true;
                doc->StartUndo();
                doc->AddUndo(UNDOTYPE::CHANGE, retopo);
                m_activeDragMode = TweakMode::Polygon;
                m_dragPolyIdx = hitPoly;
                m_dragPolyNumPts = numPts;
                for (Int32 k = 0; k < 4; ++k) m_dragPolyPts[k] = polyPts[k];
            }

            Vector newPts[4];
            Bool allNewPtsValid = true;

            for (Int32 k = 0; k < numPts; ++k)
            {
                Float currSx = sPts[k].x + totalDx;
                Float currSy = sPts[k].y + totalDy;

                Bool gotPt = false;
                if (target)
                {
                    SnapResult hk = m_snapper.RaycastSurface(bd, target, currSx, currSy);
                    if (hk.valid)
                    {
                        newPts[k] = hk.worldPos;
                        lastPts[k] = newPts[k];
                        lastNorms[k] = hk.normal;
                        gotPt = true;
                    }
                    else
                    {
                        Vector cand = bd->SW_Reference(currSx, currSy, lastPts[k]);
                        SnapResult pr = m_snapper.ProjectPointAlongNormal(target, cand, lastNorms[k], 500.0);
                        if (pr.valid)
                        {
                            newPts[k] = pr.worldPos;
                            gotPt = true;
                        }
                    }
                }

                if (!gotPt && bd)
                {
                    newPts[k] = bd->SW_Reference(currSx, currSy, initPts[k]);
                    gotPt = true;
                }

                if (!gotPt)
                {
                    allNewPtsValid = false;
                    break;
                }
            }

            if (allNewPtsValid)
            {
                for (Int32 k = 0; k < numPts; ++k)
                {
                    m_builder.SetVertexPosition(retopo, polyPts[k], newPts[k]);
                }
                StatusSetText(FormatString("QuadDraw: Moving polygon #@"_s, hitPoly));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();

        if (isDragging)
        {
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                doc->DoUndo(true);
            }
            else
            {
                StatusSetText(FormatString("QuadDraw: Polygon #@ moved."_s, hitPoly));
                doc->EndUndo();
                EventAdd();

                m_hoverTweak.mode = TweakMode::Polygon;
                m_hoverTweak.index = hitPoly;
                m_hoverTweak.polyIsQuad = (numPts == 4);
                m_hoverTweak.polyPts[0] = polyPts[0];
                m_hoverTweak.polyPts[1] = polyPts[1];
                m_hoverTweak.polyPts[2] = polyPts[2];
                m_hoverTweak.polyPts[3] = polyPts[3];
                for (Int32 k = 0; k < numPts; ++k)
                    m_hoverTweak.polyWorld[k] = retopo->GetMg() * retopo->GetPointR()[polyPts[k]];
            }
        }

        m_cursorX = mx + totalDx;
        m_cursorY = my + totalDy;
        m_dragPolyIdx = NOTOK;
        m_dragPolyNumPts = 0;
        m_activeDragMode = TweakMode::None;
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Priority 4: Place Point
    if (activeTool == QUADDRAW_TOOL_MOVE)
    {
        return true; // In Move mode, do not place points on empty surface
    }
    Vector dropPos;
    Bool canPlace = false;
    SnapResult snap;
    if (target)
    {
        snap = m_snapper.RaycastSurface(bd, target, mx, my);
        if (snap.valid)
        {
            dropPos = snap.worldPos;
            canPlace = true;
        }
    }
    else if (bd && retopo)
    {
        Vector refPt = retopo->GetMg().off;
        if (retopo->GetPointCount() > 0)
        {
            refPt = retopo->GetMg() * retopo->GetPointR()[0];
        }
        dropPos = bd->SW_Reference(mx, my, refPt);
        canPlace = true;
    }

    if (canPlace)
    {
        if (m_builder.FindPolygonUnderScreen(bd, retopo, mx, my, target, &m_snapper, nullptr, snap.valid ? &snap : nullptr) != NOTOK)
        {
            StatusSetText("QuadDraw: Cannot place point — polygon already exists here."_s);
            return true;
        }

        doc->StartUndo();
        doc->AddUndo(UNDOTYPE::CHANGE, retopo);
        Int32 dragIdx = m_builder.AddVertex(retopo, dropPos);

        if (dragIdx != NOTOK)
        {
            m_activeDragMode = TweakMode::Vertex;
            m_dragVertexIdx = dragIdx;
            m_weldTargetIdx = NOTOK;

            BaseContainer device;
            win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

            Float dx, dy;
            Float totalDx = 0.0, totalDy = 0.0;
            Bool hasMoved = false;
            const Float DRAG_THRESHOLD = 3.0;

            while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
            {
                if (dx == 0.0 && dy == 0.0) continue;
                totalDx += dx;
                totalDy += dy;

                if (!hasMoved)
                {
                    Float distSoFar = std::sqrt(totalDx * totalDx + totalDy * totalDy);
                    if (distSoFar < DRAG_THRESHOLD)
                        continue;
                    hasMoved = true;
                }

                Float currX = mx + totalDx;
                Float currY = my + totalDy;

                Vector newPos;
                Bool hasNewPos = false;
                if (target)
                {
                    SnapResult surfaceHit = m_snapper.RaycastSurface(bd, target, currX, currY);
                    if (surfaceHit.valid)
                    {
                        newPos = surfaceHit.worldPos;
                        hasNewPos = true;
                    }
                }
                else if (bd)
                {
                    newPos = bd->SW_Reference(currX, currY, dropPos);
                    hasNewPos = true;
                }

                if (hasNewPos)
                {
                    Vector curScreen = bd->WS(newPos);
                    Int32 weldTarget = m_snapper.FindNearestRetopoVertex(bd, retopo, curScreen.x, curScreen.y, 12.0, dragIdx, target);
                    if (weldTarget != NOTOK && weldTarget != dragIdx)
                    {
                        Vector targetPos = retopo->GetMg() * retopo->GetPointR()[weldTarget];
                        Vector targetScreen = bd->WS(targetPos);

                        // 1. Depth check: must be on the same surface layer
                        Float maxDepthDiff = maxon::Max(Float(6.0), Float(curScreen.z * 0.015));
                        Bool depthOk = (std::abs(targetScreen.z - curScreen.z) <= maxDepthDiff);

                        // 2. World distance check: 12 pixels at current depth in world units
                        Vector p0 = bd->SW(Vector(curScreen.x, curScreen.y, curScreen.z));
                        Vector p1 = bd->SW(Vector(curScreen.x + 12.0, curScreen.y, curScreen.z));
                        Float maxWorldDist = (p1 - p0).GetLength() * 2.0;
                        Bool distOk = ((targetPos - newPos).GetLength() <= maxWorldDist);

                        if (depthOk && distOk)
                        {
                            m_weldTargetIdx = weldTarget;
                            m_builder.SetVertexPosition(retopo, dragIdx, targetPos);
                            StatusSetText(FormatString("QuadDraw: Release to Weld vertex #@ into #@"_s, dragIdx, m_weldTargetIdx));
                        }
                        else
                        {
                            weldTarget = NOTOK;
                        }
                    }

                    if (weldTarget == NOTOK)
                    {
                        m_weldTargetIdx = NOTOK;
                        m_builder.SetVertexPosition(retopo, dragIdx, newPos);
                        StatusSetText(FormatString("QuadDraw: Moving vertex #@"_s, dragIdx));
                    }
                    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                }
            }

            MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                doc->DoUndo(true);
                m_hoverTweak.mode = TweakMode::None;
            }
            else if (hasMoved && m_weldTargetIdx != NOTOK && m_weldTargetIdx != dragIdx)
            {
                Int32 targetV = m_weldTargetIdx;
                if (targetV > dragIdx)
                    targetV--;
                m_builder.WeldVertices(retopo, dragIdx, m_weldTargetIdx);
                StatusSetText("QuadDraw: Vertices welded!"_s);
                doc->EndUndo();
                EventAdd();
                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = targetV;
            }
            else
            {
                StatusSetText(FormatString("QuadDraw: Point #@ placed."_s, dragIdx));
                doc->EndUndo();
                EventAdd();
                m_hoverTweak.mode = TweakMode::Vertex;
                m_hoverTweak.index = dragIdx;
            }

            m_hoverSnap.valid = false;
            m_cursorX = mx + totalDx;
            m_cursorY = my + totalDy;
            m_dragVertexIdx = NOTOK;
            m_weldTargetIdx = NOTOK;
            m_activeDragMode = TweakMode::None;
            EventAdd();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }
    }

    return true;
}

TOOLDRAW QuadDrawToolData::Draw(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, BaseDrawHelp* bh, BaseThread* bt, TOOLDRAWFLAGS flags)
{
    if (!doc || !bd) return TOOLDRAW::NONE;

    bd->SetMatrix_Matrix(nullptr, Matrix());

    // Read user display & color settings from tool data
    const Vector defaultFaceColor(0.0, 150.0 / 255.0, 1.0); // 0 150 255
    const Vector defaultWireColor(0.0, 0.0, 0.0);           // Black
    const Vector defaultHighlightColor(1.0, 1.0, 1.0);      // White

    Bool disableCustomShading = data.GetBool(QUADDRAW_DISABLE_CUSTOM_SHADING, true);
    Bool disableXRay          = data.GetBool(QUADDRAW_DISABLE_XRAY, true);

    // If X-Ray is disabled, completely reject Cinema 4D's occluded / Inverse-Z draw pass!
    if (disableXRay && (flags & TOOLDRAWFLAGS::INVERSE_Z))
        return TOOLDRAW::NONE;

    Vector faceColor = data.GetVector(QUADDRAW_MESH_COLOR, defaultFaceColor);
    Float faceOpacity = data.GetFloat(QUADDRAW_FACE_OPACITY, 0.35);
    if (faceOpacity > 1.0) faceOpacity /= 100.0;
    if (faceOpacity < 0.0) faceOpacity = 0.0;
    if (faceOpacity > 1.0) faceOpacity = 1.0;
    Int32 transVal = (faceOpacity >= 0.999) ? 0 : -Int32((1.0 - faceOpacity) * 255.0);

    Vector wireColor = data.GetVector(QUADDRAW_WIRE_COLOR, defaultWireColor);
    Float lineWidth = data.GetFloat(QUADDRAW_LINE_WIDTH, 1.0);
    if (lineWidth < 1.0) lineWidth = 1.0;
    if (lineWidth > 10.0) lineWidth = 10.0;

    Float pointSize = data.GetFloat(QUADDRAW_POINT_SIZE, 6.0);
    if (pointSize < 1.0) pointSize = 1.0;
    if (pointSize > 10.0) pointSize = 10.0;

    Vector previewColor = data.GetVector(QUADDRAW_PREVIEW_COLOR, Vector(0.15, 0.85, 0.45));
    Vector cutColor = data.GetVector(QUADDRAW_CUT_COLOR, Vector(0.2, 1.0, 0.4));
    Vector highlightColor = data.GetVector(QUADDRAW_HIGHLIGHT_COLOR, defaultHighlightColor);

    Float hoverLineWidth = data.GetFloat(QUADDRAW_HOVER_LINE_WIDTH, 1.4);
    if (hoverLineWidth < 1.0) hoverLineWidth = 1.0;
    if (hoverLineWidth > 10.0) hoverLineWidth = 10.0;

    GeData oldLineWidth = bd->GetDrawParam(DRAW_PARAMETER_LINEWIDTH);
    GeData oldUseZ = bd->GetDrawParam(DRAW_PARAMETER_USE_Z);
    GeData oldSetZ = bd->GetDrawParam(DRAW_PARAMETER_SETZ);
    bd->SetDrawParam(DRAW_PARAMETER_LINEWIDTH, GeData(lineWidth));

    auto drawThickLine = [&](const Vector& p1, const Vector& p2, Float width, Bool depthTest = false, Int32 zOffset = 2)
    {
        if (depthTest)
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
            bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
            bd->LineZOffset(zOffset);
        }
        else
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
        }

        bd->DrawLine(p1, p2, 0);
        if (width >= 1.35)
        {
            Vector s1 = bd->WS(p1);
            Vector s2 = bd->WS(p2);
            Vector dir = Vector(s2.x - s1.x, s2.y - s1.y, 0.0);
            Float len = Sqrt(dir.x * dir.x + dir.y * dir.y);
            if (len > 0.001)
            {
                Vector perp(-dir.y / len, dir.x / len, 0.0);
                Int32 extraSteps = (Int32)Floor((width - 0.7) * 0.8) + 1;
                if (extraSteps < 1) extraSteps = 1;
                if (extraSteps > 5) extraSteps = 5;

                for (Int32 step = 1; step <= extraSteps; ++step)
                {
                    Float offset = Float(step);
                    Vector p1_a = bd->SW(Vector(s1.x + perp.x * offset, s1.y + perp.y * offset, s1.z));
                    Vector p2_a = bd->SW(Vector(s2.x + perp.x * offset, s2.y + perp.y * offset, s2.z));
                    Vector p1_b = bd->SW(Vector(s1.x - perp.x * offset, s1.y - perp.y * offset, s1.z));
                    Vector p2_b = bd->SW(Vector(s2.x - perp.x * offset, s2.y - perp.y * offset, s2.z));
                    bd->DrawLine(p1_a, p2_a, 0);
                    bd->DrawLine(p1_b, p2_b, 0);
                }
            }
        }

        if (depthTest)
            bd->LineZOffset(0);
    };

    auto drawPoint = [&](const Vector& p, const Vector& col, Float size, Bool depthTest = false, const Vector& toCam = Vector(0.0))
    {
        DRAWHANDLE hType = DRAWHANDLE::MIDDLE;
        if (size <= 1.5)      hType = DRAWHANDLE::MINI;
        else if (size <= 2.5) hType = DRAWHANDLE::SMALL;
        else if (size <= 4.0) hType = DRAWHANDLE::MIDDLE;
        else if (size <= 6.5) hType = DRAWHANDLE::BIG;
        else                  hType = DRAWHANDLE::VERYBIG;

        if (depthTest)
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
            bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
            bd->LineZOffset(3);
            bd->SetPen(col);
            Vector drawP = (toCam.GetSquaredLength() > 0.5) ? (p + toCam * 0.05) : p;
            bd->DrawHandle(drawP, hType, 0);
            bd->LineZOffset(0);
        }
        else
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
            bd->DrawHandleWorld(p, col, hType);
        }

        if (size >= 7.5)
        {
            Vector s = bd->WS(p);
            if (s.z > 0.0)
            {
                Float r = Floor(size * 0.5);
                Vector pA = bd->SW(Vector(s.x - r, s.y - r, s.z));
                Vector pB = bd->SW(Vector(s.x + r, s.y - r, s.z));
                Vector pC = bd->SW(Vector(s.x + r, s.y + r, s.z));
                Vector pD = bd->SW(Vector(s.x - r, s.y + r, s.z));
                bd->SetPen(col);
                if (depthTest)
                {
                    bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
                    bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
                    bd->LineZOffset(3);
                }
                else
                {
                    bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
                }
                bd->DrawLine(pA, pB, 0);
                bd->DrawLine(pB, pC, 0);
                bd->DrawLine(pC, pD, 0);
                bd->DrawLine(pD, pA, 0);
                if (depthTest)
                    bd->LineZOffset(0);
            }
        }
    };

    PolygonObject* retopo = GetEditableMesh(doc, false);

    // 1. Draw existing retopo polygons in user face color with transparency and wireframe lines (if custom mesh shading is enabled)
    if (!disableCustomShading && retopo && retopo->GetPolygonCount() > 0)
    {
        Int32 polyCount = retopo->GetPolygonCount();
        const CPolygon* polys = retopo->GetPolygonR();
        const Vector* pts = retopo->GetPointR();
        Matrix rMg = retopo->GetMg();

        Vector camPos = bd->GetMg().off;
        Bool isOrtho = (bd->GetProjection() != Pperspective);
        Vector orthoLook = -bd->GetMg().sqmat.v3.GetNormalized();

        Vector faceColors[4] = { faceColor, faceColor, faceColor, faceColor };

        if (disableXRay)
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
            bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
            bd->LineZOffset(0);
        }
        else
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
        }

        // Draw translucent faces so underlying target mesh remains visible
        bd->SetTransparency(transVal);
        for (Int32 i = 0; i < polyCount; ++i)
        {
            const CPolygon& p = polys[i];
            Bool isQuad = (p.c != p.d);

            Vector qPts[4] = {
                rMg * pts[p.a],
                rMg * pts[p.b],
                rMg * pts[p.c],
                rMg * pts[p.d]
            };

            if (disableXRay)
            {
                // Backface culling: do not draw back-facing polygons through front geometry
                Vector fn = Cross(qPts[1] - qPts[0], qPts[2] - qPts[0]);
                Vector polyCenter = (qPts[0] + qPts[1] + qPts[2]) * (1.0 / 3.0);
                Vector toCam = isOrtho ? orthoLook : (camPos - polyCenter).GetNormalized();
                if (Dot(fn, toCam) <= 0.0)
                    continue;
            }

            bd->DrawPolygon(qPts, faceColors, isQuad);
        }
        bd->DrawArrayEnd();

        // Draw wireframe lines in user-configured wire color and thickness
        bd->SetTransparency(0);
        bd->SetPen(wireColor);
        for (Int32 i = 0; i < polyCount; ++i)
        {
            const CPolygon& p = polys[i];
            Bool isQuad = (p.c != p.d);

            Vector qPts[4] = {
                rMg * pts[p.a],
                rMg * pts[p.b],
                rMg * pts[p.c],
                rMg * pts[p.d]
            };

            if (disableXRay)
            {
                Vector fn = Cross(qPts[1] - qPts[0], qPts[2] - qPts[0]);
                Vector polyCenter = (qPts[0] + qPts[1] + qPts[2]) * (1.0 / 3.0);
                Vector toCam = isOrtho ? orthoLook : (camPos - polyCenter).GetNormalized();
                if (Dot(fn, toCam) <= 0.0)
                    continue;
            }

            drawThickLine(qPts[0], qPts[1], lineWidth, disableXRay, 2);
            drawThickLine(qPts[1], qPts[2], lineWidth, disableXRay, 2);
            if (isQuad)
            {
                drawThickLine(qPts[2], qPts[3], lineWidth, disableXRay, 2);
                drawThickLine(qPts[3], qPts[0], lineWidth, disableXRay, 2);
            }
            else
            {
                drawThickLine(qPts[2], qPts[0], lineWidth, disableXRay, 2);
            }
        }
    }

    // 2. Draw prospective Quad preview when holding Shift
    if (m_shiftHeld && !m_ctrlHeld && m_shiftQuadPreview.valid)
    {
        Vector prevPts[4] = {
            m_shiftQuadPreview.worldPositions[0],
            m_shiftQuadPreview.worldPositions[1],
            m_shiftQuadPreview.worldPositions[2],
            m_shiftQuadPreview.worldPositions[3]
        };

        if (disableXRay)
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
            bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
            bd->LineZOffset(0);
        }
        else
        {
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
        }

        Vector greenColors[4] = { previewColor, previewColor, previewColor, previewColor };

        bd->SetTransparency(transVal);
        bd->DrawPolygon(prevPts, greenColors, true);
        bd->DrawArrayEnd();

        // Bright contour lines
        bd->SetTransparency(0);
        bd->SetPen(previewColor);
        drawThickLine(prevPts[0], prevPts[1], hoverLineWidth, disableXRay, 3);
        drawThickLine(prevPts[1], prevPts[2], hoverLineWidth, disableXRay, 3);
        drawThickLine(prevPts[2], prevPts[3], hoverLineWidth, disableXRay, 3);
        drawThickLine(prevPts[3], prevPts[0], hoverLineWidth, disableXRay, 3);

        // Highlight the 4 corner vertices
        for (Int32 k = 0; k < 4; ++k)
        {
            drawPoint(prevPts[k], previewColor, pointSize + 2.0, disableXRay);
        }
    }

    // 2b. Draw Relax Brush circle when holding Shift (or during brush resize)
    if (m_isResizingBrush || (m_shiftHeld && !m_ctrlHeld && (!m_shiftQuadPreview.valid || m_isRelaxDragging)))
    {
        Float relaxRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float cx = m_isResizingBrush ? m_brushResizeCenterX : m_cursorX;
        Float cy = m_isResizingBrush ? m_brushResizeCenterY : m_cursorY;

        bd->SetMatrix_Screen();
        Vector circleColor = m_isResizingBrush ? Vector(1.0, 1.0, 1.0) :
                            (m_isRelaxDragging ? Vector(0.15, 0.9, 1.0) : Vector(0.3, 0.75, 1.0));
        bd->SetPen(circleColor);

        const Int32 numSegs = 48;
        for (Int32 i = 0; i < numSegs; ++i)
        {
            Float a0 = (Float)i * (2.0 * PI / (Float)numSegs);
            Float a1 = (Float)(i + 1) * (2.0 * PI / (Float)numSegs);
            Vector p0(cx + cos(a0) * relaxRadius, cy + sin(a0) * relaxRadius, 0.0);
            Vector p1(cx + cos(a1) * relaxRadius, cy + sin(a1) * relaxRadius, 0.0);
            bd->DrawLine(p0, p1, 0);
        }

        if (m_isResizingBrush)
        {
            // Center crosshair
            bd->DrawLine(Vector(cx - 5.0, cy, 0.0), Vector(cx + 5.0, cy, 0.0), 0);
            bd->DrawLine(Vector(cx, cy - 5.0, 0.0), Vector(cx, cy + 5.0, 0.0), 0);
            // Horizontal radius indicator line to the edge
            bd->DrawLine(Vector(cx, cy, 0.0), Vector(cx + relaxRadius, cy, 0.0), 0);
        }

        bd->SetMatrix_Matrix(nullptr, Matrix());
    }

    // 3A. Draw prospective Edge Loop Cut line when in Knife mode
    if (m_edgeCutPreview.valid)
    {
        bd->SetTransparency(0);
        bd->SetPen(cutColor);

        for (Int32 k = 0; k < (Int32)m_edgeCutPreview.cutSegments.GetCount(); ++k)
        {
            const CutSegment& seg = m_edgeCutPreview.cutSegments[k];
            drawThickLine(seg.p0, seg.p1, hoverLineWidth, disableXRay, 3);
        }

        // Draw handles at each cut point along the edges
        for (Int32 k = 0; k < (Int32)m_edgeCutPreview.cutPoints.GetCount(); ++k)
        {
            const CutPoint& cp = m_edgeCutPreview.cutPoints[k];
            drawPoint(cp.worldPos, cutColor, pointSize, disableXRay);
        }
    }

    // 3A2. Draw Multi-Cut Preview (Placed points, cut path, candidate hover, and slice drag)
    Int32 activeTool = data.GetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);
    if (activeTool == QUADDRAW_TOOL_MULTICUT && !m_ctrlHeld)
    {
        // 1. Draw Slice Drag if active
        if (m_sliceDrag.active)
        {
            // 2D slice line across the screen
            bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
            bd->SetTransparency(0);
            bd->SetPen(Vector(1.0, 1.0, 0.2));
            Vector pA = bd->SW(Vector(m_sliceDrag.startX, m_sliceDrag.startY, 500.0));
            Vector pB = bd->SW(Vector(m_sliceDrag.currX, m_sliceDrag.currY, 500.0));
            bd->DrawLine(pA, pB, 0);

            // Draw 3D cut segments on intersected polygons
            bd->SetPen(cutColor);
            for (Int32 k = 0; k < (Int32)m_sliceDrag.result.previewSegments.GetCount(); ++k)
            {
                const MultiCutSliceSegment& seg = m_sliceDrag.result.previewSegments[k];
                drawThickLine(seg.p0, seg.p1, hoverLineWidth + 1.2, disableXRay, 3);
                drawPoint(seg.p0, cutColor, pointSize + 1.5, disableXRay);
                drawPoint(seg.p1, cutColor, pointSize + 1.5, disableXRay);
            }
        }
        else
        {
            Int32 ptCount = (Int32)m_multiCutPoints.GetCount();
            bd->SetTransparency(0);
            bd->SetPen(cutColor);

            // Draw placed points and connecting lines
            for (Int32 k = 0; k < ptCount; ++k)
            {
                drawPoint(m_multiCutPoints[k].worldPos, cutColor, pointSize + 2.5, disableXRay);
                if (k > 0)
                {
                    drawThickLine(m_multiCutPoints[k - 1].worldPos, m_multiCutPoints[k].worldPos, hoverLineWidth + 1.2, disableXRay, 3);
                }
            }

            // Draw candidate preview segments from m_multiCutPreview ONLY if valid and points placed
            if (m_multiCutPreview.valid && ptCount > 0)
            {
                for (Int32 k = 0; k < (Int32)m_multiCutPreview.previewSegments.GetCount(); ++k)
                {
                    const MultiCutSliceSegment& seg = m_multiCutPreview.previewSegments[k];
                    drawThickLine(seg.p0, seg.p1, hoverLineWidth + 1.0, disableXRay, 3);
                }
            }

            // Draw rubberband line to hover point
            if (ptCount > 0 && m_multiCutHover.type != MultiCutSnapType::None)
            {
                drawThickLine(m_multiCutPoints[ptCount - 1].worldPos, m_multiCutHover.worldPos, hoverLineWidth, disableXRay, 3);
            }

            // Draw hover point and indicator
            if (m_multiCutHover.type == MultiCutSnapType::Vertex)
            {
                drawPoint(m_multiCutHover.worldPos, highlightColor, pointSize + 3.0, disableXRay);
            }
            else if (m_multiCutHover.type == MultiCutSnapType::Edge)
            {
                drawPoint(m_multiCutHover.worldPos, cutColor, pointSize + 2.0, disableXRay);
                if (retopo && m_multiCutHover.edgeV0 != NOTOK && m_multiCutHover.edgeV1 != NOTOK)
                {
                    Int32 numPts = retopo->GetPointCount();
                    if (m_multiCutHover.edgeV0 < numPts && m_multiCutHover.edgeV1 < numPts)
                    {
                        Vector w0 = retopo->GetMg() * retopo->GetPointR()[m_multiCutHover.edgeV0];
                        Vector w1 = retopo->GetMg() * retopo->GetPointR()[m_multiCutHover.edgeV1];
                        drawThickLine(w0, w1, hoverLineWidth * 0.8, disableXRay, 2);
                    }
                }
            }
        }
    }

    // 3B. Draw Component Loop Highlight when holding Ctrl (Vertex Loop, Edge Loop, Polygon Loop)
    if (m_ctrlHeld && !m_shiftHeld && m_activeDragMode == TweakMode::None && m_componentLoop.type != ComponentLoopType::None)
    {
        if (m_componentLoop.type == ComponentLoopType::Vertex)
        {
            // Vertex Loop: prominent points and connecting edge lines
            bd->SetTransparency(0);
            bd->SetPen(highlightColor);

            Matrix rMg = retopo ? retopo->GetMg() : Matrix();
            const Vector* rPts = retopo ? retopo->GetPointR() : nullptr;
            Int32 ptCount = retopo ? retopo->GetPointCount() : 0;

            for (Int32 k = 0; k < (Int32)m_componentLoop.edges.GetCount(); ++k)
            {
                LoopEdge& le = m_componentLoop.edges[k];
                Vector w0 = (rPts && le.v0 >= 0 && le.v0 < ptCount) ? (rMg * rPts[le.v0]) : le.worldPos0;
                Vector w1 = (rPts && le.v1 >= 0 && le.v1 < ptCount) ? (rMg * rPts[le.v1]) : le.worldPos1;
                drawThickLine(w0, w1, hoverLineWidth + 0.5, disableXRay, 3);
            }

            if (rPts)
            {
                for (Int32 k = 0; k < (Int32)m_componentLoop.vertices.GetCount(); ++k)
                {
                    Int32 vi = m_componentLoop.vertices[k];
                    if (vi >= 0 && vi < ptCount)
                    {
                        Vector wPos = rMg * rPts[vi];
                        drawPoint(wPos, highlightColor, pointSize + 3.0, disableXRay);
                    }
                }
            }
        }
        else if (m_componentLoop.type == ComponentLoopType::Edge)
        {
            // Edge Loop: thick lines and endpoint dots
            bd->SetTransparency(0);
            bd->SetPen(highlightColor);

            Matrix rMg = retopo ? retopo->GetMg() : Matrix();
            const Vector* rPts = retopo ? retopo->GetPointR() : nullptr;
            Int32 ptCount = retopo ? retopo->GetPointCount() : 0;

            for (Int32 k = 0; k < (Int32)m_componentLoop.edges.GetCount(); ++k)
            {
                LoopEdge& le = m_componentLoop.edges[k];
                Vector w0 = (rPts && le.v0 >= 0 && le.v0 < ptCount) ? (rMg * rPts[le.v0]) : le.worldPos0;
                Vector w1 = (rPts && le.v1 >= 0 && le.v1 < ptCount) ? (rMg * rPts[le.v1]) : le.worldPos1;
                drawThickLine(w0, w1, hoverLineWidth + 1.2, disableXRay, 3);
                drawPoint(w0, highlightColor, pointSize + 1.5, disableXRay);
                drawPoint(w1, highlightColor, pointSize + 1.5, disableXRay);
            }
        }
        else if (m_componentLoop.type == ComponentLoopType::Polygon)
        {
            // Polygon Loop: semi-transparent faces and highlighted outlines
            if (retopo)
            {
                Matrix rMg = retopo->GetMg();
                const Vector* rPts = retopo->GetPointR();
                const CPolygon* rPolys = retopo->GetPolygonR();
                Int32 rPolyCount = retopo->GetPolygonCount();

                Vector polyColors[4] = { highlightColor, highlightColor, highlightColor, highlightColor };

                if (disableXRay)
                {
                    bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
                    bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
                    bd->LineZOffset(0);
                }
                else
                {
                    bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
                }

                // Draw filled polygons
                bd->SetTransparency(-140);
                for (Int32 i = 0; i < (Int32)m_componentLoop.polygons.GetCount(); ++i)
                {
                    Int32 pIdx = m_componentLoop.polygons[i];
                    if (pIdx < 0 || pIdx >= rPolyCount) continue;
                    const CPolygon& p = rPolys[pIdx];
                    Bool isQuad = (p.c != p.d);
                    Vector wPts[4] = { rMg * rPts[p.a], rMg * rPts[p.b], rMg * rPts[p.c], rMg * rPts[p.d] };
                    bd->DrawPolygon(wPts, polyColors, isQuad);
                }
                bd->DrawArrayEnd();

                // Draw boundary lines for all polygons in the loop
                bd->SetTransparency(0);
                bd->SetPen(highlightColor);
                for (Int32 i = 0; i < (Int32)m_componentLoop.polygons.GetCount(); ++i)
                {
                    Int32 pIdx = m_componentLoop.polygons[i];
                    if (pIdx < 0 || pIdx >= rPolyCount) continue;
                    const CPolygon& p = rPolys[pIdx];
                    Bool isQuad = (p.c != p.d);
                    Vector wPts[4] = { rMg * rPts[p.a], rMg * rPts[p.b], rMg * rPts[p.c], rMg * rPts[p.d] };
                    drawThickLine(wPts[0], wPts[1], hoverLineWidth, disableXRay, 3);
                    drawThickLine(wPts[1], wPts[2], hoverLineWidth, disableXRay, 3);
                    if (isQuad)
                    {
                        drawThickLine(wPts[2], wPts[3], hoverLineWidth, disableXRay, 3);
                        drawThickLine(wPts[3], wPts[0], hoverLineWidth, disableXRay, 3);
                    }
                    else
                    {
                        drawThickLine(wPts[2], wPts[0], hoverLineWidth, disableXRay, 3);
                    }
                }
            }
        }
    }

    // 4. Draw Red Deletion Highlight when Ctrl + Shift are held (Maya QuadDraw Delete Mode)
    if (m_ctrlHeld && m_shiftHeld && m_deleteHighlight.type != DeleteTargetType::None)
    {
        const Vector deleteRed(1.0, 0.15, 0.15);

        if (m_deleteHighlight.type == DeleteTargetType::Vertex)
        {
            // Red highlighted vertex
            drawPoint(m_deleteHighlight.worldPos0, deleteRed, pointSize + 2.0, disableXRay);
        }
        else if (m_deleteHighlight.type == DeleteTargetType::Edge)
        {
            // Red highlighted edge loop (strip of edges)
            bd->SetTransparency(0);
            bd->SetPen(deleteRed);
            if (m_deleteHighlight.loopEdges.GetCount() > 0)
            {
                for (Int32 k = 0; k < (Int32)m_deleteHighlight.loopEdges.GetCount(); ++k)
                {
                    const LoopEdge& le = m_deleteHighlight.loopEdges[k];
                    drawThickLine(le.worldPos0, le.worldPos1, hoverLineWidth, disableXRay, 3);
                    drawPoint(le.worldPos0, deleteRed, pointSize + 1.0, disableXRay);
                    drawPoint(le.worldPos1, deleteRed, pointSize + 1.0, disableXRay);
                }
            }
            else
            {
                drawThickLine(m_deleteHighlight.worldPos0, m_deleteHighlight.worldPos1, hoverLineWidth, disableXRay, 3);
                drawPoint(m_deleteHighlight.worldPos0, deleteRed, pointSize + 1.0, disableXRay);
                drawPoint(m_deleteHighlight.worldPos1, deleteRed, pointSize + 1.0, disableXRay);
            }
        }
        else if (m_deleteHighlight.type == DeleteTargetType::Polygon)
        {
            // Red highlighted polygon face
            const Vector redFace(0.9, 0.18, 0.18);
            Vector redColors[4] = { redFace, redFace, redFace, redFace };

            if (disableXRay)
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
                bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
                bd->LineZOffset(0);
            }
            else
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
            }

            bd->SetTransparency(-140);
            bd->DrawPolygon(m_deleteHighlight.polyPts, redColors, m_deleteHighlight.polyIsQuad);
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(deleteRed);
            drawThickLine(m_deleteHighlight.polyPts[0], m_deleteHighlight.polyPts[1], hoverLineWidth, disableXRay, 3);
            drawThickLine(m_deleteHighlight.polyPts[1], m_deleteHighlight.polyPts[2], hoverLineWidth, disableXRay, 3);
            if (m_deleteHighlight.polyIsQuad)
            {
                drawThickLine(m_deleteHighlight.polyPts[2], m_deleteHighlight.polyPts[3], hoverLineWidth, disableXRay, 3);
                drawThickLine(m_deleteHighlight.polyPts[3], m_deleteHighlight.polyPts[0], hoverLineWidth, disableXRay, 3);
            }
            else
            {
                drawThickLine(m_deleteHighlight.polyPts[2], m_deleteHighlight.polyPts[0], hoverLineWidth, disableXRay, 3);
            }
        }
    }

    // 5. Draw normal mode hover highlight (Edge or Polygon)
    if (!m_shiftHeld && !m_ctrlHeld && m_activeDragMode == TweakMode::None)
    {
        if (m_hoverTweak.mode == TweakMode::Edge)
        {
            Vector w0 = m_hoverTweak.edgeWorld0;
            Vector w1 = m_hoverTweak.edgeWorld1;
            if (retopo && m_hoverTweak.edgeV0 >= 0 && m_hoverTweak.edgeV0 < retopo->GetPointCount() &&
                m_hoverTweak.edgeV1 >= 0 && m_hoverTweak.edgeV1 < retopo->GetPointCount())
            {
                Matrix rMg = retopo->GetMg();
                const Vector* rPts = retopo->GetPointR();
                w0 = rMg * rPts[m_hoverTweak.edgeV0];
                w1 = rMg * rPts[m_hoverTweak.edgeV1];
            }

            bd->SetTransparency(0);
            bd->SetPen(highlightColor);
            drawThickLine(w0, w1, hoverLineWidth, disableXRay, 3);
            drawPoint(w0, highlightColor, pointSize + 2.0, disableXRay);
            drawPoint(w1, highlightColor, pointSize + 2.0, disableXRay);
        }
        else if (m_hoverTweak.mode == TweakMode::Polygon)
        {
            Int32 numPts = m_hoverTweak.polyIsQuad ? 4 : 3;
            Vector wPts[4];
            Bool hasPts = false;
            if (retopo)
            {
                Int32 ptCount = retopo->GetPointCount();
                const Vector* rPts = retopo->GetPointR();
                Matrix rMg = retopo->GetMg();
                hasPts = true;
                for (Int32 k = 0; k < numPts; ++k)
                {
                    Int32 vi = m_hoverTweak.polyPts[k];
                    if (vi < 0 || vi >= ptCount)
                    {
                        hasPts = false;
                        break;
                    }
                    wPts[k] = rMg * rPts[vi];
                }
            }
            if (!hasPts)
            {
                for (Int32 k = 0; k < 4; ++k)
                    wPts[k] = m_hoverTweak.polyWorld[k];
            }

            if (disableXRay)
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
                bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
                bd->LineZOffset(0);
            }
            else
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
            }

            Vector polyColors[4] = { highlightColor, highlightColor, highlightColor, highlightColor };
            bd->SetTransparency(-140);
            bd->DrawPolygon(wPts, polyColors, m_hoverTweak.polyIsQuad);
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(highlightColor);
            drawThickLine(wPts[0], wPts[1], hoverLineWidth, disableXRay, 3);
            drawThickLine(wPts[1], wPts[2], hoverLineWidth, disableXRay, 3);
            if (m_hoverTweak.polyIsQuad)
            {
                drawThickLine(wPts[2], wPts[3], hoverLineWidth, disableXRay, 3);
                drawThickLine(wPts[3], wPts[0], hoverLineWidth, disableXRay, 3);
            }
            else
            {
                drawThickLine(wPts[2], wPts[0], hoverLineWidth, disableXRay, 3);
            }
            for (Int32 k = 0; k < numPts; ++k)
            {
                drawPoint(wPts[k], highlightColor, pointSize + 2.0, disableXRay);
            }
        }
    }

    // 6. Draw active drag highlight (Edge or Polygon)
    if (m_activeDragMode != TweakMode::None && retopo)
    {
        Matrix rMg = retopo->GetMg();
        const Vector* rPts = retopo->GetPointR();

        if (m_activeDragMode == TweakMode::Edge && m_dragEdgeV0 != NOTOK && m_dragEdgeV1 != NOTOK)
        {
            Vector w0 = rMg * rPts[m_dragEdgeV0];
            Vector w1 = rMg * rPts[m_dragEdgeV1];
            bd->SetTransparency(0);
            bd->SetPen(Vector(1.0, 0.9, 0.1));
            drawThickLine(w0, w1, hoverLineWidth, disableXRay, 3);
            drawPoint(w0, Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay);
            drawPoint(w1, Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay);
        }
        else if (m_activeDragMode == TweakMode::Polygon && m_dragPolyNumPts > 0)
        {
            Vector wPts[4];
            for (Int32 k = 0; k < m_dragPolyNumPts; ++k)
                wPts[k] = rMg * rPts[m_dragPolyPts[k]];

            if (disableXRay)
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(true));
                bd->SetDrawParam(DRAW_PARAMETER_SETZ, GeData(DRAW_Z_LOWEREQUAL));
                bd->LineZOffset(0);
            }
            else
            {
                bd->SetDrawParam(DRAW_PARAMETER_USE_Z, GeData(false));
            }

            const Vector yellowFace(0.95, 0.85, 0.2);
            Vector yColors[4] = { yellowFace, yellowFace, yellowFace, yellowFace };
            bd->SetTransparency(-140);
            bd->DrawPolygon(wPts, yColors, (m_dragPolyNumPts == 4));
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(Vector(1.0, 0.9, 0.1));
            drawThickLine(wPts[0], wPts[1], hoverLineWidth, disableXRay, 3);
            drawThickLine(wPts[1], wPts[2], hoverLineWidth, disableXRay, 3);
            if (m_dragPolyNumPts == 4)
            {
                drawThickLine(wPts[2], wPts[3], hoverLineWidth, disableXRay, 3);
                drawThickLine(wPts[3], wPts[0], hoverLineWidth, disableXRay, 3);
            }
            else
            {
                drawThickLine(wPts[2], wPts[0], hoverLineWidth, disableXRay, 3);
            }
            for (Int32 k = 0; k < m_dragPolyNumPts; ++k)
                drawPoint(wPts[k], Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay);
        }
        else if ((m_activeDragMode == TweakMode::LoopExtrude || m_activeDragMode == TweakMode::LoopMove) &&
                 m_componentLoop.edges.GetCount() > 0)
        {
            const Vector yellow(1.0, 0.9, 0.1);
            bd->SetTransparency(0);
            bd->SetPen(yellow);

            Int32 ptCount = retopo->GetPointCount();
            for (Int32 k = 0; k < (Int32)m_componentLoop.edges.GetCount(); ++k)
            {
                const LoopEdge& le = m_componentLoop.edges[k];
                if (le.v0 >= 0 && le.v0 < ptCount && le.v1 >= 0 && le.v1 < ptCount)
                {
                    Vector w0 = rMg * rPts[le.v0];
                    Vector w1 = rMg * rPts[le.v1];
                    drawThickLine(w0, w1, hoverLineWidth + 1.2, disableXRay, 3);
                    drawPoint(w0, yellow, pointSize + 2.0, disableXRay);
                    drawPoint(w1, yellow, pointSize + 2.0, disableXRay);
                }
            }

            // Draw magnetized weld targets in bright red!
            for (Int32 k = 0; k < (Int32)m_loopWeldTargets.GetCount(); ++k)
            {
                Int32 wt = m_loopWeldTargets[k];
                if (wt != NOTOK && wt >= 0 && wt < ptCount)
                {
                    Vector targetPos = rMg * rPts[wt];
                    drawPoint(targetPos, Vector(1.0, 0.2, 0.2), pointSize + 3.0, disableXRay);
                }
            }
        }
    }

    // 7. Draw all retopo vertices (dots)
    if (retopo && retopo->GetPointCount() > 0)
    {
        Int32 ptCount = retopo->GetPointCount();
        const Vector* pts = retopo->GetPointR();
        Matrix rMg = retopo->GetMg();

        Vector camPos = bd->GetMg().off;
        Bool isOrtho = (bd->GetProjection() != Pperspective);
        Vector orthoLook = -bd->GetMg().sqmat.v3.GetNormalized();

        maxon::BaseArray<Vector> pointNormals;
        if (disableXRay)
        {
            pointNormals.Resize(ptCount) iferr_ignore("Resize");
            for (Int32 i = 0; i < ptCount; ++i) pointNormals[i] = Vector(0.0);

            // Accumulate normals from retopo polygons (if any)
            Int32 polyCount = retopo->GetPolygonCount();
            if (polyCount > 0)
            {
                const CPolygon* polys = retopo->GetPolygonR();
                for (Int32 p = 0; p < polyCount; ++p)
                {
                    const CPolygon& poly = polys[p];
                    Vector pA = rMg * pts[poly.a];
                    Vector pB = rMg * pts[poly.b];
                    Vector pC = rMg * pts[poly.c];
                    Vector fn = Cross(pB - pA, pC - pA);
                    Float lenSq = Dot(fn, fn);
                    if (lenSq > 1e-12)
                    {
                        fn /= Sqrt(lenSq);
                        pointNormals[poly.a] += fn;
                        pointNormals[poly.b] += fn;
                        pointNormals[poly.c] += fn;
                        if (poly.c != poly.d)
                            pointNormals[poly.d] += fn;
                    }
                }
            }

            // For vertices without retopo polygon normals (e.g. isolated dots), query normal from target surface
            PolygonObject* target = GetTargetMesh(doc, retopo);
            if (target && target->GetPolygonCount() > 0)
            {
                for (Int32 i = 0; i < ptCount; ++i)
                {
                    if (pointNormals[i].GetSquaredLength() < 1e-6)
                    {
                        Vector wPos = rMg * pts[i];
                        SnapResult proj = m_snapper.ProjectPointAlongNormal(target, wPos, Vector(0.0, 1.0, 0.0), 100.0);
                        if (!proj.valid)
                        {
                            Vector s = bd->WS(wPos);
                            if (s.z > 0.0)
                                proj = m_snapper.RaycastSurface(bd, target, s.x, s.y);
                        }
                        if (proj.valid)
                            pointNormals[i] = proj.normal;
                    }
                }
            }
        }

        for (Int32 i = 0; i < ptCount; ++i)
        {
            Vector wPos = rMg * pts[i];
            Vector sPos = bd->WS(wPos);
            if (sPos.z <= 0.0) continue; // Behind camera

            Vector toCam = isOrtho ? orthoLook : (camPos - wPos).GetNormalized();

            Bool isLoopWeldTarget = false;
            for (Int32 w = 0; w < (Int32)m_loopWeldTargets.GetCount(); ++w)
            {
                if (m_loopWeldTargets[w] == i) { isLoopWeldTarget = true; break; }
            }

            Bool isHighlightedOrInteracting = (m_weldTargetIdx == i) || (m_weldTargetIdx2 == i) || isLoopWeldTarget ||
                (m_activeDragMode == TweakMode::Vertex && m_dragVertexIdx == i) ||
                (m_activeDragMode == TweakMode::Edge && (m_dragEdgeV0 == i || m_dragEdgeV1 == i)) ||
                (m_activeDragMode == TweakMode::Polygon && (m_dragPolyPts[0] == i || m_dragPolyPts[1] == i || m_dragPolyPts[2] == i || (m_dragPolyNumPts == 4 && m_dragPolyPts[3] == i))) ||
                (m_activeDragMode == TweakMode::LoopExtrude || m_activeDragMode == TweakMode::LoopMove) ||
                (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Vertex && m_hoverTweak.index == i) ||
                (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Edge && (m_hoverTweak.edgeV0 == i || m_hoverTweak.edgeV1 == i)) ||
                (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Polygon && (m_hoverTweak.polyPts[0] == i || m_hoverTweak.polyPts[1] == i || m_hoverTweak.polyPts[2] == i || (m_hoverTweak.polyIsQuad && m_hoverTweak.polyPts[3] == i)));

            if (disableXRay && !isHighlightedOrInteracting)
            {
                // Backface culling: normal facing away from camera
                if (pointNormals.GetCount() == ptCount && pointNormals[i].GetSquaredLength() > 1e-6)
                {
                    if (Dot(pointNormals[i], toCam) < -0.05)
                        continue;
                }
            }

            if (m_weldTargetIdx == i || m_weldTargetIdx2 == i || isLoopWeldTarget)
            {
                // Weld target in bright red
                drawPoint(wPos, Vector(1.0, 0.2, 0.2), pointSize + 3.0, disableXRay, toCam);
            }
            else if (m_activeDragMode == TweakMode::Vertex && m_dragVertexIdx == i)
            {
                // Actively dragged vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay, toCam);
            }
            else if (m_activeDragMode == TweakMode::Edge && (m_dragEdgeV0 == i || m_dragEdgeV1 == i))
            {
                // Actively dragged edge vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay, toCam);
            }
            else if (m_activeDragMode == TweakMode::Polygon &&
                     (m_dragPolyPts[0] == i || m_dragPolyPts[1] == i || m_dragPolyPts[2] == i || (m_dragPolyNumPts == 4 && m_dragPolyPts[3] == i)))
            {
                // Actively dragged polygon vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0, disableXRay, toCam);
            }
            else if (m_ctrlHeld && m_shiftHeld && m_deleteHighlight.type == DeleteTargetType::Vertex && m_deleteHighlight.index == i)
            {
                // Already drawn in delete highlight
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Vertex && m_hoverTweak.index == i)
            {
                // Hovered vertex in highlight color (white)
                drawPoint(wPos, highlightColor, pointSize + 2.0, disableXRay, toCam);
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Edge && (m_hoverTweak.edgeV0 == i || m_hoverTweak.edgeV1 == i))
            {
                // Hovered edge vertex in highlight color
                drawPoint(wPos, highlightColor, pointSize + 2.0, disableXRay, toCam);
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Polygon &&
                     (m_hoverTweak.polyPts[0] == i || m_hoverTweak.polyPts[1] == i || m_hoverTweak.polyPts[2] == i || (m_hoverTweak.polyIsQuad && m_hoverTweak.polyPts[3] == i)))
            {
                // Hovered polygon vertex in highlight color
                drawPoint(wPos, highlightColor, pointSize + 2.0, disableXRay, toCam);
            }
            else
            {
                // Standard retopo dot in wire color
                drawPoint(wPos, wireColor, pointSize, disableXRay, toCam);
            }
        }
    }

    // 8. Draw snap cursor indicator on surface (when placing dots)
    if (!m_shiftHeld && !m_ctrlHeld && m_activeDragMode == TweakMode::None && m_hoverTweak.mode == TweakMode::None && m_hoverSnap.valid)
    {
        drawPoint(m_hoverSnap.worldPos, wireColor, pointSize, disableXRay);
    }

    bd->SetDrawParam(DRAW_PARAMETER_USE_Z, oldUseZ);
    bd->SetDrawParam(DRAW_PARAMETER_SETZ, oldSetZ);
    bd->SetDrawParam(DRAW_PARAMETER_LINEWIDTH, oldLineWidth);
    bd->LineZOffset(0);
    return TOOLDRAW::HANDLES | TOOLDRAW::AXIS;
}

Bool QuadDrawToolData::KeyboardInput(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win, const BaseContainer& msg)
{
    Int32 qual = msg.GetInt32(BFM_INPUT_QUALIFIER);
    Bool oldCtrl = m_ctrlHeld;
    m_shiftHeld = (qual & QSHIFT) != 0;
    m_ctrlHeld  = (qual & QCTRL)  != 0;

    Int32 key = msg.GetInt32(BFM_INPUT_CHANNEL);
    Int32 activeTool = data.GetInt32(QUADDRAW_ACTIVE_TOOL, QUADDRAW_TOOL_QUAD);

    if (activeTool == QUADDRAW_TOOL_MULTICUT)
    {
        if (oldCtrl && !m_ctrlHeld)
        {
            m_edgeCutPreview.valid = false;
            m_edgeCutPreview.cutPoints.Reset();
            m_edgeCutPreview.cutSegments.Reset();
            m_edgeCutPreview.quadSplits.Reset();
            m_edgeCutPreview.triSplits.Reset();
            m_cachedCutV0 = NOTOK;
            m_cachedCutV1 = NOTOK;
            m_cachedCutT = -1.0;
            m_cachedCutPoly = NOTOK;
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }
        else if (!oldCtrl && m_ctrlHeld)
        {
            m_componentLoop.Reset();
            m_multiCutHover = MultiCutPoint();
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }
    }

    if (key == KEY_ENTER)
    {
        if (activeTool == QUADDRAW_TOOL_MULTICUT)
        {
            PolygonObject* retopo = GetEditableMesh(doc, false);
            PolygonObject* target = GetTargetMesh(doc, retopo);
            if (m_multiCutPoints.GetCount() >= 2 && retopo)
            {
                CommitMultiCut(doc, data, bd, retopo, target);
                m_multiCutPoints.Reset();
                m_multiCutPreview.valid = false;
                m_multiCutPreview.cuts.Reset();
                m_multiCutPreview.previewSegments.Reset();
                m_multiCutHover = MultiCutPoint();
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }
    }
    else if (key == KEY_BACKSPACE || key == KEY_DELETE)
    {
        if (activeTool == QUADDRAW_TOOL_MULTICUT && m_multiCutPoints.GetCount() > 0)
        {
            m_multiCutPoints.Pop();
            PolygonObject* retopo = GetEditableMesh(doc, false);
            PolygonObject* target = GetTargetMesh(doc, retopo);
            if (m_multiCutPoints.GetCount() > 0)
            {
                m_multiCutPreview = m_builder.BuildMultiCutFromPoints(bd, retopo, target, m_snapper, m_multiCutPoints, nullptr);
            }
            else
            {
                m_multiCutPreview.valid = false;
                m_multiCutPreview.cuts.Reset();
                m_multiCutPreview.previewSegments.Reset();
            }
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
        }
    }
    else if (key == KEY_ESC)
    {
        m_shiftQuadPreview.valid = false;
        m_edgeCutPreview.valid = false;
        m_edgeCutPreview.cutPoints.Reset();
        m_edgeCutPreview.cutSegments.Reset();
        m_edgeCutPreview.quadSplits.Reset();
        m_edgeCutPreview.triSplits.Reset();
        m_cachedCutV0 = NOTOK;
        m_cachedCutV1 = NOTOK;
        m_cachedCutT = -1.0;
        m_cachedCutPoly = NOTOK;
        m_deleteHighlight.type = DeleteTargetType::None;
        m_deleteHighlight.loopEdges.Reset();
        m_componentLoop.Reset();
        m_multiCutPoints.Reset();
        m_multiCutPreview.valid = false;
        m_multiCutPreview.cuts.Reset();
        m_multiCutPreview.previewSegments.Reset();
        m_multiCutHover = MultiCutPoint();
        m_sliceDrag.active = false;
        m_sliceDrag.result.valid = false;
        m_sliceDrag.result.cuts.Reset();
        m_sliceDrag.result.previewSegments.Reset();
        m_hoverTweak.mode = TweakMode::None;
        m_activeDragMode = TweakMode::None;
        m_dragVertexIdx = NOTOK;
        m_dragEdgeV0 = NOTOK;
        m_dragEdgeV1 = NOTOK;
        m_dragPolyIdx = NOTOK;
        m_dragPolyNumPts = 0;
        m_weldTargetIdx = NOTOK;
        m_weldTargetIdx2 = NOTOK;
        m_isResizingBrush = false;
        m_isRelaxDragging = false;
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
    return false;
}

Bool RegisterQuadDraw()
{
    return RegisterToolPlugin(
        PLUGIN_ID_QUADDRAW,
        "QuadDraw Retopo"_s,
        PLUGINFLAG_TOOL_HIGHLIGHT,
        AutoBitmap("quaddraw.png"_s),
        "QuadDraw Retopo Tool (Maya-style)\n- Tool Mode in Settings: Extrude, Move / Tweak, Knife (Cut Loops), or Multi-Cut\n- Multi-Cut: LMB Click to place points on edges/vertices, Shift to snap 50%/25%, Enter/RMB to commit, Backspace to undo, Esc to cancel, LMB Drag to slice cut\n- LMB: Click on surface to drop points (in Knife mode: insert edge loop)\n- LMB Drag on Border Edge: Extrude border edge (toggle in tool settings)\n- LMB Drag: Move/tweak vertex or edge (weld on drop onto another vertex)\n- Shift + Hover: Preview prospective quad polygon\n- Shift + LMB: Create quad polygon\n- Shift + LMB Drag: Relax mesh (Maya-style Relax Brush)\n- Shift + MMB Drag: Adjust relax brush radius (horizontal) & strength (vertical)\n- Ctrl + Hover: Highlight loop of components (Vertex, Edge, or Polygon Loop)\n- Ctrl + LMB: Select component loop\n- Ctrl + Shift + Hover: Highlight Vertex, Edge, or Polygon in red for deletion\n- Ctrl + Shift + LMB: Delete highlighted component\n- Esc: Clear active preview"_s,
        NewObjClear(QuadDrawToolData)
    );
}

} // namespace cinema
