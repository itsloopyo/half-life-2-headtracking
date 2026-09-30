// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo / CameraUnlock
#include <cstdio>
#include <cstring>

int RunSourceMathTests();
int RunPositionMappingTests();
int RunLogThrottleTests();
int RunBuildProfileTests();
int RunDiscoveryImageProbe(const char* path);
int RunDiscoveryTests();

int main(int argc, char** argv) {
    if (argc == 3 && std::strcmp(argv[1], "--image") == 0) return RunDiscoveryImageProbe(argv[2]);
    std::printf("HalfLife2HeadTracking tests\n===========================\n");
    const int failures =
        RunSourceMathTests() + RunPositionMappingTests() + RunLogThrottleTests() +
        RunBuildProfileTests() + RunDiscoveryTests();
    if (failures == 0) {
        std::printf("\nAll tests passed\n");
        return 0;
    }
    std::printf("\n%d test(s) FAILED\n", failures);
    return 1;
}
