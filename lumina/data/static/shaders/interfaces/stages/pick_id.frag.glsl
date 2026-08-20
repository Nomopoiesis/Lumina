
layout(location = 0) flat in uint vPickID;

// R32_UINT target. 0 is the clear value and means "nothing here", so the ids
// written are 1-based.
layout(location = 0) out uint outPickID;
