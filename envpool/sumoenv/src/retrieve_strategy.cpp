#include <string>

#include "retrieve_strategy.h"

RetrieveStrategy::RetrieveStrategy() {
    ProcessTlsId();
    ProcessLanes();
};
RetrieveStrategy::~RetrieveStrategy() = default;

void RetrieveStrategy::ProcessTlsId() {
    tl_ids_ = TrafficLight::getIDList();
}

void RetrieveStrategy::ProcessLanes() {
    for (const auto& tl_id : tl_ids_) {
        in_lanes_map_[tl_id] = TrafficLight::getControlledLanes(tl_id);
        RemoveElements(in_lanes_map_[tl_id]);
    }
}

void RetrieveStrategy::RemoveElements(vector<string>& lanes) {
    vector<string> temp;

    // std::cout << "before remove: " << std::endl;
    // for (const auto& lane : lanes) {
    //     std::cout << lane << std::endl;
    // }

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

    // std::cout << "after remove: " << std::endl;
    // for (const auto& lane : lanes) {
    //     std::cout << lane << std::endl;
    // }

    return;
}


void RetrieveStrategyImp::Retrieve(std::unordered_map<string, ContainerVariant>& context, size_t max_num_players, size_t state_dim) {
    vector<int> trafficlight_queue_length(max_num_players);
    vector<int> lane_queue_length(max_num_players * state_dim);
    vector<float> lane_length(max_num_players * state_dim);
    vector<float> lane_max_speed(max_num_players * state_dim);
    // vector<vector<vector<double>>> vehicle_speeds(tl_ids_.size()); temporarily unavailable
    // vector<vector<vector<double>>> vehicle_positions(tl_ids_.size()); temporarily unavailable
    // vector<vector<vector<double>>> vehicle_acceleratoins(tl_ids_.size()); temporarily unavailable

    size_t tl_idx = 0;
    for (const string& tl_id : tl_ids_) {
        const auto& lanes = in_lanes_map_[tl_id];

        size_t lane_idx = 0;
        int tl_queue_length = 0;
        for (const string& lane_id : lanes) {
            
            size_t cur_idx = tl_idx * state_dim + lane_idx;
            lane_length[cur_idx] = static_cast<float>(Lane::getLength(lane_id));
            lane_queue_length[cur_idx] = Lane::getLastStepHaltingNumber(lane_id);
            lane_max_speed[cur_idx] = static_cast<float>(Lane::getMaxSpeed(lane_id));
            
            tl_queue_length += Lane::getLastStepHaltingNumber(lane_id);

            // const auto& last_step_vehicle_ids = Lane::getLastStepVehicleIDs(lane_id);
            // vehicle_speeds[tl_idx][lane_idx].reserve(last_step_vehicle_ids.size());
            // vehicle_positions[tl_idx][lane_idx].reserve(last_step_vehicle_ids.size());
            // for (const string& vehicle_id : last_step_vehicle_ids) {
            //     vehicle_speeds[tl_idx][lane_idx].emplace_back(Vehicle::getSpeed(vehicle_id));
            //     vehicle_positions[tl_idx][lane_idx].emplace_back(Vehicle::getLanePosition(vehicle_id));
            //     vehicle_acceleratoins[tl_idx][lane_idx].emplace_back(Vehicle::getAcceleration(vehicle_id));
            // }

            ++lane_idx;
        }
        trafficlight_queue_length[tl_idx] = tl_queue_length;
        ++tl_idx;
    }

    context["trafficlight_queue_length"] = std::move(trafficlight_queue_length);
    context["lane_length"] = std::move(lane_length);
    context["lane_queue_length"] = std::move(lane_queue_length);
    context["lane_max_speed"] = std::move(lane_max_speed);
    // context["vehicle_speed"] = std::move(vehicle_speeds);
    // context["vehicle_position"] = std::move(vehicle_positions);
    // context["vehicle_acceleration"] = std::move(vehicle_acceleratoins);

    return;
}
