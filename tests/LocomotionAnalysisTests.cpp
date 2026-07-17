#include "LocomotionAnalysis.h"

#include <cassert>
#include <cmath>

int main()
{
    using TLV::ClassifyForwardSpeed;
    using TLV::LocomotionBand;
    using TLV::NormalizeStick;

    const TLV::AnalysisSettings settings{
        0.08F,
        0.18F,
        0.55F,
        0.75F
    };

    assert(std::fabs(NormalizeStick(32767) - 1.0F) < 0.0001F);
    assert(std::fabs(NormalizeStick(-32768) + 1.0F) < 0.0001F);
    assert(ClassifyForwardSpeed(0.0F, settings) == LocomotionBand::idle);
    assert(ClassifyForwardSpeed(0.17F, settings) == LocomotionBand::idle);
    assert(ClassifyForwardSpeed(0.18F, settings) == LocomotionBand::walk);
    assert(ClassifyForwardSpeed(0.55F, settings) == LocomotionBand::run);
    assert(ClassifyForwardSpeed(0.75F, settings) == LocomotionBand::sprint);
    assert(ClassifyForwardSpeed(-0.80F, settings) == LocomotionBand::idle);

    return 0;
}
