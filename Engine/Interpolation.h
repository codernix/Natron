//  Natron
//
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

#ifndef NATRON_ENGINE_INTERPOLATION_H_
#define NATRON_ENGINE_INTERPOLATION_H_

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "Global/Enums.h"
namespace Natron {
/**
 * @brief Interpolates using the control points P0(t0,v0) , P3(t3,v3)
 * and the derivatives P1(t1,v1) (being the derivative at P0 with respect to
 * t \in [t1,t2]) and P2(t2,v2) (being the derivative at P3 with respect to
 * t \in [t1,t2]) the value at 'currentTime' using the
 * interpolation method "interp".
 **/
double interpolate(double tcur, const double vcur, //start control point
                   const double vcurDerivRight, //being the derivative dv/dt at tcur
                   const double vnextDerivLeft, //being the derivative dv/dt at tnext
                   double tnext, const double vnext, //end control point
                   double currentTime,
                   KeyframeTypeEnum interp,
                   KeyframeTypeEnum interpNext) WARN_UNUSED_RETURN;

/// derive at currentTime. The derivative is with respect to currentTime
double derive(double tcur, const double vcur, //start control point
              const double vcurDerivRight, //being the derivative dv/dt at tcur
              const double vnextDerivLeft, //being the derivative dv/dt at tnext
              double tnext, const double vnext, //end control point
              double currentTime,
              KeyframeTypeEnum interp,
              KeyframeTypeEnum interpNext) WARN_UNUSED_RETURN;

/// derive at currentTime. The derivative is with respect to currentTime. The function is clamped between vmin an vmax.
double derive_clamp(double tcur, const double vcur, //start control point
                    const double vcurDerivRight, //being the derivative dv/dt at tcur
                    const double vnextDerivLeft, //being the derivative dv/dt at tnext
                    double tnext, const double vnext, //end control point
                    double currentTime,
                    double vmin, double vmax,
                    KeyframeTypeEnum interp,
                    KeyframeTypeEnum interpNext) WARN_UNUSED_RETURN;

/// integrate from time1 to time2 - be careful that time1 and time2 have to be in the range [tcur,tnext]
double integrate(double tcur, const double vcur, //start control point
                 const double vcurDerivRight, //being the derivative dv/dt at tcur
                 const double vnextDerivLeft, //being the derivative dv/dt at tnext
                 double tnext, const double vnext, //end control point
                 double time1, double time2,
                 KeyframeTypeEnum interp,
                 KeyframeTypeEnum interpNext) WARN_UNUSED_RETURN;

/// integrate from time1 to time2 - be careful that time1 and time2 have to be in the range [tcur,tnext]. The function is clamped between vmin an vmax.
double integrate_clamp(double tcur, const double vcur, //start control point
                       const double vcurDerivRight, //being the derivative dv/dt at tcur
                       const double vnextDerivLeft, //being the derivative dv/dt at tnext
                       double tnext, const double vnext, //end control point
                       double time1, double time2,
                       double vmin, double vmax,
                       Natron::KeyframeTypeEnum interp,
                       Natron::KeyframeTypeEnum interpNext) WARN_UNUSED_RETURN;

/**
 * @brief This function will set the left and right derivative of 'cur', depending on the interpolation method 'interp' and the
 * previous and next key frames.
 * ----------------------------------------------------------------------------
 * Using the Bezier cubic equation, its 2nd derivative can be expressed as such:
 * B''(t) = 6(1-t)(P2 - 2P1 + P0) + 6t(P3 - 2P2 + P1)
 * We have P1 = P0 + P0'_r / 3
 * and Q2 = Q3 - Q3'_l / 3
 * We can insert it in the 2nd derivative form, which yields:
 * B''(t) = 6(1-t)(P3 - P3'_l/3 - P0 - 2P0'_r/3) + 6t(P0 - P3 + 2P3'_l/3 + P0'_r/3)
 *
 * So for t = 0, we have:
 * B''(0) = 6(P3 - P0 - P3'_l / 3 - 2P0'_r / 3)
 * and for t = 1 , we have:
 * Q''(1) = 6(Q0 - Q3 + 2Q3'_l / 3 + Q0'_r / 3)
 *
 * We also know that the 1st derivative of B(t) at 0 is the derivative to P0
 * and the 1st derivative of B(t) at 1 is the derivative to P3, i.e:
 * B'(0) = P0'_r
 * B'(1) = P3'_l
 **/
/*
   Maple code to compute the values for each case:
   with(CodeGeneration):

   P := t -> (1-t)**3 * P0 + 3 * (1-t)**2 * t * P1 + 3 * (1-t) * t**2 * P2 + t**3 * P3:
   Q := t -> (1-t)**3 * Q0 + 3 * (1-t)**2 * t * Q1 + 3 * (1-t) * t**2 * Q2 + t**3 * Q3:

   dP := D(P):
   dP2 := D(dP):
   dQ := D(Q):
   dQ2 := D(dQ):

   P1 := P0 + P0pr / 3:
   Q2 := Q3 - Q3pl / 3:
   Q1 := Q0 + Q0pr / 3:
   P2 := P3 - P3pl / 3:
   Q3 := P0:

   derivativeAtCurRight := dP(0)/(tnext-tcur):
   curvatureAtCurRight := dP2(0)/(tnext-tcur):
   curvatureAtNextLeft:= dP2(1)/(tnext - tcur):
   derivativeAtCurLeft := dQ(1)/(tcur-tprev):
   curvatureAtCurLeft:= dQ2(1)/(tcur - tprev):
   curvatureAtPrevRight:= dQ2(0)/(tcur - tprev):

   printf("linear, general case:"):
   solve( {curvatureAtCurRight = 0, curvatureAtCurLeft = 0}, { P0pr, Q3pl });
   map(C,%):

   printf("linear, prev is linear:"):
   solve({curvatureAtCurRight = 0, curvatureAtCurLeft = 0, curvatureAtPrevRight = 0}, { P0pr, Q3pl, Q0pr});
   map(C,%):

   printf("linear, next is linear:"):
   solve({curvatureAtCurRight = 0, curvatureAtCurLeft = 0, curvatureAtNextLeft = 0}, {P0pr, Q3pl, P3pl});
   map(C,%):

   printf("linear, prev and next are linear:"):
   solve({curvatureAtCurRight = 0, curvatureAtCurLeft = 0, curvatureAtPrevRight = 0, curvatureAtNextLeft = 0}, {P0pr, Q3pl, Q0pr, P3pl});
   map(C,%):

   printf("cubic, general case:"):
   solve({curvatureAtCurRight = curvatureAtCurLeft, derivativeAtCurRight = derivativeAtCurLeft}, {P0pr, Q3pl});
   map(C,%):

   printf("cubic, prev is linear:"):
   solve({curvatureAtCurRight = curvatureAtCurLeft, derivativeAtCurRight = derivativeAtCurLeft, curvatureAtPrevRight = 0},{P0pr, Q3pl, Q0pr});
   map(C,%):

   printf("cubic, next is linear:"):
   solve({curvatureAtCurRight = curvatureAtCurLeft, derivativeAtCurRight = derivativeAtCurLeft, curvatureAtNextLeft = 0}, {P0pr, Q3pl, P3pl});
   map(C,%):

   printf("cubic, prev and next are linear"):
   solve({curvatureAtCurRight = curvatureAtCurLeft, derivativeAtCurRight = derivativeAtCurLeft, curvatureAtPrevRight = 0, curvatureAtNextLeft = 0},{P0pr, Q3pl, Q0pr, P3pl});
   map(C,%):

 */
void autoComputeDerivatives(Natron::KeyframeTypeEnum interpPrev,
                            Natron::KeyframeTypeEnum interp,
                            Natron::KeyframeTypeEnum interpNext,
                            double tprev, const double vprev, // vprev = Q0
                            double tcur, const double vcur, // vcur = Q3 = P0
                            double tnext, const double vnext, // vnext = P3
                            const double vprevDerivRight, // Q0'_r
                            const double vnextDerivLeft, // P3'_l
                            double *vcurDerivLeft, // Q3'_l
                            double *vcurDerivRight);  // P0'_r


/// solve linear equation c0 + c1*x = 0.
/// @returns the number of solutions.
/// solutions an and their order are put in s and o
int solveLinear(double c0, double c1, double s[1], int o[1]);

/// solve quadric c0 + c1*x + c2*x2 = 0.
/// @returns the number of solutions.
/// solutions an and their order are put in s and o
int solveQuadric(double c0, double c1, double c2, double s[2], int o[2]);

/// solve cubic c0 + c1*x + c2*x2 + c3*x3 = 0.
/// @returns the number of solutions.
/// solutions an and their order are put in s and o
int solveCubic(double c0, double c1, double c2, double c3, double s[3], int o[3]);

/// solve quartic c0 + c1*x + c2*x2 + c3*x3 +c4*x4 = 0.
/// @returns the number of solutions.
/// solutions an and their order are put in s and o
int solveQuartic(double c0, double c1, double c2, double c3, double c4, double s[4], int o[4]);
}


#endif // NATRON_ENGINE_INTERPOLATION_H_
