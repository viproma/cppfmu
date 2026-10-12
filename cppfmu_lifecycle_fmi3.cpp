/* Copyright 2016-2026, SINTEF Ocean.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "cppfmu_lifecycle_fmi3.hpp"

namespace cppfmu
{

namespace
{

using State = Lifecycle::State;

// One bit per State, so a set of legal states is a mask the current state is
// tested against.
constexpr unsigned StateBit(State s)
{
    return 1u << static_cast<unsigned>(s);
}

constexpr unsigned kInstantiated = StateBit(State::Instantiated);
constexpr unsigned kInitialization = StateBit(State::Initialization);
constexpr unsigned kStep = StateBit(State::Step);
constexpr unsigned kEvent = StateBit(State::Event);
constexpr unsigned kTerminated = StateBit(State::Terminated);
constexpr unsigned kFailed = StateBit(State::Failed);

// A slave is free to build its simulation in ExitInitializationMode and to
// tear it down in Terminate, so "there is a run" spans Initialization, Step
// and Event.
constexpr unsigned kRunning = kInitialization | kStep | kEvent;

// A variable may be written in every state before Terminate, and read in
// those plus Terminated.
constexpr unsigned kWritable = kInstantiated | kRunning;
constexpr unsigned kReadable = kWritable | kTerminated;

// A transition's target. kNoTransition marks a call whose success leaves the
// instance where it was; the value is out of State's range so it can never be
// mistaken for one.
constexpr int kNoTransition = -1;

struct Rule
{
    LifecycleCall call;
    unsigned legalStates;
    int transition; // a State value, or kNoTransition
};

constexpr int To(State s)
{
    return static_cast<int>(s);
}

/* The whole state graph, one row per rule.
 *
 * The legal-state set and the transition of every FMI 3.0 Co-Simulation call
 * this layer serves live here and nowhere else. A reader auditing "what is
 * legal when, and where does it lead?" reads this table; a test iterates it.
 */
constexpr Rule kRules[] = {
    // Lifecycle transitions.
    {LifecycleCall::EnterInitialization,
     kInstantiated,
     To(State::Initialization)},
    {LifecycleCall::ExitInitialization, kInitialization, To(State::Step)},
    {LifecycleCall::EnterEventMode, kStep, To(State::Event)},
    {LifecycleCall::EnterStepMode, kEvent, To(State::Step)},
    {LifecycleCall::Terminate, kRunning, To(State::Terminated)},
    {LifecycleCall::Reset,
     kInstantiated | kRunning | kTerminated | kFailed,
     To(State::Instantiated)},

    // Non-transitioning calls.
    {LifecycleCall::ReadVariable, kReadable, kNoTransition},
    {LifecycleCall::WriteVariable, kWritable, kNoTransition},
    {LifecycleCall::SnapshotMemory,
     kInstantiated | kRunning | kTerminated | kFailed,
     kNoTransition},
    {LifecycleCall::FmuStateRead,
     kInstantiated | kRunning | kTerminated,
     kNoTransition},
    {LifecycleCall::FmuStateRestore,
     kInstantiated | kRunning | kTerminated | kFailed,
     kNoTransition},
    {LifecycleCall::StepModeQuery, kStep, kNoTransition},
    {LifecycleCall::EvaluateDiscreteStates,
     kInitialization | kEvent,
     kNoTransition},
    {LifecycleCall::UpdateDiscreteStates, kEvent, kNoTransition},
    {LifecycleCall::DoStep, kStep, kNoTransition},
};

const Rule& RuleFor(LifecycleCall call)
{
    for (const auto& rule : kRules) {
        if (rule.call == call) {
            return rule;
        }
    }
    // Every LifecycleCall has a row; the first is a safe, non-permissive
    // fallback that can never be reached.
    return kRules[0];
}

} // namespace

Lifecycle::Lifecycle() noexcept
    : m_state{State::Instantiated}
{ }

Lifecycle::State Lifecycle::CurrentState() const noexcept
{
    return m_state;
}

bool Lifecycle::Allows(LifecycleCall call) const noexcept
{
    return (RuleFor(call).legalStates & StateBit(m_state)) != 0u;
}

void Lifecycle::OnSuccess(LifecycleCall call) noexcept
{
    const auto transition = RuleFor(call).transition;
    if (transition != kNoTransition) {
        m_state = static_cast<State>(transition);
    }
}

void Lifecycle::Fail() noexcept
{
    m_state = State::Failed;
}

void Lifecycle::RestoreFromSnapshot() noexcept
{
    if (m_state == State::Failed) {
        m_state = State::Step;
    }
}

} // namespace cppfmu
