/* Copyright 2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef CPPFMU_EXAMPLE_MASS_SPRING_DAMPER_MODEL_HPP
#define CPPFMU_EXAMPLE_MASS_SPRING_DAMPER_MODEL_HPP

/* Shared physics for the mass-spring-damper example.
 *
 * This header is deliberately FMI-agnostic: it knows nothing about cppfmu,
 * value references, or any FMI type. Both the FMI 2.0 and the FMI 3.0 slave
 * adapters wrap this same struct, which keeps the two examples consistent and
 * illustrates a good practice -- keep your model separate from the FMI glue.
 *
 * The model is a single mass on a spring and a damper, driven by an external
 * force:
 *
 *     m * x'' + c * x' + k * x = F
 *
 * written as a first-order system in position (x) and velocity (v):
 *
 *     x' = v
 *     v' = (F - c * v - k * x) / m
 */

namespace msd
{

struct MassSpringDamper
{
    // Parameters (set before/at initialization).
    double mass = 1.0;       // m  [kg]   (must be > 0)
    double stiffness = 10.0; // k  [N/m]
    double damping = 0.5;    // c  [N*s/m]

    // Input (set every step).
    double force = 0.0; // F  [N]

    // State / outputs.
    double position = 0.0; // x  [m]
    double velocity = 0.0; // v  [m/s]

    // Resets state to the given initial conditions. Parameters are left as-is.
    void Reset(double initialPosition = 0.0, double initialVelocity = 0.0)
    {
        position = initialPosition;
        velocity = initialVelocity;
    }

    /* Advances the state by 'dt' seconds using semi-implicit (symplectic)
   * Euler integration, which is stable and energy-preserving for this kind
   * of oscillatory system. A single FMI DoStep may call this several times
   * with a smaller internal step for accuracy.
   */
    void Integrate(double dt)
    {
        const double acceleration =
            (force - damping * velocity - stiffness * position) / mass;
        velocity += acceleration * dt; // update velocity first ...
        position += velocity * dt;     // ... then position (symplectic Euler)
    }

    /* Advances by 'stepSize' seconds, subdividing it into internal steps no
   * larger than 'maxInternalStep' for accuracy.
   */
    void DoStep(double stepSize, double maxInternalStep = 1e-3)
    {
        if (stepSize <= 0.0) return;
        int subSteps = static_cast<int>(stepSize / maxInternalStep);
        if (subSteps < 1) subSteps = 1;
        const double dt = stepSize / subSteps;
        for (int i = 0; i < subSteps; ++i) {
            Integrate(dt);
        }
    }
};

} // namespace msd

#endif // header guard
