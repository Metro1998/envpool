#include "sumo_client.h"

#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"

const string SumoClient::kMaxDepartDelay = "-1";
const string SumoClient::kWaitingTimeMemory = "1000";
const string SumoClient::kTimeToTeleport = "-1";
const string SumoClient::kNoWarnings = "true";

SumoClient::SumoClient(const string& path_to_sumo, const string& net,
                       const string& route, const string& addition,
                       const int yellow_time, const int random_seed,
                       const double end_time)
    : path_to_sumo_(path_to_sumo),
      net_file_(net),
      route_file_(route),
      additional_file_(addition),
      yellow_time_(yellow_time),
      seed_(random_seed),
      end_time_(end_time),
      sumo_cmd_({path_to_sumo, "--net-file", net, "--route-files", route,
                 "--additional-files", addition, "--max-depart-delay",
                 kMaxDepartDelay, "--waiting-time-memory", kWaitingTimeMemory,
                 "--time-to-teleport", kTimeToTeleport, "--no-warnings",
                 kNoWarnings, "--seed", std::to_string(random_seed), "--end",
                 std::to_string(end_time)}) {
  auto res = Simulation::start(sumo_cmd_);
  SetTrafficLights();
}

void SumoClient::SetTrafficLights() {
  vector<std::string> tls_ids = TrafficLight::getIDList();
  std::for_each(tls_ids.begin(), tls_ids.end(), [this](const string& id) {
    traffic_lights_.emplace_back(
        std::make_unique<TrafficLightImp>(id, yellow_time_));
  });
}


// const std::unordered_map<string, ContainerVariant>& SumoClient::Retrieve() {
//   retrieve_strategy_imp_->Retrieve(this->context_,
//                                    static_cast<size_t>(max_num_players_),
//                                    static_cast<size_t>(state_dim_));
//   return context_;
// }

void SumoClient::Reset() {
  Simulation::close();
  auto res = Simulation::start(sumo_cmd_);
}

void SumoClient::Step(const Action& action) {
  // Set the duration of each traffic light stage according to the action
  for (int i = 0; i < action.size(); ++i) {
    traffic_lights_[i]->SetStageDuration(action["action:stage"_][i],
                                         action["action:duration"_][i]);
  }

  // Calculate and accumulate the current queue length as a reward
  // TODO
  int last_queue_length =
      std::accumulate(traffic_lights_.begin(), traffic_lights_.end(), 0,
                      [](int sum, const TrafficLightImp& tl) {
                        return sum + tl->RetrieveReward();
                      });

  vector<int> checks(max_num_players_);

  // Run the simulation until specific conditions are met
  while (true) {
    Simulation::step();
    // Check each traffic light and update the checks vector
    for (int i = 0; i < max_num_players_; ++i) {
      checks[i] = -traffic_lights_[i]->Check();
      traffic_lights_[i]->Pop();
    }

    // Exit the loop if the end condition is met
    if (std::find(checks.begin(), checks.end(), 1) != checks.end() ||
        Simulation::getTime() >= Simulation::getEndTime()) {
      this->context_["agents_to_update"] = std::move(checks);
      break;
    }
  }
  // // Retrieve information from the traffic lights to update context_
  // retrieve_strategy_imp_->Retrieve(this->context_, max_num_players_,
  //                                  state_dim_);

  // // Use context_["agents_to_update"] to index context_["queue_length"],
  // i.e.,
  // // reward
  // std::vector<int>& agents_to_update = context_["agents_to_update"];
  // std::vector<int>& queue_length = context_["queue_length"];
  // std::vector<int> left_time;
  // left_time.reserve(max_num_players);

  // // Ensure that the lengths of agents_to_update and queue_length are the
  // same if (agents_to_update.size() != queue_length.size()) {
  //   throw std::runtime_error("Vector lengths do not match");
  // }

  // // Use std::transform to update queue_length
  // std::transform(
  //     agents_to_update.begin(), agents_to_update.end(), queue_length.begin(),
  //     [](int update, int length) { return update == 1 ? length : 0; });

  // // Calculate left_time based on the values in agents_to_update
  // std::transform(traffic_lights_.begin(), traffic_lights_.end(),
  //                agents_to_update.begin(), std::back_inserter(left_time),
  //                [](const TrafficLightImp& tl, int update) {
  //                  return update == 1 ? 0 : tl.RetrieveLeftTime();
  //                });
  // context_["left_time"] = std::move(left_time);

  // // Calculate the current queue length and update global_reward
  // int cur_queue_length =
  //     std::accumulate(traffic_lights_.begin(), traffic_lights_.end(), 0,
  //                     [](int sum, const TrafficLightImp& tl) {
  //                       return sum + tl->RetrieveReward();
  //                     });
  // context_["global_reward"] = last_queue_length - cur_queue_length;
  retrieve_strategy_imp_->Retrieve(max_num_players_, state_dim_,
                                   pre_queue_length,
                                   context["agents_to_update"], in_lanes_map_);

  return;
}

bool SumoClient::IsDone() {
  return Simulation::getTime() >= Simulation::getEndTime();
}

void SumoClient::WriteState() { State state = Allocate(); }

void SumoClient::ProcessLanes() {
  for (const auto& tl_id : TrafficLight::getIDList()) {
    in_lanes_map_[tl_id] = TrafficLight::getControlledLanes(tl_id);
    RemoveElements(in_lanes_map_[tl_id]);
  }
}

void SumoClient::RemoveElements(std::vector<std::string>& lanes) {
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