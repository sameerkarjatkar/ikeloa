//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "D3D12HelloTriangle.h"
#include "tiny_obj_loader.h"
#include <unordered_map>
#include <algorithm>

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 618; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

D3D12HelloTriangle::D3D12HelloTriangle(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0)
{
}

void D3D12HelloTriangle::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

// Load the rendering pipeline dependencies.
void D3D12HelloTriangle::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void D3D12HelloTriangle::LoadAssets()
{
    // Load OBJ 
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
        "Model/torus.obj",
        nullptr, true);

    if (!warn.empty()) OutputDebugStringA(warn.c_str());
    if (!err.empty())  OutputDebugStringA(err.c_str());
    if (!ret) throw std::runtime_error("Failed to load OBJ");

    const auto& shape = shapes[0];
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
    vertices.reserve(attrib.vertices.size() / 3);
    indices.reserve(shape.mesh.indices.size());
    std::unordered_map<int, UINT> indexMap;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f)
    {
        int fv = shape.mesh.num_face_vertices[f];
        if (fv != 3) continue; // skip non-triangles for now

        for (int v = 0; v < 3; ++v)
        {
            tinyobj::index_t idx = shape.mesh.indices[3 * f + v];

            Vertex vert{};
            vert.position.x = attrib.vertices[3 * idx.vertex_index + 0];
            vert.position.y = attrib.vertices[3 * idx.vertex_index + 1];
            vert.position.z = attrib.vertices[3 * idx.vertex_index + 2];
            vert.scalar = sinf(vert.position.x) * cosf(vert.position.z);
            //noise
            vert.scalar += 0.2f * sinf(10.0f * vert.position.x);
            //vert.color = XMFLOAT4(1, 1, 1, 1);
            UINT vertexIndex = static_cast<UINT>(vertices.size());
            vertices.push_back(vert);
            indices.push_back(vertexIndex);
        }
    }

    for (auto& v : vertices)
    {
        v.normal = { 0.0f, 0.0f, 0.0f };
    }

    //compute face normals 
    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];

        XMVECTOR p0 = XMLoadFloat3(&vertices[i0].position);
        XMVECTOR p1 = XMLoadFloat3(&vertices[i1].position);
        XMVECTOR p2 = XMLoadFloat3(&vertices[i2].position);

        XMVECTOR e1 = p1 - p0;
        XMVECTOR e2 = p2 - p0;

        XMVECTOR n = XMVector3Cross(e1, e2);
        n = XMVector3Normalize(n);

        XMFLOAT3 normal;
        XMStoreFloat3(&normal, n);

        vertices[i0].normal.x += normal.x;
        vertices[i0].normal.y += normal.y;
        vertices[i0].normal.z += normal.z;

        vertices[i1].normal.x += normal.x;
        vertices[i1].normal.y += normal.y;
        vertices[i1].normal.z += normal.z;

        vertices[i2].normal.x += normal.x;
        vertices[i2].normal.y += normal.y;
        vertices[i2].normal.z += normal.z;
    }

    // Normalize vertex normals
    for (auto& v : vertices)
    {
        using namespace DirectX;

        XMVECTOR n = XMLoadFloat3(&v.normal);
        n = XMVector3Normalize(n);
        XMStoreFloat3(&v.normal, n);
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif


        ComPtr<ID3DBlob> vsBlob;
        ComPtr<ID3DBlob> errorBlob;
        HRESULT hr = D3DCompileFromFile(
            L"vertex.hlsl",          // or separate .vs file
            nullptr,                  // defines
            nullptr,                  // include handler
            "main",                   // entry point
            "vs_5_0",                 // ← use vs_5_1 or vs_6_0 in 2026
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            &vsBlob,
            &errorBlob);

        if (FAILED(hr))
        {
            if (errorBlob)
            {
                OutputDebugStringA((char*)errorBlob->GetBufferPointer());
                // Usually prints something useful like:
                // "error X3000: unrecognized identifier 'matrix'"
                // or "cannot implicitly convert float3 to float4"
            }
            ThrowIfFailed(hr);  // or handle gracefully
        }

        ThrowIfFailed(D3DCompileFromFile(L"vertex.hlsl", nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
             { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            
        };

        // Create a root signature.
        CreateRootSignature();
        
        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    

    m_indexCount = static_cast<UINT>(indices.size());
    UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(Vertex));
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vbByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)));
    ComPtr<ID3D12Resource> uploadVB;
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vbByteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadVB)));
    

    // Copy
    D3D12_SUBRESOURCE_DATA subData = {};
    subData.pData = vertices.data();
    subData.RowPitch = vbByteSize;
    subData.SlicePitch = vbByteSize;
    UpdateSubresources<1>(m_commandList.Get(), m_vertexBuffer.Get(), uploadVB.Get(), 0, 0, 1, &subData);

    // Final barrier
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    // View
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vbByteSize;

    {
        UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(UINT));  // or sizeof(uint16_t) if you use 16-bit indices

        // Create the real GPU index buffer (default heap)
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(ibByteSize),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&m_indexBuffer)));

        // Create temporary upload buffer (CPU → GPU staging)
        ComPtr<ID3D12Resource> uploadIB;
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(ibByteSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadIB)));

        // Prepare subresource data 
        D3D12_SUBRESOURCE_DATA sub = {};
        sub.pData = indices.data();                    // your std::vector<UINT>
        sub.RowPitch = ibByteSize;                        // for buffers, RowPitch == total byte size
        sub.SlicePitch = ibByteSize;

        // Transition → copy destination
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_indexBuffer.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_COPY_DEST));

        // Copy data from upload → default heap
        UpdateSubresources<1>(m_commandList.Get(), m_indexBuffer.Get(), uploadIB.Get(), 0, 0, 1, &sub);

        // Transition back to usable state
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
            m_indexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER));  // ← note: INDEX_BUFFER, not VERTEX_AND_CONSTANT_BUFFER

        // Fill the index buffer view
        m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        m_indexBufferView.SizeInBytes = ibByteSize;
        m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;   // or DXGI_FORMAT_R16_UINT if you used uint16_t

        const UINT constantBufferSize =
            (sizeof(SceneCB) + 255) & ~255; // must be 256-byte aligned

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));
        CD3DX12_RANGE readRange(0, 0);

        ThrowIfFailed(m_constantBuffer->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&m_pCbvDataBegin)));
        // Create synchronization objects and wait until assets have been uploaded to the GPU.
        {
            ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
            m_fenceValue = 1;

            // Create an event handle to use for frame synchronization.
            m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (m_fenceEvent == nullptr)
            {
                ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
            }
            ThrowIfFailed(m_commandList->Close());

            // Execute the upload commands
            ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
            m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
            // Wait for the command list to execute; we are reusing the same command 
            // list in our main loop but for now, we just want to wait for setup to 
            // complete before continuing.
            WaitForPreviousFrame();
        }
    }
    
}

// Update frame-based values.
void D3D12HelloTriangle::OnUpdate()
{
    using namespace DirectX;

    XMVECTOR eye = GetCameraPosition();
    XMVECTOR target = XMLoadFloat3(&m_camera.target);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    XMMATRIX view = XMMatrixLookAtLH(eye, target, up);

    float aspect = (float)m_width / (float)m_height;

    XMMATRIX proj = XMMatrixPerspectiveFovLH(
        XM_PIDIV4,
        aspect,
        0.1f,
        100.0f
    );
    XMMATRIX world = XMMatrixIdentity();
    XMMATRIX wvp = world * view * proj;

    XMStoreFloat4x4(&m_constantBufferData.WorldViewProj, XMMatrixTranspose(wvp));
    memcpy(
        m_pCbvDataBegin,
        &m_constantBufferData,
        sizeof(m_constantBufferData)
    );
}

void D3D12HelloTriangle::OnSizeChange()
{
}

void D3D12HelloTriangle::OnMouseMove(UINT dx, UINT dy)
{
    const float sensitivity = 0.01f;

    m_camera.yaw += dx * sensitivity;
    m_camera.pitch += dy * sensitivity;
    PanCamera(dx, dy);

    // prevent flip
    //m_camera.pitch = std::clamp(m_camera.pitch, -1.5f, 1.5f);
}

void D3D12HelloTriangle::OnMouseDrag(UINT dx, UINT dy)
{
    OrbitCamera(dx, dy);
}

void D3D12HelloTriangle::PanCamera(float dx, float dy)
{
    dx = (dx < -50) ? -50 : (dx > 50 ? 50 : dx);
    dy = (dy < -50) ? -50 : (dy > 50 ? 50 : dy);
 
    XMVECTOR eye = GetCameraPosition();
    XMVECTOR target = XMLoadFloat3(&m_camera.target);

    XMVECTOR forward = XMVector3Normalize(target - eye);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), forward));
    XMVECTOR up = XMVector3Cross(forward, right);

    float panSpeed = m_camera.radius * 0.0005f;
    /*XMVECTOR offset = (-dx * panSpeed) * right + (dy * panSpeed) * up;
    eye += offset;
    target += offset;*/

    target += (-dx * panSpeed) * right;
    target += (dy * panSpeed) * up;

    XMStoreFloat3(&m_camera.target, target);
}

void D3D12HelloTriangle::OrbitCamera(int dx, int dy)
{
    const float sensitivity = 0.01f;

    m_camera.yaw += dx * sensitivity;
    m_camera.pitch += dy * sensitivity;

    // Prevent the camera from flipping over the poles
    const float limit = DirectX::XM_PIDIV2 - 0.01f;

    if (m_camera.pitch > limit)
        m_camera.pitch = limit;

    if (m_camera.pitch < -limit)
        m_camera.pitch = -limit;
}

void D3D12HelloTriangle::CreateRootSignature()
{
    {
        CD3DX12_ROOT_PARAMETER rootParams[1] = {};
        rootParams[0].InitAsConstantBufferView(
            0,                               // BaseShaderRegister = 0 → b0
            0,                               // RegisterSpace = 0
            
            D3D12_SHADER_VISIBILITY_VERTEX   // Or _ALL if you plan to use it in other stages later
        );
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
            1, rootParams, // NO ROOT PARAMETERS
            0, nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );
        //CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        //    _countof(rootParams), rootParams,
        //    0, nullptr,  // No static samplers for now
        //    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT  // Important for vertex input
        //);;
        

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }
}

// Render the scene.
void D3D12HelloTriangle::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void D3D12HelloTriangle::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
   m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);
    m_commandList->SetPipelineState(m_pipelineState.Get());
    m_commandList->SetGraphicsRootConstantBufferView(
        0,
        m_constantBuffer->GetGPUVirtualAddress());
    m_commandList->DrawIndexedInstanced(
        m_indexCount,    // number of indices (usually triangles × 3)
        1,               // instance count
        0,               // start index location
        0,               // base vertex location
        0                // start instance location
    );

    // Indicate that the back buffer will now be used to present.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
