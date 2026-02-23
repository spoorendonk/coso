#include "model/packing_model.h"

namespace coso {

int PackingModel::add_bin_type(BinTypeParams /*p*/)
{
    return 0;
}

int PackingModel::add_item(ItemParams /*p*/)
{
    return 0;
}

void PackingModel::add_conflict(int /*item_a*/, int /*item_b*/) {}

void PackingModel::minimize_bins() {}

Result PackingModel::solve(TimeLimit /*tl*/)
{
    return {};
}

} // namespace coso
