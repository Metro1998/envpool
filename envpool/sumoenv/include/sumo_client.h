#ifndef SUMOCLIENT_H
#define SUMOCLIENT_H

#include <libsumo/libsumo.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"
// #include "retrieve_strategy.h"
#include "observer.h"
#include "traffic_light.h"

namespace sumoenv {

class SumoEnvFns {
 public:
  static decltype(auto) DefaultConfig() {
    return MakeDict(
        "duration_threshold"_.Bind(45),
        "path_to_sumo"_.Bind(std::string("/usr/bin/sumo")),
        "net_file"_.Bind(std::string("nets/grid.net.xml")),
        "route_file"_.Bind(std::string("nets/grid.rou.xml")),
        "addition_file"_.Bind(std::string("nets/grid.add.xml")),
        "yellow_time"_.Bind(3), "seed"_.Bind(1),
        "end_time"_.Bind(3600.0),  // How long does a simulation last.
    );
  }

  template <typename Config>
  static decltype(auto) StateSpec(const Config& conf) {
    return MakeDict(
        "obs:lane_queue_length"_.Bind(Spec<int>({-1, 8})),
        "obs:lane_length"_.Bind(Spec<float>({-1, 8})),
        "obs:vehicle_speed"_.Bind(Spec<Container<float>>({-1, 8})),
        "obs:vehicle_position"_.Bind(Spec<Container<float>>({-1, 8})),
        "obs:vehicle_acceleration"_.Bind(Spec<Container<float>>({-1, 8})),
        "info:agent_to_update"_.Bind(Spec<bool>({-1})),
        "info:done"_.Bind(Spec<bool>({})));
  }

  template <typename Config>
  static decltype(auto) ActionSpec(const Config& conf) {
    return MakeDict("action:stage"_.Bind(Spec<int>({-1}, {0, 7})),
                    "action:duration"_.Bind(
                        Spec<int>({-1}, {0, conf["duration_threshold"_] - 1})));
  }
};

using SumoEnvSpec = EnvSpec<SumoEnvFns>;
using Simulation = libsumo::Simulation;

class RetrieveStrategy;

class SumoClient : public Env<SumoEnvSpec> {
 private:
  const std::string path_to_sumo_;
  const std::string net_file_;
  const std::string route_file_;
  const std::string additional_file_;
  const int max_num_players_;
  const int yellow_time_;
  const int seed_;
  const int state_dim_;
  const double end_time_;

  int pre_queue_length_;

  static const std::string kMaxDepartDelay;
  static const std::string kWaitingTimeMemory;
  static const std::string kTimeToTeleport;
  static const std::string kNoWarnings;

  std::vector<std::string> sumo_cmd_;
  std::unordered_map<std::string, std::vector<std::string>> in_lanes_map_;
  void ProcessLanes();
  void RemoveElements(std::vector<std::string>& lanes);
  std::vector<std::unique_ptr<TrafficLightImp>> traffic_lights_;
  std::vector<std::unique_ptr<ObserverInterface>> observers_;
  // std::unordered_map<std::string, ContainerVariant> context_;
  // std::unique_ptr<RetrieveStrategy> retrieve_strategy_imp_;
  // friend class RetrieveStrategy;

  void WriteState() {
    // todo
    State state = Allocate(max_num_players_);
    state["obs:lane_queue_length"_].Assign(context_["lane_queue_length"].data(),
                                           max_num_players_ * state_dim_);
    state["obs:lane_length"_].Assign(context_["lane_length"].data(),
                                     max_num_players_ * state_dim_);
    state["reward"_].Assign(context_["trafficlight_lane_length"].data(),
                            max_num_players_);
    state["info:agent_to_update"_] =
        static_cast<float>(context_["agent_to_update"]);
    state["info:done"_] = static_cast<float>(context_["done"]);
  }

  void Attach() {
    observers_.emplace_back(std::make_unique<StateObserver>());
    observers_.emplace_back(std::make_unique<RewardObserver>());
    observers_.emplace_back(std::make_unique<InfoObserver>());
  }

  void Detach() { observers_.clear(); }

  void Notify(std::vector<int>& agents_to_update) {
    State state = Allocate(max_num_players_);
    for (auto& observer : observers_) {
      observer->Update(state, agents_to_update, max_num_players_, state_dim_);
    }
  }
  void Retrieve();  // 这里会有好几层引用传递的问题
  void SetTrafficLights();
  void SetStrategies();
  void ProcessLanes();
  void RemoveElements(std::vector<std::string>& lanes);

 public:
  SumoClient(const Spec& spec, int env_id)
      : Env<SumoEnvSpec>(spec, env_id),
        path_to_sumo_(spec.config["path_to_sumo"_]),
        net_file_(spec.config["net_file"_]),
        route_file_(spec.config["route_file"_]),
        additional_file_(spec.config["addition_file"_]),
        max_num_players_(spec.config["max_num_players"_]),
        yellow_time_(spec.config["yellow_time"_]),
        seed_(spec.config["seed"_]),
        end_time_(spec.config["end_time"_]),
        state_dim_(spec.config["state_dim"_]),
        pre_queue_length_(0) {
    SetTrafficLights();
    SetStrategies();
  }

  bool IsDone();  // todo
  void Reset();   // todo
  void Step(const Action& action) {
    for (int i = 0; i < action.size(); ++i) {
      traffic_lights_[i]->SetStageDuration(action["action:stage"_][i],
                                           action["action:duration"_][i]);
    }
    vector<int> agents_to_update(max_num_players_);

    // Run the simulation until specific conditions are met
    while (true) {
      Simulation::step();
      for (int i = 0; i < max_num_players_; ++i) {
        agents_to_update[i] = -traffic_lights_[i]->Check();
        traffic_lights_[i]->Pop();
      }

      // Condition: at least one agent has finished its [last] stage and ready
      // to update or the simulation has reached the end time
      if (std::find(agents_to_update.begin(), agents_to_update.end(), 1) !=
              agents_to_update.end() ||
          Simulation::getTime() >= Simulation::getEndTime()) {
        break;
      }
    }

    Notify(agents_to_update); //关注一下变量的时效
  }

  // 当这些最基础的东西成熟之后，至少现在有点零乱
  // sumoenv？
};

};  // namespace sumoenv

#endif  // SUMOCLIENT_H
