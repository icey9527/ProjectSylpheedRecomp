#pragma once

#include <rex/audio/audio_driver.h>
#include <rex/audio/audio_system.h>
#include <SDL3/SDL.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <stack>
#include <thread>

namespace sylpheed::audio {

// Retains the SDK's guest mixer / audio worker. Only the host output queue
// changes; no game addresses or WMV/XAudio algorithms are replaced.
class HostAudioDriver : public rex::audio::AudioDriver {
 public:
  HostAudioDriver(rex::memory::Memory* memory, rex::thread::Semaphore* semaphore,
                  size_t client, bool trace);
  ~HostAudioDriver() override;
  bool Initialize();
  void SubmitFrame(uint32_t samples_ptr) override;
  void Shutdown();

 protected:
  static void SDLCALL Callback(void* userdata, SDL_AudioStream* stream,
                              int additional_amount, int total_amount);
  void Enqueue(const float* samples);
  void Report();
  static constexpr unsigned sample_rate = 48000, channel_samples = 256;
  static constexpr unsigned frame_samples = channel_samples * 6;
  rex::thread::Semaphore* semaphore_;
  SDL_AudioStream* stream_ = nullptr;
  unsigned channels_ = 6;
  bool initialized_ = false;
  std::queue<float*> queued_;
  std::stack<float*> unused_;
  std::mutex queue_mutex_;

 private:
  size_t client_;
  bool trace_;
  unsigned starvation_break_ms_ = 0;
  uint64_t started_us_ = 0;
  std::atomic<bool> starvation_break_seen_{false};
  std::atomic<uint64_t> submitted_{0}, played_{0}, silence_{0}, callbacks_{0};
  std::atomic<uint64_t> max_submit_gap_us_{0}, max_callback_us_{0};
  std::atomic<uint64_t> last_submit_us_{0};
  std::condition_variable_any report_wakeup_;
  std::mutex report_mutex_;
  std::jthread reporter_;
};

class HostAudioSystem final : public rex::audio::AudioSystem {
 public:
  explicit HostAudioSystem(rex::runtime::FunctionDispatcher* dispatcher);
  static std::unique_ptr<rex::audio::AudioSystem> Create(rex::runtime::FunctionDispatcher* dispatcher);
  rex::X_STATUS CreateDriver(size_t index, rex::thread::Semaphore* semaphore,
                        rex::audio::AudioDriver** driver) override;
  void DestroyDriver(rex::audio::AudioDriver* driver) override;
};

}  // namespace sylpheed::audio
