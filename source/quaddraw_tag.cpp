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

    if (!isCloneInit)
    {
        // QUADDRAW_TAG_TARGET is empty by default
    }

    return true;
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
