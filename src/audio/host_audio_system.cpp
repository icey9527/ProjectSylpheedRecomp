// Host queue adaptation of ReXGlue v0.10.0 src/audio/sdl/sdl_audio_driver.cpp.
// Copyright 2020 Ben Vanik; adapted by Tom Clay, 2026. BSD-3-Clause.
// Original license and attribution: THIRD-PARTY-NOTICES.txt.
#include "host_audio_system.h"

#include <rex/assert.h>
#include <rex/audio/conversion.h>
#include <rex/audio/downmix.h>
#include <rex/audio/flags.h>
#include <rex/audio/sdl/sdl_audio_system.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <chrono>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#endif

REXCVAR_DEFINE_BOOL(host_audio_queue, true, "Audio", "Use the host output queue with short locks (restart required)");
REXCVAR_DEFINE_BOOL(trace_audio_queue, false, "Audio", "Report audio queue supply and silence once per second");
REXCVAR_DEFINE_INT32(audio_starvation_break_ms, 0, "Audio", "Diagnostic debugger break after a supply gap in ms (0 disables)");

namespace sylpheed::audio {
using rex::X_STATUS;
namespace {
uint64_t NowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
void RaiseMax(std::atomic<uint64_t>& target, uint64_t value) {
  auto previous = target.load(std::memory_order_relaxed);
  while (previous < value && !target.compare_exchange_weak(previous, value, std::memory_order_relaxed)) {}
}
}

HostAudioDriver::HostAudioDriver(rex::memory::Memory* memory, rex::thread::Semaphore* semaphore,
                                 size_t client, bool trace)
    : AudioDriver(memory), semaphore_(semaphore), client_(client), trace_(trace) {
  const auto threshold = REXCVAR_GET(audio_starvation_break_ms);
  starvation_break_ms_ = threshold > 0 && threshold <= 1000 ? threshold : 0;
  if (starvation_break_ms_) trace_ = true;
}

HostAudioDriver::~HostAudioDriver() { Shutdown(); }

bool HostAudioDriver::Initialize() {
  SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
  SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, "project_sylpheed");
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    REXLOG_ERROR("SYLPHEED_AUDIO SDL audio initialization failed: {}", SDL_GetError());
    return false;
  }
  initialized_ = true;
  SDL_AudioSpec desired{SDL_AUDIO_F32LE, 6, sample_rate}, obtained{};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, Callback, this);
  if (!stream_) return false;
  auto device = SDL_GetAudioStreamDevice(stream_);
  if (!device) return false;
  if (!SDL_GetAudioDeviceFormat(device, &obtained, nullptr)) obtained = desired;
  // Match the SDK's explicit stereo fold; do not let SDL choose a different mix.
  if (obtained.channels <= 2) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    channels_ = 2;
    desired.channels = 2;
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, Callback, this);
    if (!stream_) return false;
    device = SDL_GetAudioStreamDevice(stream_);
    if (!device) return false;
  }
  // The SDK permits at most 64 outstanding frames. Allocate reusable blocks
  // before starting the device, never in its realtime callback.
  for (unsigned i = 0; i < 64; ++i) unused_.push(new float[frame_samples]);
  REXLOG_INFO("SYLPHEED_AUDIO client={} host queue endpoint='{}' channels={} rate={} trace={}",
              client_, SDL_GetAudioDeviceName(device), channels_, obtained.freq, trace_);
  started_us_ = NowUs();
  if (!SDL_ResumeAudioDevice(device)) return false;
  if (trace_) {
    reporter_ = std::jthread([this](std::stop_token stop) {
      std::unique_lock lock(report_mutex_);
      while (!stop.stop_requested()) {
        report_wakeup_.wait_for(lock, stop, std::chrono::seconds(1), [] { return false; });
        if (!stop.stop_requested()) Report();
      }
    });
  }
  return true;
}

void HostAudioDriver::SubmitFrame(uint32_t samples_ptr) {
  Enqueue(memory_->TranslateVirtual<const float*>(samples_ptr));
}

void HostAudioDriver::Enqueue(const float* samples) {
  float* buffer = nullptr;
  {
    std::lock_guard lock(queue_mutex_);
    if (!unused_.empty()) { buffer = unused_.top(); unused_.pop(); }
  }
  if (!buffer) buffer = new float[frame_samples];  // Same producer-side fallback as the SDK.
  std::memcpy(buffer, samples, frame_samples * sizeof(float));
  {
    std::lock_guard lock(queue_mutex_);
    queued_.push(buffer);
  }
  if (trace_) {
    const auto now = NowUs();
    const auto previous = last_submit_us_.exchange(now, std::memory_order_relaxed);
    if (previous && now >= previous) RaiseMax(max_submit_gap_us_, now - previous);
    submitted_.fetch_add(1, std::memory_order_relaxed);
  }
}

void SDLCALL HostAudioDriver::Callback(void* userdata, SDL_AudioStream* stream,
                                      int additional_amount, int) {
  auto& driver = *static_cast<HostAudioDriver*>(userdata);
  const auto started = driver.trace_ ? NowUs() : 0;
  std::array<float, frame_samples> data;
  const int bytes = channel_samples * driver.channels_ * sizeof(float);
  const auto fold = rex::audio::GetStereoFold();
  const auto mix = rex::audio::GetSurroundMix();
  const auto gain = rex::audio::GetOutputGain();
  while (additional_amount > 0) {
    float* buffer = nullptr;
    {
      std::lock_guard lock(driver.queue_mutex_);
      if (!driver.queued_.empty()) { buffer = driver.queued_.front(); driver.queued_.pop(); }
    }
#ifdef _WIN32
    // Opt-in diagnostic only. Freeze at the shortage itself, not a second
    // later in the reporting thread. Exclude startup and never hold queue_mutex_.
    if (!buffer && driver.starvation_break_ms_ && IsDebuggerPresent()) {
      const auto now = NowUs();
      const auto previous = driver.last_submit_us_.load(std::memory_order_relaxed);
      if (previous && now >= previous && now - driver.started_us_ >= 10000000 &&
          now - previous >= uint64_t(driver.starvation_break_ms_) * 1000 &&
          !driver.starvation_break_seen_.exchange(true)) DebugBreak();
    }
#endif
    // No queue lock during conversion, stream submission or semaphore wakeup.
    if (!buffer || REXCVAR_GET(audio_mute)) {
      std::memset(data.data(), 0, bytes);
    } else if (driver.channels_ == 2) {
      rex::audio::conversion::sequential_6_BE_to_interleaved_2_LE(
          data.data(), buffer, channel_samples, fold, gain);
    } else {
      rex::audio::conversion::sequential_6_BE_to_interleaved_6_LE(
          data.data(), buffer, channel_samples, mix, gain);
    }
    const bool submitted = SDL_PutAudioStreamData(stream, data.data(), bytes);
    if (buffer) {
      { std::lock_guard lock(driver.queue_mutex_); driver.unused_.push(buffer); }
      if (submitted) {
        const auto released = driver.semaphore_->Release(1, nullptr);
        assert_true(released);
      }
    }
    if (!submitted) {
      REXLOG_ERROR("SYLPHEED_AUDIO output failed: {}", SDL_GetError());
      break;
    }
    if (driver.trace_) {
      (buffer ? driver.played_ : driver.silence_).fetch_add(1, std::memory_order_relaxed);
    }
    additional_amount -= bytes;
  }
  if (driver.trace_) {
    driver.callbacks_.fetch_add(1, std::memory_order_relaxed);
    RaiseMax(driver.max_callback_us_, NowUs() - started);
  }
}

void HostAudioDriver::Report() {
  size_t queued;
  { std::lock_guard lock(queue_mutex_); queued = queued_.size(); }
  REXLOG_INFO("SYLPHEED_AUDIO client={} submit={} played={} silence={} queued={} callbacks={} max_submit_gap_us={} max_callback_us={}",
              client_, submitted_.exchange(0), played_.exchange(0), silence_.exchange(0), queued,
              callbacks_.exchange(0), max_submit_gap_us_.exchange(0), max_callback_us_.exchange(0));
}

void HostAudioDriver::Shutdown() {
  const bool active = stream_ || initialized_ || reporter_.joinable();
  if (reporter_.joinable()) { reporter_.request_stop(); reporter_.join(); }
  // SDL waits for callbacks before freeing the stream; only then free blocks.
  if (stream_) { SDL_DestroyAudioStream(stream_); stream_ = nullptr; }
  if (initialized_) { SDL_QuitSubSystem(SDL_INIT_AUDIO); initialized_ = false; }
  if (trace_ && active) Report();
  std::lock_guard lock(queue_mutex_);
  while (!queued_.empty()) { delete[] queued_.front(); queued_.pop(); }
  while (!unused_.empty()) { delete[] unused_.top(); unused_.pop(); }
}

HostAudioSystem::HostAudioSystem(rex::runtime::FunctionDispatcher* dispatcher) : AudioSystem(dispatcher) {}

std::unique_ptr<rex::audio::AudioSystem> HostAudioSystem::Create(rex::runtime::FunctionDispatcher* dispatcher) {
  if (!REXCVAR_GET(host_audio_queue)) return rex::audio::sdl::SDLAudioSystem::Create(dispatcher);
  return std::make_unique<HostAudioSystem>(dispatcher);
}

X_STATUS HostAudioSystem::CreateDriver(size_t index, rex::thread::Semaphore* semaphore,
                                      rex::audio::AudioDriver** out_driver) {
  auto driver = std::make_unique<HostAudioDriver>(memory_, semaphore, index, REXCVAR_GET(trace_audio_queue));
  if (!driver->Initialize()) {
    REXLOG_ERROR("SYLPHEED_AUDIO host queue initialization failed: {}", SDL_GetError());
    return X_STATUS_UNSUCCESSFUL;
  }
  *out_driver = driver.release();
  return X_STATUS_SUCCESS;
}

void HostAudioSystem::DestroyDriver(rex::audio::AudioDriver* driver) { delete driver; }

}  // namespace sylpheed::audio
