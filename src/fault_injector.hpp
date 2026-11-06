#pragma once

namespace file_collector::detail {

enum class FaultPoint {
    BeforeEnqueue,
    AfterReserve,
    BeforeCopy,
    BeforeCommit,
};

class FaultInjector {
public:
    virtual ~FaultInjector() = default;
    virtual void Hit(FaultPoint point) = 0;
};

}  // namespace file_collector::detail

