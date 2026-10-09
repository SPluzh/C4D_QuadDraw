#ifndef QUADDRAW_TAG_H__
#define QUADDRAW_TAG_H__

#include "c4d.h"
#include "description/tquaddraw.h"

#define PLUGIN_ID_QUADDRAW_TAG 1071076

namespace cinema
{

class QuadDrawTagData : public TagData
{
public:
    AutoAlloc<BaseSelect> m_pinned;

    virtual Bool Init(GeListNode* node, Bool isCloneInit) override;
    virtual void Free(GeListNode* node) override;
    virtual Bool Read(GeListNode* node, HyperFile* hf, Int32 level) override;
    virtual Bool Write(const GeListNode* node, HyperFile* hf) const override;
    virtual Bool CopyTo(NodeData* dest, const GeListNode* snode, GeListNode* dnode, COPYFLAGS flags, AliasTrans* trn) const override;
    virtual Bool GetDDescription(const GeListNode* node, Description* description, DESCFLAGS_DESC& flags) const override;
    virtual EXECUTIONRESULT Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags) override;

    BaseSelect* GetPinnedSelection() { return m_pinned; }
    const BaseSelect* GetPinnedSelection() const { return m_pinned; }

    static NodeData* Alloc() { return NewObjClear(QuadDrawTagData); }
};

QuadDrawTagData* GetQuadDrawTagData(PolygonObject* mesh);
BaseSelect*      GetMeshPinnedSelection(PolygonObject* mesh);
Bool             IsVertexPinned(PolygonObject* mesh, Int32 ptIdx);
void             RemapPinnedVertices(PolygonObject* mesh, const maxon::BaseArray<Int32>& oldToNew);

Bool RegisterQuadDrawTag();

} // namespace cinema

#endif // QUADDRAW_TAG_H__
