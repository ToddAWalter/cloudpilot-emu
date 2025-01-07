#ifndef _LOGGING_H_
#define _LOGGING_H_

#include "EmCommon.h"

namespace logging {
    enum Domain : uint32 { domainNetlib = 0x1, domainDebugger = 0x2, domainInstaller = 0x04 };

    int printf(const char* format, ...);

    int printf(uint32 domain, const char* format, ...);

    void enable();

    void disable();

    void enableDomain(Domain domain);

    void disableDomain(Domain domain);
}  // namespace logging

#endif  // _LOGGING_H
