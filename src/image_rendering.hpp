#pragma once

#include "imgui.h"
#include "plot_types.hpp"
#include "ui_state.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <cstdio>

// Include stb_image for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// -------------------------------------------------------------------------
// TEXTURE MANAGEMENT
// -------------------------------------------------------------------------

// Load image from file and create OpenGL texture
// Returns texture ID (0 on failure)
unsigned int LoadImageToTexture(const std::string& filePath, int* out_width, int* out_height) {
    // Load image using stb_image
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4); // Force RGBA
    
    if (!data) {
        printf("[ImageWindow] Failed to load image: %s\\n", filePath.c_str());
        return 0;
    }
    
    // Create OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload image data to GPU
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    // Free image data from system memory
    stbi_image_free(data);
    
    *out_width = width;
    *out_height = height;
    
    printf("[ImageWindow] Loaded texture %u from %s (%dx%d)\\n", 
           textureID, filePath.c_str(), width, height);
    
    return textureID;
}

// Update existing texture with raw RGBA data (for video streams)
bool UpdateTextureData(unsigned int textureID, const void* data, int width, int height) {
    if (textureID == 0 || !data) {
        return false;
    }
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    
    return true;
}

// Delete OpenGL texture
void DeleteTexture(unsigned int textureID) {
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
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
            ImVec2 imageSize(imageWindow.width, imageWindow.height);
            
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
            
            // Display the texture
            ImGui::Image((ImTextureID)(intptr_t)imageWindow.textureID, imageSize);
            
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
