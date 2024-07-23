#include <WebGPUlib/VertexBuffer.hpp>

#include <utility>

using namespace WebGPUlib;

VertexBuffer::VertexBuffer( WGPUBuffer&& _buffer, std::size_t _vertexCount)  // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
: Buffer( std::move(_buffer) )  // NOLINT(performance-move-const-arg)
, vertexCount( _vertexCount )
{}