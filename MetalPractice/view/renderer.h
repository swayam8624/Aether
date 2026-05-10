//
// Created by Swayam Singal on 10/05/26.
//


#pragma once
#include "../config.h"

class Renderer
{
public:
    Renderer(MTL::Device* device);
    ~Renderer();
    void draw(MTK::View* view);

private:
    MTL::Device* device;
    MTL::CommandQueue* commandQueue;
};