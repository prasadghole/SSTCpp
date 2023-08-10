//============================================================================
// Super-Simple Tasker (SST/C++)
//
// Copyright (C) 2006-2023 Quantum Leaps, <state-machine.com>.
//
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//============================================================================
#include "sst.hpp"      // Super-Simple Tasker (SST) in C++
#include "dbc_assert.h" // Design By Contract (DBC) assertions

//............................................................................
namespace { // unnamed namespace

DBC_MODULE_NAME("sst")  // for DBC assertions in this module

} // unnamed namespace

namespace SST {

// SST kernel facilities -----------------------------------------------------
int Task::run(void) {
    SST::start(); // port-specific start of multitasking
    onStart(); // configure and start the interrupts

    for (;;) { // idle loop of the SST kernel
        onIdle();
    }
}

// SST Task facilities -------------------------------------------------------
void Task::start(
    TaskPrio prio,
    Evt const **qBuf, QCtr qLen,
    Evt const * const ie)
{
    //! @pre
    //! - the priority must be greater than zero
    //! - the queue storage and length must be provided
    DBC_REQUIRE(200,
        (0U < prio)
        && (qBuf != nullptr) && (qLen > 0U));

    m_qBuf  = qBuf;
    m_end   = qLen - 1U;
    m_head  = 0U;
    m_tail  = 0U;
    m_nUsed = 0U;

    setPrio(prio);

    // initialize this task with the initialization event
    init(ie); // virtual call
    // TBD: implement event recycling
}
//............................................................................
void Task::post(Evt const * const e) noexcept {
    //! @pre the queue must be sized adequately and cannot overflow
    DBC_REQUIRE(300, m_nUsed <= m_end);

    SST_PORT_CRIT_STAT
    SST_PORT_CRIT_ENTRY();
    m_qBuf[m_head] = e; // insert event into the queue
    // need to wrap the head?
    if (m_head == 0U) {
        m_head = m_end; // wrap around
    }
    else {
        --m_head;
    }
    ++m_nUsed;
    SST_PORT_TASK_PEND();
    SST_PORT_CRIT_EXIT();
}

//----------------------------------------------------------------------------
static TimeEvt *timeEvt_head = nullptr;

//............................................................................
TimeEvt::TimeEvt(Signal sig, Task *task) {
    this->sig  = sig;
    m_task     = task;
    m_ctr      = 0U;
    m_interval = 0U;

    // insert this time event into the linked-list
    m_next = timeEvt_head;
    timeEvt_head = this;
}
//............................................................................
void TimeEvt::arm(TCtr ctr, TCtr interval) {
    SST_PORT_CRIT_STAT
    SST_PORT_CRIT_ENTRY();
    m_ctr      = ctr;
    m_interval = interval;
    SST_PORT_CRIT_EXIT();
}
//............................................................................
bool TimeEvt::disarm(void) {
    SST_PORT_CRIT_STAT
    SST_PORT_CRIT_ENTRY();
    bool status = (m_ctr != 0U);
    m_ctr       = 0U;
    m_interval  = 0U;
    SST_PORT_CRIT_EXIT();
    return status;
}
//............................................................................
void TimeEvt::tick(void) {
    for (TimeEvt *t = timeEvt_head; t != nullptr; t = t->m_next) {
        SST_PORT_CRIT_STAT
        SST_PORT_CRIT_ENTRY();
        if (t->m_ctr == 0U) { // disarmed? (most frequent case)
            SST_PORT_CRIT_EXIT();
        }
        else if (t->m_ctr == 1U) { // expiring?
            t->m_ctr = t->m_interval;
            SST_PORT_CRIT_EXIT();

            t->m_task->post(t);
        }
        else { // timing out
            --t->m_ctr;
            SST_PORT_CRIT_EXIT();
        }
    }
}

} // namespace SST
