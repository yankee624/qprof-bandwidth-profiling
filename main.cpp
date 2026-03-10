#include "Profiler.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

int main() {
  qprof::Profiler& profiler = qprof::Profiler::Get();

  profiler.PrintCapabilities();
  profiler.Start();

  std::this_thread::sleep_for(std::chrono::milliseconds(5*1000));

  profiler.Stop();
  
  return 0;
}