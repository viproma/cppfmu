#include <iostream>

#include "cppfmu_cs.hpp"

cppfmu::UniquePtr<cppfmu::SlaveInstance> CppfmuInstantiateSlave(
  cppfmu::FMIString instanceName, cppfmu::FMIString,
  cppfmu::FMIString, cppfmu::FMIString,
  cppfmu::FMIReal, cppfmu::FMIBoolean, cppfmu::FMIBoolean,
  cppfmu::Memory memory, cppfmu::Logger) {

  return nullptr;
}


int main()
{
#ifdef CPPFMU_USE_FMI_1_0
    std::cout << "Platform: " << fmiGetTypesPlatform() << std::endl;
#else
    std::cout << "Platform: " << fmi2GetTypesPlatform() << std::endl;
#endif
    return 0;
}
