#include "traffic_light.h"

#include <functional>
#include <iostream>
#include <utility>

using TrafficLight = libsumo::TrafficLight;

// Constructor
TrafficLightImp::TrafficLightImp(const std::string& tls_id, int yellow_time)
    : tl_ids_(tls_id), yellow_time_(yellow_time), stage_pre_(-1) {
  mapping_ = {
      {-1, 8, 8, 8, 9, 8, 10, 8},       {11, -1, 11, 11, 11, 12, 11, 13},
      {14, 14, -1, 14, 15, 14, 16, 14}, {17, 17, 17, -1, 17, 18, 17, 19},
      {20, 22, 21, 22, -1, 22, 22, 22}, {23, 24, 23, 25, 23, -1, 23, 23},
      {26, 27, 28, 27, 27, 27, -1, 27}, {29, 30, 29, 31, 29, 29, 29, -1}};
}

// Destructor
TrafficLightImp::~TrafficLightImp() {
  // Clean up resources if needed
}

// Check method
int TrafficLightImp::Check() {
  if (schedule_.empty()) {
    throw std::runtime_error("Error: Schedule is empty.");
  }
  if (schedule_.front() > 0) {
    TrafficLight::setPhase(tl_ids_, stage_pre_);
    schedule_.insert(schedule_.end(), schedule_.front(), 0);
    schedule_.pop_front();
    schedule_.push_back(-1);
  }
  return schedule_.front();
}

void TrafficLightImp::SetStageDuration(int stage, int duration) {
  if (stage_pre_ != -1 && stage_pre_ != stage) {
    int yellow_stage = mapping_[stage_pre_][stage];
    TrafficLight::setPhase(tl_ids_, yellow_stage);
    std::fill_n(std::back_inserter(schedule_), yellow_time_, 0);
  }
  stage_pre_ = stage;
  schedule_.push_back(duration);
}

int TrafficLightImp::RetrieveLeftTime() const {
  if (!schedule_.empty() && schedule_.back() == -1) {
    return schedule_.size();
  } else {
    return schedule_.empty() ? 0 : schedule_.size() - 1 + schedule_.back();
  }
}

int TrafficLightImp::RetrieveStageIndex() const { return stage_pre_; }