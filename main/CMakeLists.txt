idf_component_register(
	
	SRCS
	"main.c"
	"aoo/aoo/src/timer.cpp"
	"aoo/aoo/src/source.cpp"
	"aoo/aoo/src/sink.cpp"
	"aoo/aoo/src/resampler.cpp"
	"aoo/aoo/src/buffer.cpp"
	"aoo/aoo/src/aoo.cpp"
	"aoo/aoo/src/codec/pcm.cpp"
#	"aoo/aoo/src/codec/opus.cpp"
	"aoo/aoo/src/net/server.cpp"
	"aoo/aoo/src/net/client.cpp"

	"aoo/deps/oscpack/osc/OscTypes.cpp"
	"aoo/deps/oscpack/osc/OscReceivedElements.cpp"
	"aoo/deps/oscpack/osc/OscPrintReceivedElements.cpp"
	"aoo/deps/oscpack/osc/OscOutboundPacketStream.cpp"

	INCLUDE_DIRS
	"aoo/include/aoo"
	"aoo/aoo/src"
	"aoo/common"
	"aoo"
	"aoo/deps/oscpack/osc"
	"aoo/deps"
)


# Build static library, do not build test executables
#option(BUILD_SHARED_LIBS OFF)
#option(BUILD_TESTING OFF)

#set(AOO_BUILD_TEST_SUITE OFF CACHE BOOL "build test suite")
set(AOO_CODEC_OPUS OFF CACHE BOOL "use Opus codec")

include_directories(
	"aoo/include"
	"aoo/common"
	"aoo"
)

#add_subdirectory("aoo/aoo")

#add_library(aoo STATIC aoo/include/aoo/aoo.h aoo/aoo/src/timer.cpp aoo/aoo/src/source.cpp aoo/aoo/src/sink.cpp aoo/aoo/src/resampler.cpp aoo/aoo/src/buffer.cpp aoo/aoo/src/aoo.cpp aoo/aoo/src/codec/pcm.cpp aoo/aoo/src/codec/opus.cpp aoo/aoo/src/net/server.cpp aoo/aoo/src/net/client.cpp aoo/include/aoo/aoo.h aoo/include/aoo/aoo_source.hpp aoo/include/aoo/aoo_sink.hpp)

#target_link_libraries(${COMPONENT_LIB} PUBLIC aoo)

