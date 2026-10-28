#include "stdafx.hpp"

#include "CampaignUI.hpp"

#include <atomic>

namespace
{
std::atomic<int32_t> mainCardSlots{0};
std::atomic<int32_t> otherCardSlots{0};
}

int32_t CampaignUI::GetMaxSlots(bool aMainCard)
{
    return (aMainCard ? mainCardSlots : otherCardSlots).load(std::memory_order_relaxed);
}

void CampaignUI::SetMaxSlots(bool aMainCard, int32_t aSlots)
{
    (aMainCard ? mainCardSlots : otherCardSlots).store(aSlots, std::memory_order_relaxed);
}

void CampaignUI::ClearMaxSlots()
{
    mainCardSlots.store(0, std::memory_order_relaxed);
    otherCardSlots.store(0, std::memory_order_relaxed);
}
