#include "quaddraw_tag.h"
#include "c4d_symbols.h"
#include "description/tquaddraw.h"

namespace cinema
{

Bool QuadDrawTagData::Init(GeListNode* node, Bool isCloneInit)
{
    BaseTag* tag = static_cast<BaseTag*>(node);
    if (!tag) return false;
    BaseContainer* data = tag->GetDataInstance();
    if (!data) return false;

    if (!isCloneInit && m_pinned)
    {
        m_pinned->DeselectAll();
    }

    return true;
}

void QuadDrawTagData::Free(GeListNode* node)
{
    if (m_pinned)
    {
        m_pinned->DeselectAll();
    }
}

Bool QuadDrawTagData::Read(GeListNode* node, HyperFile* hf, Int32 level)
{
    if (m_pinned && hf)
    {
        m_pinned->Read(hf);
    }
    return TagData::Read(node, hf, level);
}

Bool QuadDrawTagData::Write(const GeListNode* node, HyperFile* hf) const
{
    if (m_pinned && hf)
    {
        m_pinned->Write(hf);
    }
    return TagData::Write(node, hf);
}

Bool QuadDrawTagData::CopyTo(NodeData* dest, const GeListNode* snode, GeListNode* dnode, COPYFLAGS flags, AliasTrans* trn) const
{
    const QuadDrawTagData* srcData = this;
    QuadDrawTagData* dstData = static_cast<QuadDrawTagData*>(dest);
    if (dstData && srcData->m_pinned && dstData->m_pinned)
    {
        srcData->m_pinned->CopyTo(dstData->m_pinned);
    }
    return TagData::CopyTo(dest, snode, dnode, flags, trn);
}

Bool QuadDrawTagData::GetDDescription(const GeListNode* node, Description* description, DESCFLAGS_DESC& flags) const
{
    if (!description)
        return false;

    if (!description->LoadDescription("Tquaddraw"_s) && !description->LoadDescription(PLUGIN_ID_QUADDRAW_TAG))
    {
        // Dynamic fallback in case file loading is bypassed
        const DescID* singleid = description->GetSingleDescID();
        DescID cid = ConstDescID(DescLevel(QUADDRAW_TAG_TARGET, DTYPE_BASELISTLINK, 0));
        if (!singleid || cid.IsPartOf(*singleid, nullptr))
        {
            BaseContainer bc = GetCustomDataTypeDefault(DTYPE_BASELISTLINK);
            bc.SetString(DESC_NAME, "Target Mesh"_s);
            bc.SetString(DESC_SHORT_NAME, "Target"_s);
            BaseContainer accept;
            accept.SetBool(Obase, true);
            bc.SetContainer(DESC_ACCEPT, accept);
            description->SetParameter(cid, bc, ConstDescID(DescLevel(ID_TAGPROPERTIES)));
        }
    }

    flags |= DESCFLAGS_DESC::LOADED;
    return TagData::GetDDescription(node, description, flags);
}

EXECUTIONRESULT QuadDrawTagData::Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags)
{
    return EXECUTIONRESULT::OK;
}

QuadDrawTagData* GetQuadDrawTagData(PolygonObject* mesh)
{
    if (!mesh) return nullptr;
    BaseTag* tag = mesh->GetTag(PLUGIN_ID_QUADDRAW_TAG);
    if (!tag) return nullptr;
    return tag->GetNodeData<QuadDrawTagData>();
}

BaseSelect* GetMeshPinnedSelection(PolygonObject* mesh)
{
    QuadDrawTagData* td = GetQuadDrawTagData(mesh);
    return td ? td->GetPinnedSelection() : nullptr;
}

Bool IsVertexPinned(PolygonObject* mesh, Int32 ptIdx)
{
    BaseSelect* bs = GetMeshPinnedSelection(mesh);
    return (bs && ptIdx >= 0) ? bs->IsSelected(ptIdx) : false;
}

void RemapPinnedVertices(PolygonObject* mesh, const maxon::BaseArray<Int32>& oldToNew)
{
    BaseSelect* bs = GetMeshPinnedSelection(mesh);
    if (!bs || bs->GetCount() == 0) return;

    AutoAlloc<BaseSelect> newPins;
    if (!newPins) return;

    Int32 seg = 0, a = 0, b = 0;
    while (bs->GetRange(seg++, LIMIT<Int32>::MAX, &a, &b))
    {
        for (Int32 i = a; i <= b; ++i)
        {
            if (i >= 0 && i < (Int32)oldToNew.GetCount())
            {
                Int32 newIdx = oldToNew[i];
                if (newIdx != NOTOK)
                {
                    newPins->Select(newIdx);
                }
            }
        }
    }
    newPins->CopyTo(bs);
}

Bool RegisterQuadDrawTag()
{
    return RegisterTagPlugin(
        PLUGIN_ID_QUADDRAW_TAG,
        "QuadDraw"_s,
        TAG_VISIBLE,
        QuadDrawTagData::Alloc,
        "Tquaddraw"_s,
        AutoBitmap("quaddraw.png"_s),
        0
    );
}

} // namespace cinema
