#pragma once

namespace Hooks::CampaignUIHook
{
// Install before the campaign UI is constructed. A failure leaves the new Lua controls unavailable.
bool Attach();
bool Detach();
bool IsReady();

// The engine's active encyclopedia prefix, or nullptr if it could not be resolved.
const char* GetEncyclopediaUrl();

// nullptr or an empty string restores the original prefix. The hook owns custom string storage.
bool SetEncyclopediaUrl(const char* aUrl);
} // namespace Hooks::CampaignUIHook
