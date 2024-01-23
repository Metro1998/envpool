#ifndef SUMOCLIENT_H
#define SUMOCLIENT_H

#include <libsumo/libsumo.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <numeric>
#include <variant>
#include <vector>
#include <stdexcept>

#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"
#include "retrieve_strategy.h"
#include "traffic_light.h"

namespace sumoenv {

class SumoEnvFns {
 public:
  static decltype(auto) DefaultConfig() {
    return MakeDict("duration_threshold"_.Bind(45),
                    "path_to_sumo"_.Bind(std::string("/usr/bin/sumo")),
                    "net_file"_.Bind(std::string("nets/grid.net.xml")),
                    "route_file"_.Bind(std::string("nets/grid.rou.xml")),
                    "addition_file"_.Bind(std::string("nets/grid.add.xml")),
                    "yellow_time"_.Bind(3),
                    "seed"_.Bind(1),
                    "end_time"_.Bind(3600.0), // How long does a simulation last.
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
        "info:done"_.Bind(Spec<bool>({}))
    );
  }

  template <typename Config>
  static decltype(auto) ActionSpec(const Config& conf) {
    return MakeDict("action:stage"_.Bind(Spec<int>({-1}, {0, 7})),
                    "action:duration"_.Bind(Spec<int>({-1}, {0, conf["duration_threshold"_] - 1}))
    );
  }
};

using SumoEnvSpec = EnvSpec<SumoEnvFns>;
using Simulation = libsumo::Simulation;

class SumoClient : public Env<SumoEnvSpec> {
 private:
  const string path_to_sumo_;
  const string net_file_;
  const string route_file_;
  const string additional_file_;
  const int max_num_players_;
  const int yellow_time_;
  const int seed_;
  const int state_dim_;
  const double end_time_;

  static const string kMaxDepartDelay;
  static const string kWaitingTimeMemory;
  static const string kTimeToTeleport;
  static const string kNoWarnings;

  vector<string> sumo_cmd_;
  vector<std::unique_ptr<TrafficLightImp>> traffic_lights_;
  std::unique_ptr<RetrieveStrategy> retrieve_strategy_imp_;

  std::unordered_map<string, ContainerVariant> context_;

  void WriteState() {
    // todo
    State state = Allocate(max_num_players_);
        state["obs:lane_queue_length"_].Assign(context_["lane_queue_length"].data(), max_num_players_ * state_dim_);
        state["obs:lane_length"_].Assign(context_["lane_length"].data(), max_num_players_ * state_dim_);
        state["reward"_].Assign(context_["trafficlight_lane_length"].data(), max_num_players_);
        // state["obs:vehicle_speed"_] =
        //     static_cast<float>(context_["vehicle_speed"]);
        // state["obs:vehicle_position"_] =
        //     static_cast<float>(context_["vehicle_position"]);
        // state["obs:vehicle_acceleration"_] =
        //     static_cast<float>(context_["vehicle_acceleration"]);
        state["info:agent_to_update"_] =
            static_cast<float>(context_["agent_to_update"]);
        state["info:done"_] = static_cast<float>(context_["done"]);
  }

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
        state_dim_(spec.config["state_dim"_]) {
    SetTrafficLights();
    SetStrategies();
  }

  void Reset();  // todo
  void SetTrafficLights();
  void SetStrategies();
  const std::unordered_map<string, ContainerVariant>&
  Retrieve();  // 这里会有好几层引用传递的问题

  void Step(const Action& action);  // todo
  void close();
  void TempTest();

  bool IsDone();  // todo

  // 当这些最基础的东西成熟之后，至少现在有点零乱
  // sumoenv？
};

};  // namespace sumoenv

#endif  // SUMOCLIENT_H
