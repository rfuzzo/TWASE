#pragma once

#include <cstdint>

namespace CampaignUI
{
constexpr int32_t MaxSlotOverride = 20;

// Zero means the game's native limit. These settings affect the next settlement-card initialization, not the DB.
int32_t GetMaxSlots(bool aMainCard);
void SetMaxSlots(bool aMainCard, int32_t aSlots);
void ClearMaxSlots();
} // namespace CampaignUI
