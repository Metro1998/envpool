#ifndef OBSERVER_H
#define OBSERVER_H

#include <vector>

#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"

class ObserverInterface {
 public:
  virtual ~ObserverInterface() = default;
  virtual void Update(State& context, std::vector<int>& agents_to_update,
                      int max_num_players, int state_dim) = 0;
};

class StateObserver : public ObserverInterface {
 public:
  ~ObserverImp() = default;
  void Update(State& context, std::vector<int>& agents_to_update,
              int max_num_players, int state_dim) override;
};

class RewardObserver : public ObserverInterface {
 public:
  ~ObserverImp() = default;
  void Update(State& context, std::vector<int>& agents_to_update,
              int max_num_players, int state_dim) override;
};

class InfoObserver : public ObserverInterface {
 public:
  ~ObserverImp() = default;
  void Update(State& context, std::vector<int>& agents_to_update,
              int max_num_players, int state_dim) override;
};

#endif  // OBSERVER_H