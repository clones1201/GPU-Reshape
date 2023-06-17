// 
// The MIT License (MIT)
// 
// Copyright (c) 2023 Miguel Petersen
// Copyright (c) 2023 Advanced Micro Devices, Inc
// Copyright (c) 2023 Avalanche Studios Group
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

//! KERNEL    Compute "main"
//! EXECUTOR "SamplerIndexOOBExecutor"
//! SCHEMA   "Schemas/Features/Descriptor.h"
//! SAFEGUARD

[[vk::binding(0)]] cbuffer ConstantBuffer : register(b0, space0) {
	uint gCBOffset;
}

[[vk::binding(1, 0)]] RWBuffer<float>  bufferRW   : register(u1, space0);
[[vk::binding(2, 0)]] Texture2D<float> textureRO  : register(t2, space0);
[[vk::binding(0, 1)]] SamplerState     samplers[] : register(s0, space1);

[numthreads(64, 1, 1)]
void main(uint dtid : SV_DispatchThreadID) {
    float data = 1.0f;

    //! MESSAGE DescriptorMismatch[64]
    data += textureRO.SampleLevel(samplers[gCBOffset], 0.0f.xx, 0.0f);

    //! MESSAGE DescriptorMismatch[0]
	bufferRW[dtid.x] = data;
}
