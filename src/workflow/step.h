//  Copyright (c) 2021-present, Tencent, Inc.  All rights reserved.
#pragma once

#include <functional>
#include <list>
#include <unordered_map>
#include "message_base.h"


/**
 * @brief 
 * 
 */
class Step {
 public:
  enum class Status {
    NotStarted,
    Running,
    Completed,
    FailTransient,
    FailPermanent,
    Rollback,
  };

  enum class Type {
    CPU,
    IO,
  };

  using StepFunc = 
    std::function<void(std::unique_ptr<MessageBase>, std::list<std::unique_ptr<MessageBase>>)>;

  struct ChildStepLink {
    std::unique_ptr<MessageBase> message;
    std::unique_ptr<Step> step;
  };

  Step(const std::string& id, Step::Type type, StepFunc&& func, std::list<ChildStepLink>&& children = {}) :
    entry_(std::move(func)), type_(type) {
      for (auto& item : children) {
        output_messages_.emplace_back(std::move(item.message));
        item.step->parent_= this;
        child_steps_.emplace(item.message->step_id, std::move(item.step));
        cChildren_++;
      }
  }

  Step* GetChildStep(const std::string& id) { 
    // TODO 
    return nullptr;
  }

 private:
  /**
   * @brief A step can have multiple children
   * Each child link is identified by its corresponding input message id
   */
  std::unordered_map<std::string, std::unique_ptr<Step>> child_steps_;
  std::list<std::unique_ptr<MessageBase>> output_messages_;

  Step* parent_;
  
  // main execution entry point for this step
  // The function takes an input MessageBase and set N output messages.
  StepFunc entry_;

  // count of immdiate children.
  int cChildren_;

  // 
  int completedChildren;

  Step::Type type_;
};