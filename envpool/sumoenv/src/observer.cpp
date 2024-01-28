#include "observer.h"

#include <algorithm>

#include "libsumo/libsumo.h"

void StateObserver::Update(State& context, std::vector<int>& agents_to_update,
                           int max_num_players, int state_dim) {
  std::vector<int> lane_queue_length(max_num_players * state_dim);
  std::vector<int> lane_queue_length_diff(max_num_players * state_dim);
  std::vector<int> stage_index(max_num_players);

  size_t tl_idx = 0;
  for (std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    stage_index[tl_idx] = TrafficLight::getStageIndex(tl_id);

    size_t lane_idx = 0;
    for (const std::string& lane_id : lanes) {
      lane_queue_length[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepHaltingNumber(lane_id);

      ++lane_idx;
    }
    ++tl_idx;
  }

  if (context.find("lane_queue_length") == context.end()) {
    std::transform(lane_queue_length.begin(), lane_queue_length.end(),
                   lane_queue_length_diff.begin(),
                   [](int length) { return ~length; });
  } else {
    std::transform(
        lane_queue_length.begin(), lane_queue_length.end(),
        std::get<std::vector<int>>(context["lane_queue_length"]).begin(),
        lane_queue_length_diff.begin(),
        [](int cur_queue, int pre_queue) { return pre_queue - cur_queue; });
  }

  context["lane_queue_length"] = std::move(lane_queue_length);
  context["lane_queue_length_diff"] = std::move(lane_queue_length_diff);
  context["stage_index"] = std::move(stage_index);
}

void RewardObserver::Update(State& context, std::vector<int>& agents_to_update,
                            int max_num_players, int state_dim) {
  std::vector<int> lane_queue_length_diff(max_num_players * state_dim);

  size_t tl_idx = 0;
  for (std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    stage_index[tl_idx] = TrafficLight::getStageIndex(tl_id);

    size_t lane_idx = 0;
    int tl_queue_length = 0;
    for (const std::string& lane_id : lanes) {
      lane_queue_length[tl_idx * state_dim + lane_idx;] =
          Lane::getLastStepHaltingNumber(lane_id);
      tl_queue_length += Lane::getLastStepHaltingNumber(lane_id);

      ++lane_idx;
    }
    ++tl_idx;
  }

  std::transform(
      lane_queue_length.begin(), lane_queue_length.end(),
      std::get<std::vector<int>>(context["lane_queue_length"]).begin(),
      lane_queue_length_diff.begin(),
      [](int cur_queue, int pre_queue) { return pre_queue - cur_queue; });

  context["lane_queue_length"] = std::move(lane_queue_length);
  std::transform(agents_to_update.begin(), agents_to_update.end(),
                 lane_queue_length_diff.begin(), [](int update, int length) {
                   return update == 1 ? length : 0;
                 });

  int cur_queue_length = std::accumulate(trafficlight_queue_length.begin(),
                                         trafficlight_queue_length.end(), 0);
  context["global_reward"] = last_queue_length - cur_queue_length;
}