#ifndef TRAFFIC_LIGHT_H
#define TRAFFIC_LIGHT_H

#include <deque>
#include <string>
#include <vector>

#include "libsumo/libsumo.h"

class TrafficLightImp {
 public:
  TrafficLightImp(const std::string& tls_id, int yellow_time);
  ~TrafficLightImp();

  int Check();
  int RetrieveLeftTime() const;
  int RetrieveStageIndex() const;
  inline void Pop() {
    schedule_.pop_front();
    return;
  }
  void SetStageDuration(int stage, int duration);

 private:
  int stage_pre_;
  int yellow_time_;
  std::string tl_ids_; // todo
  std::deque<int> schedule_;
  std::vector<std::vector<int>> mapping_;
};

#endif  // TRAFFIC_LIGHT_H