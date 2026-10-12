/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

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
    // Terminated. Neither reaches a Failed instance.
    AssertLegalIn(
        LifecycleCall::WriteVariable,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event});
    AssertLegalIn(
        LifecycleCall::ReadVariable,
        {State::Instantiated,
         State::Initialization,
         State::Step,
         State::Event,
         State::Terminated});

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

    // Derivatives and stepping are Step-Mode only; discrete-state calls are
    // confined to the modes that evaluate them.
    AssertLegalIn(LifecycleCall::StepModeQuery, {State::Step});
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

    // Reset returns to Instantiated from every state, Failed included.
    for (const auto s : kAllStates) {
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

    // RestoreFromSnapshot() is the one way from Failed back to Step; from any
    // other state it leaves the instance where it was.
    {
        auto lc = At(State::Failed);
        lc.RestoreFromSnapshot();
        assert(lc.CurrentState() == State::Step);
    }
    for (const auto s : kAllStates) {
        if (s == State::Failed) continue;
        auto lc = At(s);
        lc.RestoreFromSnapshot();
        assert(lc.CurrentState() == s);
    }

    return 0;
}
