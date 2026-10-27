#pragma once

namespace Integrity
{
// Snapshot __TEXT before any hooks/patches, redirect the hash anchor, and keep its one executable call in the live
// image. The snapshot lives until process exit. Failure means callers must not install code modifications.
bool InstallCleanTextAnchor();
} // namespace Integrity
