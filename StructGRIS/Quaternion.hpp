#pragma once

#include <array>

// Note : the functions in this files are defined in the hpp because constexpr functions can't be declared in a .hpp
// and defined in a .cpp

namespace gris
{

using Quaternion = std::array<float, 4>;

/**
 * Compute a SpeakerGroup's rotation quaternion from its yaw pitch and roll euler angles.
 */
[[nodiscard]] Quaternion
    getQuaternionFromEulerAngles(const float yawParam, const float pitchParam, const float rollParam);

/**
 * Quaternion multiplication.
 */
[[nodiscard]] constexpr Quaternion quatMult(const Quaternion & a, const Quaternion & b)
{
    Quaternion result;
    result[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    result[1] = a[0] * b[1] + a[1] * b[0] - a[2] * b[3] + a[3] * b[2];
    result[2] = a[0] * b[2] + a[1] * b[3] + a[2] * b[0] - a[3] * b[1];
    result[3] = a[0] * b[3] - a[1] * b[2] + a[2] * b[1] + a[3] * b[0];
    return result;
}

/**
 * Quaternion inverse.
 */
[[nodiscard]] constexpr Quaternion quatInv(const Quaternion & a)
{
    return { a[0], -a[1], -a[2], -a[3] };
}

/**
 * Quaternion rotation of a xyz position. Returns a std::array of {x,y,z}.
 */
[[nodiscard]] constexpr std::array<float, 3> quatRotation(const std::array<float, 3> & xyz, const Quaternion & rotQuat)
{
    Quaternion xyzQuat = { 0, xyz[0], xyz[1], xyz[2] };
    auto resultQuat = quatMult(quatMult(quatInv(rotQuat), xyzQuat), rotQuat);
    return { resultQuat[1], resultQuat[2], resultQuat[3] };
}
} // namespace gris
