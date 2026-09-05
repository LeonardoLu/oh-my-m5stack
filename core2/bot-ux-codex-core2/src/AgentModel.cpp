#include "AgentModel.h"

namespace {
constexpr uint32_t kThinkMs[3] = {900, 1500, 2300};
constexpr uint32_t kRunMs[3] = {1800, 2700, 3900};
}

void AgentModel::_copyTask(char* dst, const char* src)
{
    uint8_t i = 0;
    while (src[i] && i < 15) { dst[i] = src[i]; ++i; }
    dst[i] = '\0';
}

void AgentModel::begin(uint32_t nowMs)
{
    (void)nowMs;
    static const Status states[kAgentCount] = {
        Status::Idle, Status::Thinking, Status::Running,
        Status::Waiting, Status::Done, Status::Error,
    };
    static const char* tasks[kAgentCount] = {
        "ready", "indexing", "tests", "approve", "docs", "build",
    };
    for (uint8_t i = 0; i < kAgentCount; ++i)
    {
        _agents[i].status = states[i];
        _copyTask(_agents[i].task, tasks[i]);
        _agents[i].deadlineMs = 0; // showcase states stay stable until an action
        _agents[i].runDurationMs = 0;
        _agents[i].waitsForApproval = states[i] == Status::Waiting;
    }
    _selected = 0;
    _reasoning = Reasoning::Medium;
}

void AgentModel::select(uint8_t index)
{
    if (index < kAgentCount) _selected = index;
}

uint32_t AgentModel::_scaled(uint32_t ms) const
{
    // Fast / normal / demo pace. Values are integer and deterministic.
    static const uint8_t pct[3] = {55, 100, 165};
    return (ms * pct[_speed]) / 100;
}

void AgentModel::_start(uint8_t index, const char* task, bool waits, uint32_t nowMs)
{
    select(index);
    Agent& a = _agents[_selected];
    a.status = Status::Thinking;
    _copyTask(a.task, task);
    a.waitsForApproval = waits;
    a.runDurationMs = _scaled(kRunMs[static_cast<uint8_t>(_reasoning)]);
    a.deadlineMs = nowMs + _scaled(kThinkMs[static_cast<uint8_t>(_reasoning)]);
}

void AgentModel::startWorkflow(Workflow workflow, uint32_t nowMs)
{
    if (workflow == Workflow::Next)
    {
        select((_selected + 1) % kAgentCount);
        return;
    }
    const bool waits = workflow == Workflow::Review || workflow == Workflow::Refactor;
    _start(_selected, workflowName(workflow), waits, nowMs);
}

void AgentModel::startNew(uint32_t nowMs)
{
    uint8_t slot = _selected;
    for (uint8_t i = 0; i < kAgentCount; ++i)
    {
        if (_agents[i].status == Status::Idle) { slot = i; break; }
    }
    _start(slot, "new task", false, nowMs);
}

void AgentModel::submitVoice(uint32_t nowMs)
{
    _start(_selected, "voice task", true, nowMs);
}

bool AgentModel::accept(uint32_t nowMs)
{
    Agent& a = _agents[_selected];
    if (a.status != Status::Waiting) return false;
    a.status = Status::Running;
    a.waitsForApproval = false;
    a.runDurationMs = _scaled(kRunMs[static_cast<uint8_t>(_reasoning)] / 2);
    a.deadlineMs = nowMs + a.runDurationMs;
    return true;
}

bool AgentModel::reject()
{
    Agent& a = _agents[_selected];
    if (a.status != Status::Waiting) return false;
    a.status = Status::Idle;
    _copyTask(a.task, "cancelled");
    a.waitsForApproval = false;
    a.deadlineMs = 0;
    a.runDurationMs = 0;
    return true;
}

void AgentModel::update(uint32_t nowMs)
{
    for (uint8_t i = 0; i < kAgentCount; ++i)
    {
        Agent& a = _agents[i];
        if (!a.deadlineMs || static_cast<int32_t>(nowMs - a.deadlineMs) < 0) continue;
        if (a.status == Status::Thinking)
        {
            a.status = Status::Running;
            a.deadlineMs = nowMs + a.runDurationMs;
        }
        else if (a.status == Status::Running)
        {
            a.status = a.waitsForApproval ? Status::Waiting : Status::Done;
            a.deadlineMs = 0;
            a.runDurationMs = 0;
        }
        else
        {
            a.deadlineMs = 0;
        }
    }
}

const char* AgentModel::statusName(Status value)
{
    static const char* names[] = {"IDLE", "THINKING", "RUNNING", "WAITING", "DONE", "ERROR"};
    return names[static_cast<uint8_t>(value)];
}

const char* AgentModel::reasoningName(Reasoning value)
{
    static const char* names[] = {"LOW", "MEDIUM", "HIGH"};
    return names[static_cast<uint8_t>(value)];
}

const char* AgentModel::workflowName(Workflow value)
{
    static const char* names[] = {"review", "debug", "refactor", "next"};
    return names[static_cast<uint8_t>(value)];
}
