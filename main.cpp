#include "Profiler.hpp"
#include <thread>
#include <chrono>
#include <cmath>
#include <iostream>

int main() {
  qprof::Profiler& profiler = qprof::Profiler::Get();

  profiler.PrintCapabilities();
  profiler.Start();

  // double sum = 0.0;
  // int len = 256 * 1024 * 1024;
  // double *arr = new double[len];
  // for (int i = 0; i < 5; ++i) {
  //   for (int j = 0; j < len; ++j) {
  //     sum += arr[j];
  //   }
  // }
  // std::cout << "Computation result: " << sum << std::endl;

  std::this_thread::sleep_for(std::chrono::milliseconds(60*1000));

  profiler.Stop();
  

  // delete[] arr;

  return 0;
}