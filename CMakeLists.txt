cmake_minimum_required( VERSION 3.8.2 )

set( CPP_STRING_TYPE "std::wstring" CACHE STRING "Specify the type which should be used for strings in the interop. Need to have an interface compatible with std::wstring!" )
set( LLVM_INSTALL_PATH "c:/Program Files/LLVM" CACHE STRING "Specify the path where the LLVM SDK is installed." )
set( MONO_INSTALL_PATH "C:/Program Files/Mono" CACHE STRING "Specify the path where the Mono SDK is installed." )

#######################################################
# CPP bindings generator
#######################################################

project( CPPtoCSInteropGenerator VERSION 0.1.0 LANGUAGES CSharp )

SET( CPPtoCSInteropGenerator_SRC CPPtoCSInteropGenerator/CSClassParser.cs )

add_executable( CPPtoCSInteropGenerator ${CPPtoCSInteropGenerator_SRC} )

set_target_properties( CPPtoCSInteropGenerator PROPERTIES DOTNET_SDK "Microsoft.NET.Sdk"
                                                          DOTNET_TARGET_FRAMEWORK "net8.0" )

#######################################################
# CS bindings generator
#######################################################

project( CStoCPPInteropGenerator )

SET( CStoCPPInteropGenerator_SRC CStoCPPInteropGenerator/CPPHeaderParser.cpp )

add_executable( CStoCPPInteropGenerator ${CStoCPPInteropGenerator_SRC} )
target_compile_definitions( CStoCPPInteropGenerator PRIVATE CPP_STRING_TYPE=L\"${CPP_STRING_TYPE}\" )
target_include_directories( CStoCPPInteropGenerator PRIVATE ${LLVM_INSTALL_PATH}/include )
target_compile_features( CStoCPPInteropGenerator PRIVATE cxx_std_20 )
target_link_libraries( CStoCPPInteropGenerator PRIVATE ${LLVM_INSTALL_PATH}/lib/libclang.lib )

