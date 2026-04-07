#pragma once
#include <Arduino.h>

// Coordinate system:
//   Origin = left anchor point
//   X = rightward, Y = downward (gravity)
//
//   Left Anchor (0,0)           Right Anchor (W,0)
//          ●──────────────────────●
//           \                    /
//        Ll  \                  / Lr
//              \              /
//               ● Pen (x, y)

namespace Kinematics {

// Inverse kinematics: Cartesian (x, y) → cable lengths
inline void cartesianToLengths(float x, float y, float anchorWidth,
                               float gondolaWidth, float &leftLen,
                               float &rightLen) {
  // Left cable attaches to (x - w/2, y) from (0,0)
  float lx = x - (gondolaWidth / 2.0f);
  leftLen = sqrtf(lx * lx + y * y);

  // Right cable attaches to (x + w/2, y) from (W,0)
  float rx = x + (gondolaWidth / 2.0f);
  float dx = anchorWidth - rx;
  rightLen = sqrtf(dx * dx + y * y);
}

// Forward kinematics: cable lengths → Cartesian (x, y)
inline bool lengthsToCartesian(float leftLen, float rightLen, float anchorWidth,
                               float gondolaWidth, float &x, float &y) {
  if (anchorWidth <= gondolaWidth)
    return false;

  // Law of Cosines approach is cleaner for offsets, but let's derive the
  // intersection of two circles Circle 1 (Left): Center (gw/2, 0) relative to
  // virtual left? No. Let's solve for x, y where: L^2 = (x - gw/2)^2 + y^2 R^2
  // = (W - (x + gw/2))^2 + y^2
  //
  // Expand:
  // L^2 = x^2 - x*gw + gw^2/4 + y^2
  // R^2 = ((W - gw/2) - x)^2 + y^2
  // Let W' = W - gw/2. Then R^2 = (W' - x)^2 + y^2 = W'^2 - 2W'x + x^2 + y^2
  //
  // y^2 = L^2 - (x - gw/2)^2
  // Substitute into R:
  // R^2 = (W - gw/2 - x)^2 + L^2 - (x - gw/2)^2
  //
  // This algebra is getting messy. Let's try matching the simple V-plotter
  // model by adjusting the "Effective Width". Actually, imagine the gondola is
  // a single point, but the anchors are moved inward by gw/2? No, that's wrong.
  //
  // Correct derivation:
  // L^2 = (x - w/2)^2 + y^2  => x^2 - wx + w^2/4 + y^2 = L^2
  // R^2 = (W - (x + w/2))^2 + y^2 => (W - w/2 - x)^2 + y^2 = R^2
  // Let A = w/2.
  // L^2 = (x-A)^2 + y^2
  // R^2 = (W-A-x)^2 + y^2
  //
  // Subtract equations:
  // L^2 - R^2 = (x-A)^2 - (W-A-x)^2
  // L^2 - R^2 = (x^2 - 2Ax + A^2) - ((W-A)^2 - 2(W-A)x + x^2)
  // L^2 - R^2 = x^2 - 2Ax + A^2 - (W-A)^2 + 2(W-A)x - x^2
  // L^2 - R^2 = -2Ax + A^2 - (W-A)^2 + 2Wx - 2Ax
  // L^2 - R^2 = 2Wx - 4Ax + A^2 - (W-A)^2
  //
  // We want to solve for x:
  // 2x(W - 2A) = L^2 - R^2 - A^2 + (W-A)^2
  // Since A = w/2, 2A = w.
  // 2x(W - w) = L^2 - R^2 - (w/2)^2 + (W - w/2)^2
  //
  // x = [ L^2 - R^2 - (w/2)^2 + (W - w/2)^2 ] / [ 2(W - w) ]
  //
  // Once we have x, solve for y:
  // y = sqrt( L^2 - (x - w/2)^2 )

  float w = gondolaWidth;
  float W = anchorWidth;
  if (W <= w)
    return false;

  float A = w / 2.0f;
  float numerator =
      leftLen * leftLen - rightLen * rightLen - A * A + (W - A) * (W - A);
  float denominator = 2.0f * (W - w);

  x = numerator / denominator;

  float term = (x - A) * (x - A);
  if (leftLen * leftLen < term)
    return false;

  y = sqrtf(leftLen * leftLen - term);
  return true;
}

// Convert mm distance to motor steps
inline long mmToSteps(float mm, float stepsPerMm) {
  return (long)(mm * stepsPerMm);
}

// Convert motor steps to mm
inline float stepsToMm(long steps, float stepsPerMm) {
  return (float)steps / stepsPerMm;
}

} // namespace Kinematics
