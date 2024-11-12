#ifdef __EMSCRIPTEN__
    #include <emscripten/html5.h>
#endif

#define SDL_MAIN_HANDLED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>
#include <sdl2webgpu.h>
#include <webgpu/webgpu.h>

#include <Timer.hpp>

#ifdef WEBGPU_BACKEND_WGPU
    #include <webgpu/wgpu.h>  // Include non-standard functions.
#endif

#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

std::map<WGPUFeatureName, std::string> featureNames = {
    { WGPUFeatureName_DepthClipControl, "DepthClipControl" },
    { WGPUFeatureName_Depth32FloatStencil8, "Depth32FloatStencil8" },
    { WGPUFeatureName_TimestampQuery, "TimestampQuery" },
    { WGPUFeatureName_TextureCompressionBC, "TextureCompressionBC" },
    { WGPUFeatureName_TextureCompressionETC2, "TextureCompressionETC2" },
    { WGPUFeatureName_TextureCompressionASTC, "TextureCompressionASTC" },
    { WGPUFeatureName_IndirectFirstInstance, "IndirectFirstInstance" },
    { WGPUFeatureName_ShaderF16, "ShaderF16" },
    { WGPUFeatureName_RG11B10UfloatRenderable, "RG11B10UfloatRenderable" },
    { WGPUFeatureName_BGRA8UnormStorage, "BGRA8UnormStorage" },
    { WGPUFeatureName_Float32Filterable, "Float32Filterable" },
};

std::map<WGPUQueueWorkDoneStatus, std::string> queueWorkDoneStatusNames = {
    { WGPUQueueWorkDoneStatus_Success, "Success" },
    { WGPUQueueWorkDoneStatus_Error, "Error" },
    { WGPUQueueWorkDoneStatus_Unknown, "Unknown" },
    { WGPUQueueWorkDoneStatus_DeviceLost, "DeviceLost" },
};

std::map<WGPUBackendType, std::string> backendTypes = {
    { WGPUBackendType_Undefined, "Undefined" }, { WGPUBackendType_Null, "Null" },
    { WGPUBackendType_WebGPU, "WebGPU" },       { WGPUBackendType_D3D11, "D3D11" },
    { WGPUBackendType_D3D12, "D3D12" },         { WGPUBackendType_Metal, "Metal" },
    { WGPUBackendType_Vulkan, "Vulkan" },       { WGPUBackendType_OpenGL, "OpenGL" },
    { WGPUBackendType_OpenGLES, "OpenGLES" },
};

constexpr int WINDOW_WIDTH  = 1280;
constexpr int WINDOW_HEIGHT = 720;
const char*   WINDOW_TITLE  = "Masterclass";
const char*   SHADER_MODULE = {
    #include "shader.wgsl"  //< eww
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
};

static Vertex vertices[8] = {
    { { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, 0.0f } },  // 0
    { { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },   // 1
    { { 1.0f, 1.0f, -1.0f }, { 1.0f, 1.0f, 0.0f } },    // 2
    { { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },   // 3
    { { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },   // 4
    { { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 1.0f } },    // 5
    { { 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f } },     // 6
    { { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 1.0f } }     // 7
};

static uint16_t indices[36] = { 0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0,
                                3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7 };

SDL_Window* window = nullptr;
Timer       timer;

WGPUInstance             instance = nullptr;
WGPUDevice               device   = nullptr;
WGPUQueue                queue    = nullptr;
WGPUSurface              surface  = nullptr;
WGPUSurfaceConfiguration surfaceConfiguration {};

WGPUTexture     depthStencilTexture     = nullptr;
WGPUTextureView depthStencilTextureView = nullptr;

WGPUBuffer vertexBuffer = nullptr;
WGPUBuffer indexBuffer  = nullptr;
WGPUBuffer uniformBuffer  = nullptr;

WGPUBindGroupLayout bindGroupLayout = nullptr;
WGPUPipelineLayout  pipelineLayout  = nullptr;
WGPURenderPipeline  renderPipeline  = nullptr;

bool isRunning = true;

void init();
void resize();
void update(void* pUserData = nullptr);
void render();
void destroy();

bool getAdapterLimits( WGPUAdapter adapter, WGPUSupportedLimits& supportedLimits )
{
#ifdef WEBGPU_BACKEND_DAWN
    // Dawn returns a status flag instead of WGPUBool.
    return wgpuAdapterGetLimits( adapter, &supportedLimits ) == WGPUStatus_Success;
#else
    return wgpuAdapterGetLimits( adapter, &supportedLimits );
#endif
}

void inspectAdapter( WGPUAdapter adapter )
{
    // List the adapter properties.
    // Note: Deprecated, use WGPUAdapterInfo instead (when it's supported on all backends).
    WGPUAdapterProperties adapterProperties {};
    wgpuAdapterGetProperties( adapter, &adapterProperties );

    std::cout << "Adapter name:   " << adapterProperties.name << std::endl;
    std::cout << "Adapter vendor: " << adapterProperties.vendorName << std::endl;
    std::cout << "Backend type:   " << backendTypes[adapterProperties.backendType] << std::endl;

    // List adapter limits.
    WGPUSupportedLimits supportedLimits {};
    if ( getAdapterLimits( adapter, supportedLimits ) )
    {
        WGPULimits limits = supportedLimits.limits;
        std::cout << "Limits: " << std::endl;
        std::cout << "  maxTextureDimension1D: " << limits.maxTextureDimension1D << std::endl;
        std::cout << "  maxTextureDimension2D: " << limits.maxTextureDimension2D << std::endl;
        std::cout << "  maxTextureDimension3D: " << limits.maxTextureDimension3D << std::endl;
        std::cout << "  maxTextureArrayLayers: " << limits.maxTextureArrayLayers << std::endl;
        std::cout << "  maxBindGroups: " << limits.maxBindGroups << std::endl;
        std::cout << "  maxBindGroupsPlusVertexBuffers: " << limits.maxBindGroupsPlusVertexBuffers << std::endl;
        std::cout << "  maxBindingsPerBindGroup: " << limits.maxBindingsPerBindGroup << std::endl;
        std::cout << "  maxDynamicUniformBuffersPerPipelineLayout: " << limits.maxDynamicUniformBuffersPerPipelineLayout
                  << std::endl;
        std::cout << "  maxDynamicStorageBuffersPerPipelineLayout: " << limits.maxDynamicStorageBuffersPerPipelineLayout
                  << std::endl;
        std::cout << "  maxSampledTexturesPerShaderStage: " << limits.maxSampledTexturesPerShaderStage << std::endl;
        std::cout << "  maxSamplersPerShaderStage:        " << limits.maxSamplersPerShaderStage << std::endl;
        std::cout << "  maxStorageBuffersPerShaderStage:  " << limits.maxStorageBuffersPerShaderStage << std::endl;
        std::cout << "  maxStorageTexturesPerShaderStage: " << limits.maxStorageTexturesPerShaderStage << std::endl;
        std::cout << "  maxUniformBuffersPerShaderStage:  " << limits.maxUniformBuffersPerShaderStage << std::endl;
        std::cout << "  maxUniformBufferBindingSize:      " << limits.maxUniformBufferBindingSize << std::endl;
        std::cout << "  maxStorageBufferBindingSize:      " << limits.maxStorageBufferBindingSize << std::endl;
        std::cout << "  minUniformBufferOffsetAlignment:  " << limits.minUniformBufferOffsetAlignment << std::endl;
        std::cout << "  minStorageBufferOffsetAlignment:  " << limits.minStorageBufferOffsetAlignment << std::endl;
        std::cout << "  maxVertexBuffers:                 " << limits.maxVertexBuffers << std::endl;
        std::cout << "  maxBufferSize:                    " << limits.maxBufferSize << std::endl;
        std::cout << "  maxVertexAttributes:              " << limits.maxVertexAttributes << std::endl;
        std::cout << "  maxVertexBufferArrayStride:       " << limits.maxVertexBufferArrayStride << std::endl;
        std::cout << "  maxInterStageShaderComponents:    " << limits.maxInterStageShaderComponents << std::endl;
        std::cout << "  maxInterStageShaderVariables:     " << limits.maxInterStageShaderVariables << std::endl;
        std::cout << "  maxColorAttachments:              " << limits.maxColorAttachments << std::endl;
        std::cout << "  maxComputeWorkgroupStorageSize:   " << limits.maxComputeWorkgroupStorageSize << std::endl;
        std::cout << "  maxComputeInvocationsPerWorkgroup:" << limits.maxComputeInvocationsPerWorkgroup << std::endl;
        std::cout << "  maxComputeWorkgroupSizeX:         " << limits.maxComputeWorkgroupSizeX << std::endl;
        std::cout << "  maxComputeWorkgroupSizeY:         " << limits.maxComputeWorkgroupSizeY << std::endl;
        std::cout << "  maxComputeWorkgroupSizeZ:         " << limits.maxComputeWorkgroupSizeZ << std::endl;
        std::cout << "  maxComputeWorkgroupsPerDimension: " << limits.maxComputeWorkgroupsPerDimension << std::endl;
    }

    // List the adapter features.
    std::vector<WGPUFeatureName> features;

    // Query the number of features.
    size_t featureCount = wgpuAdapterEnumerateFeatures( adapter, nullptr );

    // Allocate memory to store the resulting features.
    features.resize( featureCount );

    // Enumerate again, now with the allocated to store the features.
    wgpuAdapterEnumerateFeatures( adapter, features.data() );

    std::cout << "Adapter features: " << std::endl;
    for ( auto feature: features )
    {
        // Print features in hexadecimal format to make it easier to compare with the WebGPU specification.
        std::cout << "  - 0x" << std::hex << feature << std::dec << ": " << featureNames[feature] << std::endl;
    }
}

WGPUAdapter lwgpuCreateAdapter(WGPUInstance instance, WGPUSurface surface)
{
    struct UserData
    {
        WGPUAdapter adapter = nullptr;
        bool done = false;
    } userData;

    auto requestCallback = []( WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* pMessage, void* pUserData)
    {
        UserData& userData = *reinterpret_cast<UserData*>( pUserData );
        if (status == WGPURequestAdapterStatus_Success)
        {
            userData.adapter = adapter;
        }
        else
        {
            std::cout << "WGPU Adapter request failed: " << pMessage << std::endl;
            userData.adapter = nullptr; //< still NULL
        }
        userData.done = true;
    };

    WGPURequestAdapterOptions adapterOptions {};
    adapterOptions.compatibleSurface    = surface;
    adapterOptions.forceFallbackAdapter = false;
    adapterOptions.powerPreference      = WGPUPowerPreference_HighPerformance;
    wgpuInstanceRequestAdapter( instance, &adapterOptions, requestCallback, &userData );

#if __EMSCRIPTEN__
    while ( !userData.done )
    {
        emscripten_sleep( 100 );
    }
#endif

    assert( userData.done );
    return userData.adapter;
}

WGPUDevice lwgpuCreateDevice(WGPUAdapter adapter)
{
    struct UserData
    {
        WGPUDevice device = nullptr;
        bool done = false;
    } userData;

    auto requestCallback = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* pMessage, void* pUserData)
    {
        UserData& data = *reinterpret_cast<UserData*>( pUserData );
        if ( status == WGPURequestDeviceStatus_Success )
        {
            data.device = device;
        }
        else
        {
            std::cout << "WGPU Device request failed: " << pMessage << std::endl;
            data.device = nullptr;
        }
        data.done = true;
    };

    WGPUDeviceDescriptor deviceDesc {};
    deviceDesc.label                    = "WGPU Device";
    deviceDesc.deviceLostCallback       = nullptr;
    deviceDesc.deviceLostUserdata       = nullptr;
    deviceDesc.requiredFeatureCount     = 0;
    deviceDesc.requiredFeatures         = nullptr;
    deviceDesc.requiredLimits           = nullptr;
    deviceDesc.defaultQueue.label       = "Device Queue";
    deviceDesc.defaultQueue.nextInChain = nullptr;
    wgpuAdapterRequestDevice( adapter, &deviceDesc, requestCallback, &userData );

#if __EMSCRIPTEN__
    while ( !userData.done )
    {
        emscripten_sleep( 100 );
    }
#endif

    assert( userData.done );
    return userData.device;
}

void lwgpuPollDevice(WGPUDevice device)
{
#if     defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll( device, false, nullptr );
#elif   defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick( device );
#elif   defined(WEBGPU_BACKEND_EMSCRIPTEN)
    // TODO(nemjit001): check if sleep is really needed
    // emscripten_sleep( 100 );
#endif
}

WGPUTexture lwgpuGetNextSurfaceTexture(WGPUSurface surface)
{
    WGPUSurfaceTexture surfaceTexture {};
    wgpuSurfaceGetCurrentTexture( surface, &surfaceTexture );

    if ( surfaceTexture.suboptimal )
    {
        if ( surfaceTexture.texture != nullptr )
        {
            wgpuTextureRelease( surfaceTexture.texture );
        }

        resize();
        return nullptr;
    }

    switch ( surfaceTexture.status )
    {
    case WGPUSurfaceGetCurrentTextureStatus_Success:
        break;
    case WGPUSurfaceGetCurrentTextureStatus_Timeout:
    case WGPUSurfaceGetCurrentTextureStatus_Outdated:
    case WGPUSurfaceGetCurrentTextureStatus_Lost:
        if ( surfaceTexture.texture != nullptr )
        {
            wgpuTextureRelease( surfaceTexture.texture );
        }

        resize();
        return nullptr;
    case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
    case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
        break;
    default:
        return nullptr;
    }

    return surfaceTexture.texture;
}

// Initialize the application.
void init()
{
    SDL_Init( SDL_INIT_VIDEO );
    window = SDL_CreateWindow( WINDOW_TITLE, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
                               WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE );

    if ( !window )
    {
        std::cerr << "Failed to create window." << std::endl;
        return;
    }

#ifdef WEBGPU_BACKEND_EMSCRIPTEN
    instance = wgpuCreateInstance( nullptr );
#else
    WGPUInstanceDescriptor instanceDesc {};
    #ifdef WEBGPU_BACKEND_DAWN
    // TODO(nemjit001): enable debugging & error handling when using Dawn backend
    #endif
    instance = wgpuCreateInstance( &instanceDesc );
#endif

    surface             = SDL_GetWGPUSurface( instance, window );
    WGPUAdapter adapter = lwgpuCreateAdapter( instance, surface );
    device              = lwgpuCreateDevice( adapter );
    queue               = wgpuDeviceGetQueue( device );

    // output adapter info
    inspectAdapter( adapter );

    // configure surface
    WGPUSurfaceCapabilities surfaceCaps {};
    wgpuSurfaceGetCapabilities( surface, adapter, &surfaceCaps );
    
    WGPUTextureFormat preferredFormat         = surfaceCaps.formats[0];
    bool              mailboxPresentSupport   = false;
    bool              immediatePresentSupport = false;

    for ( uint32_t i = 0; i < surfaceCaps.formatCount; i++ )
    {
        if (surfaceCaps.formats[i] == WGPUTextureFormat_BGRA8UnormSrgb)
        {
            preferredFormat = surfaceCaps.formats[i];
            std::cout << "Selecting SRGB surface format" << std::endl;
            break;
        }
    }

    for (uint32_t i = 0; i < surfaceCaps.presentModeCount; i++)
    {
        if (surfaceCaps.presentModes[i] == WGPUPresentMode_Mailbox)
        {
            mailboxPresentSupport = true;
        }

        if ( surfaceCaps.presentModes[i] == WGPUPresentMode_Immediate )
        {
            immediatePresentSupport = true;
        }
    }

    wgpuSurfaceCapabilitiesFreeMembers( surfaceCaps );

    surfaceConfiguration                 = WGPUSurfaceConfiguration {};
    surfaceConfiguration.device          = device;
    surfaceConfiguration.format          = preferredFormat;
    surfaceConfiguration.width           = WINDOW_WIDTH;
    surfaceConfiguration.height          = WINDOW_HEIGHT;
    surfaceConfiguration.usage           = WGPUTextureUsage_RenderAttachment;
    surfaceConfiguration.viewFormatCount = 0;
    surfaceConfiguration.viewFormats     = nullptr;
    surfaceConfiguration.alphaMode       = WGPUCompositeAlphaMode_Auto;
    surfaceConfiguration.presentMode     = immediatePresentSupport ? WGPUPresentMode_Immediate :
                                           mailboxPresentSupport   ? WGPUPresentMode_Mailbox :
                                                                     WGPUPresentMode_Fifo;
    surfaceConfiguration.nextInChain     = nullptr;
    wgpuSurfaceConfigure( surface, &surfaceConfiguration );
    wgpuAdapterRelease( adapter );
    resize(); // Resize to init swap dependent resources

    // Buffer creation
    WGPUBufferDescriptor vertexBufferDesc {};
    vertexBufferDesc.label            = "Vertex Buffer";
    vertexBufferDesc.mappedAtCreation = false;
    vertexBufferDesc.size             = sizeof(vertices);
    vertexBufferDesc.usage            = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vertexBufferDesc.nextInChain      = nullptr;
    vertexBuffer                      = wgpuDeviceCreateBuffer( device, &vertexBufferDesc );
    wgpuQueueWriteBuffer( queue, vertexBuffer, 0, vertices, sizeof( vertices ) );

    WGPUBufferDescriptor indexBufferDesc {};
    indexBufferDesc.label            = "Index Buffer";
    indexBufferDesc.mappedAtCreation = false;
    indexBufferDesc.size             = sizeof( indices );
    indexBufferDesc.usage            = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    indexBufferDesc.nextInChain      = nullptr;
    indexBuffer                      = wgpuDeviceCreateBuffer( device, &indexBufferDesc );
    wgpuQueueWriteBuffer( queue, indexBuffer, 0, indices, sizeof( indices ) );

    WGPUBufferDescriptor uniformBufferDesc {};
    uniformBufferDesc.label            = "Uniform Buffer";
    uniformBufferDesc.mappedAtCreation = false;
    uniformBufferDesc.size             = sizeof( glm::mat4 );
    uniformBufferDesc.usage            = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    uniformBufferDesc.nextInChain      = nullptr;
    uniformBuffer                      = wgpuDeviceCreateBuffer( device, &uniformBufferDesc );

    // Pipeline creation
    WGPUBindGroupLayoutEntry bindGroupEntry {};
    bindGroupEntry.binding                 = 0;
    bindGroupEntry.visibility              = WGPUShaderStage_Vertex;
    bindGroupEntry.buffer.type             = WGPUBufferBindingType_Uniform;
    bindGroupEntry.buffer.hasDynamicOffset = false;
    bindGroupEntry.buffer.minBindingSize   = sizeof( glm::mat4 );

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc {};
    bindGroupLayoutDesc.label      = "Bind Group Layout";
    bindGroupLayoutDesc.entryCount = 1;
    bindGroupLayoutDesc.entries    = &bindGroupEntry;
    bindGroupLayout                = wgpuDeviceCreateBindGroupLayout( device, &bindGroupLayoutDesc );

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc {};
    pipelineLayoutDesc.label                = "Pipeline Layout";
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts     = &bindGroupLayout;
    pipelineLayout                          = wgpuDeviceCreatePipelineLayout( device, &pipelineLayoutDesc );

    WGPUShaderModuleDescriptor shaderModuleDesc {};
    shaderModuleDesc.label = "Shader Module";

    WGPUShaderModuleWGSLDescriptor wgslDesc {};
    wgslDesc.chain.sType          = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.chain.next           = nullptr;
    wgslDesc.code                 = SHADER_MODULE;
    shaderModuleDesc.nextInChain  = &wgslDesc.chain;
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule( device, &shaderModuleDesc );

    WGPUVertexAttribute vertexAttributes[] = {
        { WGPUVertexFormat_Float32x3, offsetof(Vertex, position), 0 },
        { WGPUVertexFormat_Float32x3, offsetof(Vertex, color), 1 },
    };

    WGPUVertexBufferLayout vertexBufferLayout {};
    vertexBufferLayout.arrayStride    = sizeof( Vertex );
    vertexBufferLayout.stepMode       = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = sizeof(vertexAttributes) / sizeof(vertexAttributes[0]);
    vertexBufferLayout.attributes     = vertexAttributes;

    WGPUStencilFaceState stencilState {};
    stencilState.compare     = WGPUCompareFunction_Always;
    stencilState.depthFailOp = WGPUStencilOperation_Keep;
    stencilState.failOp      = WGPUStencilOperation_Keep;
    stencilState.passOp      = WGPUStencilOperation_Keep;

    WGPUDepthStencilState depthStencilState {};
    depthStencilState.format              = WGPUTextureFormat_Depth32Float;
    depthStencilState.depthWriteEnabled   = true;
    depthStencilState.depthCompare        = WGPUCompareFunction_Less;
    depthStencilState.stencilFront        = stencilState;
    depthStencilState.stencilBack         = stencilState;
    depthStencilState.stencilReadMask     = UINT32_MAX;
    depthStencilState.stencilWriteMask    = UINT32_MAX;
    depthStencilState.depthBias           = 0;
    depthStencilState.depthBiasSlopeScale = 0.0F;
    depthStencilState.depthBiasClamp      = 0.0F;

    WGPUColorTargetState colorTargetState {};
    colorTargetState.format    = surfaceConfiguration.format;
    colorTargetState.writeMask = WGPUColorWriteMask_All;
    colorTargetState.blend     = nullptr;

    WGPUFragmentState fragmentState {};
    fragmentState.module        = shaderModule;
    fragmentState.entryPoint    = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants     = nullptr;
    fragmentState.targetCount   = 1;
    fragmentState.targets       = &colorTargetState;

    WGPURenderPipelineDescriptor renderPipelineDesc {};
    renderPipelineDesc.label                              = "Render Pipeline";
    renderPipelineDesc.layout                             = pipelineLayout;
    renderPipelineDesc.vertex.module                      = shaderModule;
    renderPipelineDesc.vertex.entryPoint                  = "vs_main";
    renderPipelineDesc.vertex.constantCount               = 0;
    renderPipelineDesc.vertex.constants                   = nullptr;
    renderPipelineDesc.vertex.bufferCount                 = 1;
    renderPipelineDesc.vertex.buffers                     = &vertexBufferLayout;
    renderPipelineDesc.primitive.topology                 = WGPUPrimitiveTopology_TriangleList;
    renderPipelineDesc.primitive.cullMode                 = WGPUCullMode_Back;
    renderPipelineDesc.primitive.frontFace                = WGPUFrontFace_CCW;
    renderPipelineDesc.primitive.stripIndexFormat         = WGPUIndexFormat_Undefined;
    renderPipelineDesc.depthStencil                       = &depthStencilState;
    renderPipelineDesc.multisample.count                  = 1;
    renderPipelineDesc.multisample.mask                   = UINT32_MAX;
    renderPipelineDesc.multisample.alphaToCoverageEnabled = false;
    renderPipelineDesc.fragment                           = &fragmentState;
    renderPipeline = wgpuDeviceCreateRenderPipeline( device, &renderPipelineDesc );

    wgpuShaderModuleRelease(shaderModule);

    std::cout << "Initialized LearnWebGPU" << std::endl;
}

void resize()
{
    int width, height;
    SDL_GetWindowSize( window, &width, &height );

    // ensure width & height are never 0
    width = std::max( width, 1 );
    height = std::max( height, 1 );

    surfaceConfiguration.width = width;
    surfaceConfiguration.height = height;
    wgpuSurfaceConfigure( surface, &surfaceConfiguration );

    if ( depthStencilTexture || depthStencilTextureView )
    {
        wgpuTextureViewRelease( depthStencilTextureView );
        wgpuTextureRelease( depthStencilTexture );
    }

    WGPUTextureFormat const depthStencilFormat = WGPUTextureFormat_Depth32Float;
    WGPUTextureDescriptor depthStencilDesc {};
    depthStencilDesc.label                   = "Depth Stencil Texture";
    depthStencilDesc.dimension               = WGPUTextureDimension_2D;
    depthStencilDesc.format                  = depthStencilFormat;
    depthStencilDesc.size.width              = width;
    depthStencilDesc.size.height             = height;
    depthStencilDesc.size.depthOrArrayLayers = 1;
    depthStencilDesc.mipLevelCount           = 1;
    depthStencilDesc.sampleCount             = 1;
    depthStencilDesc.usage                   = WGPUTextureUsage_RenderAttachment;
    depthStencilDesc.viewFormatCount         = 0;
    depthStencilDesc.viewFormats             = nullptr;
    depthStencilTexture                      = wgpuDeviceCreateTexture( device, &depthStencilDesc );

    WGPUTextureViewDescriptor depthStencilViewDesc {};
    depthStencilViewDesc.label           = "Depth Stencil Texture View";
    depthStencilViewDesc.format          = depthStencilFormat;
    depthStencilViewDesc.dimension       = WGPUTextureViewDimension_2D;
    depthStencilViewDesc.baseMipLevel    = 0;
    depthStencilViewDesc.mipLevelCount   = 1;
    depthStencilViewDesc.baseArrayLayer  = 0;
    depthStencilViewDesc.arrayLayerCount = 1;
    depthStencilViewDesc.aspect          = WGPUTextureAspect_All;
    depthStencilTextureView              = wgpuTextureCreateView( depthStencilTexture, &depthStencilViewDesc );
}

void render()
{
    WGPUTexture surfaceTexture = lwgpuGetNextSurfaceTexture( surface );
    if ( surfaceTexture == nullptr )
    {
        return;
    }

    WGPUTextureViewDescriptor surfaceTextureViewDesc {};
    surfaceTextureViewDesc.label           = "Surface Texture View";
    surfaceTextureViewDesc.format          = wgpuTextureGetFormat( surfaceTexture );
    surfaceTextureViewDesc.dimension       = WGPUTextureViewDimension_2D;
    surfaceTextureViewDesc.baseMipLevel    = 0;
    surfaceTextureViewDesc.mipLevelCount   = 1;
    surfaceTextureViewDesc.baseArrayLayer  = 0;
    surfaceTextureViewDesc.arrayLayerCount = 1;
    surfaceTextureViewDesc.aspect          = WGPUTextureAspect_All;
    WGPUTextureView surfaceTextureView     = wgpuTextureCreateView( surfaceTexture, &surfaceTextureViewDesc );

    WGPUCommandEncoderDescriptor encoderDesc {};
    encoderDesc.label                 = "Command Encoder";
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder( device, &encoderDesc );

    WGPURenderPassColorAttachment colorAttachment {};
    colorAttachment.view          = surfaceTextureView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp        = WGPULoadOp_Clear;
    colorAttachment.storeOp       = WGPUStoreOp_Store;
    colorAttachment.clearValue    = { 0.4F, 0.6F, 0.9F, 1.0F };
    colorAttachment.nextInChain   = nullptr;
#ifndef WEBGPU_BACKEND_WGPU
    colorAttachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDepthStencilAttachment depthStencilAttachment {};
    depthStencilAttachment.view              = depthStencilTextureView;
    depthStencilAttachment.depthLoadOp       = WGPULoadOp_Load;
    depthStencilAttachment.depthStoreOp      = WGPUStoreOp_Store;
    depthStencilAttachment.depthClearValue   = 1.0F;
    depthStencilAttachment.depthReadOnly     = false;
    depthStencilAttachment.stencilLoadOp     = WGPULoadOp_Undefined;
    depthStencilAttachment.stencilStoreOp    = WGPUStoreOp_Undefined;
    depthStencilAttachment.stencilClearValue = 0x00;
    depthStencilAttachment.stencilReadOnly   = false;

    WGPURenderPassDescriptor renderPassDesc {};
    renderPassDesc.label                    = "Render Pass";
    renderPassDesc.colorAttachmentCount     = 1;
    renderPassDesc.colorAttachments         = &colorAttachment;
    renderPassDesc.depthStencilAttachment   = &depthStencilAttachment;
    renderPassDesc.occlusionQuerySet        = nullptr;
    renderPassDesc.timestampWrites          = nullptr;
    WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass( commandEncoder, &renderPassDesc );

    WGPUBindGroupEntry bindGroupEntry {};
    bindGroupEntry.binding = 0;
    bindGroupEntry.offset  = 0;
    bindGroupEntry.size    = sizeof( glm::mat4 );
    bindGroupEntry.buffer  = uniformBuffer;

    WGPUBindGroupDescriptor bindGroupDescriptor {};
    bindGroupDescriptor.label      = "Bind Group";
    bindGroupDescriptor.layout     = bindGroupLayout;
    bindGroupDescriptor.entryCount = 1;
    bindGroupDescriptor.entries    = &bindGroupEntry;
    WGPUBindGroup bindGroup        = wgpuDeviceCreateBindGroup( device, &bindGroupDescriptor );

    wgpuRenderPassEncoderSetPipeline( renderPassEncoder, renderPipeline );
    wgpuRenderPassEncoderSetBindGroup( renderPassEncoder, 0, bindGroup, 0, nullptr );

    wgpuRenderPassEncoderSetVertexBuffer( renderPassEncoder, 0, vertexBuffer, 0, wgpuBufferGetSize( vertexBuffer ) );
    wgpuRenderPassEncoderSetIndexBuffer( renderPassEncoder, indexBuffer, WGPUIndexFormat_Uint16, 0, wgpuBufferGetSize( indexBuffer ) );
    wgpuRenderPassEncoderDrawIndexed( renderPassEncoder, sizeof( indices ) / sizeof( indices[0] ), 1, 0, 0, 0 );

    wgpuRenderPassEncoderEnd( renderPassEncoder );
    wgpuRenderPassEncoderRelease( renderPassEncoder );

    WGPUCommandBufferDescriptor cmdBufDesc {};
    cmdBufDesc.label                = "Command Buffer";
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish( commandEncoder, &cmdBufDesc );
    wgpuQueueSubmit( queue, 1, &commandBuffer );

#ifndef WEBGPU_BACKEND_EMSCRIPTEN
    wgpuSurfacePresent( surface );
#endif

    lwgpuPollDevice( device ); // Wait on device work to be done

    // wgpuBindGroupRelease( bindGroup );
    wgpuCommandBufferRelease( commandBuffer );
    wgpuCommandEncoderRelease( commandEncoder );
    wgpuTextureViewRelease( surfaceTextureView );
    wgpuTextureRelease( surfaceTexture );
}

void update( void* pUserData )
{
    timer.tick();

    SDL_Event event;
    while ( SDL_PollEvent( &event ) )
    {
        switch ( event.type )
        {
        case SDL_QUIT:
            isRunning = false;
            break;
        case SDL_KEYDOWN:
            std::cout << "Key pressed: " << SDL_GetKeyName( event.key.keysym.sym ) << std::endl;
            if ( event.key.keysym.sym == SDLK_ESCAPE )
            {
                isRunning = false;
            }
            break;
        default:;
        }
    }

    int width, height;
    SDL_GetWindowSize( window, &width, &height );

    width  = std::max( 1, width );
    height = std::max( 1, height );

    float     angle            = static_cast<float>( timer.totalSeconds() * 90.0 );
    glm::vec3 axis             = glm::vec3( 0.0f, 1.0f, 1.0f );
    glm::mat4 modelMatrix      = glm::rotate( glm::mat4 { 1 }, glm::radians( angle ), axis );
    glm::mat4 viewMatrix       = glm::lookAt( glm::vec3 { 0, 0, -10 }, glm::vec3 { 0, 0, 0 }, glm::vec3 { 0, 1, 0 } );
    glm::mat4 projectionMatrix = glm::perspective(
        glm::radians( 45.0f ), static_cast<float>( width ) / static_cast<float>( height ), 0.1f, 100.0f );
    glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * modelMatrix;
    wgpuQueueWriteBuffer( queue, uniformBuffer, 0, &mvpMatrix, sizeof( mvpMatrix ) );

    render();
}

void destroy()
{
    std::cout << "Shutting down LearnWebGPU" << std::endl;

    wgpuRenderPipelineRelease( renderPipeline );
    wgpuPipelineLayoutRelease( pipelineLayout );
    wgpuBindGroupLayoutRelease( bindGroupLayout );

    wgpuBufferRelease( uniformBuffer );
    wgpuBufferRelease( indexBuffer );
    wgpuBufferRelease( vertexBuffer );

    wgpuQueueRelease( queue );
    wgpuDeviceRelease( device );
    wgpuSurfaceRelease( surface );
    wgpuInstanceRelease( instance );

    SDL_DestroyWindow( window );
    SDL_Quit();
}

int main( int, char** )
{
    init();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg( update, nullptr, 0, 1 );
#else

    while ( isRunning )
    {
        update();
    }

    destroy();

#endif

    return 0;
}
