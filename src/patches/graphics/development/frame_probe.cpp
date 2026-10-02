#include "generated/xacalite_scriptteam/project_sylpheed_pch.h"
#include "features/performance/frame_metrics.h"

// D3DDevice_Swap, d3d9:swap.obj; MAP/PDB public and module exact match.
// Preserve the original function and its entire PPC context. Count completed
// game Swap calls, not UI paints or monitor refreshes. No generated edits.
REX_EXTERN(__imp__sub_8235CD78);

extern "C" REX_FUNC(sub_8235CD78) {
  __imp__sub_8235CD78(ctx, base);
  sylpheed::performance::Frames().Record();
}
