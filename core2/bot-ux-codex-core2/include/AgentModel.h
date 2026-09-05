#pragma once

#include <stdint.h>

// Fixed-size, platform-independent state model for the local Codex Micro demo.
// It deliberately has no Arduino or display dependency so its transitions can be
// tested on a host compiler.
class AgentModel {
public:
    static constexpr uint8_t kAgentCount = 6;

    enum class Status : uint8_t { Idle, Thinking, Running, Waiting, Done, Error };
    enum class Reasoning : uint8_t { Low, Medium, High };
    enum class Workflow : uint8_t { Review, Debug, Refactor, Next };

    struct Agent {
        Status status;
        char task[16];
        uint32_t deadlineMs;
        uint32_t runDurationMs;
        bool waitsForApproval;
    };

    void begin(uint32_t nowMs = 0);
    void update(uint32_t nowMs);

    const Agent& agent(uint8_t index) const { return _agents[index < kAgentCount ? index : 0]; }
    const Agent& selectedAgent() const { return _agents[_selected]; }
    uint8_t selected() const { return _selected; }
    void select(uint8_t index);

    Reasoning reasoning() const { return _reasoning; }
    void setReasoning(Reasoning value) { _reasoning = value; }
    void setSpeed(uint8_t value) { _speed = value > 2 ? 2 : value; }

    void startWorkflow(Workflow workflow, uint32_t nowMs);
    void startNew(uint32_t nowMs);
    void submitVoice(uint32_t nowMs);
    bool accept(uint32_t nowMs);
    bool reject();

    static const char* statusName(Status value);
    static const char* reasoningName(Reasoning value);
    static const char* workflowName(Workflow value);

private:
    void _start(uint8_t index, const char* task, bool waits, uint32_t nowMs);
    uint32_t _scaled(uint32_t ms) const;
    static void _copyTask(char* dst, const char* src);

    Agent _agents[kAgentCount]{};
    uint8_t _selected = 0;
    uint8_t _speed = 1;
    Reasoning _reasoning = Reasoning::Medium;
};
