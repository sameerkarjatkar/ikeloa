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

#pragma once

#include "DXSample.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

class D3D12HelloTriangle : public DXSample
{
public:
    D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnSizeChange() ;
    virtual void OnMouseMove(UINT x, UINT y) override;
    virtual void OnMouseDrag(UINT x, UINT y) override;

private:
    static const UINT FrameCount = 2;

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        float scalar;
       // XMFLOAT4 color;
    };

    struct SceneCB
    {
        DirectX::XMFLOAT4X4 WorldViewProj;
    };

    ComPtr<ID3D12Resource> m_constantBuffer;
    SceneCB m_constantBufferData;

    UINT8* m_pCbvDataBegin = nullptr;

    struct OrbitCamera
    {
        float yaw = 0.0f;        // horizontal rotation
        float pitch = 0.3f;      // vertical rotation
        float radius = 5.0f;     // distance from target

        DirectX::XMFLOAT3 target = { 0,0,0 }; // mesh center
    };

    XMVECTOR GetCameraPosition()
    {
        using namespace DirectX;

        float x = m_camera.radius * cosf(m_camera.pitch) * sinf(m_camera.yaw);
        float y = m_camera.radius * sinf(m_camera.pitch);
        float z = m_camera.radius * cosf(m_camera.pitch) * cosf(m_camera.yaw);

        XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
        XMVECTOR target = XMLoadFloat3(&m_camera.target);

        return XMVectorAdd(target, pos);
    }
    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_indexBuffer;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    OrbitCamera m_camera;
    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    UINT m_indexCount = 0;

    XMMATRIX m_world;
    XMMATRIX m_view;
    XMMATRIX m_proj;

    float m_aspectRatio = 1.0f;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void CreateRootSignature();
    void PanCamera(float dx, float dy);
    void OrbitCamera(int dx, int dy);
};
