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
    ExitInitialization,  // Initialization    -> Step (Event if eventModeUsed)
    EnterEventMode,      // Step              -> Event
    EnterStepMode,       // Event             -> Step
    Terminate,           // Init/Step/Event   -> Terminated
    Reset,               // any               -> Instantiated

    // Non-transitioning calls -- legal in a set of states, but a success
    // leaves the instance where it was.
    ReadVariable,      // every state before Terminate, plus Terminated
    WriteVariable,     // every state before Terminate
    SnapshotMemory,    // any state, Failed included
    FmuStateRead,      // any state except Failed
    FmuStateRestore,   // any state (the mode is set by RestoreFromSnapshot)
    DerivativeQuery,   // Initialization, Step, Event or Terminated
                       // (directional/adjoint derivatives)
    OutputDerivatives, // Step, and Intermediate Update when gets are allowed
    EvaluateDiscreteStates, // Initialization or Event
    UpdateDiscreteStates,   // Event only
    DoStep                  // Step only
};

/* The lifecycle of one FMI 3.0 Co-Simulation instance.
 *
 * Owns the instance's state and is the only thing that can change it. A state
 * change happens only through a named transition -- OnSuccess() for the calls
 * whose success advances the instance, Fail() for a throw that is not a mere
 * refusal, and RestoreFromSnapshot() for a restored FMU state.
 * The legal-transition table it consults lives in one place, so the whole
 * state graph reads and tests as a unit, without instantiating an FMU or
 * crossing the C linkage boundary.
 */
class Lifecycle
{
public:
    /* The six modes FMI 3.0 Co-Simulation moves an instance through, plus
   * Failed: not a mode of the interface but the state an instance is left in
   * once a call has returned fmi3Error, from which only freeing, resetting
   * and restoring a saved state escape. IntermediateUpdate is entered only by
   * the FMU, from within fmi3DoStep, for as long as it is in the importer's
   * fmi3IntermediateUpdateCallback.
   */
    enum class State
    {
        Instantiated,
        Initialization,
        Step,
        Event,
        Terminated,
        Failed,
        IntermediateUpdate
    };

    /* 'eventModeUsed' is the argument of fmi3InstantiateCoSimulation: FMI 3.0
   * puts an instance that uses Event Mode in Event Mode, not Step Mode, when
   * it leaves Initialization Mode.
   */
    explicit Lifecycle(bool eventModeUsed = false) noexcept;

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

    /* True when an FMU state can be taken in 'mode', and so restored into it:
   * every mode but Failed and IntermediateUpdate, where FMI 3.0 does not
   * allow fmi3GetFMUState.
   */
    static bool IsSnapshotMode(State mode) noexcept;

    /* Puts the instance in 'saved', the mode the restored FMU state was taken
   * in: FMI 3.0 requires a state to hold everything needed to continue from
   * it (2.2.6), so an instance restored into keeps going from that mode, not
   * its own. This is also FMI 3.0's recovery idiom -- restoring a Step-mode
   * state is the way back from Failed other than Reset. A no-op when
   * 'saved' is not a snapshot mode.
   */
    void RestoreFromSnapshot(State saved) noexcept;

    /* Step -> IntermediateUpdate, around the importer's
   * fmi3IntermediateUpdateCallback. In it, reading variables and output
   * derivatives is legal if 'variableGetAllowed' and until the first write;
   * writing variables if 'variableSetRequested'; nothing else. A no-op
   * outside Step.
   */
    void EnterIntermediateUpdate(
        bool variableSetRequested,
        bool variableGetAllowed) noexcept;

    /* IntermediateUpdate -> Step. A no-op from any other state: an instance
   * that failed during the update stays Failed.
   */
    void LeaveIntermediateUpdate() noexcept;

private:
    State m_state;
    bool m_eventModeUsed;
    bool m_setRequested = false;
    bool m_getAllowed = false;
    bool m_setStarted = false;
};

} // namespace cppfmu

#endif // header guard
