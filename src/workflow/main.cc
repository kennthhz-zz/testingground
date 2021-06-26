#include "step.h"

void Func(std::unique_ptr<MessageBase>, std::list<std::unique_ptr<MessageBase>>){

}

int main() 
{
  auto s = std::make_unique<Step>(
          "id_1", 
          Step::Type::CPU,
          std::move(Func));

  /*  std::list<ChildStepLink> list = { 
      {.message = std::make_unique<Message>(), .step = std::make_unique<Step>("id_2_1", func, {})},
      {.message = std::make_unique<Message>(), .step = std::make_unique<Step>("id_2_2", func, {})}
    };

    auto step = std::make_unique<Step>(
      "id_0",
      std::move(Func),
      {
        .message = std::make_unique<MessageBase>(),
        .step = std::make_unique<Step>(
          "id_1", 
          std::move(Func), 
          {
            {
              .message = std::make_unique<MessageBase>(), 
              .step = std::make_unique<Step>("id_2_1", Func, {}),
            },
            {
              .message = std::make_unique<MessageBase>(), 
              .step = std::make_unique<Step>("id_2_2", Func, {}),
            }
          }
        ),
      }
    );*/
  return 1;
}