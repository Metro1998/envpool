// Copyright 2021 Garena Online Private Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SUMO_ENVPOOL_H_
#define SUMO_ENVPOOL_H_

#include <memory>
#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"
#include "sumo_client.h"

using string = std::string;

namespace sumo {

class SumoEnvFns {
 public:

  static decltype(auto) DefaultConfig() {
    return MakeDict("path_to_sumo"_.Bind(string("/usr/local/bin")),
                    "net_file"_.Bind(string("data/sumo/cross.net.xml")),
                    "route_file"_.Bind(string("data/sumo/cross.rou.xml")),
                    "add_file"_.Bind(string("data/sumo/cross.add.xml")),
                    "yellow_time"_.Bind(3)
                   );
  }

  // max_num_players & seed are defined in common config.


  template <typename Config>
  static decltype(auto) StateSpec(const Config& conf) {
    return MakeDict("obs"_.Bind(Spec<int>({-1, 8}, {0, 200})),
                    "reward"_.Bind(Spec<float>({-1}, {-100, 100})),
                    "info:global_reward"_.Bind(Spec<int>({})),
                    "info:terminated"_.Bind(Spec<bool>({})),
                    "info:agent_to_update"_.Bind(Spec<bool>({-1})),
                    "info:left_time"_.Bind(Spec<int>({-1}))
                    );
  }

 
  template <typename Config>
  static decltype(auto) ActionSpec(const Config& conf) {
    return MakeDict("action:discrete_action"_.Bind(Spec<int>({-1}, {0, 7})),
                    "action:continuous_action"_.Bind(Spec<float>({-1}, {-1, 1}))
                    );
  }
};

/**
 * Create an DummyEnvSpec by passing the above functions to EnvSpec.
 */
using SumoEnvSpec = EnvSpec<SumoEnvFns>;  // Here you will get the common config

/**
 * The main part of the single env.
 * It inherits and implements the interfaces defined in Env specialized by the
 * DummyEnvSpec we defined above.
 */
class SumoEnv : public Env<SumoEnvSpec> {
 protected:
  string path_to_sumo_;
  string net_file_;
  string route_file_;
  string add_file_;
  int yellow_time_;
  int max_num_players_;

  std::unique_ptr<SumoClient> sumo_client_;
  std::uniform_int_distribution<int> dist_;
  

 public:
  /**
   * Initilize the env, in this function we perform tasks like loading the game
   * rom etc.
   */
    SumoEnv(const Spec& spec, int env_id) 
        : Env<SumoEnvSpec>(spec, env_id),
          path_to_sumo_(spec.config["path_to_sumo"_]),
          net_file_(spec.config["net_file"_]),
          route_file_(spec.config["route_file"_]),
          add_file_(spec.config["add_file"_]),
          yellow_time_(spec.config["yellow_time"_]),
          max_num_players_(spec.config["max_num_players"_]),
          sumo_client_(new SumoClient(
            path_to_sumo_,
            net_file_,
            route_file_,
            add_file_,
            yellow_time_,
            dist_(gen_))
          ) {}

      


  /**
   * Reset this single env, this has the same meaning as the openai gym's reset
   * The reset function usually returns the state after reset, here, we first
   * call `Allocate` to create the state (which is managed by envpool), and
   * populate it with the returning state.
   */
  void Reset() override {
    sumo_client_->SetSimulation(dist_(gen_));
    auto observation = sumo_client_->RetrieveObservation();
    auto reward = sumo_client_->RetrieveReward();

    auto state = Allocate(max_num_players_);

    for (int i = 0; i < max_num_players_; ++i) {
      state["obs"_][i] = observation[i];
      state["reward"_][i] = reward[i];
      state["info:agent_to_update"_][i] = true;
      state["info:left_time"_][i] = 0;   // 有什么用？
    }
    state["info:global_reward"_] = 0; 
    state["info:terminated"_] = false; 

    // global_reward 是为mappo服务的， reward则是每智能体在根新之后获得的reward

    // todo: reward 应该装在里面吗

    /* 现在的思路就是一些金昌要作获取操作可以在sumoclient中添加 其他的在这个类里面完成
    */
  }

  /**
   * Step is the central function of a single env.
   * It takes an action, executes the env, and returns the next state.
   *
   * Similar to Reset, Step also return the state through `Allocate` function.
   *
   */
  void Step(const Action& action) override {
    ++state_;
    int num_players =
        max_num_players_ <= 1 ? 1 : state_ % (max_num_players_ - 1) + 1;

    // Parse the action, and execute the env (dummy env has nothing to do)
    int action_num = action["players.env_id"_].Shape(0);
    for (int i = 0; i < action_num; ++i) {
      if (static_cast<int>(action["players.env_id"_][i]) != env_id_) {
        action_num = 0;
      }
    }

    // Check if actions can successfully pass into envpool
    double x = action["list_action"_][0];

    for (int i = 0; i < 6; ++i) {
      double y = action["list_action"_][i];
      CHECK_EQ(x, y);
    }

    // Ask envpool to allocate a piece of memory where we can write the state
    // after reset.
    auto state = Allocate(num_players);

    // write the information of the next state into the state.
    for (int i = 0; i < num_players; ++i) {
      state["info:players.id"_][i] = i;
      state["info:players.done"_][i] = IsDone();
      state["obs:raw"_](i, 0) = state_;
      state["obs:raw"_](i, 1) = action_num;
      state["reward"_][i] = -i;
      Container<int>& dyn = state["obs:dyn"_][i];
      auto dyn_spec = ::Spec<int>({env_id_ + 1, spec_.config["state_num"_]});
      dyn = std::make_unique<TArray<int>>(dyn_spec);
      dyn->Fill(env_id_);
    }
  }

  /**
   * Whether the single env has ended the current episode.
   */
  bool IsDone() override { return state_ >= seed
/**
 * Pass the DummyEnv we defined above as an template parameter to the
 * AsyncEnvPool template, it gives us a parallelized version of the single env.
 */
using DummyEnvPool = AsyncEnvPool<DummyEnv>;

}  // namespace dummy

#endif  // SUMO_ENVPOOL_H_



