#ifndef SUMOCLIENT_H
#define SUMOCLIENT_H

#include <vector>

#include <memory>
#include <string>
#include <variant>
#include <iostream>
#include <algorithm>
#include <unordered_map>

#include <libsumo/libsumo.h>

#include "traffic_light.h"
#include "retrieve_strategy.h"

namespace sumoenv {

class SumoEnvFns {
 public:
  /**
   * Returns a dict, keys are the names of the configurable variables of this
   * env, values stores the default values of the corresponding variable.
   *
   * EnvPool will append to your configuration some common fields, currently
   * there are the envpool specific configurations that defines the behavior of
   * envpool.
   *
   * 1. num_envs: number of envs to be launched in envpool
   * 2. batch_size: the batch_size when interacting with the envpool
   * 3. num_threads: the number of threads to run all the envs
   * 4. thread_affinity_offset: sets the thread affinity of the threads
   * 5. base_path: contains the path of the envpool python package
   * 6. seed: random seed
   *
   * These's also single env specific configurations
   *
   * 7. max_num_players: defines the number of players in a single env.
   *
   */
  static decltype(auto) DefaultConfig() {
    return MakeDict("duration_thred"_.Bind(45), 
                    "action_num"_.Bind(6));
  }

  /**
   * Returns a dict, keys are the names of the states of this env,
   * values are the ArraySpec of the state (as each state is stored in an
   * array).
   *
   * The array spec can be created by calling `Spec<dtype>(shape, bounds)`.
   *
   * Similarly, envpool also append to this state spec, there're:
   *
   * 1. info:env_id: a int array that has shape [batch_size], when there's a
   * batch of states, it tells the user from which `env_id` that these states
   * come from.
   * 2. info:players.env_id: This is similar to `env_id`, but it has a shape of
   * [total_num_player], where the `total_num_player` is the total number of
   * players summed.
   *
   * For example, if in one batch we have states from envs [1, 3, 4],
   * in env 1 there're players [1, 2], in env 2 there're players [2, 3, 4],
   * in env 3 there're players [1]. Then: 
   * `info:env_id == [1, 3, 4]`
   * `info:players.env_id == [1, 1, 3, 3, 3, 4]`
   *
   * 3. elapsed_step: the total elapsed steps of the envs.
   * 4. done: whether it is the end of episode for each env.
   */
  template <typename Config>
  static decltype(auto) StateSpec(const Config& conf) {
    return MakeDict("obs:lane_queue_length"_.Bind(Spec<int>({-1, 8})),
                    "obs:lane_length"_.Bind(Spec<float>({-1, 8})),
                    "obs:vehicle_speed"_.Bind(Spec<Container<float>>({-1, 8})),
                    "obs:vehicle_position"_.Bind(Spec<Container<float>>({-1, 8})),
                    "obs:vehicle_acceleration"_.Bind(Spec<Container<float>>({-1, 8})),
                    "info:agent_to_update"_.Bind(Spec<bool>({-1})),
                    "info:done"_.Bind(Spec<bool>({})),
                    // "reward"_.Bind(Spec<float>({-1})), included in the original definition.
                    );      
  }

  /**
   * Returns a dict, keys are the names of the actions of this env,
   * values are the ArraySpec of the actions (each action is stored in an
   * array).
   *
   * Similarly, envpool also append to this state spec, there're:
   *
   * 1. env_id
   * 2. players.env_id
   *
   * Their meanings are the same as in the `StateSpec`.
   */
  template <typename Config>
  static decltype(auto) ActionSpec(const Config& conf) {
    return MakeDict("action:stage"_.Bind(Spec<int>({-1}, {0, 7})),
                    "action:duration"_.Bind(Spec<int>({-1}, {0, conf["duration_thred"_]-1}))
                    )
  }
};


using SumoEnvSpec = EnvSpec<SumoEnvFns>;
using Simulation = libsumo::Simulation;

class SumoClient: public Env<SumoEnvSpec> { 
 private:
    const string path_to_sumo_;
    const string net_;
    const string route_;
    const string addition_;
    const int num_agent_;
    const int yellow_time_;
    const int random_seed_;
    const double end_time_;


    static const string kMaxDepartDelay;
    static const string kWaitingTimeMemory;
    static const string kTimeToTeleport;
    static const string kNoWarnings;

    vector<string> sumo_cmd_;
    vector<std::unique_ptr<TrafficLightImp>> traffic_lights_;
    std::unique_ptr<RetrieveStrategy> retrieve_strategy_imp_;

    std::unordered_map<string, ContainerVariant> context_;

    void WriteState(){
        // todo
        State state = Allocate(num_agent_);
        state["obs:lane_queue_length"_] = static_cast<float>(context_["lane_queue_length"]]);
        state["obs:lane_length"_] = static_cast<float>(context_["lane_length"]);
        state["obs:vehicle_speed"_] = static_cast<float>(context_["vehicle_speed"]);
        state["obs:vehicle_position"_] = static_cast<float>(context_["vehicle_position"]);
        state["obs:vehicle_acceleration"_] = static_cast<float>(context_["vehicle_acceleration"]);
        state["info:agent_to_update"_] = static_cast<float>(context_["agent_to_update"]);
        state["info:done"_] = static_cast<float>(context_["done"]);

    }

 public:
    SumoClient(
        const Spec& spec,
        // todo
        const string& path_to_sumo,
        const string& net,
        const string& route,
        const string& addition,
        int yellow_time,
        int random_seed,
        double end_time
    );

    void Reset(); //todo
    void SetTrafficLights();
    void SetStrategies();
    const std::unordered_map<string, ContainerVariant>& Retrieve(); // 这里会有好几层引用传递的问题

    void Step(const Action& action); //todo
    void close();
    void TempTest();

    bool IsDone(); //todo

    //当这些最基础的东西成熟之后，至少现在有点零乱
    //sumoenv？


};

#endif // SUMOCLIENT_H
