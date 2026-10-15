/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// The checks are assert()s: keep them in Release builds, which the conan
// recipe tests in.
#undef NDEBUG

/* Exhaustive test of the FMI 3.0 lifecycle transition table.
 *
 * The point of the module is that its rules can be tested without a slave, an
 * FMU, or the C entry points. This test reaches the rules directly: for every
 * call it asserts the states it is legal in, and for every transition it
 * asserts the state a success leaves the instance in -- including the Failed
 * latch and the one conditional way back from it, which the C-ABI test cannot
 * easily reach.
 */
#include "cppfmu_lifecycle_fmi3.hpp"

#include <cassert>
#include <initializer_list>

using cppfmu::Lifecycle;
using cppfmu::LifecycleCall;
using State = cppfmu::Lifecycle::State;

namespace
{

const State kAllStates[] = {
    State::Instantiated,
    State::Initialization,
    State::Step,
    State::Event,
    State::Terminated,
    State::Failed,
    State::IntermediateUpdate,
};

// Drives a fresh Lifecycle to 'target' through the transitions that reach it,
// so a test can assert what a call does from any starting state.
Lifecycle At(State target)
{
    Lifecycle lc;
    switch (target) {
    case State::Instantiated:
        break;
    case State::Initialization:
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        break;
    case State::Step:
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        break;
    case State::Event:
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        lc.OnSuccess(LifecycleCall::EnterEventMode);
        break;
    case State::Terminated:
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        lc.OnSuccess(LifecycleCall::Terminate);
        break;
    case State::Failed:
        lc.Fail();
        break;
    case State::IntermediateUpdate:
        // Opened for both gets and sets: the widest the update can be.
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        lc.EnterIntermediateUpdate(true, true);
        break;
    }
    assert(lc.CurrentState() == target);
    return lc;
}

bool Contains(std::initializer_list<State> set, State s)
{
    for (const auto x : set) {
        if (x == s) return true;
    }
    return false;
}

// Asserts 'call' is allowed in exactly the states in 'legalIn' and no others.
void AssertLegalIn(LifecycleCall call, std::initializer_list<State> legalIn)
{
    for (const auto s : kAllStates) {
        const bool expected = Contains(legalIn, s);
        assert(At(s).Allows(call) == expected);
    }
}

// Asserts a successful 'call' from 'from' leaves the instance in 'to'.
void AssertTransition(State from, LifecycleCall call, State to)
{
    auto lc = At(from);
    lc.OnSuccess(call);
    assert(lc.CurrentState() == to);
}

} // namespace

int main()
{
    // -- A fresh instance is Instantiated. --
    assert(Lifecycle{}.CurrentState() == State::Instantiated);

    // ========================================================================
    // Legal-state sets: every call, every state.
    // ========================================================================

    // Lifecycle transitions are each legal in exactly one mode (Reset in all).
    AssertLegalIn(LifecycleCall::EnterInitialization, {State::Instantiated});
    AssertLegalIn(LifecycleCall::ExitInitialization, {State::Initialization});
    AssertLegalIn(LifecycleCall::EnterEventMode, {State::Step});
    AssertLegalIn(LifecycleCall::EnterStepMode, {State::Event});
    AssertLegalIn(
        LifecycleCall::Terminate,
        {State::Initialization, State::Step, State::Event});
    AssertLegalIn(
        LifecycleCall::Reset,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated,
         State::Failed});

    // A variable may be written before Terminate, and read in those plus
    // Terminated, and both in an intermediate update that allows it. Neither
    // reaches a Failed instance.
    AssertLegalIn(
        LifecycleCall::WriteVariable,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::IntermediateUpdate});
    AssertLegalIn(
        LifecycleCall::ReadVariable,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated,
         State::IntermediateUpdate});

    // Snapshot memory may be handled in any state, Failed included; a fresh
    // snapshot may be read in any state but Failed; a snapshot may be restored
    // from any state.
    AssertLegalIn(
        LifecycleCall::SnapshotMemory,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated,
         State::Failed});
    AssertLegalIn(
        LifecycleCall::FmuStateRead,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated});
    AssertLegalIn(
        LifecycleCall::FmuStateRestore,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated,
         State::Failed});

    // Derivatives may be asked for in every mode with a model (FMI 3.0, 2.3.3,
    // 2.3.5, 2.3.8, 4.2.1); stepping is Step-Mode only; discrete-state calls
    // are confined to the modes that evaluate them.
    AssertLegalIn(
        LifecycleCall::DerivativeQuery,
        {State::Initialization, State::Step, State::Event, State::Terminated});
    AssertLegalIn(
        LifecycleCall::OutputDerivatives,
        {State::Step, State::IntermediateUpdate});
    AssertLegalIn(LifecycleCall::DoStep, {State::Step});
    AssertLegalIn(
        LifecycleCall::EvaluateDiscreteStates,
        {State::Initialization, State::Event});
    AssertLegalIn(LifecycleCall::UpdateDiscreteStates, {State::Event});

    // ========================================================================
    // Transitions: where a successful call leaves the instance.
    // ========================================================================

    AssertTransition(
        State::Instantiated,
        LifecycleCall::EnterInitialization,
        State::Initialization);
    AssertTransition(
        State::Initialization,
        LifecycleCall::ExitInitialization,
        State::Step);
    AssertTransition(State::Step, LifecycleCall::EnterEventMode, State::Event);
    AssertTransition(State::Event, LifecycleCall::EnterStepMode, State::Step);
    AssertTransition(State::Step, LifecycleCall::Terminate, State::Terminated);
    AssertTransition(State::Event, LifecycleCall::Terminate, State::Terminated);
    AssertTransition(
        State::Initialization,
        LifecycleCall::Terminate,
        State::Terminated);

    // Reset returns to Instantiated from every state it is legal in, Failed
    // included.
    for (const auto s : kAllStates) {
        if (s == State::IntermediateUpdate) continue;
        AssertTransition(s, LifecycleCall::Reset, State::Instantiated);
    }

    // Non-transitioning calls leave the state alone in every state they are
    // legal in.
    for (const auto s : kAllStates) {
        AssertTransition(s, LifecycleCall::ReadVariable, s);
        AssertTransition(s, LifecycleCall::WriteVariable, s);
        AssertTransition(s, LifecycleCall::SnapshotMemory, s);
        AssertTransition(s, LifecycleCall::DoStep, s);
    }

    // ========================================================================
    // The Failed latch and the one way back.
    // ========================================================================

    // Fail() latches Failed from any state.
    for (const auto s : kAllStates) {
        auto lc = At(s);
        lc.Fail();
        assert(lc.CurrentState() == State::Failed);
    }

    // Once Failed, only free, reset and restore are still accepted; the running
    // and variable calls are refused.
    {
        auto lc = At(State::Failed);
        assert(lc.Allows(LifecycleCall::Reset));
        assert(lc.Allows(LifecycleCall::SnapshotMemory));
        assert(lc.Allows(LifecycleCall::FmuStateRestore));
        assert(!lc.Allows(LifecycleCall::ReadVariable));
        assert(!lc.Allows(LifecycleCall::WriteVariable));
        assert(!lc.Allows(LifecycleCall::DoStep));
        assert(!lc.Allows(LifecycleCall::Terminate));
        assert(!lc.Allows(LifecycleCall::FmuStateRead));
    }

    // ========================================================================
    // Restoring an FMU state puts the instance in the mode the state was taken
    // in, whatever mode it was in itself -- Failed included, which is how a
    // Step-mode state recovers a failed instance.
    // ========================================================================
    const State kSnapshotModes[] = {
        State::Instantiated,
        State::Initialization,
        State::Step,
        State::Event,
        State::Terminated,
    };
    for (const auto s : kAllStates) {
        const bool expected =
            s != State::Failed && s != State::IntermediateUpdate;
        assert(Lifecycle::IsSnapshotMode(s) == expected);
        assert(At(s).Allows(LifecycleCall::FmuStateRead) == expected);
    }
    for (const auto from : kAllStates) {
        if (!At(from).Allows(LifecycleCall::FmuStateRestore)) continue;
        for (const auto saved : kSnapshotModes) {
            auto lc = At(from);
            lc.RestoreFromSnapshot(saved);
            assert(lc.CurrentState() == saved);
        }
        // A mode no state can be taken in is ignored.
        for (const auto saved : {State::Failed, State::IntermediateUpdate}) {
            auto lc = At(from);
            lc.RestoreFromSnapshot(saved);
            assert(lc.CurrentState() == from);
        }
    }
    {
        // A Step-mode state restored into a fresh instance can be stepped
        // without going through initialization.
        Lifecycle lc;
        lc.RestoreFromSnapshot(State::Step);
        assert(lc.Allows(LifecycleCall::DoStep));
    }

    // ========================================================================
    // Intermediate Update Mode: entered from Step only, narrowed to what the
    // FMU opened it for, gets before sets, and left back to Step.
    // ========================================================================
    for (const auto s : kAllStates) {
        if (s == State::Step) continue;
        auto lc = At(s);
        lc.EnterIntermediateUpdate(true, true);
        assert(lc.CurrentState() == s);
    }
    {
        auto lc = At(State::Step);
        lc.EnterIntermediateUpdate(false, true);  // gets only
        assert(lc.CurrentState() == State::IntermediateUpdate);
        assert(lc.Allows(LifecycleCall::ReadVariable));
        assert(lc.Allows(LifecycleCall::OutputDerivatives));
        assert(!lc.Allows(LifecycleCall::WriteVariable));
        assert(!lc.Allows(LifecycleCall::DoStep));
        assert(!lc.Allows(LifecycleCall::DerivativeQuery));
        assert(!lc.Allows(LifecycleCall::FmuStateRead));
        assert(!lc.Allows(LifecycleCall::EnterEventMode));
        assert(!lc.Allows(LifecycleCall::Terminate));
        assert(!lc.Allows(LifecycleCall::Reset));
        lc.LeaveIntermediateUpdate();
        assert(lc.CurrentState() == State::Step);
        assert(lc.Allows(LifecycleCall::DoStep));
    }
    {
        auto lc = At(State::Step);
        lc.EnterIntermediateUpdate(true, false);  // sets only
        assert(!lc.Allows(LifecycleCall::ReadVariable));
        assert(!lc.Allows(LifecycleCall::OutputDerivatives));
        assert(lc.Allows(LifecycleCall::WriteVariable));
    }
    {
        auto lc = At(State::Step);
        lc.EnterIntermediateUpdate(true, true);
        assert(lc.Allows(LifecycleCall::ReadVariable));
        lc.OnSuccess(LifecycleCall::ReadVariable);
        assert(lc.Allows(LifecycleCall::WriteVariable));
        lc.OnSuccess(LifecycleCall::WriteVariable);
        assert(lc.CurrentState() == State::IntermediateUpdate);
        assert(!lc.Allows(LifecycleCall::ReadVariable));  // no gets after a set
        assert(!lc.Allows(LifecycleCall::OutputDerivatives));
        assert(lc.Allows(LifecycleCall::WriteVariable));
        lc.LeaveIntermediateUpdate();
        lc.EnterIntermediateUpdate(true, true);  // a new update starts afresh
        assert(lc.Allows(LifecycleCall::ReadVariable));
    }
    {
        // A failure in the update stays a failure after it.
        auto lc = At(State::IntermediateUpdate);
        lc.Fail();
        lc.LeaveIntermediateUpdate();
        assert(lc.CurrentState() == State::Failed);
    }

    // -- With Event Mode in use, Initialization leads to Event Mode, and the
    //    rest of the graph is unchanged. --
    {
        Lifecycle lc{true};
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        assert(lc.CurrentState() == State::Event);
        assert(lc.Allows(LifecycleCall::UpdateDiscreteStates));
        assert(!lc.Allows(LifecycleCall::DoStep));
        lc.OnSuccess(LifecycleCall::EnterStepMode);
        assert(lc.CurrentState() == State::Step);
        lc.OnSuccess(LifecycleCall::EnterEventMode);
        assert(lc.CurrentState() == State::Event);
        lc.OnSuccess(LifecycleCall::Reset);
        lc.OnSuccess(LifecycleCall::EnterInitialization);
        lc.OnSuccess(LifecycleCall::ExitInitialization);
        assert(lc.CurrentState() == State::Event);
    }

    return 0;
}
