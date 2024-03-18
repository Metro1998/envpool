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

using ContainerVariant =
    std::variant<std::vector<int>, std::vector<float>, int>;

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
        "obs:stage_index"_.Bind(Spec<int>({-1})),
        "info:global_reward"_.Bind(Spec<float>({})),
        "info:individual_reward"_.Bind(Spec<float>({-1})),
        "info:agents_to_update"_.Bind(Spec<bool>({-1})),
        "info:left_time"_.Bind(Spec<int>({-1})),
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

  static const std::string kMaxDepartDelay;
  static const std::string kWaitingTimeMemory;
  static const std::string kTimeToTeleport;
  static const std::string kNoWarnings;

  std::vector<std::string> sumo_cmd_;
  std::unordered_map<std::string, std::vector<std::string>> in_lanes_map_;
  std::vector<std::unique_ptr<TrafficLightImp>> traffic_lights_;
  std::vector<std::unique_ptr<ObserverInterface>> observers_;
  std::unordered_map<std::string, ContainerVariant>
      context_;  // We use the context_ to store the variables that we're
                 // interested in, e.g. agent_to_update

  void InitContext() {
    // State: queue length and stage index
    context_["lane_queue_length"] =
        std::vector<int>(max_num_players_ * state_dim_, 0);
    context_["stage_index"] = std::vector(int)(max_num_players_, 0);
    // Global reward for CTDE farmework
    context_["cur_tot_queue_length"] = 0;
    context_["last_tot_queue_length"] = 0;
    context_["global_reward"] = 0;

    // Individual reward for decentralized actors
    context_["cur_tl_queue_length"] = std::vector<int>(max_num_players_, 0);
    context_["last_tl_queue_length"] = std::vector<int>(max_num_players_, 0);
    context_["individual_reward"] = std::vector<int>(max_num_players_, 0);

    // Info: agent_to_update, metric(queue_length, waitting_time), left time and
    // done
    context_["queue_length"] = 0;
    context_["waitting_time"] = 0;
    context_["left_time"] = std::vector<int>(max_num_players_, 0);
    context_["agents_to_update"] = std::vector<int>(max_num_players_, 1);
    context_["done"] = 0;
  }

  void WriteState() {
    State state = Allocate(max_num_players_);
    state["obs:lane_queue_length"_].Assign(context_["lane_queue_length"].data(),
                                           max_num_players_ * state_dim_);
    state["obs:stage_index"_].Assign(context_["stage_index"].data(),
                                      max_num_players_);
    
    state["info:global_reward"_] = static_cast<float>(context_["global_reward"]);
    state["info:individual_reward"_].Assign(
        context_["individual_reward"].data(), max_num_players_);
    state["info:agents_to_update"_] =
        static_cast<float>(context_["agent_to_update"]);
    state["info:left_time"_].Assign(context_["left_time"].data(),
                                    max_num_players_);
    state["info:done"_] = static_cast<float>(context_["done"]);

  }

  void Attach() {
    observers_.emplace_back(std::make_unique<StateObserver>());
    observers_.emplace_back(std::make_unique<RewardObserver>());
    observers_.emplace_back(std::make_unique<InfoObserver>());
  }

  void Detach() { observers_.clear(); }

  void Notify() {
    State state = Allocate(max_num_players_);
    for (auto& observer : observers_) {
      observer->Update(context_, traffic_lights_, max_num_players_, state_dim_);
    }
  }

  void SetTrafficLights() {
    vector<std::string> tls_ids = TrafficLight::getIDList();
    std::for_each(tls_ids.begin(), tls_ids.end(), [this](const string& id) {
      traffic_lights_.emplace_back(
          std::make_unique<TrafficLightImp>(id, yellow_time_));
    });
  }

  void RemoveElements(std::vector<std::string>& lanes) {
    std::vector<std::string> temp;

    for (size_t i = 0; i < lanes.size(); ++i) {
      if (i % 3 == 0) {
        temp.emplace_back(lanes[i]);
      }
    }
    lanes = std::move(temp);
    temp.clear();

    for (size_t i = 0; i < lanes.size(); ++i) {
      if (i % 3 != 0) {
        temp.emplace_back(lanes[i]);
      }
    }
    lanes = std::move(temp);

    return;
  }

  void ProcessLanes() {
    for (const auto& tl_id : TrafficLight::getIDList()) {
      in_lanes_map_[tl_id] = TrafficLight::getControlledLanes(tl_id);
      RemoveElements(in_lanes_map_[tl_id]);
    }
    return;
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
        state_dim_(spec.config["state_dim"_]),
        pre_queue_length_(0) {
    SetTrafficLights();
  }

  bool IsDone() override {
    return Simulation::getTime() >= Simulation::getEndTime();
  }
  void Reset() override {
    Simulation::close();
    auto res = Simulation::start(sumo_cmd_);
    context_["done"] = 0;
    Notify();
  }
  void Step(const Action& action) override{
    for (int i = 0; i < action.size(); ++i) {
      if (context_["agent_to_update"][i] == 1) {
        traffic_lights_[i]->SetStageDuration(action["action:stage"_][i],
                                             action["action:duration"_][i]);
      }
    }

    // Run the simulation until specific conditions are met
    while (true) {
      Simulation::step();
      for (int i = 0; i < max_num_players_; ++i) {
        context_["agent_to_update"][i] = -traffic_lights_[i]->Check();
        traffic_lights_[i]->Pop();
      }

      // Condition: at least one agent has finished its signal stage and ready
      // to update the signal stage or the simulation has reached the end time
      if (std::find(agents_to_update.begin(), agents_to_update.end(), 1) !=
              agents_to_update.end() ||
          Simulation::getTime() >= Simulation::getEndTime()) {
        break;
      }
    }

    // Inform the observers to update the state, reward and info through the context
    Notify();
    return;
  }
};

};  // namespace sumoenv

#endif  // SUMOCLIENT_H
