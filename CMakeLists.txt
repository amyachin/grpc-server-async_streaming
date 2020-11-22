cmake_minimum_required(VERSION 3.5.1)
project(helloworld-streaming VERSION 0.1.0)

if(NOT MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  add_definitions(-D_WIN32_WINNT=0x600)
endif()

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

find_package(Threads)
 message(STATUS "Using threading library ${CMAKE_THREAD_LIBS_INIT}")

# find_package(TBB CONFIG REQUIRED)
#  message(STATUS "Using TBB ${TBB_VERSION}")


# Find Protobuf installation
  # Looks for protobuf-config.cmake file installed by Protobuf's cmake installation.
  set(protobuf_MODULE_COMPATIBLE TRUE)
  find_package(Protobuf CONFIG REQUIRED)
  message(STATUS "Using protobuf ${Protobuf_VERSION}")

  set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
  set(_REFLECTION gRPC::grpc++_reflection)
  if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
  else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  endif()

  # Find gRPC installation
  # Looks for gRPCConfig.cmake file installed by gRPC's cmake installation.
  find_package(gRPC CONFIG REQUIRED)
  message(STATUS "Using gRPC ${gRPC_VERSION}")

  set(_GRPC_GRPCPP gRPC::grpc++)
  if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
  else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  endif()



# Proto file (hellowstreaming world)
get_filename_component(hw_proto "./protos/hellostreamingworld.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Proto file (chatroom)
get_filename_component(cr_proto "./protos/chatroom.proto" ABSOLUTE)
get_filename_component(cr_proto_path "${cr_proto}" PATH)


# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/hellostreamingworld.grpc.pb.h")

set(cr_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/chatroom.pb.cc")
set(cr_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chatroom.pb.h")
set(cr_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/chatroom.grpc.pb.cc")
set(cr_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/chatroom.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")


add_custom_command(
      OUTPUT "${cr_proto_srcs}" "${cr_proto_hdrs}" "${cr_grpc_srcs}" "${cr_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${cr_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${cr_proto}"
      DEPENDS "${cr_proto}")



# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_executable(helloworld-streaming-client "multi_greeter_client.cpp"
    ${hw_proto_srcs}
    ${hw_grpc_srcs})


target_link_libraries(helloworld-streaming-client
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})


add_executable(helloworld-streaming-server "multi_greeter_server.cpp" "multi_greeter_service.cpp"
    ${hw_proto_srcs}
    ${hw_grpc_srcs})

target_link_libraries(helloworld-streaming-server
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})

add_executable(chatroom-server "chatroom_server.cpp"
    ${cr_proto_srcs}
    ${cr_grpc_srcs})


target_link_libraries(chatroom-server
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
