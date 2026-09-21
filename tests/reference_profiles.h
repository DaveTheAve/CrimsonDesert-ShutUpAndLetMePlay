#pragma once
// Test fixtures only. run_tests.sh authenticates the complete reference file by
// SHA-256 before isolated native code is executed. Runtime resolution never uses
// these timestamps, expected RVAs or fixture boundary addresses.
inline crimson::U32 referenceCodeDelta(const crimson::Image& image) {
    if(image.timestamp==0x6AABB038u&&image.size==0x17FCD000u)return 0;
    if(image.timestamp==0x6AB0B06Du&&image.size==0x17479000u)return 16;
    throw std::runtime_error("unrecognized isolation-test reference image");
}
