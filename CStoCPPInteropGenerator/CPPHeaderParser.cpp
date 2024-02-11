#include "clang-c/Index.h"
#include <iostream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <cassert>
#include <regex>
#include <Windows.h>

auto csFileTemplate = LR"(
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace @
{
  public @class @ : @
  {
@
@
    @@( ulong thiz ) : base( thiz )
    {
    }
@
  }
})";

auto csLocalStructTemplate = LR"(
    [StructLayout(LayoutKind.Sequential)]
    public struct @
    {
@
    }
)";

auto csStandaloneStructTemplate = LR"(
  [StructLayout(LayoutKind.Sequential)]
  public struct @
  {
@
  }
)";

auto csLocalEnumTemplate = LR"(
    public enum @ : @
    {
@
    }
)";

auto csStandaloneEnumTemplate = LR"(
  public enum @ : @
  {
@
  }
)";

auto csCtorTemplate = LR"(
    public @( @ ) : base( Ctor( @ ) )
    {
    }
)";

auto csExportedCtorTemplate = LR"(
    [MethodImpl(MethodImplOptions.InternalCall)]
    extern static UInt64 Ctor( @ );
)";

auto csExportedMethodTemplate = LR"(
    [MethodImpl(MethodImplOptions.InternalCall)]
    public extern @@ @( @ );
)";

auto csDelegateTemplate = LR"(
@public delegate @ @( @ );
)";

auto cppFileTemplate = LR"(
#include <mono/metadata/loader.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

#include <memory>

@
namespace EasyMono
{
  namespace Detail
  {
    ScriptedClass* LoadNativePointer( MonoObject* monoObject );

    MonoImage* GetMainMonoImage();

    struct GCHolder
    {
      uint32_t handle = 0;
      GCHolder( uint32_t handle ) : handle( handle ) {}
      ~GCHolder() { if ( handle ) mono_gchandle_free( handle ); }
      GCHolder( const GCHolder& ) = delete;
      GCHolder& operator=( const GCHolder& ) = delete;
      GCHolder( GCHolder&& other ) { handle = other.handle; other.handle = 0; }
      GCHolder& operator=( GCHolder&& other ) { handle = other.handle; other.handle = 0; }
    };

    using GCHolderPtr = std::shared_ptr< GCHolder >;

    static GCHolderPtr Hold( MonoObject* monoObject )
    {
      return std::make_shared< GCHolder >( mono_gchandle_new( monoObject, false ) );
    }

    /* Some explanation. When a function is returning with a structure by value, the structure itself matters
     * in how the function is called on the assembly level. In Mono, all structures are PODs, and therefore
     * if their size is less or equal to 8 bytes, they will be passed differently as opposed to being larger.
     *
     * Our structures are different, as they are based on EasyMono::ScriptedStruct, therefore they are passed
     * differently (basically as if they are larger than 8 bytes). So to match how Mono will call the wrapper
     * functions, for types with size less or equal to 8 bytes, we use uint64_t as a return type, and cast our
     * type to that. Mono will interpret the data correctly.
     */

    template<size_t A, size_t B>
    struct is_less_equal
    {
      static constexpr bool value = A <= B;
    };

    template<size_t A, size_t B>
    struct is_greater
    {
      static constexpr bool value = A > B;
    };

    template< typename T, typename Enable = void >
    struct interop_struct;

    template< typename T >
    struct interop_struct< T, std::enable_if_t< std::is_pointer_v< T > || std::is_reference_v< T > > >
    {
      using type = T;
      static T process( T v ) { return v; }
    };

    template< typename T >
    struct interop_struct< T, std::enable_if_t< !std::is_pointer_v< T > && !std::is_reference_v< T > && is_less_equal< sizeof( T ), 8 >::value > >
    {
      using type = uint64_t;
      static uint64_t process( const T& v ) { return *reinterpret_cast<const uint64_t*>( &v ); }
    };

    template< typename T >
    struct interop_struct< T, std::enable_if_t< !std::is_pointer_v< T > && !std::is_reference_v< T > && is_greater< sizeof( T ), 8 >::value > >
    {
      using type = T;
      static const T& process( const T& v ) { return v; }
    };
  }

  extern thread_local bool creatingFromCS;
}

template< typename T, typename Enable = void >
using IS = EasyMono::Detail::interop_struct< T, Enable >;

void RegisterScriptInterface()
{
@

}
@
)";

auto cppClassTemplate = LR"(
  struct @
  {
@
  };
@
)";

auto cppClassGetterTemplate = LR"(
MonoClass* @::GetMonoClass()
{
  static MonoClass* monoClass = nullptr;
  if ( !monoClass )
  {
    monoClass = mono_class_from_name( EasyMono::Detail::GetMainMonoImage(), "@", "@" );
    assert( monoClass );
  }
  
  return monoClass;
}
)";

auto cppCtorTemplate = LR"(
    static uint64_t __stdcall @( @ )
    {
      EasyMono::creatingFromCS = true;

      auto thiz = reinterpret_cast< uint64_t >( new @( @ ) );
    
      EasyMono::creatingFromCS = false;
      return thiz;
    }
)";

auto cppMethodTemplate = LR"(
    static @ __stdcall @( MonoObject* thiz@@ )
    {@
      auto nativeThis = reinterpret_cast< @* >( EasyMono::Detail::LoadNativePointer( thiz ) );
      @ @( nativeThis->@( @ )@ );@
    }
)";

auto cppStaticMethodTemplate = LR"(
    static @ __stdcall @( @ )
    {@
      @ @( @::@( @ )@ );@
    }
)";

auto cppMethodRegistrationTemplate = LR"(
  mono_add_internal_call( "@::@", @::@ );)";

auto cppStringArgTranslationTemplate = LR"(
      @ @CStr = @ ? @( mono_string_chars( @ ) ) : @();
)";

auto cppDelegateWrapperTemplate = LR"(
      auto @__wrapper = [gchandle = EasyMono::Detail::Hold( @ )]( @ )
      {
        auto monoDelegateObject = mono_gchandle_get_target( gchandle->handle ); assert( monoDelegateObject );
@
        MonoObject* monoException = nullptr;
        @mono_runtime_delegate_invoke( monoDelegateObject, @, &monoException );
        if ( monoException )
        {
          EasyMono::Detail::PrintException( monoException );
          assert( false );
        }@
      };
)";

template< typename T >
std::wstring ToString( T t )
{
  return std::to_wstring( t );
}

template<>
std::wstring ToString( std::wstring t )
{
  return t;
}

template<>
std::wstring ToString( const wchar_t* t )
{
  return t;
}

template< typename ... T >
static std::wstring FormatString( const wchar_t* s, T&& ... args )
{
  std::wstring out;

  ( [ & ]
    {
      while ( *s && *s != L'@' )
      {
        out += *s;
        ++s;
      }

      out += ToString( args );

      ++s;
    } ( ), ... );

  if ( *s )
    out += s;

  return out;
}

inline std::wstring W( const char* s )
{
  if ( !s || *s == 0 )
    return std::wstring();

  int len = int( strlen( s ) );

  auto wideCharCount = MultiByteToWideChar( CP_UTF8, 0, s, len, nullptr, 0 );
  std::vector< wchar_t > wideChars( wideCharCount );
  MultiByteToWideChar( CP_UTF8, 0, s, len, wideChars.data(), int( wideChars.size() ) );
  return std::wstring( wideChars.data(), wideChars.size() );
}

inline std::wstring W( const std::string& s )
{
  return W( s.data() );
}

std::wstring JustName( const wchar_t* name )
{
  std::wstring justName = name;
  auto sep = justName.rfind( ':' );
  if ( sep != std::wstring::npos )
    justName = justName.substr( sep + 1 );
  return justName;
}

std::wstring JustName( const std::wstring& name )
{
  return JustName( name.data() );
}

bool IsStartsWith( const std::wstring& a, const std::wstring& b )
{
  return wcsstr( a.data(), b.data() ) == a.data();
}

struct KnownStructure
{
  std::wstring nativeName;
  std::wstring managedName;
};

std::vector< KnownStructure > knownStructures;

struct TypeDesc
{
  enum class Kind
  {
    None,
    Unknown,
    Primitive,
    Struct,
    Class,
    String,
    Enum,
    Delegate,
  };

  Kind kind;
  std::wstring csName;
  std::wstring cppName;
};

struct StructDesc
{
  std::wstring name;
  std::wstring nameSpace;
  std::wstring header;
  std::vector< std::pair< TypeDesc, std::wstring > > fields;
  bool isScripted = false;
  mutable bool isExported = false;
};

struct ArgumentDesc
{
  std::wstring name;
  TypeDesc type;
  bool isConst;
  bool isLValRef;
  bool isRValRef;
  bool isPointer;
};

struct MethodDesc
{
  std::wstring name;
  ArgumentDesc returnValue;
  std::vector< ArgumentDesc > arguments;
  bool isStatic;
  bool isVirtual;
  bool isOverride;
};

struct ClassDesc
{
  std::wstring nameSpace;
  std::wstring name;

  std::wstring header;

  bool isFinal;

  std::vector< MethodDesc > methods;

  std::vector< std::wstring > baseClasses;
};

struct EnumDesc
{
  std::wstring csType;
  std::wstring cppType;
  std::wstring name;
  std::wstring nameSpace;
  std::wstring header;
  std::vector< std::pair< std::wstring, int64_t > > values;
  mutable bool isExported = false;
};

struct DelegateDesc
{
  std::wstring name;
  std::wstring nameSpace;

  ArgumentDesc returnValue;
  std::vector< ArgumentDesc > arguments;
  mutable bool isExported = false;
};

std::unordered_map< std::wstring, ClassDesc > classes;
std::unordered_map< std::wstring, StructDesc > structs;
std::unordered_map< std::wstring, EnumDesc > enums;
std::unordered_map< std::wstring, DelegateDesc > delegates;

std::vector<CXCursor> stack;

const ClassDesc* GetScriptedBase( const ClassDesc& desc, const wchar_t* scriptedBaseName )
{
  for ( auto& base : desc.baseClasses )
  {
    auto iter = classes.find( base );
    if ( iter != classes.end() )
    {
      if ( iter->second.name == scriptedBaseName )
        return &iter->second;
      if ( GetScriptedBase( iter->second, scriptedBaseName ) )
        return &iter->second;
    }
  }

  return nullptr;
}

std::wstring ToString( CXString&& s )
{
  auto str = W( clang_getCString( s ) );
  clang_disposeString( s );
  return str;
}

std::wstring ToString( CXCursor cursor )
{
  return ToString( clang_getCursorSpelling( cursor ) );
}

std::wstring ToString( CXType type )
{
  auto str = ToString( clang_getTypeSpelling( clang_getNonReferenceType( type ) ) );

  // There has to be a better way of getting the type without const and pointer...
  if ( auto constLoc = str.find( L"const " ); constLoc != std::string::npos )
    str.erase( constLoc, 6 );
  if ( auto ptrLoc = str.find( L" *" ); ptrLoc != std::string::npos )
    str.erase( ptrLoc, 2 );
  return str;
}

enum class Language { CS, CPP };
std::wstring GetFullName( CXCursor cursor, Language lang )
{
  if ( cursor.kind == CXCursor_TranslationUnit )
    return L"";

  if ( cursor.kind == CXCursor_ClassDecl
    || cursor.kind == CXCursor_ClassTemplate
    || cursor.kind == CXCursor_StructDecl
    || cursor.kind == CXCursor_Namespace
    || cursor.kind == CXCursor_EnumDecl
    || cursor.kind == CXCursor_TypeAliasDecl )
  {
    auto parent = GetFullName( clang_getCursorSemanticParent( cursor ), lang );
    if ( parent.empty() )
      return ToString( cursor );
    if ( lang == Language::CS )
      return parent + L"." + ToString( cursor );
    else
      return parent + L"::" + ToString( cursor );
  }

  return L"";
}

ArgumentDesc ParseArgument( CXType type, bool isConst, bool isPointer, bool isLValRef, bool isRValRef );

std::wstring GetFullName( CXType type, Language lang )
{
  auto getTemplateArgument = []( CXCursor typeDecl, int argIx )
  {
    int numTemplateArguments = clang_Cursor_getNumTemplateArguments( typeDecl );

    auto arrayType = clang_Cursor_getTemplateArgumentType( typeDecl, argIx );
    auto arrayTypeIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( arrayType ) );
    return ParseArgument( arrayType, arrayTypeIsConst, false, false, false );
  };

  auto typeDecl = clang_getTypeDeclaration( clang_getCanonicalType( type ) );
  auto fullName = GetFullName( typeDecl, lang );
  if ( ( lang == Language::CS && fullName == L"EasyMono.Array" ) || ( lang == Language::CPP && fullName == L"EasyMono::Array" ) )
  {
    auto asArgument = getTemplateArgument( typeDecl, 0 );

    if ( lang == Language::CS )
      return asArgument.type.csName + L"[]";

    return L"MonoArray*";
  }
  if ( ( lang == Language::CS && fullName == L"EasyMono.List" ) || ( lang == Language::CPP && fullName == L"EasyMono::List" ) )
  {
    auto asArgument = getTemplateArgument( typeDecl, 0 );

    if ( lang == Language::CS )
      return L"System.Collections.Generic.List<" + asArgument.type.csName + L">";

    return L"MonoObject*";
  }
  if ( ( lang == Language::CS && fullName == L"EasyMono.Dictionary" ) || ( lang == Language::CPP && fullName == L"EasyMono::Dictionary" ) )
  {
    auto keyArgument = getTemplateArgument( typeDecl, 0 );
    auto valueArgument = getTemplateArgument( typeDecl, 1 );

    if ( lang == Language::CS )
      return L"System.Collections.Generic.Dictionary<" + keyArgument.type.csName + L", " + valueArgument.type.csName + L">";

    return L"MonoObject*";
  }
  return fullName;
}

bool Contains( const std::wstring& s, const wchar_t* sub )
{
  return s.find( sub ) != std::string::npos;
}

void TranslateKnownStructureNames( TypeDesc& arg )
{
  auto iter = std::find_if( knownStructures.begin(), knownStructures.end(), [ & ]( const KnownStructure& s ) { return arg.cppName == s.nativeName; } );
  if ( iter != knownStructures.end() )
    arg.csName = iter->managedName;
}

ArgumentDesc ParsePrimitiveArgument( const wchar_t* csName, const wchar_t* cppName, bool isConst, bool isPointer, bool isLValRef, bool isRValRef )
{
  ArgumentDesc arg;
  arg.type.kind    = TypeDesc::Kind::Primitive;
  arg.type.csName  = csName;
  arg.type.cppName = cppName;
  arg.isConst      = isConst;
  arg.isPointer    = isPointer;
  arg.isLValRef    = isLValRef;
  arg.isRValRef    = isRValRef;
  return arg;
}

TypeDesc::Kind GetTypeKind( CXType type )
{
  if ( type.kind == CXType_WChar )
    return TypeDesc::Kind::String;

  auto decl = clang_getTypeDeclaration( clang_getCanonicalType( type ) );
  switch ( decl.kind )
  {
  case CXCursor_ClassDecl:
  {
    auto name = GetFullName( decl, Language::CPP );
    if ( name == L"std::function" )
      return TypeDesc::Kind::Delegate;
    return TypeDesc::Kind::Class;
  }
  case CXCursor_StructDecl: return TypeDesc::Kind::Struct;
  case CXCursor_EnumDecl: return TypeDesc::Kind::Enum;
  default: return TypeDesc::Kind::Primitive;
  }
}

ArgumentDesc ParseArgument( CXType type, bool isConst, bool isPointer, bool isLValRef, bool isRValRef )
{
  switch ( type.kind )
  {
  case CXType_Void: return ParsePrimitiveArgument( L"void", L"void", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Bool: return ParsePrimitiveArgument( L"bool", L"bool", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Char_S: return ParsePrimitiveArgument( L"char", L"char", isConst, isPointer, isLValRef, isRValRef );
  case CXType_UChar: return ParsePrimitiveArgument( L"byte", L"uint8_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_UShort: return ParsePrimitiveArgument( L"ushort", L"uint16_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_UInt: return ParsePrimitiveArgument( L"uint", L"uint32_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_ULong: return ParsePrimitiveArgument( L"uint", L"uint32_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_ULongLong: return ParsePrimitiveArgument( L"ulong", L"uint64_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_SChar: return ParsePrimitiveArgument( L"sbyte", L"int8_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Short: return ParsePrimitiveArgument( L"short", L"int16_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Int: return ParsePrimitiveArgument( L"int", L"int32_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Long: return ParsePrimitiveArgument( L"int", L"int32_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_LongLong: return ParsePrimitiveArgument( L"long", L"int64_t", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Float: return ParsePrimitiveArgument( L"float", L"float", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Double: return ParsePrimitiveArgument( L"double", L"double", isConst, isPointer, isLValRef, isRValRef );
  case CXType_Pointer: return ParseArgument( clang_getPointeeType( type ), isConst || clang_isConstQualifiedType( clang_getPointeeType( type ) ), true, isLValRef, isRValRef );
  case CXType_LValueReference: return ParseArgument( clang_getNonReferenceType( type ), isConst, isPointer, true, isRValRef );
  case CXType_RValueReference: return ParseArgument( clang_getNonReferenceType( type ), isConst, isPointer, isLValRef, true );
  case CXType_Typedef: return ParseArgument( clang_getCanonicalType( type ), isConst, isPointer, isLValRef, isRValRef );
  case CXType_WChar:
  {
    if ( !isPointer )
    {
      std::wcerr << L"wchar_t should be a pointer for " << ToString( type ) << " !" << std::endl;
      break;
    }

    ArgumentDesc arg;
    arg.type.kind    = TypeDesc::Kind::String;
    arg.type.csName  = L"string";
    arg.type.cppName = L"wchar_t";
    arg.isConst      = isConst;
    arg.isPointer    = isPointer;
    arg.isLValRef    = isLValRef;
    arg.isRValRef    = isRValRef;
    return arg;
  }
  case CXType_Elaborated:
  {
    ArgumentDesc arg;
    arg.type.kind    = GetTypeKind( clang_Type_getNamedType( type ) );
    arg.type.csName  = GetFullName( clang_Type_getNamedType( type ), Language::CS );
    arg.type.cppName = GetFullName( clang_Type_getNamedType( type ), Language::CPP );
    arg.isConst      = isConst;
    arg.isPointer    = isPointer;
    arg.isLValRef    = isLValRef;
    arg.isRValRef    = isRValRef;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_Record:
  {
    ArgumentDesc arg;
    arg.type.kind    = GetTypeKind( ( type ) );
    arg.type.csName  = GetFullName( ( type ), Language::CS );
    arg.type.cppName = GetFullName( ( type ), Language::CPP );
    arg.isConst      = isConst;
    arg.isPointer    = isPointer;
    arg.isLValRef    = isLValRef;
    arg.isRValRef    = isRValRef;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  default:
    std::wcout << L"ParseArgument default: " << ToString( type ) << std::endl;
    break;
  }

  ArgumentDesc arg;
  arg.type.kind    = TypeDesc::Kind::Unknown;
  arg.type.csName  = L"unknown";
  arg.type.cppName = L"unknown";
  arg.isConst      = false;
  arg.isPointer    = false;
  arg.isLValRef    = false;
  arg.isRValRef    = false;
  return arg;
}

const wchar_t* ignoredNamespaces[] =
{
  L"std", L"ATL"
};

bool IsInIgnoredNamespace()
{
  if ( !stack.empty() )
    if ( auto name = ToString( stack.front() ); std::find( std::begin( ignoredNamespaces ), std::end( ignoredNamespaces ), name ) != std::end( ignoredNamespaces ) )
      return true;
  return false;
}

std::wstring GetStackNamespace()
{
  std::wstring nameSpace;
  for ( auto& s : stack )
  {
    if ( !nameSpace.empty() )
      nameSpace += L"::";
    nameSpace += ToString( s );
  }
  return nameSpace;
}

std::wstring MakeKey( const std::wstring& nameSpace, const std::wstring& name )
{
  return nameSpace.empty() ? name : ( nameSpace + L"::" + name );
}

std::wstring ParseHeader( CXCursor cursor )
{
  auto classExtent = clang_getCursorExtent( cursor );
  auto classStart = clang_getRangeStart( classExtent );

  CXFile* filePath;
  clang_getExpansionLocation( classStart, (CXFile*)&filePath, nullptr, nullptr, nullptr );

  return ToString( clang_getFileName( filePath ) );
}

void ParseStruct( CXCursor cursor )
{
  if ( IsInIgnoredNamespace() )
    return;

  std::wstring nameSpace = GetStackNamespace();

  if ( nameSpace.empty() )
    return;

  auto name = GetFullName( cursor, Language::CPP );
  auto key = name;

  auto iter = structs.find( key );
  if ( iter != structs.end() )
    return;

  StructDesc strukt;

  strukt.name = name;
  strukt.nameSpace = nameSpace;
  strukt.header = ParseHeader( cursor );

  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
    {
      auto& desc = *static_cast<StructDesc*>( client_data );

      auto kind = clang_getCursorKind( cursor );
      switch ( kind )
      {
      case CXCursor_CXXAccessSpecifier:
        if ( clang_getCXXAccessSpecifier( cursor ) != CX_CXXPublic )
          std::wcerr << L"The structure '" + desc.name + L"' has non public fields. The interop can only work with public fields.\n";
        break;
      case CXCursor_CXXBaseSpecifier:
        if ( ToString( clang_getTypeDeclaration( clang_getCursorType( cursor ) ) ) == L"ScriptedStruct" )
          desc.isScripted = true;
        break;
      case CXCursor_FieldDecl:
        if (desc.isScripted)
        {
          auto name = ToString( cursor );
          auto type = clang_getCursorType( cursor );
          auto asArg = ParseArgument( type, false, false, false, false );
          if ( asArg.isPointer || asArg.isLValRef || asArg.isRValRef )
          {
            std::wcerr << L"The '" << name << L"' field of the struct '" << desc.name << L"' is a pointer or reference. These are not supported." << std::endl;
            desc.isScripted = false;
            break;
          }
          desc.fields.emplace_back( std::make_pair( asArg.type, name ) );
          if ( desc.fields.back().first.kind != TypeDesc::Kind::Primitive )
          {
            std::wcerr << L"The '" << name << L"' field of the struct '" << desc.name << L"' is not a primitive. Only primitives are supported." << std::endl;
            desc.isScripted = false;
            break;
          }
        }
        break;
      default:
        break;
      }

      return CXChildVisit_Continue;
    }, &strukt );

  if ( strukt.isScripted )
    structs.insert( { key, strukt } );
}

MethodDesc ParseMethod( CXCursor cursor )
{
  MethodDesc desc;

  desc.name = ToString( cursor );

  bool isCtor = clang_getCursorKind( cursor ) == CXCursor_Constructor;

  if ( isCtor )
  {
    desc.isStatic = false;
    desc.isVirtual = false;
    desc.isOverride = false;

    desc.returnValue.type.kind = TypeDesc::Kind::None;
  }
  else
  {
    desc.isStatic = clang_CXXMethod_isStatic( cursor );
    desc.isVirtual = clang_CXXMethod_isVirtual( cursor );
    desc.isOverride = false;

    auto retType = clang_getCursorResultType( cursor );
    auto retIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( retType ) );
    desc.returnValue = ParseArgument( retType, retIsConst, false, false, false );
  }

  int numArgs = clang_Cursor_getNumArguments( cursor );
  for ( int arg = 0; arg < numArgs; ++arg )
  {
    auto argCursor = clang_Cursor_getArgument( cursor, arg );
    auto argType = clang_getCursorType( argCursor );
    auto argIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( argType ) );
    desc.arguments.emplace_back( ParseArgument( argType, argIsConst, false, false, false ) );
    auto& methodArg = desc.arguments.back();
    methodArg.name = ToString( argCursor );
    if ( methodArg.type.kind == TypeDesc::Kind::Delegate )
    {
      if ( !methodArg.isRValRef )
      {
        methodArg.name = L"DelegatesNeedToBeRValueReferences";
        continue;
      }

      methodArg.type.csName.clear();
      methodArg.type.cppName.clear();

      auto nonRefType = clang_getNonReferenceType( argType );
      for ( auto c = clang_getTypeDeclaration( nonRefType ); c.kind != CXCursor_TranslationUnit; c = clang_getCursorSemanticParent( c ) )
      {
        methodArg.type.csName.insert( 0, L"." + ToString( c ) );
        methodArg.type.cppName.insert( 0, L"::" + ToString( c ) );
      }

      while ( methodArg.type.cppName[ 0 ] == L':' )
        methodArg.type.cppName.erase( methodArg.type.cppName.begin() );
      if ( methodArg.type.csName[ 0 ] == L'.' )
        methodArg.type.csName.erase( methodArg.type.csName.begin() );
    }
  }

  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
  {
    auto& desc = *static_cast< MethodDesc* >( client_data );

    auto kind = clang_getCursorKind( cursor );
    switch ( kind )
    {
    case CXCursor_CXXOverrideAttr:
      desc.isOverride = true;
      return CXChildVisit_Break;
    }
    return CXChildVisit_Continue;
  }, &desc);

  return desc;
}

void ParseEnum( CXCursor cursor )
{
  if ( IsInIgnoredNamespace() )
    return;

  auto name = GetFullName( cursor, Language::CPP );

  if ( name.find( L' ' ) != name.npos )
    return;

  auto key = name;

  std::wstring nameSpace = GetStackNamespace();

  if ( nameSpace.empty() )
    return;

  auto iter = enums.find( key );
  if ( iter != enums.end() )
    return;

  auto enumType = clang_getEnumDeclIntegerType( cursor );
  auto type     = ParseArgument( enumType.kind == CXTypeKind::CXType_Elaborated ? clang_Type_getNamedType( enumType ) : enumType, false, false, false, false ).type;

  auto& inum = enums[ key ];
  inum.csType    = type.csName;
  inum.cppType   = type.cppName;
  inum.name      = name;
  inum.nameSpace = nameSpace;
  inum.header    = ParseHeader( cursor );

  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
    {
      auto& inum = *static_cast<EnumDesc*>( client_data );

      auto kind = clang_getCursorKind( cursor );
      switch ( kind )
      {
      case CXCursor_EnumConstantDecl:
        inum.values.emplace_back( std::make_pair( ToString( cursor ), clang_getEnumConstantDeclValue( cursor ) ) );
        return CXChildVisit_Continue;

      default:
        return CXChildVisit_Continue;
      }
    }, &inum );
}

void ParseClass( CXCursor cursor )
{
  if ( IsInIgnoredNamespace() )
    return;

  std::wstring nameSpace = GetStackNamespace();

  auto name = GetFullName( cursor, Language::CPP );
  auto key = name;

  auto& klass = classes[ key ];

  klass.name = name;
  klass.nameSpace = nameSpace;
  klass.header = ParseHeader( cursor );

  struct ClassContext
  {
    ClassDesc* klass;
    bool exporting = false;
    bool skipNextAccessSpecifier = false;
  } ctx;

  ctx.klass = &klass;

  // We are not interested in fields and such. Only in what we can export to the interop.
  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
  {
    auto& ctx = *static_cast< ClassContext* >( client_data );

    auto kind = clang_getCursorKind( cursor );
    switch ( kind )
    {
    case CXCursor_CXXAccessSpecifier:
      if ( !ctx.skipNextAccessSpecifier )
        ctx.exporting = false;
      ctx.skipNextAccessSpecifier = false;
      break;
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
      if ( ctx.exporting )
        ctx.klass->methods.emplace_back( ParseMethod( cursor ) );
      break;
    case CXCursor_CXXFinalAttr:
      ctx.klass->isFinal = true;
      break;
    case CXCursor_CXXBaseSpecifier:
      ctx.klass->baseClasses.emplace_back( GetFullName( clang_getTypeDeclaration( clang_getCursorType( cursor ) ), Language::CPP ) );
      break;
    case CXCursor_StructDecl:
      if ( ToString( cursor ) == L"__ManagedExport__" )
      {
        ctx.exporting = true;
        ctx.skipNextAccessSpecifier = true;
      }
      else if ( ctx.exporting )
        ParseStruct( cursor );
      return CXChildVisit_Continue;
    case CXCursor_EnumDecl:
      if ( ctx.exporting )
        ParseEnum( cursor );
      return CXChildVisit_Continue;
    default:
      break;
    }

    return CXChildVisit_Continue;
  }, &ctx );
}

void ParseDelegate( CXCursor cursor )
{
  if ( stack.empty() )
    return;

  auto type = clang_getCursorType( cursor );
  if ( type.kind != CXType_Typedef )
    return;

  auto realType = clang_getCanonicalType( type );
  if ( realType.kind != CXType_Record )
    return;

  struct Context
  {
    bool isValid = true;
    DelegateDesc desc;
  } context;

  clang_visitChildren( cursor, []( CXCursor cursor, CXCursor parent, CXClientData client_data )
  {
    auto& context = *reinterpret_cast< Context* >( client_data );

    switch ( cursor.kind )
    {
    case CXCursor_NamespaceRef:
      if ( ToString( cursor ) != L"std" )
        context.isValid = false;
      break;
    case CXCursor_TemplateRef:
      if ( ToString( cursor ) != L"function" )
        context.isValid = false;
      break;
    case CXCursor_ParmDecl:
    {
      auto argType = clang_getCursorType( cursor );
      auto argIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( argType ) );
      context.desc.arguments.emplace_back( ParseArgument( argType, argIsConst, false, false, false ) );
      context.desc.arguments.back().name = ToString( cursor );
      break;
    }
    default:
      context.isValid = false;
    }

    return context.isValid ? CXChildVisit_Continue : CXChildVisit_Break;
  }, &context );

  if ( !context.isValid )
    return;

  context.desc.nameSpace = GetStackNamespace();
  context.desc.name = GetFullName( cursor, Language::CPP );

  int numTemplateArguments = clang_Type_getNumTemplateArguments( realType );
  if ( numTemplateArguments != 1 )
    return;

  auto functionType = clang_Type_getTemplateArgumentAsType( realType, 0 );
  if ( functionType.kind != CXType_FunctionProto )
    return;

  auto functionCursor = clang_getTypeDeclaration( functionType );
  auto returnType = clang_getResultType( functionType );
  auto retIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( returnType ) );
  context.desc.returnValue = ParseArgument( returnType, retIsConst, false, false, false );

  auto key = context.desc.name;

  auto iter = delegates.find( key );
  if ( iter != delegates.end() )
    return;

  delegates.insert( { key, context.desc } );
}

CXChildVisitResult visitTranslationUnit( CXCursor cursor, CXCursor parent, CXClientData client_data )
{
  auto parentKind = clang_getCursorKind( parent );

  if ( parentKind == CXCursor_TranslationUnit )
    while ( !stack.empty() )
      stack.pop_back();
  else if ( parentKind == CXCursor_Namespace )
    while ( !stack.empty() && !clang_equalCursors( parent, stack.back() ) )
      stack.pop_back();
  else
    assert( false );

  auto kind = clang_getCursorKind( cursor );
  switch ( kind )
  {
  case CXCursor_Namespace:
    stack.push_back( cursor );
    return CXChildVisit_Recurse;
  case CXCursor_ClassDecl:
    ParseClass( cursor );
    return CXChildVisit_Continue;
  case CXCursor_ClassTemplate:
    ParseClass( cursor );
    return CXChildVisit_Continue;
  case CXCursor_StructDecl:
    ParseStruct( cursor );
    return CXChildVisit_Continue;
  case CXCursor_EnumDecl:
    ParseEnum( cursor );
    break;
  case CXCursor_TypeAliasDecl:
    ParseDelegate( cursor );
    break;
  default:
    break;
  }

  return CXChildVisit_Continue;
}

void parseHeader( std::filesystem::path filePath, const char** clangArguments, int numClangArguments )
{
  std::cout << "Parsing " << filePath << std::endl;

  std::vector< const char* > args = { "-x", "c++", "-DEASY_MONO_PARSER"};
  for ( int argIx = 0; argIx < numClangArguments; ++argIx )
    args.push_back( clangArguments[ argIx ] );

  CXIndex index = clang_createIndex( 0, 1 );
  CXTranslationUnit unit = clang_parseTranslationUnit( index, filePath.string().data(), args.data(), int( args.size() ), nullptr, 0, CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing );

  if ( !unit )
  {
    std::cerr << "Unable to parse translation unit. Quitting.\n";
    return;
  }

  CXCursor cursor = clang_getTranslationUnitCursor( unit );
  clang_visitChildren( cursor, visitTranslationUnit, nullptr );
}

/////////////////////////////////////////////////////////////
/// Output formatting
/////////////////////////////////////////////////////////////

std::wstring ChangeNamespaceSeparator( const std::wstring ns, const wchar_t* c )
{
  auto t = ns;
  while ( true )
  {
    auto sep = t.find( L"::" );
    if ( sep == std::wstring::npos )
      break;
    t.replace( sep, 2, c );
  }
  return t;
}

std::wstring ExportCSArgumentType( const ArgumentDesc& argDesc )
{
  std::wstring s;

  if ( argDesc.type.kind != TypeDesc::Kind::Class
    && argDesc.type.kind != TypeDesc::Kind::String
    && argDesc.type.kind != TypeDesc::Kind::Delegate )
  {
    if ( argDesc.isLValRef || argDesc.isRValRef || argDesc.isPointer )
      s += L"ref ";

    if ( argDesc.isConst )
      s += L"readonly ";
  }

  s += argDesc.type.csName;

  if ( argDesc.isPointer )
    s += L"?";

  return s;
}

std::wstring ExportCSArgument( const ArgumentDesc& argDesc )
{
  return ExportCSArgumentType( argDesc ) + L" " + argDesc.name;
}

std::wstring ExportCPPArgumentType( const ArgumentDesc& argDesc, bool returnValue )
{
  std::wstring s;

  if ( argDesc.type.kind == TypeDesc::Kind::String )
  {
    s += L"MonoString*";
  }
  else if ( argDesc.type.kind == TypeDesc::Kind::Delegate )
  {
    s += L"MonoObject*";
  }
  else if ( argDesc.type.kind == TypeDesc::Kind::Class )
  {
    if ( argDesc.isPointer || argDesc.isLValRef || argDesc.isRValRef )
      s += L"MonoObject*";
    else
      s += L"ClassesShouldBePointersOrReferences";
  }
  else
  {
    if ( returnValue && argDesc.type.kind == TypeDesc::Kind::Struct )
      s += L"IS<";

    if ( argDesc.isConst )
      s += L"const ";

    s += argDesc.type.cppName;

    if ( argDesc.isLValRef )
      s += L"&";
    if ( argDesc.isRValRef )
      s += L"&&";
    else if ( argDesc.isPointer )
      s += L"*";

    if ( returnValue && argDesc.type.kind == TypeDesc::Kind::Struct )
      s += L">::type";
  }

  return s;
}

std::wstring ExportCPPArgument( const ArgumentDesc& argDesc )
{
  return ExportCPPArgumentType( argDesc, false ) + L" " + argDesc.name;
}

std::wstring ExportBaseName( const ClassDesc& scriptedBase, const wchar_t* scriptedBaseName )
{
  bool isSC = scriptedBase.name == scriptedBaseName;
  return isSC ? L"EasyMono.NativeReference" : ChangeNamespaceSeparator( scriptedBase.name, L"." );
}

std::wstring ExportClassQualifier( const ClassDesc& desc )
{
  return desc.isFinal ? L"sealed " : L"";
}

std::wstring ExportCSMethodArguments( const MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += ExportCSArgument( argDesc ) + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCSDelegateArguments( const DelegateDesc& delegateDesc )
{
  if ( delegateDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : delegateDesc.arguments )
    code += ExportCSArgument( argDesc ) + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportArgCallPrefix( const ArgumentDesc& arg )
{
  return arg.type.kind == TypeDesc::Kind::Struct && ( arg.isPointer || arg.isLValRef || arg.isRValRef ) ? L"in " : L"";
}

std::wstring ExportCSMethodArgumentNames( const MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += ExportArgCallPrefix( argDesc ) + argDesc.name + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCSMethodQualifier( const MethodDesc& methodDesc )
{
  if ( methodDesc.isStatic )
    return L"static ";
  if ( methodDesc.isOverride )
    return L"override ";
  if ( methodDesc.isVirtual )
    return L"virtual ";
  return L"";
}

std::wstring ExportCSMethods( const ClassDesc& desc )
{
  std::wstring code;

  // Write the constructors first
  for ( auto& methodDesc : desc.methods )
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      code += FormatString( csCtorTemplate
                          , JustName( desc.name )
                          , ExportCSMethodArguments( methodDesc )
                          , ExportCSMethodArgumentNames( methodDesc ) );

  for ( auto& methodDesc : desc.methods )
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      code += FormatString( csExportedCtorTemplate
                          , ExportCSMethodArguments( methodDesc ) );
    else
      code += FormatString( csExportedMethodTemplate
                          , ExportCSMethodQualifier( methodDesc )
                          , ExportCSArgumentType( methodDesc.returnValue )
                          , methodDesc.name
                          , ExportCSMethodArguments( methodDesc ) );

  return code;
}

std::wstring ExportStructureFields( const StructDesc& desc, const wchar_t* indentation )
{
  std::wstring code;

  desc.isExported = true;
  for ( auto& field : desc.fields )
    code += indentation + std::wstring() + L"public " + field.first.csName + L" " + field.second + L";\n";

  return code;
}

std::wstring ExportEnumValues( const EnumDesc& desc, const wchar_t* indentation )
{
  std::wstring code;

  desc.isExported = true;
  for ( auto& value : desc.values )
    code += indentation + value.first + L" = " + std::to_wstring( value.second ) + L",\n";

  return code;
}

std::wstring ExportLocalStructures( const ClassDesc& desc )
{
  std::wstring code;

  for ( auto& strukt : structs )
    if ( IsStartsWith( strukt.first, desc.name ) )
      code += FormatString( csLocalStructTemplate, JustName( strukt.second.name ), ExportStructureFields( strukt.second, L"      " ) );
  
  return code;
}

std::wstring ExportLocalEnums( const ClassDesc& desc )
{
  std::wstring code;

  for ( auto& inum : enums )
    if ( IsStartsWith( inum.first, desc.name ) )
      code += FormatString( csLocalEnumTemplate, JustName( inum.second.name ), inum.second.csType, ExportEnumValues( inum.second, L"      " ) );

  return code;
}

void WriteCSClass( const ClassDesc& desc, std::filesystem::path path, const wchar_t* scriptedBaseName )
{
  if ( desc.nameSpace.empty() )
    return;

  auto scriptedBase = GetScriptedBase( desc, scriptedBaseName );
  if ( !scriptedBase )
    return;

  auto fileContents = FormatString( csFileTemplate
                                  , ChangeNamespaceSeparator( desc.nameSpace, L"." )
                                  , ExportClassQualifier( desc )
                                  , JustName( desc.name )
                                  , ExportBaseName( *scriptedBase, scriptedBaseName )
                                  , ExportLocalStructures( desc )
                                  , ExportLocalEnums( desc )
                                  , desc.isFinal ? L"" : L"protected "
                                  , JustName( desc.name )
                                  , ExportCSMethods( desc ) );

  path.append( ChangeNamespaceSeparator( desc.nameSpace, L"/" ) );

  std::filesystem::create_directories( path );

  path.append( JustName( desc.name ) + L".cs" );

  FILE* csFileHandle = nullptr;
  if ( fopen_s( &csFileHandle, path.string().data(), "wt" ) == 0 )
  {
    fwprintf_s( csFileHandle, L"%s", fileContents.data() );
    fclose( csFileHandle );
  }
}

void WriteCSStructs( std::filesystem::path path )
{
  std::unordered_set< std::wstring > namespacesToWrite;
  std::unordered_multimap< std::wstring, StructDesc > structuresToWrite;
  std::unordered_multimap< std::wstring, EnumDesc > enumsToWrite;
  std::unordered_multimap< std::wstring, DelegateDesc > delegatesToWrite;

  for ( auto& strukt : structs )
    if ( !strukt.second.isExported )
    {
      namespacesToWrite.insert( strukt.second.nameSpace );
      structuresToWrite.insert( { strukt.second.nameSpace, strukt.second } );
    }

  for ( auto& inum : enums )
    if ( !inum.second.isExported )
    {
      namespacesToWrite.insert( inum.second.nameSpace );
      enumsToWrite.insert( { inum.second.nameSpace, inum.second } );
    }

  for ( auto& del : delegates )
    if ( !del.second.isExported )
    {
      namespacesToWrite.insert( del.second.nameSpace );
      delegatesToWrite.insert( { del.second.nameSpace, del.second } );
    }

  if ( namespacesToWrite.empty() )
    return;

  std::wstring code = L"using System.Runtime.InteropServices;\n\n";

  for ( auto& nameSpace : namespacesToWrite )
  {
    code += L"namespace " + ChangeNamespaceSeparator( nameSpace, L"." ) + L"\n{\n";

    auto structRange = structuresToWrite.equal_range( nameSpace );
    for ( auto iter = structRange.first; iter != structRange.second; ++iter )
      code += FormatString( csStandaloneStructTemplate, JustName( iter->second.name ), ExportStructureFields( iter->second, L"    " ) );

    auto enumRange = enumsToWrite.equal_range( nameSpace );
    for ( auto iter = enumRange.first; iter != enumRange.second; ++iter )
      code += FormatString( csStandaloneEnumTemplate, JustName( iter->second.name ), iter->second.csType, ExportEnumValues( iter->second, L"    " ) );

    auto delegateRange = delegatesToWrite.equal_range( nameSpace );
    for ( auto iter = delegateRange.first; iter != delegateRange.second; ++iter )
      code += FormatString( csDelegateTemplate
                          , L"  "
                          , ExportCSArgumentType( iter->second.returnValue )
                          , JustName( iter->second.name )
                          , ExportCSDelegateArguments( iter->second ) );

    code += L"}\n\n";
  }

  std::filesystem::create_directories( path );

  path.append( L"CppStructures.cs" );

  FILE* csFileHandle = nullptr;
  if ( fopen_s( &csFileHandle, path.string().data(), "wt" ) == 0 )
  {
    fwprintf_s( csFileHandle, L"%s", code.data() );
    fclose( csFileHandle );
  }
}

std::wstring ExportClassInclude( const ClassDesc& desc )
{
  if ( desc.methods.empty() )
    return L"";

  return L"#include \"" + desc.header + L"\"\n";
}

std::wstring ExportClassGetter( const ClassDesc& desc )
{
  return FormatString( cppClassGetterTemplate
                     , desc.name
                     , ChangeNamespaceSeparator( desc.nameSpace, L"." )
                     , JustName( desc.name ) );
}

std::wstring ExportClassGetters( const wchar_t* scriptedBaseName )
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second, scriptedBaseName ) )
        code += ExportClassGetter( iter.second );
  return code;
}

std::wstring ExportClassIncludes( const wchar_t* scriptedBaseName )
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second, scriptedBaseName ) )
        code += ExportClassInclude( iter.second );
  return code;
}

std::wstring ExportCPPMethodArguments( const MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code = L"";
  for ( auto& argDesc : methodDesc.arguments )
    code += ExportCPPArgument( argDesc ) + L", ";
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportCPPMethodArgumentNames( const MethodDesc& methodDesc )
{
  if ( methodDesc.arguments.empty() )
    return L"";

  std::wstring code;
  for ( auto& argDesc : methodDesc.arguments )
  {
    if ( argDesc.type.kind == TypeDesc::Kind::String )
    {
      code += argDesc.name + L" ? mono_string_chars( " + argDesc.name + L" ) : nullptr";
    }
    else if ( argDesc.type.kind == TypeDesc::Kind::Delegate )
    {
      code += L"std::move( " + argDesc.name + L"__wrapper )";
    }
    else if ( argDesc.type.kind == TypeDesc::Kind::Class )
    {
      if ( argDesc.isLValRef )
        code += L"*";
      else if ( argDesc.isRValRef )
        code += L"*";
      code += L"reinterpret_cast< " + argDesc.type.cppName + L"* >( EasyMono::Detail::LoadNativePointer( " + argDesc.name + L" ) )";
    }
    else
      code += argDesc.name;
    code += L", ";
  }
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportNativeCallPrefix( const MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
    return L"auto monoRetStr =";
  else
    return L"return";
}

std::wstring ExportNativeCallPostfix( const MethodDesc& methodDesc )
{
  return methodDesc.returnValue.type.kind == TypeDesc::Kind::Class ? L"->GetOrCreateMonoObject()" : L"";
}

std::wstring ExportCPPReturnStringHandling( const MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
  {
    return L"\n      return monoRetStr ? mono_string_new_utf16( mono_domain_get(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;";
  }
  return L"";
}

std::wstring ExportReturnStructureHandling( const MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::Struct )
    return L"IS< " + methodDesc.returnValue.type.cppName + L">::process";

  return L"";
}

const DelegateDesc* FindDelegate( const std::wstring& cppName )
{
  auto delegateDesc = delegates.find( cppName );
  return delegateDesc == delegates.end() ? nullptr : &delegateDesc->second;
}

std::wstring ExportDelegateWrapperArgumentValueList( const ArgumentDesc& desc )
{
  std::wstring code;

  auto delegateDesc = FindDelegate( desc.type.cppName );
  assert( delegateDesc );

  if ( delegateDesc->arguments.empty() )
    return code;

  code += L"\n        const void* delegate_args[] =\n        {\n";
  for ( auto& arg : delegateDesc->arguments )
  {
    if ( arg.type.kind == TypeDesc::Kind::String )
      code += L"          " + arg.name + L" ? mono_string_from_utf16( (mono_unichar2*)" + arg.name + L" ) : nullptr,\n";
    else if ( arg.type.kind == TypeDesc::Kind::Class && arg.isPointer )
      code += L"          " + arg.name + L"->GetOrCreateMonoObject(),\n";
    else if ( arg.type.kind == TypeDesc::Kind::Class )
      code += L"          " + arg.name + L".GetOrCreateMonoObject(),\n";
    else
      code += L"          &" + arg.name + L",\n";
  }
  code += L"        };\n";
  return code;
}

std::wstring ExportDelegateWrapperArgumentValuePass( const ArgumentDesc& desc )
{
  auto delegateDesc = FindDelegate( desc.type.cppName );
  assert( delegateDesc );

  return delegateDesc->arguments.empty() ? L"nullptr" : L"(void**)&delegate_args";
}

std::wstring ExportDelegateWrapperHoldReturnValue( const ArgumentDesc& desc )
{
  auto delegateDesc = FindDelegate( desc.type.cppName );
  assert( delegateDesc );

  return delegateDesc->returnValue.type.cppName == L"void" ? L"" : L"MonoObject* delegate_return = ";
}

std::wstring ExportDelegateWrapperPassReturnValue( const ArgumentDesc& desc )
{
  auto delegateDesc = FindDelegate( desc.type.cppName );
  assert( delegateDesc );

  auto& type = delegateDesc->returnValue.type;

  if ( type.cppName == L"void" )
    return L"";
  else if ( type.kind == TypeDesc::Kind::Enum || type.kind == TypeDesc::Kind::Primitive || type.kind == TypeDesc::Kind::Struct )
    return L"\n        return *(int32_t*)mono_object_unbox( delegate_return );";
  else if ( type.kind == TypeDesc::Kind::String )
    return L"\n        return mono_string_chars( (MonoString*)delegate_return );";
  else
    return L"\n        return delegate_return;";
}

std::wstring ExportDelegateWrapperArguments( const ArgumentDesc& desc )
{
  std::wstring code;

  auto delegateDesc = FindDelegate( desc.type.cppName );
  assert( delegateDesc );

  for ( auto& arg : delegateDesc->arguments )
  {
    if ( arg.isConst )
      code += L"const ";
    code += arg.type.cppName;
    if ( arg.isPointer )
      code += L"*";
    if ( arg.isLValRef )
      code += L"&";
    if ( arg.isRValRef )
      code += L"&&";

    code += L" " + arg.name + L", ";
  }
  if ( !code.empty() )
    code.resize( code.size() - 2 );
  return code;
}

std::wstring ExportDelegateWrappers( const MethodDesc& desc )
{
  std::wstring code;
  for ( auto& arg : desc.arguments )
    if ( arg.type.kind == TypeDesc::Kind::Delegate )
      code += FormatString( cppDelegateWrapperTemplate
                          , arg.name
                          , arg.name
                          , ExportDelegateWrapperArguments( arg )
                          , ExportDelegateWrapperArgumentValueList( arg )
                          , ExportDelegateWrapperHoldReturnValue( arg )
                          , ExportDelegateWrapperArgumentValuePass( arg )
                          , ExportDelegateWrapperPassReturnValue( arg ) );
  return code;
}

std::wstring ExportClassBindings( const ClassDesc& desc )
{
  if ( desc.methods.empty() )
    return L"";

  std::wstring code;

  // Write Ctor wrappers first
  for ( auto& methodDesc : desc.methods )
  {
    if ( methodDesc.returnValue.type.kind != TypeDesc::Kind::None )
      continue;

    code += FormatString( cppCtorTemplate
                        , methodDesc.name
                        , ExportCPPMethodArguments( methodDesc )
                        , desc.name
                        , ExportCPPMethodArgumentNames( methodDesc ) );
  }

  for ( auto& methodDesc : desc.methods )
  {
    if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::None )
      continue;

    if ( methodDesc.isStatic )
      code += FormatString( cppStaticMethodTemplate
                          , ExportCPPArgumentType( methodDesc.returnValue, true )
                          , methodDesc.name
                          , ExportCPPMethodArguments( methodDesc )
                          , ExportDelegateWrappers( methodDesc )
                          , ExportNativeCallPrefix( methodDesc )
                          , ExportReturnStructureHandling( methodDesc )
                          , desc.name
                          , methodDesc.name
                          , ExportCPPMethodArgumentNames( methodDesc )
                          , ExportNativeCallPostfix( methodDesc )
                          , ExportCPPReturnStringHandling( methodDesc ) );
    else
      code += FormatString( cppMethodTemplate
                          , ExportCPPArgumentType( methodDesc.returnValue, true )
                          , methodDesc.name
                          , methodDesc.arguments.empty() ? L"" : L", "
                          , ExportCPPMethodArguments( methodDesc )
                          , ExportDelegateWrappers( methodDesc )
                          , desc.name
                          , ExportNativeCallPrefix( methodDesc )
                          , ExportReturnStructureHandling( methodDesc )
                          , methodDesc.name
                          , ExportCPPMethodArgumentNames( methodDesc )
                          , ExportNativeCallPostfix( methodDesc )
                          , ExportCPPReturnStringHandling( methodDesc ) );
  }

  return code;
}

std::wstring ExportClassRegistrationName( const ClassDesc& desc )
{
  return ChangeNamespaceSeparator( desc.nameSpace + L"::" + JustName( desc.name ), L"." );
}

std::wstring ExportClassWrapperName( const ClassDesc& desc )
{
  return ChangeNamespaceSeparator( desc.nameSpace + L"::" + desc.name, L"__" );
}

std::wstring ExportClassMethodRegistration( const ClassDesc& desc, const MethodDesc& methodDesc )
{
  bool isCtor = methodDesc.returnValue.type.kind == TypeDesc::Kind::None;
  return FormatString( cppMethodRegistrationTemplate
                     , ExportClassRegistrationName( desc )
                     , isCtor ? L"Ctor" : methodDesc.name
                     , ExportClassWrapperName( desc )
                     , methodDesc.name );
}

std::wstring ExportClassMethodRegistrations( const ClassDesc& desc )
{
  std::wstring code;
  for ( auto& methodDesc : desc.methods )
    code += ExportClassMethodRegistration( desc, methodDesc );
  return code;
}

std::wstring ExportClassMethods( const wchar_t* scriptedBaseName )
{
  std::wstring code;
  for ( auto& iter : classes )
    if ( !iter.second.nameSpace.empty() )
      if ( GetScriptedBase( iter.second, scriptedBaseName ) )
        code += FormatString( cppClassTemplate
                            , ExportClassWrapperName( iter.second )
                            , ExportClassBindings( iter.second )
                            , ExportClassMethodRegistrations( iter.second ) );

  return code;
}

void WriteCPPBindings( std::filesystem::path path, const wchar_t* scriptedBaseName )
{
  std::wstring code = FormatString( cppFileTemplate
                                  , ExportClassIncludes( scriptedBaseName )
                                  , ExportClassMethods( scriptedBaseName )
                                  , ExportClassGetters( scriptedBaseName ) );

  auto folder = path;
  folder.remove_filename();
  std::filesystem::create_directories( folder );

  FILE* cppFileHandle = nullptr;
  if ( fopen_s( &cppFileHandle, path.string().data(), "wt" ) == 0 )
  {
    fwprintf_s( cppFileHandle, L"%s", code.data() );
    fclose( cppFileHandle );
  }
}

bool ParseDictionary( const char* path )
{
  FILE* file = nullptr;
  if ( fopen_s( &file, path, "rt" ) )
    return false;

  fseek( file, 0, SEEK_END );
  auto fileSize = ftell( file );
  fseek( file, 0, SEEK_SET );

  std::string fileData( fileSize + 1, ' ' );

  fread_s( fileData.data(), fileData.size(), 1, fileSize, file );

  auto split = []( const std::string & s ) -> std::vector< std::string >
  {
    std::regex regex{ R"([\s\t\n\r,;]+)" };
    std::sregex_token_iterator it{ s.begin(), s.end(), regex, -1 };
    return { it, {} };
  };

  auto tokens = split( fileData );

  if ( tokens.size() % 2 )
    return false;

  for ( auto iter = tokens.begin(); iter != tokens.end(); iter += 2 )
    knownStructures.emplace_back( KnownStructure{ .nativeName = W( iter[ 0 ] ), .managedName = W( iter[ 1 ] ) } );

  return true;
}

int main( int argc, char* argv[] )
{
  if ( argc < 6 )
  {
    std::cout << "Invalid number of parameters." << std::endl << std::endl;
    std::cout << "For parsing a single header file:" << std::endl;
    std::cout << "CStoCPPInteropGenerator.exe HeaderFilePath GeneratedCppFilePath GeneratedCSFolderPath DictionaryPath ScriptedBaseName CompilerArgs..." << std::endl << std::endl;
    std::cout << "For parsing header files in a folder recursively:" << std::endl;
    std::cout << "CStoCPPInteropGenerator.exe SourceFolderPath GeneratedCppFilePath GeneratedCSFolderPath DictionaryPath ScriptedBaseName CompilerArgs..." << std::endl;
    return -1;
  }

  std::filesystem::path source = argv[ 1 ];
  std::filesystem::path cppDestination = argv[ 2 ];
  std::filesystem::path csDestination = argv[ 3 ];
  std::filesystem::path dictionary = argv[ 4 ];
  
  auto scriptedBaseName = W( argv[ 5 ] );

  if ( std::filesystem::exists( cppDestination ) && std::filesystem::is_directory( cppDestination ) )
  {
    std::cout << "The GeneratedCppFilePath should not be a folder!" << std::endl;
    return -1;
  }

  if ( std::filesystem::exists( csDestination ) && !std::filesystem::is_directory( csDestination ) )
  {
    std::cout << "The GeneratedCSFolderPath should not be a file!" << std::endl;
    return -1;
  }

  if ( std::filesystem::exists( dictionary ) && std::filesystem::is_directory( dictionary ) )
  {
    std::cout << "The DictionaryPath should not be a folder!" << std::endl;
    return -1;
  }

  if ( !ParseDictionary( dictionary.string().data()) )
  {
    std::cout << "Parsing the dictionary failed!" << std::endl;
    return -1;
  }

  if ( std::filesystem::is_directory( source ) )
  {
    for ( auto& entry : std::filesystem::recursive_directory_iterator( source ) )
      if ( entry.is_regular_file() && entry.path().has_extension() && entry.path().extension() == ".h" )
        parseHeader( entry.path(), (const char**)&argv[ 6 ], argc - 6 );
  }
  else
    parseHeader( source, (const char**)&argv[ 6 ], argc - 6 );

  for ( auto& iter : classes )
    WriteCSClass( iter.second, csDestination, scriptedBaseName.data() );

  WriteCSStructs( csDestination );

  WriteCPPBindings( cppDestination, scriptedBaseName.data() );

  return 0;
}
