#ifndef RETRIEVE_STRATEGY_H
#define RETRIEVE_STRATEGY_H

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "libsumo/libsumo.h"

using TrafficLight = libsumo::TrafficLight;
using Lane = libsumo::Lane;
using Vehicle = libsumo::Vehicle;
using ContainerVariant =
    std::variant<std::vector<int>, std::vector<float>, int>;

class SumoClient;

class RetrieveStrategyInterface {
 public:
  virtual ~RetrieveStrategy() = default;
  virtual Retrieve(
      int max_num_players, int state_dim,
      const std::vector<int>& agents_to_update,
      const std::unordered_map<std::string, std::vector<std::string>>& in_lanes_map,
      std::unordered_map<std::string, ContainerVariant>& context) = 0;
};

class RetrieveStrategyImp : public RetrieveStrategyInterface {
 public:
  void Retrieve(
      int max_num_players, int state_dim,
      const std::vector<int>& agents_to_update,
      const std::unordered_map<std::string, std::vector<std::string>>& in_lanes_map,
      std::unordered_map<std::string, ContainerVariant>& context) override;
};

#endif  // RETRIEVE_STRATEGY_H
