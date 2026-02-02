#pragma once

#include "imgui.h"
#include "plot_types.hpp"
#include "ui_state.hpp"
#include <d3d11.h>
#include <string>
#include <cstdio>

// Include stb_image for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// External access to D3D11 globals
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;

// -------------------------------------------------------------------------
// TEXTURE MANAGEMENT
// -------------------------------------------------------------------------

// Load image from file and create DX11 ShaderResourceView
// Returns pointer to SRV (as uintptr_t) or 0 on failure
unsigned int LoadImageToTexture(const std::string& filePath, int* out_width, int* out_height) {
    if (!g_pd3dDevice) return 0;

    // Load image using stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4); // Force RGBA
    
    if (!data) {
        printf("[ImageWindow] Failed to load image: %s\n", filePath.c_str());
        return 0;
    }
    
    // Create DX11 Texture
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA subResource;
    subResource.pSysMem = data;
    subResource.SysMemPitch = desc.Width * 4;
    subResource.SysMemSlicePitch = 0;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
    
    // Free image data from system memory
    stbi_image_free(data);

    if (FAILED(hr)) {
        printf("[ImageWindow] Failed to create texture for %s\n", filePath.c_str());
        return 0;
    }

    // Create Shader Resource View
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    ID3D11ShaderResourceView* pTextureView = nullptr;
    g_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &pTextureView);
    pTexture->Release(); // SRV holds a reference

    if (!pTextureView) {
        printf("[ImageWindow] Failed to create SRV for %s\n", filePath.c_str());
        return 0;
    }

    *out_width = width;
    *out_height = height;
    
    printf("[ImageWindow] Loaded texture %p from %s (%dx%d)\n", 
           pTextureView, filePath.c_str(), width, height);
    
    return (unsigned int)(uintptr_t)pTextureView;
}

// Update existing texture with raw RGBA data (for video streams)
// Note: This requires the texture to be created with D3D11_USAGE_DEFAULT or D3D11_USAGE_DYNAMIC
// ImGui examples usually handle this by creating a new texture if size changes, 
// or using UpdateSubresource for same size.
bool UpdateTextureData(unsigned int textureID, const void* data, int width, int height) {
    if (textureID == 0 || !data || !g_pd3dDeviceContext) {
        return false;
    }
    
    ID3D11ShaderResourceView* pTextureView = (ID3D11ShaderResourceView*)(uintptr_t)textureID;
    
    // Get the underlying resource
    ID3D11Resource* pResource = nullptr;
    pTextureView->GetResource(&pResource);
    if (!pResource) return false;

    // Check dimensions (optional, but good safety)
    // For simplicity, we assume the caller matches dimensions or we might need to recreate.
    // D3D11 UpdateSubresource requires matching size conceptually, but it just overwrites memory.
    // If width/height changed, we really should destroy and recreate.
    
    // For now, assume size matches.
    g_pd3dDeviceContext->UpdateSubresource(pResource, 0, nullptr, data, width * 4, 0);

    pResource->Release();
    return true;
}

// Delete DX11 texture (release SRV)
void DeleteTexture(unsigned int textureID) {
    if (textureID != 0) {
        ID3D11ShaderResourceView* pTextureView = (ID3D11ShaderResourceView*)(uintptr_t)textureID;
        pTextureView->Release();
    }
}

// -------------------------------------------------------------------------
// IMAGE WINDOW DESTRUCTOR IMPLEMENTATION
// -------------------------------------------------------------------------

ImageWindow::~ImageWindow() {
    if (textureID != 0) {
        DeleteTexture(textureID);
        textureID = 0;
    }
}

// -------------------------------------------------------------------------
// IMAGE WINDOW RENDERING
// -------------------------------------------------------------------------

void RenderImageWindows(UIPlotState& uiPlotState, float menuBarHeight) {
    for (auto& imageWindow : uiPlotState.activeImageWindows) {
        if (!imageWindow.isOpen) continue;
        
        std::string windowID = imageWindow.title + "##ImageWindow" + std::to_string(imageWindow.id);
        
        ImGui::Begin(windowID.c_str(), &imageWindow.isOpen);
        
        // Toolbar
        if (ImGui::Button("Fit to Window")) {
            imageWindow.fitToWindow = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Actual Size")) {
            imageWindow.fitToWindow = false;
        }
        
        if (!imageWindow.filePath.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("Source: %s", imageWindow.filePath.c_str());
        }
        
        ImGui::Separator();
        
        // Display image
        if (imageWindow.textureID != 0 && imageWindow.width > 0 && imageWindow.height > 0) {
            // Calculate display size
            ImVec2 imageSize((float)imageWindow.width, (float)imageWindow.height);
            
            if (imageWindow.fitToWindow) {
                // Fit to available content region while maintaining aspect ratio
                ImVec2 availSize = ImGui::GetContentRegionAvail();
                float aspectRatio = (float)imageWindow.width / (float)imageWindow.height;
                
                if (availSize.x / availSize.y > aspectRatio) {
                    // Constrained by height
                    imageSize.y = availSize.y;
                    imageSize.x = availSize.y * aspectRatio;
                } else {
                    // Constrained by width
                    imageSize.x = availSize.x;
                    imageSize.y = availSize.x / aspectRatio;
                }
            }
            
            // Display the texture (cast to void* for ImGui)
            ImGui::Image((void*)(uintptr_t)imageWindow.textureID, imageSize);
            
            // Show image info on hover
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Size: %dx%d pixels", imageWindow.width, imageWindow.height);
                ImGui::EndTooltip();
            }
        } else {
            ImGui::TextDisabled("No image loaded");
        }
        
        ImGui::End();
    }
}
