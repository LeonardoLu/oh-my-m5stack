#include "AgentModel.h"

#include <assert.h>
#include <string.h>

int main()
{
    AgentModel model;
    model.begin(100);
    assert(model.agent(0).status == AgentModel::Status::Idle);
    assert(model.agent(3).status == AgentModel::Status::Waiting);
    for (uint8_t i = 0; i < AgentModel::kAgentCount; ++i)
        assert(model.agent(i).deadlineMs == 0); // showcase states do not advance by themselves

    model.select(4);
    assert(model.selected() == 4);
    assert(model.agent(4).status == AgentModel::Status::Done); // selection is inert

    model.setReasoning(AgentModel::Reasoning::Low);
    model.setSpeed(0);
    model.startWorkflow(AgentModel::Workflow::Review, 1000);
    assert(model.agent(4).status == AgentModel::Status::Thinking);
    model.update(model.agent(4).deadlineMs - 1);
    assert(model.agent(4).status == AgentModel::Status::Thinking);
    model.update(2000);
    assert(model.agent(4).status == AgentModel::Status::Running);
    model.update(4000);
    assert(model.agent(4).status == AgentModel::Status::Waiting);
    assert(model.accept(4000));
    model.update(5000);
    assert(model.agent(4).status == AgentModel::Status::Done);

    model.select(3);
    assert(model.reject());
    assert(model.agent(3).status == AgentModel::Status::Idle);
    assert(strcmp(model.agent(3).task, "cancelled") == 0);

    model.startNew(6000);
    assert(model.selected() == 0); // first idle slot
    assert(model.agent(0).status == AgentModel::Status::Thinking);
    model.select(5);
    model.submitVoice(7000);
    assert(strcmp(model.agent(5).task, "voice task") == 0);

    AgentModel low;
    AgentModel high;
    low.begin();
    high.begin();
    low.setReasoning(AgentModel::Reasoning::Low);
    high.setReasoning(AgentModel::Reasoning::High);
    low.startWorkflow(AgentModel::Workflow::Debug, 100);
    high.startWorkflow(AgentModel::Workflow::Debug, 100);
    assert(low.selectedAgent().deadlineMs < high.selectedAgent().deadlineMs);
    assert(!low.reject());

    AgentModel snapshotted;
    snapshotted.begin();
    snapshotted.setReasoning(AgentModel::Reasoning::Low);
    snapshotted.setSpeed(0);
    snapshotted.startWorkflow(AgentModel::Workflow::Debug, 1000);
    const uint32_t thinkingDeadline = snapshotted.selectedAgent().deadlineMs;
    const uint32_t expectedRunDuration = snapshotted.selectedAgent().runDurationMs;
    snapshotted.setReasoning(AgentModel::Reasoning::High);
    snapshotted.setSpeed(2);
    snapshotted.update(thinkingDeadline);
    assert(snapshotted.selectedAgent().status == AgentModel::Status::Running);
    assert(snapshotted.selectedAgent().deadlineMs == thinkingDeadline + expectedRunDuration);
    snapshotted.update(snapshotted.selectedAgent().deadlineMs);
    assert(snapshotted.selectedAgent().status == AgentModel::Status::Done);

    AgentModel approval;
    approval.begin();
    approval.select(1);
    approval.startWorkflow(AgentModel::Workflow::Refactor, 10);
    approval.update(approval.selectedAgent().deadlineMs);
    approval.update(approval.selectedAgent().deadlineMs);
    assert(approval.selectedAgent().status == AgentModel::Status::Waiting);
    assert(approval.reject());
    assert(approval.selectedAgent().status == AgentModel::Status::Idle);
    assert(!approval.accept(100));

    const uint8_t selected = low.selected();
    low.startWorkflow(AgentModel::Workflow::Next, 200);
    assert(low.selected() == (selected + 1) % AgentModel::kAgentCount);
    assert(low.selectedAgent().deadlineMs == 0); // Next only navigates.
    return 0;
}
