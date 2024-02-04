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
    @( ulong thiz ) : base( thiz )
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
    public extern @ @( @ );
)";

auto csGetterSetterTemplate = LR"(
    public @ @
    {
      get => @();
      set => @( @value );
    }
)";

auto csGetterTemplate = LR"(
    public @ @ => @();
)";

auto csSetterTemplate = LR"(
    public @ @ { set => @( @value ); }
)";

auto cppFileTemplate = LR"(
#include <mono/metadata/loader.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/appdomain.h>

@
namespace EasyMono
{
  ScriptedClass* LoadNativePointer( MonoObject* monoObject );

  namespace Detail
  {
    MonoImage* GetMainMonoImage();

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
using IS = ::EasyMono::Detail::interop_struct< T, Enable >;

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
    {
      auto nativeThis = reinterpret_cast< @* >( EasyMono::LoadNativePointer( thiz ) );
      @ @( nativeThis->@( @ )@ );@
    }
)";

auto cppMethodRegistrationTemplate = LR"(
  mono_add_internal_call( "@::@", @::@ );)";

auto cppStringArgTranslationTemplate = LR"(
      @ @CStr = @ ? @( mono_string_chars( @ ) ) : @();
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
  };

  Kind kind;
  std::wstring name;
  std::wstring csSpecificName;
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

struct ClassDesc
{
  struct MethodDesc
  {
    struct ArgumentDesc
    {
      std::wstring name;
      TypeDesc type;
      bool isConst;
      bool isReference;
      bool isPointer;
    };

    std::wstring name;
    ArgumentDesc returnValue;
    std::vector< ArgumentDesc > arguments;
  };

  std::wstring nameSpace;
  std::wstring name;

  std::wstring header;

  bool isFinal;

  std::vector< MethodDesc > methods;

  std::vector< std::wstring > baseClasses;
};

struct EnumDesc
{
  std::wstring type;
  std::wstring name;
  std::wstring nameSpace;
  std::wstring header;
  std::vector< std::pair< std::wstring, int64_t > > values;
  mutable bool isExported = false;
};

std::unordered_map< std::wstring, ClassDesc > classes;
std::unordered_map< std::wstring, StructDesc > structs;
std::unordered_map< std::wstring, EnumDesc > enums;

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

std::wstring GetFullName( CXCursor cursor )
{
  if ( cursor.kind == CXCursor_TranslationUnit )
    return L"";

  if ( cursor.kind == CXCursor_ClassDecl || cursor.kind == CXCursor_ClassTemplate || cursor.kind == CXCursor_StructDecl || cursor.kind == CXCursor_Namespace || cursor.kind == CXCursor_EnumDecl )
    return GetFullName( clang_getCursorSemanticParent( cursor ) ) + L"::" + ToString( cursor );

  return L"";
}

std::wstring GetFullName( CXType type )
{
  return GetFullName( clang_getTypeDeclaration( clang_getCanonicalType( type ) ) );
}

bool Contains( const std::wstring& s, const wchar_t* sub )
{
  return s.find( sub ) != std::string::npos;
}

void TranslateKnownStructureNames( TypeDesc& arg )
{
  if ( arg.kind == TypeDesc::Kind::String )
    arg.csSpecificName = L"string";
  else
  {
    auto iter = std::find_if( knownStructures.begin(), knownStructures.end(), [ & ]( const KnownStructure& s ) { return arg.name == s.nativeName; } );
    if ( iter != knownStructures.end() )
      arg.csSpecificName = iter->managedName;
  }
}

ClassDesc::MethodDesc::ArgumentDesc ParsePrimitiveArgument( const wchar_t* name, bool isConst, bool isPointer, bool isReference )
{
  ClassDesc::MethodDesc::ArgumentDesc arg;
  arg.type.kind   = TypeDesc::Kind::Primitive;
  arg.type.name   = name;
  arg.isConst     = isConst;
  arg.isPointer   = false;
  arg.isReference = false;
  return arg;
}

TypeDesc::Kind GetTypeKind( CXType type )
{
  if ( type.kind == CXType_WChar )
    return TypeDesc::Kind::String;

  auto decl = clang_getTypeDeclaration( clang_getCanonicalType( type ) );
  switch ( decl.kind )
  {
  case CXCursor_ClassDecl: return TypeDesc::Kind::Class;
  case CXCursor_StructDecl: return TypeDesc::Kind::Struct;
  default: return TypeDesc::Kind::Primitive;
  }
}

ClassDesc::MethodDesc::ArgumentDesc ParseArgument( CXType type, bool isConst )
{
  switch ( type.kind )
  {
  case CXType_Void: return ParsePrimitiveArgument( L"void", isConst, false, false );
  case CXType_Bool: return ParsePrimitiveArgument( L"bool", isConst, false, false );
  case CXType_Char_S: return ParsePrimitiveArgument( L"char", isConst, false, false );
  case CXType_UChar: return ParsePrimitiveArgument( L"byte", isConst, false, false );
  case CXType_UShort: return ParsePrimitiveArgument( L"ushort", isConst, false, false );
  case CXType_UInt: return ParsePrimitiveArgument( L"uint", isConst, false, false );
  case CXType_ULong: return ParsePrimitiveArgument( L"uint", isConst, false, false );
  case CXType_ULongLong: return ParsePrimitiveArgument( L"ulong", isConst, false, false );
  case CXType_SChar: return ParsePrimitiveArgument( L"sbyte", isConst, false, false );
  case CXType_Short: return ParsePrimitiveArgument( L"short", isConst, false, false );
  case CXType_Int: return ParsePrimitiveArgument( L"int", isConst, false, false );
  case CXType_Long: return ParsePrimitiveArgument( L"int", isConst, false, false );
  case CXType_LongLong: return ParsePrimitiveArgument( L"long", isConst, false, false );
  case CXType_Float: return ParsePrimitiveArgument( L"float", isConst, false, false );
  case CXType_Double: return ParsePrimitiveArgument( L"double", isConst, false, false );
  case CXType_Pointer:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_getPointeeType( type ) );
    arg.type.name   = GetFullName( clang_getPointeeType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = true;
    arg.isReference = false;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_LValueReference:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_getNonReferenceType( type ) );
    arg.type.name   = GetFullName( clang_getNonReferenceType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = false;
    arg.isReference = true;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_Elaborated:
  {
    ClassDesc::MethodDesc::ArgumentDesc arg;
    arg.type.kind   = GetTypeKind( clang_Type_getNamedType( type ) );
    arg.type.name   = GetFullName( clang_Type_getNamedType( type ) );
    arg.isConst     = isConst;
    arg.isPointer   = false;
    arg.isReference = false;
    TranslateKnownStructureNames( arg.type );
    return arg;
  }
  case CXType_Typedef:
    return ParseArgument( clang_getCanonicalType( type ), isConst);
  }

  ClassDesc::MethodDesc::ArgumentDesc arg;
  arg.type.kind   = TypeDesc::Kind::Unknown;
  arg.type.name   = L"unknown";
  arg.isConst     = false;
  arg.isPointer   = false;
  arg.isReference = false;
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

  auto name = GetFullName( cursor );
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
          auto asArg = ParseArgument( type, false );
          if ( asArg.isPointer || asArg.isReference )
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

ClassDesc::MethodDesc ParseMethod( CXCursor cursor )
{
  ClassDesc::MethodDesc desc;

  desc.name = ToString( cursor );

  bool isCtor = clang_getCursorKind( cursor ) == CXCursor_Constructor;

  if ( isCtor )
  {
    desc.returnValue.type.kind = TypeDesc::Kind::None;
  }
  else
  {
    auto retType = clang_getCursorResultType( cursor );
    auto retIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( retType ) );
    desc.returnValue = ParseArgument( retType, retIsConst );
  }

  int numArgs = clang_Cursor_getNumArguments( cursor );
  for ( int arg = 0; arg < numArgs; ++arg )
  {
    auto argCursor = clang_Cursor_getArgument( cursor, arg );
    auto argType = clang_getCursorType( argCursor );
    auto argIsConst = clang_isConstQualifiedType( clang_getNonReferenceType( argType ) );
    desc.arguments.emplace_back( ParseArgument( argType, argIsConst ) );
    desc.arguments.back().name = ToString( argCursor );
  }

  return desc;
}

void ParseEnum( CXCursor cursor )
{
  if ( IsInIgnoredNamespace() )
    return;

  auto name = GetFullName( cursor );

  if ( name.find( L' ' ) != name.npos )
    return;

  auto key = name;

  std::wstring nameSpace = GetStackNamespace();

  auto iter = enums.find( key );
  if ( iter != enums.end() )
    return;

  auto& inum = enums[ key ];

  auto enumType = clang_getEnumDeclIntegerType( cursor );

  inum.type = ParseArgument( enumType.kind == CXTypeKind::CXType_Elaborated ? clang_Type_getNamedType( enumType ) : enumType, false ).type.name;
  inum.name = name;
  inum.nameSpace = nameSpace;
  inum.header = ParseHeader( cursor );

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

  auto name = GetFullName( cursor );
  auto key = name;

  auto iter = classes.find( key );
  if ( iter != classes.end() )
    return;

  auto& klass = classes[ key ];

  klass.name = name;
  klass.nameSpace = nameSpace;
  klass.header = ParseHeader( cursor );

  struct ClassContext
  {
    ClassDesc* klass;
    bool isPublic = false;
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
      ctx.isPublic = clang_getCXXAccessSpecifier( cursor ) == CX_CXXPublic;
      break;
    case CXCursor_CXXMethod:
    case CXCursor_Constructor:
      if ( ctx.isPublic )
        ctx.klass->methods.emplace_back( ParseMethod( cursor ) );
      break;
    case CXCursor_CXXFinalAttr:
      ctx.klass->isFinal = true;
      break;
    case CXCursor_CXXBaseSpecifier:
      ctx.klass->baseClasses.emplace_back( GetFullName( clang_getTypeDeclaration( clang_getCursorType( cursor ) ) ) );
      break;
    case CXCursor_StructDecl:
      ParseStruct( cursor );
      return CXChildVisit_Continue;
    case CXCursor_EnumDecl:
      ParseEnum( cursor );
      return CXChildVisit_Continue;
    default:
      break;
    }

    return CXChildVisit_Continue;
  }, &ctx );
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
  default:
    break;
  }

  return CXChildVisit_Continue;
}

void parseHeader( std::filesystem::path filePath, const char** clangArguments, int numClangArguments )
{
  std::cout << "Parsing " << filePath << std::endl;

  std::vector< const char* > args = { "-x", "c++" };
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

std::wstring ExportCSTypeName( const std::wstring& name )
{
  auto csName = name;
  if ( csName.size() > 1 && csName[ 0 ] == L':' && csName[ 1 ] == L':' )
    csName = csName.substr( 2 );

  return ChangeNamespaceSeparator( csName, L"." );
}

std::wstring ExportCSArgumentType( const ClassDesc::MethodDesc::ArgumentDesc& argDesc, bool forAttribute = false )
{
  std::wstring s;

  if ( argDesc.type.kind != TypeDesc::Kind::Class && argDesc.type.kind != TypeDesc::Kind::String && !forAttribute )
  {
    if ( argDesc.isReference || argDesc.isPointer )
      s += L"ref ";

    if ( argDesc.isConst )
      s += L"readonly ";
  }

  s += argDesc.type.csSpecificName.empty() ? ExportCSTypeName( argDesc.type.name ) : argDesc.type.csSpecificName;

  if ( argDesc.isPointer )
    s += L"?";

  return s;
}

std::wstring ExportCSArgument( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  return ExportCSArgumentType( argDesc ) + L" " + argDesc.name;
}

std::wstring ExportCPPArgumentType( const ClassDesc::MethodDesc::ArgumentDesc& argDesc, bool returnValue )
{
  std::wstring s;

  if ( argDesc.type.kind == TypeDesc::Kind::String )
  {
    s += L"MonoString*";
  }
  else if ( argDesc.type.kind == TypeDesc::Kind::Class )
  {
    if ( argDesc.isPointer || argDesc.isReference )
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

    s += argDesc.type.name;

    if ( argDesc.isReference )
      s += L"&";
    else if ( argDesc.isPointer )
      s += L"*";

    if ( returnValue && argDesc.type.kind == TypeDesc::Kind::Struct )
      s += L">::type";
  }

  return s;
}

std::wstring ExportCPPArgument( const ClassDesc::MethodDesc::ArgumentDesc& argDesc )
{
  return ExportCPPArgumentType( argDesc, false ) + L" " + argDesc.name;
}

std::wstring ExportBaseName( const ClassDesc& scriptedBase, const wchar_t* scriptedBaseName )
{
  bool isSC = scriptedBase.name == scriptedBaseName;
  return isSC ? L"EasyMono.NativeReference" : ( L"public " + scriptedBase.nameSpace + L"." + scriptedBase.name );
}

std::wstring ExportClassQualifier( const ClassDesc& desc )
{
  return desc.isFinal ? L"sealed " : L"";
}

std::wstring ExportCSMethodArguments( const ClassDesc::MethodDesc& methodDesc )
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

std::wstring ExportArgCallPrefix( const ClassDesc::MethodDesc::ArgumentDesc& arg )
{
  return arg.type.kind == TypeDesc::Kind::Struct && ( arg.isPointer || arg.isReference ) ? L"in " : L"";
}

std::wstring ExportCSMethodArgumentNames( const ClassDesc::MethodDesc& methodDesc )
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
    code += indentation + std::wstring() + L"public " + field.first.name + L" " + field.second + L";\n";

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
      code += FormatString( csLocalEnumTemplate, JustName( inum.second.name ), inum.second.type, ExportEnumValues( inum.second, L"      " ) );

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

  if ( namespacesToWrite.empty() )
    return;

  std::wstring code = L"using System.Runtime.InteropServices;\n\n";

  for ( auto& nameSpace : namespacesToWrite )
  {
    code += L"namespace " + ChangeNamespaceSeparator( nameSpace, L"." ) + L"\n{\n";

    auto structRange = structuresToWrite.equal_range( nameSpace );
    if ( structRange.first != structRange.second )
      for ( auto iter = structRange.first; iter != structRange.second; ++iter )
        code += FormatString( csStandaloneStructTemplate, JustName( iter->second.name ), ExportStructureFields( iter->second, L"    " ) );

    auto enumRange = enumsToWrite.equal_range( nameSpace );
    if ( enumRange.first != enumRange.second )
      for ( auto iter = enumRange.first; iter != enumRange.second; ++iter )
        code += FormatString( csStandaloneEnumTemplate, JustName( iter->second.name ), iter->second.type, ExportEnumValues( iter->second, L"    " ) );

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

std::wstring ExportCPPMethodArguments( const ClassDesc::MethodDesc& methodDesc )
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

std::wstring ExportCPPMethodArgumentNames( const ClassDesc::MethodDesc& methodDesc )
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
    else if ( argDesc.type.kind == TypeDesc::Kind::Class )
    {
      if ( argDesc.isReference )
        code += L"*";
      code += L"reinterpret_cast< " + argDesc.type.name + L"* >( EasyMono::LoadNativePointer( " + argDesc.name + L" ) )";
    }
    else
      code += argDesc.name;
    code += L", ";
  }
  code.pop_back();
  code.pop_back();
  return code;
}

std::wstring ExportNativeThisPrefix( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
    return L"auto monoRetStr =";
  else
    return L"return";
}

std::wstring ExportNativeThisPostfix( const ClassDesc::MethodDesc& methodDesc )
{
  return methodDesc.returnValue.type.kind == TypeDesc::Kind::Class ? L"->GetOrCreateMonoObject()" : L"";
}

std::wstring ExportCPPReturnStringHandling( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::String )
  {
    return L"\n      return monoRetStr ? mono_string_new_utf16( mono_get_root_domain(), monoRetStr, int( wcslen( monoRetStr ) ) ) : nullptr;";
  }
  return L"";
}

std::wstring ExportReturnStructureHandling( const ClassDesc::MethodDesc& methodDesc )
{
  if ( methodDesc.returnValue.type.kind == TypeDesc::Kind::Struct )
    return L"IS< " + methodDesc.returnValue.type.name + L">::process";

  return L"";
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

    code += FormatString( cppMethodTemplate
                        , ExportCPPArgumentType( methodDesc.returnValue, true )
                        , methodDesc.name
                        , methodDesc.arguments.empty() ? L"" : L", "
                        , ExportCPPMethodArguments( methodDesc )
                        , desc.name
                        , ExportNativeThisPrefix( methodDesc )
                        , ExportReturnStructureHandling( methodDesc )
                        , methodDesc.name
                        , ExportCPPMethodArgumentNames( methodDesc )
                        , ExportNativeThisPostfix( methodDesc )
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

std::wstring ExportClassMethodRegistration( const ClassDesc& desc, const ClassDesc::MethodDesc& methodDesc )
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
