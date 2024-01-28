#pragma once

#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/debug-helpers.h>

namespace Scripting
{
  namespace Detail
  {
    MonoImage* GetMainMonoImage();
    void PrintException( MonoObject* e );
  }
}

namespace MonoTest::Testbed
{
  inline int TestInterop( Test::ScriptTest* nativeTester, const wchar_t* stringArg )
  {
    auto monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "MonoTest", "Testbed" ); assert( monoClass );
    auto monoDesc  = mono_method_desc_new( ":TestInterop(ScriptTest,string)", false ); assert( monoDesc );
    auto monoFunc  = mono_method_desc_search_in_class( monoDesc, monoClass ); assert( monoFunc );
    mono_method_desc_free( monoDesc );
    
    void* args[] = { nativeTester->GetOrCreateMonoObject(), stringArg ? mono_string_new_utf16( mono_get_root_domain(), stringArg, int( wcslen( stringArg ) ) ) : nullptr };

    MonoObject* monoException = nullptr;
    MonoObject* retVal = mono_runtime_invoke( monoFunc, nullptr, (void**)args, &monoException );
    if ( monoException )
    {
      EasyMono::Detail::PrintException( monoException );
      assert( false );
    }

    return *(int32_t*)mono_object_unbox( retVal );
  }

}