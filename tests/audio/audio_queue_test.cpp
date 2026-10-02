#include "audio/host_audio_system.h"
#include <rex/audio/conversion.h>
#include <rex/audio/downmix.h>
#include <rex/cvar.h>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
#include <iostream>

void Require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

class QueueDriver : public sylpheed::audio::HostAudioDriver {
 public:
  QueueDriver(rex::thread::Semaphore* semaphore, unsigned channels)
      : HostAudioDriver(nullptr, semaphore, 0, false) {
    channels_ = channels;
    SDL_AudioSpec spec{SDL_AUDIO_F32LE, int(channels), 48000};
    stream_ = SDL_CreateAudioStream(&spec, &spec);
    Require(stream_ != nullptr, "test stream created");
  }
  using HostAudioDriver::Enqueue;
  void Fill(unsigned blocks) { Callback(this, stream_, blocks * channels_ * 256 * 4, 0); }
  std::vector<float> Read(unsigned blocks) {
    std::vector<float> result(blocks * channels_ * 256);
    const int bytes = result.size() * sizeof(float);
    Require(SDL_GetAudioStreamData(stream_, result.data(), bytes) == bytes, "exact output block size");
    return result;
  }
};

std::array<float, 6 * 256> Frame(float value) {
  std::array<float, 6 * 256> result;
  for (unsigned c = 0; c < 6; ++c) for (unsigned i = 0; i < 256; ++i) {
    result[c * 256 + i] = std::bit_cast<float>(std::byteswap(std::bit_cast<uint32_t>(value * (c + 1))));
  }
  return result;
}

void CheckOutput(unsigned channels, bool mute) {
  std::cerr << "Checking channels=" << channels << " mute=" << mute << std::endl;
  Require(rex::cvar::SetFlagByName("audio_mute", mute ? "true" : "false"), "mute configuration");
  auto sem = rex::thread::Semaphore::Create(0, 64);
  QueueDriver driver(sem.get(), channels);
  const auto first = Frame(.01f), second = Frame(-.02f);
  driver.Enqueue(first.data()); driver.Enqueue(second.data());
  driver.Fill(3);  // two supplied frames followed by a missing frame
  const auto actual = driver.Read(3);
  std::vector<float> expected(actual.size(), 0);
  if (!mute) {
    const auto gain = rex::audio::GetOutputGain();
    if (channels == 2) {
      const auto fold = rex::audio::GetStereoFold();
      rex::audio::conversion::sequential_6_BE_to_interleaved_2_LE(expected.data(), first.data(), 256, fold, gain);
      rex::audio::conversion::sequential_6_BE_to_interleaved_2_LE(expected.data()+512, second.data(), 256, fold, gain);
    } else {
      const auto mix = rex::audio::GetSurroundMix();
      rex::audio::conversion::sequential_6_BE_to_interleaved_6_LE(expected.data(), first.data(), 256, mix, gain);
      rex::audio::conversion::sequential_6_BE_to_interleaved_6_LE(expected.data()+1536, second.data(), 256, mix, gain);
    }
  }
  Require(std::memcmp(actual.data(), expected.data(), actual.size()*sizeof(float)) == 0,
          "FIFO sample conversion, mute and missing-frame silence match the SDK rules");
  Require(rex::thread::Wait(sem.get(), false, std::chrono::milliseconds(0)) == rex::thread::WaitResult::kSuccess &&
          rex::thread::Wait(sem.get(), false, std::chrono::milliseconds(0)) == rex::thread::WaitResult::kSuccess &&
          rex::thread::Wait(sem.get(), false, std::chrono::milliseconds(0)) == rex::thread::WaitResult::kTimeout,
          "only consumed supplied frames return producer credits; silence must not release credits");
  driver.Enqueue(first.data());  // shutdown also frees still-queued blocks
  driver.Shutdown(); driver.Shutdown();
}

int Run() {
  CheckOutput(2, false); CheckOutput(6, false); CheckOutput(2, true); CheckOutput(6, true);
  rex::cvar::SetFlagByName("audio_mute", "false");
  // Exercise simultaneous producer and consumer transfers with real SDK
  // semaphore credits, not a mirror of the queue implementation.
  auto sem = rex::thread::Semaphore::Create(1, 64);
  QueueDriver driver(sem.get(), 2);
  const auto samples = Frame(.01f);
  std::atomic<bool> done = false;
  std::exception_ptr producer_error;
  std::jthread producer([&] {
    try {
      for (int i = 0; i < 500; ++i) {
        Require(rex::thread::Wait(sem.get(), false, std::chrono::seconds(2)) == rex::thread::WaitResult::kSuccess,
                "producer must regain credits without deadlock");
        driver.Enqueue(samples.data());
      }
    } catch (...) { producer_error = std::current_exception(); }
    done = true;
  });
  while (!done) { driver.Fill(1); driver.Read(1); std::this_thread::yield(); }
  producer.join();
  if (producer_error) std::rethrow_exception(producer_error);
  driver.Shutdown();
  return 0;
}

int main() {
  try { return Run(); }
  catch (const std::exception& error) { std::cerr << error.what() << std::endl; return 1; }
}
