#include "nvperf_wrapper.h"
#include "nvperf_host_impl.h"  // Include ONLY here
#define NV_PERF_ENABLE_INSTRUMENTATION
//#include <NvPerfReportGeneratorVulkan.h>
namespace NvPerfWrapper {
    bool InitializeNvPerf() {
        // Implementation using nvperf functions
        return true;
    }
    
    void ShutdownNvPerf() {
        // Implementation using nvperf functions
    }
    
    // Implement other wrapper functions
}