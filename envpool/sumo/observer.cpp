#include "observer.h"
#include "traffic_light.h"

#include <algorithm>

#include "libsumo/libsumo.h"

void StateObserver::Update(ContainerVariant& context, std::vector<std::unique_ptr<TrafficLightImp>>& traffic_lights, int max_num_players, int state_dim) {
  std::vector<int> lane_queue_length(max_num_players * state_dim);
  std::vector<int> stage_index(max_num_players);

  size_t tl_idx = 0;
  for (const std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    stage_index[tl_idx] = TrafficLight::getStageIndex(tl_id); #TODO

    size_t lane_idx = 0;
    for (const std::string& lane_id : lanes) {
      lane_queue_length[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepHaltingNumber(lane_id);

      ++lane_idx;
    }
    ++tl_idx;
  }

  context["lane_queue_length"] = std::move(lane_queue_length);
  context["stage_index"] = std::move(stage_index);
}

void RewardObserver::Update(ContainerVariant& context, std::vector<std::unique_ptr<TrafficLightImp>>& traffic_lights, int max_num_players, int state_dim) {
  std::vector<int> lane_queue_length(max_num_players * state_dim);

  size_t tl_idx = 0;
  for (const std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    size_t lane_idx = 0;
    for (const std::string& lane_id : lanes) {
      lane_queue_length[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepHaltingNumber(lane_id);

      ++lane_idx;
    }
    ++tl_idx;
  }

  // Global reward for CTDE farmework
  int cur_tot_queue_length = std::accumulate(lane_queue_length.begin(), lane_queue_length.end(), 0);
  context["last_tot_queue_length"] = context["cur_tot_queue_length"];
  context["cur_tot_queue_length"] = cur_tot_queue_length;
  context["global_reward"] = context["last_tot_queue_length"] - context["cur_tot_queue_length"];

  // Individual reward for decentralized actors
  std::vector<int> tl_queue_length;
  for (int i = 0; i < max_num_players; ++i){
    size_t start_idx = i * state_dim;
    size_t end_idx = (i + 1) * state_dim;

    int sum = std::accumulate(lane_queue_length.begin() + start_idx, lane_queue_length.end() + end_idx, 0);
    tl_queue_length.push_back(sum);
  }

  context["last_tl_queue_length"] =  context["cur_tl_queue_length"];
  context["cur_tl_queue_length"] = tl_queue_length;
  for (const auto update: context["agents_to_update"]){
    if (update) {
      context["individual_reward"] = context["last_tl_queue_length"] - context["cur_tl_queue_length"];
    } else {
      context["individual_reward"] = 0;
    }
  }

  return;
}


void InfoObserver::Update(ContainerVariant& context, std::vector<std::unique_ptr<TrafficLightImp>>& traffic_lights, int max_num_players, int state_dim) {
  std::vector<int> lane_queue_length(max_num_players * state_dim);
  std::vector<int> lane_waitting_time(max_num_players * state_dim);
  std::vector<int> left_time(max_num_players);

  size_t tl_idx = 0;
  for (const auto& tl : traffic_lights) {
    left_time[tl_idx] = tl->RetrieveLeftTime();
    tl_idx++;
  }

  size_t tl_idx = 0;
  for (const std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    size_t lane_idx = 0;
    for (const std::string& lane_id : lanes) {
      lane_queue_length[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepHaltingNumber(lane_id);
      lane_waitting_time[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepWaitingTime(lane_id);

      ++lane_idx;
    }
    ++tl_idx;
  }

  context["queue_length"] = std::accumulate(lane_queue_length.begin(), lane_queue_length.end(), 0);
  context["waitting_time"] = std::accumulate(lane_waitting_time.begin(), lane_waitting_time.end(), 0);
  context["left_time"] = std::move(left_time);
}