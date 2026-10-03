#ifndef QUADDRAW_TAG_H__
#define QUADDRAW_TAG_H__

#include "c4d.h"
#include "description/tquaddraw.h"

#define PLUGIN_ID_QUADDRAW_TAG 1067829

namespace cinema
{

class QuadDrawTagData : public TagData
{
public:
    virtual Bool Init(GeListNode* node, Bool isCloneInit) override;
    virtual Bool GetDDescription(const GeListNode* node, Description* description, DESCFLAGS_DESC& flags) const override;
    virtual EXECUTIONRESULT Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags) override;

    static NodeData* Alloc() { return NewObjClear(QuadDrawTagData); }
};

Bool RegisterQuadDrawTag();

} // namespace cinema

#endif // QUADDRAW_TAG_H__
