#pragma once

#include <array>

// Note : the functions in this files are defined in the hpp because constexpr functions can't be declared in a .hpp
// and defined in a .cpp

namespace gris
{
/**
 * Compute a SpeakerGroup's rotation quaternion from its yaw pitch and roll euler angles.
 */
[[nodiscard]] std::array<float, 4>
    getQuaternionFromEulerAngles(const float yawParam, const float pitchParam, const float rollParam)
{
    // The formula used here is for spaces with the y axis being up.
    // internally, spatGRIS is z axis up. We need to mixup the angles for our
    // quaternion to match this.
    const float yawDeg{ pitchParam };
    const float pitchDeg{ yawParam };
    const float rollDeg{ rollParam };

    const float yaw = yawDeg * radians_t::RADIAN_PER_DEGREE * -0.5;
    const float pitch = pitchDeg * radians_t::RADIAN_PER_DEGREE * 0.5;
    const float roll = rollDeg * radians_t::RADIAN_PER_DEGREE * 0.5;

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
    return std::array<float, 4>{
        cosYaw * sinPitch * cosRoll - sinYaw * cosPitch * sinRoll, // X
        sinYaw * cosPitchCosRoll + cosYaw * sinPitchSinRoll,       // Z
        cosYaw * cosPitch * sinRoll + sinYaw * sinPitch * cosRoll, // Y
        -(cosYaw * cosPitchCosRoll - sinYaw * sinPitchSinRoll)     // -W
    };
}
/**
 * Quaternion multiplication.
 */
[[nodiscard]] constexpr std::array<float, 4> quatMult(const std::array<float, 4> & a, const std::array<float, 4> & b)
{
    std::array<float, 4> result;
    result[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    result[1] = a[0] * b[1] + a[1] * b[0] - a[2] * b[3] + a[3] * b[2];
    result[2] = a[0] * b[2] + a[1] * b[3] + a[2] * b[0] - a[3] * b[1];
    result[3] = a[0] * b[3] - a[1] * b[2] + a[2] * b[1] + a[3] * b[0];
    return result;
}

/**
 * Quaternion inverse.
 */
[[nodiscard]] constexpr std::array<float, 4> quatInv(const std::array<float, 4> & a)
{
    return { a[0], -a[1], -a[2], -a[3] };
}

/**
 * Quaternion rotation of a xyz position. Returns a std::array of {x,y,z}.
 */
[[nodiscard]] constexpr std::array<float, 3> quatRotation(const std::array<float, 3> & xyz,
                                                          const std::array<float, 4> & rotQuat)
{
    std::array<float, 4> xyzQuat = { 0, xyz[0], xyz[1], xyz[2] };
    auto resultQuat = quatMult(quatMult(quatInv(rotQuat), xyzQuat), rotQuat);
    return { resultQuat[1], resultQuat[2], resultQuat[3] };
}
} // namespace gris
