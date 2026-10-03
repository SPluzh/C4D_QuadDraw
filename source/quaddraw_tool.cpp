#include "quaddraw_tool.h"
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
    }

    return true;
}

void QuadDrawToolData::InitDefaultSettings(BaseDocument* doc, BaseContainer& data)
{
    const Vector defaultFaceColor(0.0, 150.0 / 255.0, 1.0); // 0 150 255
    const Vector defaultWireColor(0.0, 0.0, 0.0);           // Black
    const Vector defaultHighlightColor(1.0, 1.0, 1.0);      // White

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

Bool QuadDrawToolData::Message(BaseDocument* doc, BaseContainer& data, Int32 type, void* t_data)
{
    if (type == MSG_DESCRIPTION_CHECKUPDATE)
    {
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }
    if (type == MSG_DESCRIPTION_COMMAND)
    {
        DescriptionCommand* dc = (DescriptionCommand*)t_data;
        if (dc && dc->_descId[0].id == MDATA_DEFAULTVALUES)
        {
            InitDefaultSettings(doc, data);
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
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
            // Priority 1: Vertex under cursor (within 10 px)
            Int32 nearVertex = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 10.0, NOTOK);
            if (nearVertex != NOTOK)
            {
                m_deleteHighlight.type = DeleteTargetType::Vertex;
                m_deleteHighlight.index = nearVertex;
                m_deleteHighlight.worldPos0 = retopo->GetMg() * retopo->GetPointR()[nearVertex];
            }
            else
            {
                // Priority 2: Edge under cursor (within 8 px)
                EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 8.0);
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
                else
                {
                    // Priority 3: Polygon under cursor
                    Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y);
                    if (nearPoly != NOTOK)
                    {
                        m_deleteHighlight.type = DeleteTargetType::Polygon;
                        m_deleteHighlight.index = nearPoly;
                        const CPolygon& p = retopo->GetPolygonR()[nearPoly];
                        const Vector* pts = retopo->GetPointR();
                        Matrix rMg = retopo->GetMg();
                        m_deleteHighlight.polyIsQuad = (p.c != p.d);
                        m_deleteHighlight.polyPts[0] = rMg * pts[p.a];
                        m_deleteHighlight.polyPts[1] = rMg * pts[p.b];
                        m_deleteHighlight.polyPts[2] = rMg * pts[p.c];
                        m_deleteHighlight.polyPts[3] = rMg * pts[p.d];
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
            EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 20.0);
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            if (edgeHit.valid)
            {
                hitV0 = edgeHit.v0;
                hitV1 = edgeHit.v1;
                hitT  = edgeHit.t;
                hitPoly = edgeHit.polyIndex;
            }
            else
            {
                Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y);
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
            }

            if (hitV0 != NOTOK && hitV1 != NOTOK)
            {
                if (std::abs(hitT - 0.5) < 0.05)
                    hitT = 0.5;

                m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, bd, hitV0, hitV1, hitT, hitPoly);
            }
            else
            {
                m_edgeCutPreview.valid = false;
            }
        }
        else
        {
            m_edgeCutPreview.valid = false;
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

    // =========================================================================
    // MODE 3: SHIFT HELD (QUAD CREATION PREVIEW OR MAYA RELAX BRUSH)
    // =========================================================================
    if (m_shiftHeld)
    {
        // Continuous raycast on surface if target exists, else use view normal
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

        if (retopo && retopo->GetPointCount() >= 4)
        {
            Vector norm = m_hoverSnap.valid ? m_hoverSnap.normal : (bd ? -bd->GetMg().sqmat.v3 : Vector(0.0, 1.0, 0.0));
            m_shiftQuadPreview = m_builder.FindPotentialQuad(bd, retopo, norm, x, y);
        }
        else
        {
            m_shiftQuadPreview.valid = false;
        }

        if (m_shiftQuadPreview.valid)
        {
            bc.SetInt32(RESULT_CURSOR, MOUSE_CROSS);
            StatusSetText(FormatString("QuadDraw | Shift+LMB: Create Quad! (Vertices: @, @, @, @) | Shift+MMB Drag: Resize Brush | Target: @"_s,
                m_shiftQuadPreview.v[0], m_shiftQuadPreview.v[1], m_shiftQuadPreview.v[2], m_shiftQuadPreview.v[3], targetName));
        }
        else
        {
            Int32 nearPoly = retopo ? m_builder.FindPolygonUnderScreen(bd, retopo, x, y) : NOTOK;
            EdgeHit nearEdge = retopo ? m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 12.0) : EdgeHit();
            Float brushRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);

            if (nearPoly != NOTOK || nearEdge.valid)
            {
                bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
                Int32 relaxMode = data.GetInt32(QUADDRAW_RELAX_MODE, QUADDRAW_RELAX_MODE_AUTOLOCK);

                if (relaxMode == QUADDRAW_RELAX_MODE_AUTOLOCK)
                {
                    Bool nearBorder = m_builder.IsCursorNearBorder(retopo, bd, x, y, brushRadius);
                    if (nearBorder)
                        StatusSetText(FormatString("QuadDraw [RELAX: Auto-lock -> Border] | Shift+LMB Drag: Relax Border | Shift+MMB Drag: Resize Brush (@ px)"_s, (Int32)(brushRadius + 0.5)));
                    else
                        StatusSetText(FormatString("QuadDraw [RELAX: Auto-lock -> Interior] | Shift+LMB Drag: Relax Interior | Shift+MMB Drag: Resize Brush (@ px)"_s, (Int32)(brushRadius + 0.5)));
                }
                else if (relaxMode == QUADDRAW_RELAX_MODE_INTERIOR)
                {
                    StatusSetText(FormatString("QuadDraw [RELAX: Interior] | Shift+LMB Drag: Relax Interior | Shift+MMB Drag: Resize Brush (@ px)"_s, (Int32)(brushRadius + 0.5)));
                }
                else if (relaxMode == QUADDRAW_RELAX_MODE_BORDER)
                {
                    StatusSetText(FormatString("QuadDraw [RELAX: Border] | Shift+LMB Drag: Relax Border | Shift+MMB Drag: Resize Brush (@ px)"_s, (Int32)(brushRadius + 0.5)));
                }
                else // QUADDRAW_RELAX_MODE_ALL
                {
                    StatusSetText(FormatString("QuadDraw [RELAX: All] | Shift+LMB Drag: Relax All | Shift+MMB Drag: Resize Brush (@ px)"_s, (Int32)(brushRadius + 0.5)));
                }
            }
            else
            {
                bc.SetInt32(RESULT_CURSOR, MOUSE_NORMAL);
                StatusSetText(FormatString("QuadDraw | Shift+LMB: Quad/Relax | Shift+MMB Drag: Resize Brush (@ px) | Target: @"_s, (Int32)(brushRadius + 0.5), targetName));
            }
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
        // 1. Check Vertex hover
        Int32 nearV = m_snapper.FindNearestRetopoVertex(bd, retopo, x, y, 10.0);
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

        // 2. Check Edge hover
        EdgeHit nearEdge = m_snapper.FindNearestRetopoEdge(bd, retopo, x, y, 8.0);
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

        // 3. Check Polygon hover
        Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, x, y);
        if (nearPoly != NOTOK && nearPoly < retopo->GetPolygonCount())
        {
            const CPolygon& p = retopo->GetPolygonR()[nearPoly];
            Matrix rMg = retopo->GetMg();
            const Vector* rPts = retopo->GetPointR();

            m_hoverTweak.mode = TweakMode::Polygon;
            m_hoverTweak.index = nearPoly;
            m_hoverTweak.polyIsQuad = (p.c != p.d);
            m_hoverTweak.polyPts[0] = p.a;
            m_hoverTweak.polyPts[1] = p.b;
            m_hoverTweak.polyPts[2] = p.c;
            m_hoverTweak.polyPts[3] = p.d;
            m_hoverTweak.polyWorld[0] = rMg * rPts[p.a];
            m_hoverTweak.polyWorld[1] = rMg * rPts[p.b];
            m_hoverTweak.polyWorld[2] = rMg * rPts[p.c];
            m_hoverTweak.polyWorld[3] = rMg * rPts[p.d];

            bc.SetInt32(RESULT_CURSOR, MOUSE_POINT_HAND);
            StatusSetText(FormatString("QuadDraw | LMB Drag: Move Polygon #@ | Target: @"_s,
                nearPoly, targetName));
            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            return true;
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
    // ACTION 0: SHIFT + MMB DRAG -> RESIZE RELAX BRUSH RADIUS (MAYA STYLE)
    // =========================================================================
    if (channel == BFM_INPUT_MOUSEMIDDLE && shiftPressed)
    {
        Float initialRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float currentRadius = initialRadius;

        m_isResizingBrush = true;
        m_brushResizeCenterX = mx;
        m_brushResizeCenterY = my;
        m_cursorX = mx;
        m_cursorY = my;

        BaseContainer device;
        win->MouseDragStart(KEY_MMIDDLE, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE);

        Float dx, dy;
        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;

            currentRadius += dx;
            if (currentRadius < 5.0) currentRadius = 5.0;
            if (currentRadius > 300.0) currentRadius = 300.0;

            data.SetFloat(QUADDRAW_RELAX_RADIUS, currentRadius);

            m_cursorX += dx;
            m_cursorY += dy;

            StatusSetText(FormatString("QuadDraw [RESIZE BRUSH] | Relax Radius: @ px (Drag Left/Right to adjust)"_s, (Int32)(currentRadius + 0.5)));

            DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        }

        win->MouseDragEnd();
        m_isResizingBrush = false;

        data.SetFloat(QUADDRAW_RELAX_RADIUS, currentRadius);
        StatusSetText(FormatString("QuadDraw: Relax Radius set to @ px"_s, (Int32)(currentRadius + 0.5)));

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
            Int32 nv = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 10.0, NOTOK);
            if (nv != NOTOK)
            {
                del.type = DeleteTargetType::Vertex;
                del.index = nv;
            }
            else
            {
                EdgeHit eh = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 8.0);
                if (eh.valid)
                {
                    del.type = DeleteTargetType::Edge;
                    del.edgeV0 = eh.v0;
                    del.edgeV1 = eh.v1;
                    EdgeLoopResult loop = m_builder.FindEdgeLoop(retopo, eh.v0, eh.v1);
                    del.loopEdges.CopyFrom(loop.edges) iferr_ignore("Copy loop edges");
                }
                else
                {
                    Int32 np = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my);
                    if (np != NOTOK)
                    {
                        del.type = DeleteTargetType::Polygon;
                        del.index = np;
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
            EdgeHit edgeHit = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 25.0);
            Int32 hitV0 = NOTOK, hitV1 = NOTOK;
            Float hitT = 0.5;
            Int32 hitPoly = NOTOK;

            if (edgeHit.valid)
            {
                hitV0 = edgeHit.v0;
                hitV1 = edgeHit.v1;
                hitT  = edgeHit.t;
                hitPoly = edgeHit.polyIndex;
            }
            else
            {
                Int32 nearPoly = m_builder.FindPolygonUnderScreen(bd, retopo, mx, my);
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
            }

            if (hitV0 != NOTOK && hitV1 != NOTOK)
            {
                if (std::abs(hitT - 0.5) < 0.05) hitT = 0.5;
                m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, bd, hitV0, hitV1, hitT, hitPoly);
            }
        }

        if (m_edgeCutPreview.valid)
        {
            BaseContainer device;
            win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

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
                    m_edgeCutPreview = m_builder.FindEdgeLoopCut(retopo, target, bd, m_edgeCutPreview.primaryV0, m_edgeCutPreview.primaryV1, newT, m_edgeCutPreview.primaryPoly);
                    Int32 pct = (Int32)(m_edgeCutPreview.paramT * 100.0 + 0.5);
                    StatusSetText(FormatString("QuadDraw [CUT] | Sliding Edge Loop (@%)"_s, pct));
                    DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
                }
            }

            MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
            if (dragResult == MOUSEDRAGRESULT::ESCAPE)
            {
                m_edgeCutPreview.valid = false;
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

        QuadPreview qp = m_shiftQuadPreview.valid ? m_shiftQuadPreview : m_builder.FindPotentialQuad(bd, retopo, norm, mx, my);

        // Sub-case 3A: Empty prospective quad under cursor -> Click to create quad
        if (qp.valid && m_builder.FindPolygonUnderScreen(bd, retopo, mx, my) == NOTOK)
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
            Bool startOnBorder = m_builder.IsCursorNearBorder(retopo, bd, mx, my, brushRadius);
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
        m_builder.RelaxVertices(retopo, target, m_snapper, bd, mx, my, brushRadius, strength, lockBorder, lockInterior);
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

        Float dx, dy;
        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            mx += dx;
            my += dy;
            m_cursorX = mx;
            m_cursorY = my;

            m_builder.RelaxVertices(retopo, target, m_snapper, bd, mx, my, brushRadius, strength, lockBorder, lockInterior);

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
    auto snapPointToSurface = [&](const Vector& pt, const Vector& norm) -> Vector {
        if (!target) return pt;
        SnapResult sr = m_snapper.ProjectPointAlongNormal(target, pt, norm, 1000.0);
        if (sr.valid) return sr.worldPos;
        Vector sPt = bd->WS(pt);
        if (sPt.z > 0.0)
        {
            SnapResult raySnap = m_snapper.RaycastSurface(bd, target, sPt.x, sPt.y);
            if (raySnap.valid) return raySnap.worldPos;
        }
        return pt;
    };

    // Priority 1: Vertex Drag
    Int32 hitV = (retopo && retopo->GetPointCount() > 0)
        ? m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 10.0) : NOTOK;

    if (hitV != NOTOK)
    {
        Vector initVertexPos = retopo->GetMg() * retopo->GetPointR()[hitV];

        m_activeDragMode = TweakMode::Vertex;
        m_dragVertexIdx = hitV;
        m_weldTargetIdx = NOTOK;

        doc->StartUndo();
        doc->AddUndo(UNDOTYPE::CHANGE, retopo);

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

        Float dx, dy;
        Bool hasMoved = false;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            hasMoved = true;
            mx += dx;
            my += dy;

            Vector movePos;
            Bool hasMovePos = false;
            if (target)
            {
                SnapResult surfaceHit = m_snapper.RaycastSurface(bd, target, mx, my);
                if (surfaceHit.valid)
                {
                    movePos = surfaceHit.worldPos;
                    hasMovePos = true;
                }
            }
            else if (bd)
            {
                movePos = bd->SW_Reference(mx, my, initVertexPos);
                hasMovePos = true;
            }

            if (hasMovePos)
            {
                Int32 weldTarget = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 12.0, hitV);
                if (weldTarget != NOTOK)
                {
                    m_weldTargetIdx = weldTarget;
                    Vector targetPos = retopo->GetMg() * retopo->GetPointR()[m_weldTargetIdx];
                    m_builder.SetVertexPosition(retopo, hitV, targetPos);
                    StatusSetText(FormatString("QuadDraw: Release to Weld vertex #@ into #@"_s, hitV, m_weldTargetIdx));
                }
                else
                {
                    m_weldTargetIdx = NOTOK;
                    m_builder.SetVertexPosition(retopo, hitV, movePos);
                    StatusSetText(FormatString("QuadDraw: Moving vertex #@"_s, hitV));
                }
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
        if (dragResult == MOUSEDRAGRESULT::ESCAPE || !hasMoved)
        {
            doc->DoUndo(true);
        }
        else if (m_weldTargetIdx != NOTOK && m_weldTargetIdx != hitV)
        {
            m_builder.WeldVertices(retopo, hitV, m_weldTargetIdx);
            StatusSetText("QuadDraw: Vertices welded!"_s);
            doc->EndUndo();
        }
        else
        {
            StatusSetText(FormatString("QuadDraw: Vertex #@ moved."_s, hitV));
            doc->EndUndo();
        }

        m_dragVertexIdx = NOTOK;
        m_weldTargetIdx = NOTOK;
        m_activeDragMode = TweakMode::None;
        EventAdd();
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Priority 2: Edge Drag
    EdgeHit hitEdge;
    if (retopo && retopo->GetPolygonCount() > 0)
    {
        hitEdge = m_snapper.FindNearestRetopoEdge(bd, retopo, mx, my, 8.0);
    }

    if (hitEdge.valid)
    {
        Int32 v0 = hitEdge.v0;
        Int32 v1 = hitEdge.v1;
        Vector initP0 = retopo->GetMg() * retopo->GetPointR()[v0];
        Vector initP1 = retopo->GetMg() * retopo->GetPointR()[v1];
        Vector initMid = (initP0 + initP1) * 0.5;

        SnapResult initialHit;
        if (target) initialHit = m_snapper.RaycastSurface(bd, target, mx, my);
        Vector initialHitPos = initialHit.valid ? initialHit.worldPos : initMid;

        m_activeDragMode = TweakMode::Edge;
        m_dragEdgeV0 = v0;
        m_dragEdgeV1 = v1;

        doc->StartUndo();
        doc->AddUndo(UNDOTYPE::CHANGE, retopo);

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

        Float dx, dy;
        Bool hasMoved = false;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            hasMoved = true;
            mx += dx;
            my += dy;

            Bool canMove = false;
            if (target)
            {
                SnapResult currHit = m_snapper.RaycastSurface(bd, target, mx, my);
                if (currHit.valid)
                {
                    Vector delta = currHit.worldPos - initialHitPos;
                    Vector p0 = snapPointToSurface(initP0 + delta, currHit.normal);
                    Vector p1 = snapPointToSurface(initP1 + delta, currHit.normal);

                    m_builder.SetVertexPosition(retopo, v0, p0);
                    m_builder.SetVertexPosition(retopo, v1, p1);
                    canMove = true;
                }
            }
            else if (bd)
            {
                Vector currWorld = bd->SW_Reference(mx, my, initMid);
                Vector delta = currWorld - initMid;
                m_builder.SetVertexPosition(retopo, v0, initP0 + delta);
                m_builder.SetVertexPosition(retopo, v1, initP1 + delta);
                canMove = true;
            }

            if (canMove)
            {
                StatusSetText(FormatString("QuadDraw: Moving edge (#@ - #@)"_s, v0, v1));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
        if (dragResult == MOUSEDRAGRESULT::ESCAPE || !hasMoved)
        {
            doc->DoUndo(true);
        }
        else
        {
            StatusSetText(FormatString("QuadDraw: Edge (#@ - #@) moved."_s, v0, v1));
            doc->EndUndo();
        }

        m_dragEdgeV0 = NOTOK;
        m_dragEdgeV1 = NOTOK;
        m_activeDragMode = TweakMode::None;
        EventAdd();
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Priority 3: Polygon Drag
    Int32 hitPoly = (retopo && retopo->GetPolygonCount() > 0)
        ? m_builder.FindPolygonUnderScreen(bd, retopo, mx, my) : NOTOK;

    if (hitPoly != NOTOK && hitPoly < retopo->GetPolygonCount())
    {
        const CPolygon& p = retopo->GetPolygonR()[hitPoly];
        Int32 numPts = (p.c != p.d) ? 4 : 3;
        Int32 polyPts[4] = { p.a, p.b, p.c, p.d };
        m_dragPolyNumPts = numPts;
        for (Int32 k = 0; k < 4; ++k) m_dragPolyPts[k] = polyPts[k];

        Vector initPts[4];
        Vector center(0.0);
        for (Int32 k = 0; k < numPts; ++k)
        {
            initPts[k] = retopo->GetMg() * retopo->GetPointR()[polyPts[k]];
            center += initPts[k];
        }
        center = center * (1.0 / Float(numPts));

        SnapResult initialHit;
        if (target) initialHit = m_snapper.RaycastSurface(bd, target, mx, my);
        Vector initialHitPos = initialHit.valid ? initialHit.worldPos : center;

        m_activeDragMode = TweakMode::Polygon;
        m_dragPolyIdx = hitPoly;

        doc->StartUndo();
        doc->AddUndo(UNDOTYPE::CHANGE, retopo);

        BaseContainer device;
        win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

        Float dx, dy;
        Bool hasMoved = false;

        while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
        {
            if (dx == 0.0 && dy == 0.0) continue;
            hasMoved = true;
            mx += dx;
            my += dy;

            Bool canMove = false;
            if (target)
            {
                SnapResult currHit = m_snapper.RaycastSurface(bd, target, mx, my);
                if (currHit.valid)
                {
                    Vector delta = currHit.worldPos - initialHitPos;
                    for (Int32 k = 0; k < numPts; ++k)
                    {
                        Vector pk = snapPointToSurface(initPts[k] + delta, currHit.normal);
                        m_builder.SetVertexPosition(retopo, polyPts[k], pk);
                    }
                    canMove = true;
                }
            }
            else if (bd)
            {
                Vector currWorld = bd->SW_Reference(mx, my, center);
                Vector delta = currWorld - center;
                for (Int32 k = 0; k < numPts; ++k)
                {
                    m_builder.SetVertexPosition(retopo, polyPts[k], initPts[k] + delta);
                }
                canMove = true;
            }

            if (canMove)
            {
                StatusSetText(FormatString("QuadDraw: Moving polygon #@"_s, hitPoly));
                DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
            }
        }

        MOUSEDRAGRESULT dragResult = win->MouseDragEnd();
        if (dragResult == MOUSEDRAGRESULT::ESCAPE || !hasMoved)
        {
            doc->DoUndo(true);
        }
        else
        {
            StatusSetText(FormatString("QuadDraw: Polygon #@ moved."_s, hitPoly));
            doc->EndUndo();
        }

        m_dragPolyIdx = NOTOK;
        m_dragPolyNumPts = 0;
        m_activeDragMode = TweakMode::None;
        EventAdd();
        DrawViews(DRAWFLAGS::ONLY_ACTIVE_VIEW | DRAWFLAGS::NO_THREAD | DRAWFLAGS::NO_ANIMATION);
        return true;
    }

    // Priority 4: Place Point
    Vector dropPos;
    Bool canPlace = false;
    if (target)
    {
        SnapResult snap = m_snapper.RaycastSurface(bd, target, mx, my);
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
        if (m_builder.FindPolygonUnderScreen(bd, retopo, mx, my) != NOTOK)
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
            win->MouseDragStart(KEY_MLEFT, mx, my, MOUSEDRAGFLAGS::DONTHIDEMOUSE | MOUSEDRAGFLAGS::NOMOVE);

            Float dx, dy;
            Bool hasMoved = false;

            while (win->MouseDrag(&dx, &dy, &device) == MOUSEDRAGRESULT::CONTINUE)
            {
                if (dx == 0.0 && dy == 0.0) continue;
                hasMoved = true;
                mx += dx;
                my += dy;

                Vector newPos;
                Bool hasNewPos = false;
                if (target)
                {
                    SnapResult surfaceHit = m_snapper.RaycastSurface(bd, target, mx, my);
                    if (surfaceHit.valid)
                    {
                        newPos = surfaceHit.worldPos;
                        hasNewPos = true;
                    }
                }
                else if (bd)
                {
                    newPos = bd->SW_Reference(mx, my, dropPos);
                    hasNewPos = true;
                }

                if (hasNewPos)
                {
                    Int32 weldTarget = m_snapper.FindNearestRetopoVertex(bd, retopo, mx, my, 12.0, dragIdx);
                    if (weldTarget != NOTOK)
                    {
                        m_weldTargetIdx = weldTarget;
                        Vector targetPos = retopo->GetMg() * retopo->GetPointR()[m_weldTargetIdx];
                        m_builder.SetVertexPosition(retopo, dragIdx, targetPos);
                        StatusSetText(FormatString("QuadDraw: Release to Weld vertex #@ into #@"_s, dragIdx, m_weldTargetIdx));
                    }
                    else
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
            }
            else if (hasMoved && m_weldTargetIdx != NOTOK && m_weldTargetIdx != dragIdx)
            {
                m_builder.WeldVertices(retopo, dragIdx, m_weldTargetIdx);
                StatusSetText("QuadDraw: Vertices welded!"_s);
                doc->EndUndo();
            }
            else
            {
                StatusSetText(FormatString("QuadDraw: Point #@ placed."_s, dragIdx));
                doc->EndUndo();
            }

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
    bd->SetDrawParam(DRAW_PARAMETER_LINEWIDTH, GeData(lineWidth));

    auto drawThickLine = [&](const Vector& p1, const Vector& p2, Float width)
    {
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
    };

    auto drawPoint = [&](const Vector& p, const Vector& col, Float size)
    {
        DRAWHANDLE hType = DRAWHANDLE::MIDDLE;
        if (size <= 1.5)      hType = DRAWHANDLE::MINI;
        else if (size <= 2.5) hType = DRAWHANDLE::SMALL;
        else if (size <= 4.0) hType = DRAWHANDLE::MIDDLE;
        else if (size <= 6.5) hType = DRAWHANDLE::BIG;
        else                  hType = DRAWHANDLE::VERYBIG;

        bd->DrawHandleWorld(p, col, hType);

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
                bd->DrawLine(pA, pB, 0);
                bd->DrawLine(pB, pC, 0);
                bd->DrawLine(pC, pD, 0);
                bd->DrawLine(pD, pA, 0);
            }
        }
    };

    PolygonObject* retopo = GetEditableMesh(doc, false);

    // 1. Draw existing retopo polygons in user face color with transparency and wireframe lines
    if (retopo && retopo->GetPolygonCount() > 0)
    {
        Int32 polyCount = retopo->GetPolygonCount();
        const CPolygon* polys = retopo->GetPolygonR();
        const Vector* pts = retopo->GetPointR();
        Matrix rMg = retopo->GetMg();

        Vector faceColors[4] = { faceColor, faceColor, faceColor, faceColor };

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

            drawThickLine(qPts[0], qPts[1], lineWidth);
            drawThickLine(qPts[1], qPts[2], lineWidth);
            if (isQuad)
            {
                drawThickLine(qPts[2], qPts[3], lineWidth);
                drawThickLine(qPts[3], qPts[0], lineWidth);
            }
            else
            {
                drawThickLine(qPts[2], qPts[0], lineWidth);
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

        Vector greenColors[4] = { previewColor, previewColor, previewColor, previewColor };

        bd->SetTransparency(transVal);
        bd->DrawPolygon(prevPts, greenColors, true);
        bd->DrawArrayEnd();

        // Bright contour lines
        bd->SetTransparency(0);
        bd->SetPen(previewColor);
        drawThickLine(prevPts[0], prevPts[1], hoverLineWidth);
        drawThickLine(prevPts[1], prevPts[2], hoverLineWidth);
        drawThickLine(prevPts[2], prevPts[3], hoverLineWidth);
        drawThickLine(prevPts[3], prevPts[0], hoverLineWidth);

        // Highlight the 4 corner vertices
        for (Int32 k = 0; k < 4; ++k)
        {
            drawPoint(prevPts[k], previewColor, pointSize + 2.0);
        }
    }

    // 2b. Draw Relax Brush circle when holding Shift (or during brush resize)
    if (m_isResizingBrush || (m_shiftHeld && !m_ctrlHeld && (!m_shiftQuadPreview.valid || m_isRelaxDragging)))
    {
        Float relaxRadius = data.GetFloat(QUADDRAW_RELAX_RADIUS, 50.0);
        Float cx = m_isResizingBrush ? m_brushResizeCenterX : m_cursorX;
        Float cy = m_isResizingBrush ? m_brushResizeCenterY : m_cursorY;

        bd->SetMatrix_Screen();
        Vector circleColor = m_isResizingBrush ? Vector(1.0, 0.75, 0.15) :
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
            bd->DrawLine(Vector(cx - 4.0, cy, 0.0), Vector(cx + 4.0, cy, 0.0), 0);
            bd->DrawLine(Vector(cx, cy - 4.0, 0.0), Vector(cx, cy + 4.0, 0.0), 0);
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
            drawThickLine(seg.p0, seg.p1, hoverLineWidth);
        }

        // Draw handles at each cut point along the edges
        for (Int32 k = 0; k < (Int32)m_edgeCutPreview.cutPoints.GetCount(); ++k)
        {
            const CutPoint& cp = m_edgeCutPreview.cutPoints[k];
            drawPoint(cp.worldPos, cutColor, pointSize);
        }
    }

    // 4. Draw Red Deletion Highlight when Ctrl + Shift are held (Maya QuadDraw Delete Mode)
    if (m_ctrlHeld && m_shiftHeld && m_deleteHighlight.type != DeleteTargetType::None)
    {
        const Vector deleteRed(1.0, 0.15, 0.15);

        if (m_deleteHighlight.type == DeleteTargetType::Vertex)
        {
            // Red highlighted vertex
            drawPoint(m_deleteHighlight.worldPos0, deleteRed, pointSize + 2.0);
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
                    drawThickLine(le.worldPos0, le.worldPos1, hoverLineWidth);
                    drawPoint(le.worldPos0, deleteRed, pointSize + 1.0);
                    drawPoint(le.worldPos1, deleteRed, pointSize + 1.0);
                }
            }
            else
            {
                drawThickLine(m_deleteHighlight.worldPos0, m_deleteHighlight.worldPos1, hoverLineWidth);
                drawPoint(m_deleteHighlight.worldPos0, deleteRed, pointSize + 1.0);
                drawPoint(m_deleteHighlight.worldPos1, deleteRed, pointSize + 1.0);
            }
        }
        else if (m_deleteHighlight.type == DeleteTargetType::Polygon)
        {
            // Red highlighted polygon face
            const Vector redFace(0.9, 0.18, 0.18);
            Vector redColors[4] = { redFace, redFace, redFace, redFace };

            bd->SetTransparency(-140);
            bd->DrawPolygon(m_deleteHighlight.polyPts, redColors, m_deleteHighlight.polyIsQuad);
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(deleteRed);
            drawThickLine(m_deleteHighlight.polyPts[0], m_deleteHighlight.polyPts[1], hoverLineWidth);
            drawThickLine(m_deleteHighlight.polyPts[1], m_deleteHighlight.polyPts[2], hoverLineWidth);
            if (m_deleteHighlight.polyIsQuad)
            {
                drawThickLine(m_deleteHighlight.polyPts[2], m_deleteHighlight.polyPts[3], hoverLineWidth);
                drawThickLine(m_deleteHighlight.polyPts[3], m_deleteHighlight.polyPts[0], hoverLineWidth);
            }
            else
            {
                drawThickLine(m_deleteHighlight.polyPts[2], m_deleteHighlight.polyPts[0], hoverLineWidth);
            }
        }
    }

    // 5. Draw normal mode hover highlight (Edge or Polygon)
    if (!m_shiftHeld && !m_ctrlHeld && m_activeDragMode == TweakMode::None)
    {
        if (m_hoverTweak.mode == TweakMode::Edge)
        {
            bd->SetTransparency(0);
            bd->SetPen(highlightColor);
            drawThickLine(m_hoverTweak.edgeWorld0, m_hoverTweak.edgeWorld1, hoverLineWidth);
            drawPoint(m_hoverTweak.edgeWorld0, highlightColor, pointSize + 2.0);
            drawPoint(m_hoverTweak.edgeWorld1, highlightColor, pointSize + 2.0);
        }
        else if (m_hoverTweak.mode == TweakMode::Polygon)
        {
            Vector polyColors[4] = { highlightColor, highlightColor, highlightColor, highlightColor };
            bd->SetTransparency(-140);
            bd->DrawPolygon(m_hoverTweak.polyWorld, polyColors, m_hoverTweak.polyIsQuad);
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(highlightColor);
            drawThickLine(m_hoverTweak.polyWorld[0], m_hoverTweak.polyWorld[1], hoverLineWidth);
            drawThickLine(m_hoverTweak.polyWorld[1], m_hoverTweak.polyWorld[2], hoverLineWidth);
            if (m_hoverTweak.polyIsQuad)
            {
                drawThickLine(m_hoverTweak.polyWorld[2], m_hoverTweak.polyWorld[3], hoverLineWidth);
                drawThickLine(m_hoverTweak.polyWorld[3], m_hoverTweak.polyWorld[0], hoverLineWidth);
            }
            else
            {
                drawThickLine(m_hoverTweak.polyWorld[2], m_hoverTweak.polyWorld[0], hoverLineWidth);
            }
            for (Int32 k = 0; k < (m_hoverTweak.polyIsQuad ? 4 : 3); ++k)
            {
                drawPoint(m_hoverTweak.polyWorld[k], highlightColor, pointSize + 2.0);
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
            drawThickLine(w0, w1, hoverLineWidth);
            drawPoint(w0, Vector(1.0, 0.9, 0.1), pointSize + 2.0);
            drawPoint(w1, Vector(1.0, 0.9, 0.1), pointSize + 2.0);
        }
        else if (m_activeDragMode == TweakMode::Polygon && m_dragPolyNumPts > 0)
        {
            Vector wPts[4];
            for (Int32 k = 0; k < m_dragPolyNumPts; ++k)
                wPts[k] = rMg * rPts[m_dragPolyPts[k]];

            const Vector yellowFace(0.95, 0.85, 0.2);
            Vector yColors[4] = { yellowFace, yellowFace, yellowFace, yellowFace };
            bd->SetTransparency(-140);
            bd->DrawPolygon(wPts, yColors, (m_dragPolyNumPts == 4));
            bd->DrawArrayEnd();

            bd->SetTransparency(0);
            bd->SetPen(Vector(1.0, 0.9, 0.1));
            drawThickLine(wPts[0], wPts[1], hoverLineWidth);
            drawThickLine(wPts[1], wPts[2], hoverLineWidth);
            if (m_dragPolyNumPts == 4)
            {
                drawThickLine(wPts[2], wPts[3], hoverLineWidth);
                drawThickLine(wPts[3], wPts[0], hoverLineWidth);
            }
            else
            {
                drawThickLine(wPts[2], wPts[0], hoverLineWidth);
            }
            for (Int32 k = 0; k < m_dragPolyNumPts; ++k)
                drawPoint(wPts[k], Vector(1.0, 0.9, 0.1), pointSize + 2.0);
        }
    }

    // 7. Draw all retopo vertices (dots)
    if (retopo && retopo->GetPointCount() > 0)
    {
        Int32 ptCount = retopo->GetPointCount();
        const Vector* pts = retopo->GetPointR();
        Matrix rMg = retopo->GetMg();

        for (Int32 i = 0; i < ptCount; ++i)
        {
            Vector wPos = rMg * pts[i];

            if (m_weldTargetIdx == i)
            {
                // Weld target in bright red
                drawPoint(wPos, Vector(1.0, 0.2, 0.2), pointSize + 2.0);
            }
            else if (m_activeDragMode == TweakMode::Vertex && m_dragVertexIdx == i)
            {
                // Actively dragged vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0);
            }
            else if (m_activeDragMode == TweakMode::Edge && (m_dragEdgeV0 == i || m_dragEdgeV1 == i))
            {
                // Actively dragged edge vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0);
            }
            else if (m_activeDragMode == TweakMode::Polygon &&
                     (m_dragPolyPts[0] == i || m_dragPolyPts[1] == i || m_dragPolyPts[2] == i || (m_dragPolyNumPts == 4 && m_dragPolyPts[3] == i)))
            {
                // Actively dragged polygon vertex in yellow
                drawPoint(wPos, Vector(1.0, 0.9, 0.1), pointSize + 2.0);
            }
            else if (m_ctrlHeld && m_shiftHeld && m_deleteHighlight.type == DeleteTargetType::Vertex && m_deleteHighlight.index == i)
            {
                // Already drawn in delete highlight
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Vertex && m_hoverTweak.index == i)
            {
                // Hovered vertex in highlight color (white)
                drawPoint(wPos, highlightColor, pointSize + 2.0);
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Edge && (m_hoverTweak.edgeV0 == i || m_hoverTweak.edgeV1 == i))
            {
                // Hovered edge vertex in highlight color
                drawPoint(wPos, highlightColor, pointSize + 2.0);
            }
            else if (!m_shiftHeld && !m_ctrlHeld && m_hoverTweak.mode == TweakMode::Polygon &&
                     (m_hoverTweak.polyPts[0] == i || m_hoverTweak.polyPts[1] == i || m_hoverTweak.polyPts[2] == i || (m_hoverTweak.polyIsQuad && m_hoverTweak.polyPts[3] == i)))
            {
                // Hovered polygon vertex in highlight color
                drawPoint(wPos, highlightColor, pointSize + 2.0);
            }
            else
            {
                // Standard retopo dot in wire color
                drawPoint(wPos, wireColor, pointSize);
            }
        }
    }

    // 8. Draw snap cursor indicator on surface (when placing dots)
    if (!m_shiftHeld && !m_ctrlHeld && m_activeDragMode == TweakMode::None && m_hoverTweak.mode == TweakMode::None && m_hoverSnap.valid)
    {
        drawPoint(m_hoverSnap.worldPos, wireColor, pointSize);
    }

    bd->SetDrawParam(DRAW_PARAMETER_LINEWIDTH, oldLineWidth);
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
        "QuadDraw Retopo Tool (Maya-style)\n- LMB: Click on surface to drop points\n- LMB Drag: Move/tweak vertex (weld on drop onto another vertex)\n- Shift + Hover: Preview prospective quad polygon\n- Shift + LMB: Create quad polygon\n- Shift + LMB Drag: Relax mesh (Maya-style Relax Brush)\n- Ctrl + Hover: Preview Cut / Insert Edge Loop (Maya-style)\n- Ctrl + LMB: Insert Edge Loop / Cut edges (drag to slide, Esc to cancel)\n- Ctrl + Shift + Hover: Highlight Vertex, Edge, or Polygon in red for deletion\n- Ctrl + Shift + LMB: Delete highlighted component\n- Esc: Clear active preview"_s,
        NewObjClear(QuadDrawToolData)
    );
}

} // namespace cinema
