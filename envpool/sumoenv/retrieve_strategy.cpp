#include "retrieve_strategy.h"

#include <string>

void RetrieveStrategyImp::Retrieve(
    int max_num_players, int state_dim, int& pre_queue_length,
    const std::vector<int>& agents_to_update,
    const std::unordered_map<std::string, std::vector<std::string>>&
        in_lanes_map,
    std::unordered_map<std::string, ContainerVariant>& context) {
  std::vector<int> trafficlight_queue_length(max_num_players);
  std::vector<int> lane_queue_length(max_num_players * state_dim);
  std::vector<int> lane_queue_length_diff(max_num_players * state_dim);
  std::vector<float> lane_length(max_num_players * state_dim);
  std::vector<float> lane_max_speed(max_num_players * state_dim);
  std::vector<int> left_time(max_num_players);
  std::vector<int> stage_index(max_num_players);

  size_t tl_idx = 0;
  for (std::string& tl_id : TrafficLight::getIDList()) {
    const auto& lanes = in_lanes_map[tl_id];

    size_t lane_idx = 0;
    int tl_queue_length = 0;
    for (const std::string& lane_id : lanes) {
      size_t cur_idx = tl_idx * state_dim + lane_idx;
      lane_length[cur_idx] = static_cast<float>(Lane::getLength(lane_id));
      lane_queue_length[cur_idx] = Lane::getLastStepHaltingNumber(lane_id);
      lane_max_speed[cur_idx] = static_cast<float>(Lane::getMaxSpeed(lane_id));

      tl_queue_length += Lane::getLastStepHaltingNumber(lane_id);

      ++lane_idx;
    }
    trafficlight_queue_length[tl_idx] = tl_queue_length;
    ++tl_idx;
  }


  std::vector<int>& agents_to_update = std::get<std::vector<int>>(context["agents_to_update"]);

  std::transform(
      lane_queue_length.begin(), lane_queue_length.end(), std::get<std::vector<int>>(context["lane_queue_length"]).begin(), lane_queue_length_diff.begin(),
      [](int cur_queue, int pre_queue) { return pre_queue - cur_queue; });
  
  context["lane_queue_length"] = std::move(lane_queue_length);
  std::transform(
      agents_to_update.begin(), agents_to_update.end(), lane_queue_length_diff.begin(),
      [](int update, int length) { return update == 1 ? length : 0; });



  std::transform(traffic_lights.begin(), traffic_lights.end(),
                 agents_to_update.begin(), left_time.begin(),
                 [](const TrafficLightImp& tl, int update) {
                   return update == 1 ? 0 : tl.RetrieveLeftTime();
                 });

  cur_queue_length = std::accumulate(trafficlight_queue_length.begin(),
                                     trafficlight_queue_length.end(), 0);

  context["trafficlight_queue_length"] = std::move(trafficlight_queue_length);
  context["lane_queue_length"] = std::move(lane_queue_length);
  context["lane_length"] = std::move(lane_length);
  context["lane_max_speed"] = std::move(lane_max_speed);
  context["left_time"] = std::move(left_time);
  context["total_queue_length"] = pre_queue_length - cur_queue_length;

  pre_queue_length = cur_queue_length;

  return;
}
