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

    data.SetBool(QUADDRAW_DISABLE_CUSTOM_SHADING, true);
    data.SetBool(QUADDRAW_DISABLE_XRAY, true);
    data.SetVector(QUADDRAW_MESH_COLOR, defaultFaceColor);
    data.SetFloat(QUADDRAW_FACE_OPACITY, 0.35); // 35% opacity
    data.SetVector(QUADDRAW_WIRE_COLOR, defaultWireColor);
    data.SetFloat(QUADDRAW_LINE_WIDTH, 1.0);
    data.SetFloat(QUADDRAW_POINT_SIZE, 6.0);

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
    m_hoverTweak.mode = TweakMode::None;
    m_activeDragMode = TweakMode::None;
    m_dragVertexIdx = NOTOK;
    m_dragEdgeV0 = NOTOK;
    m_dragEdgeV1 = NOTOK;
    m_dragPolyIdx = NOTOK;
    m_dragPolyNumPts = 0;
    m_weldTargetIdx = NOTOK;
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
                Bool isShift = m_shiftHeld;
                if (ask->msg)
                {
                    Int32 qual = ask->msg->GetInt32(BFM_INPUT_QUALIFIER);
                    if ((qual & QSHIFT) != 0)
                        isShift = true;
                }
                BaseContainer ks;
                if (GetInputState(BFM_INPUT_KEYBOARD, BFM_INPUT_QUALIFIER, ks))
                {
                    if ((ks.GetInt32(BFM_INPUT_QUALIFIER) & QSHIFT) != 0)
                        isShift = true;
                }

                if (isShift)
                {
                    ask->use_middlemouse = true;
                    ask->resize_allowed = true;
                }
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
    // MODE 2: CTRL HELD ALONE (CUT / INSERT EDGE LOOP MODE - Maya style)
    // =========================================================================
    if (m_ctrlHeld && !m_shiftHeld)
    {
        m_shiftQuadPreview.valid = false;

        if (retopo && retopo->GetPolygonCount() > 0)
        {
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            // Priority 1: Check if cursor is directly over a front-facing polygon
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
                // Priority 2: Cursor near a visible boundary edge
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
            StatusSetText(FormatString("QuadDraw [CUT] | Ctrl+LMB: Insert Edge Loop (@% across @ quads) | Drag to Slide | Esc to Cancel"_s, pct, count));
        }
        else
        {
            StatusSetText("QuadDraw [CUT] | Hover over Edge or Quad to Insert Edge Loop (Ctrl+LMB) | Maya-style Cut"_s);
        }

        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Clear cut preview when not in Ctrl mode
    m_edgeCutPreview.valid = false;
    m_cachedCutV0 = NOTOK;
    m_cachedCutV1 = NOTOK;
    m_cachedCutT = -1.0;
    m_cachedCutPoly = NOTOK;

    // =========================================================================
    // =========================================================================
    // MODE 3: SHIFT HELD (QUAD CREATION PREVIEW OR MAYA RELAX BRUSH)
    // =========================================================================
    if (m_shiftHeld)
    {
        Float brushRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float brushStrength = data.GetFloat(QUADDRAW_RELAX_STRENGTH, 0.35);

        // Check quad creation preview only if retopo has at least 4 vertices
        if (retopo && retopo->GetPointCount() >= 4)
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
    // MODE 3: NORMAL (DOT PLACEMENT / TWEAK DRAG: VERTEX, EDGE, POLYGON)
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
                StatusSetText(FormatString("QuadDraw | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s,
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

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                StatusSetText(FormatString("QuadDraw | LMB Drag: Move Edge (#@ - #@) | Target: @"_s,
                    polyEdge.v0, polyEdge.v1, targetName));
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
            StatusSetText(FormatString("QuadDraw | LMB Drag: Move Polygon #@ | Target: @"_s,
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
                StatusSetText(FormatString("QuadDraw | LMB Drag: Move Vertex #@ (Weld on drop) | Target: @"_s,
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

                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                StatusSetText(FormatString("QuadDraw | LMB Drag: Move Edge (#@ - #@) | Target: @"_s,
                    nearEdge.v0, nearEdge.v1, targetName));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }
        }
    }

    // 4. Empty surface hover -> ready to place point
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

Bool QuadDrawToolData::MouseInput(BaseDocument* doc, BaseContainer& data, BaseDraw* bd, EditorWindow* win, const BaseContainer& msg)
{
    if (!doc || !bd || !win) return false;

    Int32 channel = msg.GetInt32(BFM_INPUT_CHANNEL);
    Float mx = msg.GetFloat(BFM_INPUT_X);
    Float my = msg.GetFloat(BFM_INPUT_Y);
    Int32 qualifier = msg.GetInt32(BFM_INPUT_QUALIFIER);
    Bool shiftPressed = ((qualifier & QSHIFT) != 0) || m_shiftHeld;

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

    if (channel != BFM_INPUT_MOUSELEFT)
        return true;

    PolygonObject* retopo = GetEditableMesh(doc, true);
    if (!retopo) return false;

    PolygonObject* target = GetTargetMesh(doc, retopo);

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
    // ACTION 2: CTRL (alone) + LMB -> CUT / INSERT EDGE LOOP (Maya-style)
    // ==========================================
    if ((qualifier & QCTRL) && !(qualifier & QSHIFT))
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
                        StatusSetText(FormatString("QuadDraw [CUT] | Sliding Edge Loop (@%)"_s, pct));
                        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                    }
                }
            }

            MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                m_edgeCutPreview.valid = false;
                m_cachedCutV0 = NOTOK;
                m_cachedCutV1 = NOTOK;
                m_cachedCutT = -1.0;
                m_cachedCutPoly = NOTOK;
                StatusSetText("QuadDraw: Cut canceled."_s);
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                return true;
            }

            doc->StartUndo();
            doc->AddUndo(UNDOTYPE::CHANGE, retopo);

            Int32 quadsSplit = (Int32)m_edgeCutPreview.quadSplits.GetCount();
            if (m_builder.ApplyEdgeLoopCut(retopo, m_edgeCutPreview))
            {
                StatusSetText(FormatString("QuadDraw: Inserted Edge Loop (@ quads split)"_s, quadsSplit));
            }
            m_edgeCutPreview.valid = false;
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
            StatusSetText("QuadDraw [CUT]: No edge or quad loop detected under cursor to cut."_s);
            return true;
        }
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

    // Priority 2: Edge Drag
    if (hitEdge.valid)
    {
        Int32 v0 = hitEdge.v0;
        Int32 v1 = hitEdge.v1;
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

    // 3. Draw prospective Edge Loop Cut line when holding Ctrl
    if (m_ctrlHeld && !m_shiftHeld && m_edgeCutPreview.valid)
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

            Bool isHighlightedOrInteracting = (m_weldTargetIdx == i) ||
                (m_activeDragMode == TweakMode::Vertex && m_dragVertexIdx == i) ||
                (m_activeDragMode == TweakMode::Edge && (m_dragEdgeV0 == i || m_dragEdgeV1 == i)) ||
                (m_activeDragMode == TweakMode::Polygon && (m_dragPolyPts[0] == i || m_dragPolyPts[1] == i || m_dragPolyPts[2] == i || (m_dragPolyNumPts == 4 && m_dragPolyPts[3] == i))) ||
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

            if (m_weldTargetIdx == i)
            {
                // Weld target in bright red
                drawPoint(wPos, Vector(1.0, 0.2, 0.2), pointSize + 2.0, disableXRay, toCam);
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
    m_shiftHeld = (qual & QSHIFT) != 0;
    m_ctrlHeld  = (qual & QCTRL)  != 0;

    Int32 key = msg.GetInt32(BFM_INPUT_CHANNEL);
    if (key == KEY_ESC)
    {
        m_shiftQuadPreview.valid = false;
        m_edgeCutPreview.valid = false;
        m_deleteHighlight.type = DeleteTargetType::None;
        m_deleteHighlight.loopEdges.Reset();
        m_hoverTweak.mode = TweakMode::None;
        m_activeDragMode = TweakMode::None;
        m_dragVertexIdx = NOTOK;
        m_dragEdgeV0 = NOTOK;
        m_dragEdgeV1 = NOTOK;
        m_dragPolyIdx = NOTOK;
        m_dragPolyNumPts = 0;
        m_weldTargetIdx = NOTOK;
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
        "QuadDraw Retopo Tool (Maya-style)\n- LMB: Click on surface to drop points\n- LMB Drag: Move/tweak vertex (weld on drop onto another vertex)\n- Shift + Hover: Preview prospective quad polygon\n- Shift + LMB: Create quad polygon\n- Shift + LMB Drag: Relax mesh (Maya-style Relax Brush)\n- Shift + MMB Drag: Adjust relax brush radius (horizontal) & strength (vertical)\n- Ctrl + Hover: Preview Cut / Insert Edge Loop (Maya-style)\n- Ctrl + LMB: Insert Edge Loop / Cut edges (drag to slide, Esc to cancel)\n- Ctrl + Shift + Hover: Highlight Vertex, Edge, or Polygon in red for deletion\n- Ctrl + Shift + LMB: Delete highlighted component\n- Esc: Clear active preview"_s,
        NewObjClear(QuadDrawToolData)
    );
}

} // namespace cinema
