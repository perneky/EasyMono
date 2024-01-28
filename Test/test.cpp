#define IMPLEMENT_EASY_MONO 1
#include <Include/EasyMono.h>

#include "ScriptTest.h"
#include "Generated/RegisterScriptInterface.h"
#include "CSInterop/MonoTest/Testbed.h"

#include <iostream>
#include <thread>

int main()
{
  std::vector< const char* > binaries = { "../TestManaged/bin/Debug/net8.0/TestManaged.dll" };

  EasyMono::Initialize( false, "C:/Program Files/Mono/lib", "C:/Program Files/Mono/etc", binaries.data(), binaries.size() );

  RegisterScriptInterface();

  auto testObject = new Test::ScriptTest( 69, L"Test object" );
  testObject->AddRef();

  MonoTest::Testbed::TestInterop( testObject, L"This" );

  testObject->Release();

  std::cout << std::endl << "Collecting garbage..." << std::endl;

  EasyMono::GarbageCollect( false );

  // Lets wait for one second to see the garbage collector calling the finalizers, and detaching the script objects.
  std::this_thread::sleep_for( std::chrono::seconds( 1 ) );

  std::cout << std::endl << "...done collecting garbage" << std::endl;

  std::cout << std::endl << "Shutting down Mono..." << std::endl;

  EasyMono::Teardown();

  std::cout << std::endl << "... done shutting down Mono" << std::endl;

  return 0;
}