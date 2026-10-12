/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef CPPFMU_LIFECYCLE_FMI3_HPP
#define CPPFMU_LIFECYCLE_FMI3_HPP

namespace cppfmu
{

/* The kind of call the host is making, grouped by the rule that governs it.
 *
 * Two calls share a value only when they share both the set of states they are
 * legal in and the transition a success performs. Every fmi3* Get* is a
 * ReadVariable, every Set* a WriteVariable, and so on: one value per distinct
 * rule, not one per C entry point. The transition-bearing lifecycle calls each
 * keep their own value because each moves to a different state.
 */
enum class LifecycleCall
{
    // Lifecycle transitions -- each moves the instance to a new state.
    EnterInitialization, // Instantiated      -> Initialization
    ExitInitialization,  // Initialization    -> Step
    EnterEventMode,      // Step              -> Event
    EnterStepMode,       // Event             -> Step
    Terminate,           // Init/Step/Event   -> Terminated
    Reset,               // any               -> Instantiated

    // Non-transitioning calls -- legal in a set of states, but a success
    // leaves the instance where it was.
    ReadVariable,    // every state before Terminate, plus Terminated
    WriteVariable,   // every state before Terminate
    SnapshotMemory,  // any state, Failed included
    FmuStateRead,    // any state except Failed
    FmuStateRestore, // any state (recovery handled by RestoreFromSnapshot)
    StepModeQuery,   // Step only (directional/adjoint/output derivatives)
    EvaluateDiscreteStates, // Initialization or Event
    UpdateDiscreteStates,   // Event only
    DoStep                  // Step only
};

/* The lifecycle of one FMI 3.0 Co-Simulation instance.
 *
 * Owns the instance's state and is the only thing that can change it. A state
 * change happens only through a named transition -- OnSuccess() for the calls
 * whose success advances the instance, Fail() for a throw that is not a mere
 * refusal, and RestoreFromSnapshot() for FMI 3.0's one way back from Failed.
 * The legal-transition table it consults lives in one place, so the whole
 * state graph reads and tests as a unit, without instantiating an FMU or
 * crossing the C linkage boundary.
 */
class Lifecycle
{
public:
    /* The five modes FMI 3.0 Co-Simulation moves an instance through, plus
   * Failed: not a mode of the interface but the state an instance is left in
   * once a call has returned fmi3Error, from which only freeing, resetting
   * and restoring a saved state escape.
   */
    enum class State
    {
        Instantiated,
        Initialization,
        Step,
        Event,
        Terminated,
        Failed
    };

    Lifecycle() noexcept;

    State CurrentState() const noexcept;

    /* True when 'call' may be made in the current state. A call refused here
   * never reaches the slave and changes nothing.
   */
    bool Allows(LifecycleCall call) const noexcept;

    /* Advances the instance to the state a successful 'call' leaves it in.
   * A no-op for calls that carry no transition.
   */
    void OnSuccess(LifecycleCall call) noexcept;

    /* Latches Failed. Called for any throw that is not a refusal (a
   * std::logic_error is a violated precondition, not a failed simulation,
   * and does not come here).
   */
    void Fail() noexcept;

    /* FMI 3.0's recovery idiom: restore a Step-mode snapshot and keep
   * stepping. The only path from Failed back to Step; a no-op from any
   * other state, where the instance keeps the mode it restored into.
   */
    void RestoreFromSnapshot() noexcept;

private:
    State m_state;
};

} // namespace cppfmu

#endif // header guard
