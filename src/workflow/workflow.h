//  Copyright (c) 2021-present, Tencent, Inc.  All rights reserved.
#pragma once

#include "step.h"
#include <stack>

class Workflow final : public Step {
 public:
  Step* get_root() { return root_.get(); }

  void Execute(Step* step, std::unique_ptr<MessageBase> input_message) {
    while (true) {
      step->entry_(std::move(input_message), {});

      for (auto& message : output_messages_) {
        auto cStep = child_steps_.at(message->step_id).get();

      }

      switch(type_) {
        case Step::Type::CPU:

          break;
        case Step::Type::IO:
          break;
      }

    }
  }

  void InternalExecute(Step* step, std::unique_ptr<MessageBase> input_message) {

 private:
  std::unique_ptr<Step> root_;
  std::stack<
};