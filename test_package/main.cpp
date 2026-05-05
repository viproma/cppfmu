#include <iostream>

#ifdef CPPFMU_USE_FMI_3_0
#include "cppfmu_cs_fmi3.hpp"

cppfmu::UniquePtr<cppfmu::SlaveInstance3> CppfmuInstantiateSlave(
  cppfmu::FMIString, cppfmu::FMIString,
  cppfmu::FMIString, cppfmu::FMIBoolean, cppfmu::FMIBoolean,
  cppfmu::FMIBoolean, cppfmu::FMIBoolean,
  const cppfmu::FMIValueReference[], std::size_t,
  cppfmu::FMIComponentEnvironment,
  std::function<void(cppfmu::FMIStatus, cppfmu::FMIString, cppfmu::FMIString)>) {

  return nullptr;
}
#else
#include "cppfmu_cs.hpp"

cppfmu::UniquePtr<cppfmu::SlaveInstance> CppfmuInstantiateSlave(
  cppfmu::FMIString instanceName, cppfmu::FMIString,
  cppfmu::FMIString, cppfmu::FMIString,
  cppfmu::FMIReal, cppfmu::FMIBoolean, cppfmu::FMIBoolean,
  cppfmu::Memory memory, cppfmu::Logger) {

  return nullptr;
}
#endif


int main()
{
#ifdef CPPFMU_USE_FMI_1_0
    std::cout << "Platform: " << fmiGetTypesPlatform() << std::endl;
#elif defined(CPPFMU_USE_FMI_3_0)
    std::cout << "Platform: " << fmi3GetVersion() << std::endl;
#else
    std::cout << "Platform: " << fmi2GetTypesPlatform() << std::endl;
#endif
    return 0;
}
