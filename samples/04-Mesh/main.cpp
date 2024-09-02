#include "TextureUnlitPipelineState.hpp"

#include <Camera.hpp>
#include <CameraController.hpp>
#include <Timer.hpp>

#include <WebGPUlib/Device.hpp>
#include <WebGPUlib/GraphicsCommandBuffer.hpp>
#include <WebGPUlib/Mesh.hpp>
#include <WebGPUlib/Queue.hpp>
#include <WebGPUlib/RenderTarget.hpp>
#include <WebGPUlib/Sampler.hpp>
#include <WebGPUlib/Scene.hpp>
#include <WebGPUlib/SceneNode.hpp>
#include <WebGPUlib/Surface.hpp>
#include <WebGPUlib/Texture.hpp>
#include <WebGPUlib/TextureView.hpp>
#include <WebGPUlib/UniformBuffer.hpp>
#include <WebGPUlib/VertexBuffer.hpp>

#ifdef __EMSCRIPTEN__
    #include <emscripten/html5.h>
#endif

#define SDL_MAIN_HANDLED
#include "WebGPUlib/Material.hpp"

#include <SDL2/SDL.h>

#include <glm/gtc/matrix_transform.hpp>  // For matrix transformations.
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <iostream>

using namespace WebGPUlib;

constexpr int WINDOW_WIDTH  = 1280;
constexpr int WINDOW_HEIGHT = 720;
const char*   WINDOW_TITLE  = "04 - Mesh";
SDL_Window*   window        = nullptr;

Timer                             timer;
Camera                            camera;
std::unique_ptr<CameraController> cameraController;

bool isRunning = true;

std::shared_ptr<Mesh>                      cubeMesh;
std::shared_ptr<UniformBuffer>             mvpBuffer;
std::shared_ptr<Texture>                   depthTexture;
std::shared_ptr<TextureView>               depthTextureView;
std::shared_ptr<Texture>                   albedoTexture;
std::shared_ptr<Sampler>                   linearRepeatSampler;
std::shared_ptr<Scene>                     scene;
std::unique_ptr<TextureUnlitPipelineState> textureUnlitPipelineState;

void onResize( uint32_t width, uint32_t height )
{
    // Resize the window surface.
    Device::get().getSurface()->resize( width, height );

    // Create the depth texture.
    WGPUTextureFormat depthTextureFormat = WGPUTextureFormat_Depth32Float;

    WGPUTextureDescriptor depthTextureDescriptor = {};
    depthTextureDescriptor.label                 = "Depth Texture";
    depthTextureDescriptor.usage                 = WGPUTextureUsage_RenderAttachment;
    depthTextureDescriptor.dimension             = WGPUTextureDimension_2D;
    depthTextureDescriptor.size                  = { width, height, 1 };
    depthTextureDescriptor.format                = depthTextureFormat;
    depthTextureDescriptor.mipLevelCount         = 1;
    depthTextureDescriptor.sampleCount           = 1;
    depthTextureDescriptor.viewFormatCount       = 1;
    depthTextureDescriptor.viewFormats           = &depthTextureFormat;

    depthTexture = Device::get().createTexture( depthTextureDescriptor );

    // Create the depth texture view.
    WGPUTextureViewDescriptor depthTextureViewDescriptor {};
    depthTextureViewDescriptor.label           = "Depth Texture View";
    depthTextureViewDescriptor.format          = WGPUTextureFormat_Depth32Float;
    depthTextureViewDescriptor.dimension       = WGPUTextureViewDimension_2D;
    depthTextureViewDescriptor.baseMipLevel    = 0;
    depthTextureViewDescriptor.mipLevelCount   = 1;
    depthTextureViewDescriptor.baseArrayLayer  = 0;
    depthTextureViewDescriptor.arrayLayerCount = 1;
    depthTextureViewDescriptor.aspect          = WGPUTextureAspect_DepthOnly;

    depthTextureView = depthTexture->getView( &depthTextureViewDescriptor );

    // Update the camera's projection matrix.
    camera.setProjection( glm::radians( 45.0f ), static_cast<float>( width ) / static_cast<float>( height ), 0.1f,
                          10000.0f );
}

void init()
{
    SDL_Init( SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER );

    // Enable polling game controllers.s
    SDL_GameControllerEventState( SDL_ENABLE );

    window = SDL_CreateWindow( WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH,
                               WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE );

    if ( !window )
    {
        std::cerr << "Failed to create window." << std::endl;
        return;
    }

    Device::create( window );

    // Create a uniform buffer large enough to hold a single 4x4 matrix.
    mvpBuffer                 = Device::get().createUniformBuffer( nullptr, sizeof( glm::mat4 ) );
    textureUnlitPipelineState = std::make_unique<TextureUnlitPipelineState>();

    cameraController = std::make_unique<CameraController>( camera, glm::vec3 { 7.5, 2, 0 }, glm::vec3 { 0, 80, 0 } );

    // Resize to configure the depth texture.
    onResize( WINDOW_WIDTH, WINDOW_HEIGHT );

    albedoTexture = Device::get().loadTexture( "assets/textures/webgpu.png" );
    cubeMesh      = Device::get().createCube( 2.0f );
    scene         = Device::get().loadScene( "assets/crytek-sponza/sponza_nobanner.obj" );

    // Scale the root node
    scene->getRootNode()->setLocalTransform( glm::scale( glm::mat4 { 1 }, glm::vec3 { 1.0f / 100.0f } ) );

    // Setup the texture sampler.
    WGPUSamplerDescriptor linearRepeatSamplerDesc {};
    linearRepeatSamplerDesc.label         = "Linear Repeat Sampler";
    linearRepeatSamplerDesc.addressModeU  = WGPUAddressMode_Repeat;
    linearRepeatSamplerDesc.addressModeV  = WGPUAddressMode_Repeat;
    linearRepeatSamplerDesc.addressModeW  = WGPUAddressMode_Repeat;
    linearRepeatSamplerDesc.magFilter     = WGPUFilterMode_Linear;
    linearRepeatSamplerDesc.minFilter     = WGPUFilterMode_Linear;
    linearRepeatSamplerDesc.mipmapFilter  = WGPUMipmapFilterMode_Linear;
    linearRepeatSamplerDesc.lodMinClamp   = 0.0f;
    linearRepeatSamplerDesc.lodMaxClamp   = FLT_MAX;
    linearRepeatSamplerDesc.compare       = WGPUCompareFunction_Undefined;
    linearRepeatSamplerDesc.maxAnisotropy = 8;

    linearRepeatSampler = Device::get().createSampler( linearRepeatSamplerDesc );
}

void renderNode( std::shared_ptr<GraphicsCommandBuffer> commandBuffer, std::shared_ptr<SceneNode> node )
{
    auto worldMatrix      = node->getWorldTransform();
    auto viewMatrix       = camera.getViewMatrix();
    auto projectionMatrix = camera.getProjectionMatrix();

    auto mvp = projectionMatrix * viewMatrix * worldMatrix;

    commandBuffer->bindDynamicUniformBuffer( 0, 0, mvp );

    for ( auto& mesh: node->getMeshes() )
    {
        auto material = mesh->getMaterial();
        if ( auto diffuseTexture = material->getTexture( TextureSlot::Diffuse ) )
        {
            commandBuffer->bindTexture( 0, 1, *( diffuseTexture->getView() ) );
            commandBuffer->draw( *mesh );
        }
    }

    for ( auto& child: node->getChildren() )
    {
        renderNode( commandBuffer, node );
    }
}

void render()
{
    auto surface = Device::get().getSurface();

    RenderTarget renderTarget;
    renderTarget.attachTexture( AttachmentPoint::Color0, surface->getNextTextureView() );
    renderTarget.attachTexture( AttachmentPoint::DepthStencil, depthTextureView );

    auto queue = Device::get().getQueue();

    auto commandBuffer = queue->createGraphicsCommandBuffer( renderTarget, ClearFlags::Color | ClearFlags::Depth,
                                                             { 0.4f, 0.6f, 0.9f, 1.0f }, 1.0f );

    // Set the pipeline state.
    commandBuffer->setGraphicsPipeline( *textureUnlitPipelineState );

    // Bind parameters.
    commandBuffer->bindBuffer( 0, 0, *mvpBuffer );
    commandBuffer->bindTexture( 0, 1, *albedoTexture->getView() );
    commandBuffer->bindSampler( 0, 2, *linearRepeatSampler );

    commandBuffer->draw( *cubeMesh );

    // Render the scene.
    renderNode( commandBuffer, scene->getRootNode() );

    queue->submit( *commandBuffer );

    surface->present();

    // Poll the device to make sure work is done.
    Device::get().poll();
}

void pollEvents()
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
            switch ( event.key.keysym.sym )
            {
            case SDLK_ESCAPE:
                isRunning = false;
                break;
            case SDLK_r:
                cameraController->reset();
                break;
            default:
                break;
            }
            break;
        case SDL_WINDOWEVENT:
            if ( event.window.event == SDL_WINDOWEVENT_RESIZED )
            {
                std::cout << "Window resized: " << event.window.data1 << "x" << event.window.data2 << std::endl;
                onResize( event.window.data1, event.window.data2 );
            }
            break;
        default:
            break;
        }
    }
}

void update( void* userdata = nullptr )
{
    // Handle input.
    pollEvents();

    timer.tick();

    cameraController->update( timer.elapsedSeconds() );

    static double   totalTime = 0.0;
    static uint64_t frames    = 0;

    totalTime += timer.elapsedSeconds();
    frames++;
    if ( totalTime > 1.0 )
    {
        std::cout << "FPS: " << frames << std::endl;
        totalTime -= 1.0;
        frames = 0;
    }

    // Update the model-view-projection matrix.
    float     angle = static_cast<float>( timer.totalSeconds() * 90.0 );
    glm::vec3 axis  = glm::vec3( 1.0f, 1.0f, 1.0f );
    glm::mat4 t     = glm::translate( glm::mat4 { 1 }, glm::vec3 { 0, 2, 0 } );
    glm::mat4 r     = glm::rotate( glm::mat4 { 1 }, glm::radians( angle ), axis );
    glm::mat4 modelMatrix      = t * r;
    glm::mat4 viewMatrix       = camera.getViewMatrix();
    glm::mat4 projectionMatrix = camera.getProjectionMatrix();
    glm::mat4 mvpMatrix        = projectionMatrix * viewMatrix * modelMatrix;

    auto queue = Device::get().getQueue();
    queue->writeBuffer( *mvpBuffer, mvpMatrix );

    render();
}

void destroy()
{
    Device::destroy();
}

int main()
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
