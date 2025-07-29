#include "Quaternion.hpp"
#include <cmath>
#include "../Data/StrongTypes/sg_Radians.hpp"

namespace gris
{
Quaternion getQuaternionFromEulerAngles(const float yawParam, const float pitchParam, const float rollParam)
{
    // The formula used here is for spaces with the y axis being up.
    // internally, spatGRIS is z axis up. We need to mixup the angles for our
    // quaternion to match this.
    const float yawDeg{ pitchParam };
    const float pitchDeg{ yawParam };
    const float rollDeg{ rollParam };

    const float yaw = yawDeg * radians_t::RADIAN_PER_DEGREE * -0.5f;
    const float pitch = pitchDeg * radians_t::RADIAN_PER_DEGREE * 0.5f;
    const float roll = rollDeg * radians_t::RADIAN_PER_DEGREE * 0.5f;

    const float sinYaw = std::sin(yaw);
    const float cosYaw = std::cos(yaw);
    const float sinPitch = std::sin(pitch);
    const float cosPitch = std::cos(pitch);
    const float sinRoll = std::sin(roll);
    const float cosRoll = std::cos(roll);
    const float cosPitchCosRoll = cosPitch * cosRoll;
    const float sinPitchSinRoll = sinPitch * sinRoll;

    // Also we needed to swap out Z and Y and negate W to make the
    // quaternion work with the left handed coordinate system spatGRIS uses.
    return Quaternion{
        cosYaw * sinPitch * cosRoll - sinYaw * cosPitch * sinRoll, // X
        sinYaw * cosPitchCosRoll + cosYaw * sinPitchSinRoll,       // Z
        cosYaw * cosPitch * sinRoll + sinYaw * sinPitch * cosRoll, // Y
        -(cosYaw * cosPitchCosRoll - sinYaw * sinPitchSinRoll)     // -W
    };
}
} // namespace gris
