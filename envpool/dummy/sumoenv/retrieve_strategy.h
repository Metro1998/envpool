// retrieve_strategy.h

#ifndef RETRIEVE_STRATEGY_H
#define RETRIEVE_STRATEGY_H

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <any>
#include "libsumo/libsumo.h"


using TrafficLight = libsumo::TrafficLight;
using Lane = libsumo::Lane;
using string = std::string;
template <typename T>
using vector = std::vector<T>;


class RetrieveStrategy {
  public:
    RetrieveStrategy() = default;
    virtual std::any Retrieve() = 0;
    virtual ~RetrieveStrategy() = default;

  protected:
    vector<string> tl_ids_;
    std::unordered_map<string, vector<string>> in_lanes_map_;
    // std::unordered_map<string, vector<string>> out_lanes_map_; // to_sting?

    void ProcessTlsId() {
      tl_ids_ = TrafficLight::getIDList();
    }

    void ProcessLanes() {
      for (const auto& tl_id : tl_ids_) {
        in_lanes_map_[tl_id] = TrafficLight::getControlledLanes(tl_id);
        RemoveElements(in_lanes_map_[tl_id]);
        RemoveElements_(in_lanes_map_[tl_id]);
      } 
    }
    
    void RemoveElements(vector<string>& lanes) {
    lanes.erase(std::remove_if(lanes.begin(), lanes.end(),
                               [i = 0](const auto&) mutable {
                                bool shouldErase = i % 3 == 1 || i % 3 == 2; 
                                i++;
                                return shouldErase; }),
                lanes.end());

    lanes.erase(std::remove_if(lanes.begin(), lanes.end(),
                               [i = 0](const auto&) mutable { return i++ % 3 == 2;}),
                lanes.end());
    }

    void RemoveElements_(vector<string>& lanes) {
    
    }
    
    // template<typename Func, typename... Args>
    // auto MetaRetrieve(Func f, Args... args) -> decltype(f(std::forward<Args>(args)...)) {
    //     return std::invoke(f, std::forward<Args>(args)...);
    // }
};


class ObservationStrategy : public RetrieveStrategy {
  public:
    ObservationStrategy() : RetrieveStrategy() {
      // Process the data you need for calculating observations and rewards here.
        ProcessTlsId();
        ProcessLanes();
    }

    std::any Retrieve() override {
      // Rewrite the Retrieve method to implement your own reward design or state representation.
      // Here we use 'hide' ranther than 'override' from virtual function to support decltype(auto) return type.
      vector<vector<int>> observation;

      for (const string& tl_id : tl_ids_) {
        vector<int> lane_vehicles_for_tl;
        for (const auto& lane_id : in_lanes_map_[tl_id]) {
          std::cout << lane_id << std::endl;
          int lane_vehicles = Lane::getLastStepHaltingNumber(lane_id);
          lane_vehicles_for_tl.push_back(lane_vehicles);
        }
        observation.push_back(lane_vehicles_for_tl);
      }
      return observation;
    }
};


class RewardStrategy : public RetrieveStrategy {
  public:
    RewardStrategy() : RetrieveStrategy() {
      // Process the data you need for calculating observations and rewards here.
        ProcessTlsId();
        ProcessLanes();
    }

    std::any Retrieve() override {
      // Rewrite the Retrieve method to implement your own reward design or state representation.
      // Here we use 'hide' ranther than 'override' from virtual function to support decltype(auto) return type.
      vector<int> reward;

      for (const string& tl_id : tl_ids_) {
        int vehicles_for_tl = 0;
        for (const auto& lane_id : in_lanes_map_[tl_id]) {
          vehicles_for_tl += Lane::getLastStepHaltingNumber(lane_id);
        }
        reward.push_back(vehicles_for_tl);
      }
      return reward;
    }
};


















#endif // RETRIEVE_STRATEGY_H