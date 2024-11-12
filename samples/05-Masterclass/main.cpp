#ifdef __EMSCRIPTEN__
    #include <emscripten/html5.h>
#endif

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <sdl2webgpu.h>
#include <webgpu/webgpu.h>

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

SDL_Window* window = nullptr;

WGPUInstance             instance = nullptr;
WGPUDevice               device   = nullptr;
WGPUQueue                queue    = nullptr;
WGPUSurface              surface  = nullptr;
WGPUSurfaceConfiguration surfaceConfiguration {};

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
    colorAttachment.view = surfaceTextureView;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp        = WGPULoadOp_Clear;
    colorAttachment.storeOp       = WGPUStoreOp_Store;
    colorAttachment.clearValue    = { 0.4F, 0.6F, 0.9F, 1.0F };
    colorAttachment.nextInChain   = nullptr;
#ifndef WEBGPU_BACKEND_WGPU
    colorAttachment.depthSlice    = WGPU_DEPTH_SLICE_UNDEFINED;
#endif

    WGPURenderPassDescriptor renderPassDesc {};
    renderPassDesc.label                  = "Render Pass";
    renderPassDesc.colorAttachmentCount   = 1;
    renderPassDesc.colorAttachments       = &colorAttachment;
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.occlusionQuerySet      = nullptr;
    renderPassDesc.timestampWrites        = nullptr;
    WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass( commandEncoder, &renderPassDesc );
    wgpuRenderPassEncoderEnd( renderPassEncoder );

    WGPUCommandBufferDescriptor cmdBufDesc {};
    cmdBufDesc.label                = "Command Buffer";
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish( commandEncoder, &cmdBufDesc );
    wgpuQueueSubmit( queue, 1, &commandBuffer );

#ifndef WEBGPU_BACKEND_EMSCRIPTEN
    wgpuSurfacePresent( surface );
#endif

    lwgpuPollDevice( device ); // Wait on device work to be done

    wgpuCommandBufferRelease( commandBuffer );
    wgpuCommandEncoderRelease( commandEncoder );
    wgpuTextureViewRelease( surfaceTextureView );
    wgpuTextureRelease( surfaceTexture );
}

void update( void* pUserData )
{
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

    render();
}

void destroy()
{
    std::cout << "Shutting down LearnWebGPU" << std::endl;

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
