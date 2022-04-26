/*
 * Copyright 2022 Garena Online Private Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ENVPOOL_MUJOCO_HUMANOID_H_
#define ENVPOOL_MUJOCO_HUMANOID_H_

#include <algorithm>
#include <limits>
#include <memory>
#include <string>

#include "envpool/core/async_envpool.h"
#include "envpool/core/env.h"
#include "envpool/mujoco/mujoco_env.h"

namespace mujoco {

class HumanoidEnvFns {
 public:
  static decltype(auto) DefaultConfig() {
    return MakeDict(
        "max_episode_steps"_.bind(1000), "frame_skip"_.bind(5),
        "post_constraint"_.bind(true), "forward_reward_weight"_.bind(1.25),
        "ctrl_cost_weight"_.bind(0.1), "contact_cost_weight"_.bind(5e-7),
        "contact_cost_max"_.bind(10.0), "healthy_reward"_.bind(5.0),
        "healthy_z_min"_.bind(1.0), "healthy_z_max"_.bind(2.0),
        "reset_noise_scale"_.bind(1e-2));
  }
  template <typename Config>
  static decltype(auto) StateSpec(const Config& conf) {
    mjtNum inf = std::numeric_limits<mjtNum>::infinity();
    return MakeDict("obs"_.bind(Spec<mjtNum>({376}, {-inf, inf})),
                    "info:reward_linvel"_.bind(Spec<mjtNum>({-1})),
                    "info:reward_quadctrl"_.bind(Spec<mjtNum>({-1})),
                    "info:reward_alive"_.bind(Spec<mjtNum>({-1})),
                    "info:reward_impact"_.bind(Spec<mjtNum>({-1})),
                    "info:x_position"_.bind(Spec<mjtNum>({-1})),
                    "info:y_position"_.bind(Spec<mjtNum>({-1})),
                    "info:distance_from_origin"_.bind(Spec<mjtNum>({-1})),
                    "info:x_velocity"_.bind(Spec<mjtNum>({-1})),
                    "info:y_velocity"_.bind(Spec<mjtNum>({-1})),
                    // TODO(jiayi): remove these two lines for speed
                    "info:qpos0"_.bind(Spec<mjtNum>({24})),
                    "info:qvel0"_.bind(Spec<mjtNum>({23})));
  }
  template <typename Config>
  static decltype(auto) ActionSpec(const Config& conf) {
    return MakeDict("action"_.bind(Spec<mjtNum>({-1, 17}, {-0.4f, 0.4f})));
  }
};

typedef class EnvSpec<HumanoidEnvFns> HumanoidEnvSpec;

class HumanoidEnv : public Env<HumanoidEnvSpec>, public MujocoEnv {
 protected:
  int max_episode_steps_, elapsed_step_;
  mjtNum ctrl_cost_weight_, contact_cost_weight_, contact_cost_max_;
  mjtNum forward_reward_weight_, healthy_reward_;
  mjtNum healthy_z_min_, healthy_z_max_;
  mjtNum mass_x_, mass_y_;
  std::unique_ptr<mjtNum> qpos0_, qvel0_;  // for align check
  std::uniform_real_distribution<> dist_;
  bool done_;

 public:
  HumanoidEnv(const Spec& spec, int env_id)
      : Env<HumanoidEnvSpec>(spec, env_id),
        MujocoEnv(spec.config["base_path"_] + "/mujoco/assets/humanoid.xml",
                  spec.config["frame_skip"_], spec.config["post_constraint"_]),
        max_episode_steps_(spec.config["max_episode_steps"_]),
        elapsed_step_(max_episode_steps_ + 1),
        ctrl_cost_weight_(spec.config["ctrl_cost_weight"_]),
        contact_cost_weight_(spec.config["contact_cost_weight"_]),
        contact_cost_max_(spec.config["contact_cost_max"_]),
        forward_reward_weight_(spec.config["forward_reward_weight"_]),
        healthy_reward_(spec.config["healthy_reward"_]),
        healthy_z_min_(spec.config["healthy_z_min"_]),
        healthy_z_max_(spec.config["healthy_z_max"_]),
        mass_x_(0),
        mass_y_(0),
        qpos0_(new mjtNum[model_->nq]),
        qvel0_(new mjtNum[model_->nv]),
        dist_(-spec.config["reset_noise_scale"_],
              spec.config["reset_noise_scale"_]),
        done_(true) {}

  void MujocoResetModel() {
    for (int i = 0; i < model_->nq; ++i) {
      data_->qpos[i] = qpos0_.get()[i] = init_qpos_[i] + dist_(gen_);
    }
    for (int i = 0; i < model_->nv; ++i) {
      data_->qvel[i] = qvel0_.get()[i] = init_qvel_[i] + dist_(gen_);
    }
  }

  bool IsDone() override { return done_; }

  void Reset() override {
    done_ = false;
    elapsed_step_ = 0;
    MujocoReset();
    WriteObs(0.0f, 0, 0, 0, 0, 0, 0);
  }

  void Step(const Action& action) override {
    // step
    mjtNum* act = static_cast<mjtNum*>(action["action"_].data());
    GetMassCenter();
    mjtNum x_before = mass_x_, y_before = mass_y_;
    MujocoStep(act);
    GetMassCenter();
    mjtNum x_after = mass_x_, y_after = mass_y_;

    // ctrl_cost
    mjtNum ctrl_cost = 0.0;
    for (int i = 0; i < model_->nu; ++i) {
      ctrl_cost += ctrl_cost_weight_ * act[i] * act[i];
    }
    // xv and yv
    mjtNum dt = frame_skip_ * model_->opt.timestep;
    mjtNum xv = (x_after - x_before) / dt;
    mjtNum yv = (y_after - y_before) / dt;
    // contact cost
    mjtNum contact_cost = 0.0;
    for (int i = 0; i < 6 * model_->nbody; ++i) {
      mjtNum x = data_->cfrc_ext[i];
      contact_cost += contact_cost_weight_ * x * x;
    }
    contact_cost = std::min(contact_cost, contact_cost_max_);

    // reward and done
    float reward = xv * forward_reward_weight_ + healthy_reward_ - ctrl_cost -
                   contact_cost;
    ++elapsed_step_;
    done_ = !IsHealthy() || (elapsed_step_ >= max_episode_steps_);
    WriteObs(reward, xv, yv, ctrl_cost, contact_cost, x_after, y_after);
  }

 private:
  bool IsHealthy() {
    return healthy_z_min_ < data_->qpos[2] && data_->qpos[2] < healthy_z_max_;
  }

  void GetMassCenter() {
    mjtNum mass_sum = 0.0;
    mass_x_ = mass_y_ = 0.0;
    for (int i = 0; i < model_->nbody; ++i) {
      mjtNum mass = model_->body_mass[i];
      mass_sum += mass;
      mass_x_ += mass * data_->xipos[3 * i + 0];
      mass_y_ += mass * data_->xipos[3 * i + 1];
    }
    mass_x_ /= mass_sum;
    mass_y_ /= mass_sum;
  }

  void WriteObs(float reward, mjtNum xv, mjtNum yv, mjtNum ctrl_cost,
                mjtNum contact_cost, mjtNum x_after, mjtNum y_after) {
    State state = Allocate();
    state["reward"_] = reward;
    // obs
    mjtNum* obs = static_cast<mjtNum*>(state["obs"_].data());
    for (int i = 2; i < model_->nq; ++i) {
      *(obs++) = data_->qpos[i];
    }
    for (int i = 0; i < model_->nv; ++i) {
      *(obs++) = data_->qvel[i];
    }
    for (int i = 0; i < 10 * model_->nbody; ++i) {
      *(obs++) = data_->cinert[i];
    }
    for (int i = 0; i < 6 * model_->nbody; ++i) {
      *(obs++) = data_->cvel[i];
    }
    for (int i = 0; i < model_->nv; ++i) {
      *(obs++) = data_->qfrc_actuator[i];
    }
    for (int i = 0; i < 6 * model_->nbody; ++i) {
      *(obs++) = data_->cfrc_ext[i];
    }
    // info
    state["info:reward_linvel"_] = xv * forward_reward_weight_;
    state["info:reward_quadctrl"_] = -ctrl_cost;
    state["info:reward_impact"_] = -contact_cost;
    state["info:reward_alive"_] = healthy_reward_;
    state["info:x_position"_] = x_after;
    state["info:y_position"_] = y_after;
    state["info:distance_from_origin"_] =
        std::sqrt(x_after * x_after + y_after * y_after);
    state["info:x_velocity"_] = xv;
    state["info:y_velocity"_] = yv;
    state["info:qpos0"_].Assign(qpos0_.get(), model_->nq);
    state["info:qvel0"_].Assign(qvel0_.get(), model_->nv);
  }
};

typedef AsyncEnvPool<HumanoidEnv> HumanoidEnvPool;

}  // namespace mujoco

#endif  // ENVPOOL_MUJOCO_HUMANOID_H_