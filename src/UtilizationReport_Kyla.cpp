// Compatibility adapter for the terminal layer. Ced's report module owns the
// utilization data and serialization so the file and console report stay aligned.
#include "Report_Ced.h"

namespace csopesy::terminal {

std::string utilization(const UtilizationSnapshot& snapshot) {
    return report::utilization(snapshot);
}

} // namespace csopesy::terminal
