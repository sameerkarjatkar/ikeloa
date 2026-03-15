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

struct PSInput
{
    float4 Pos : SV_POSITION;
    float3 Normal : TEXCOORD1;
    float scalar : TEXCOORD0;
};

float3 TurboColormap(float x)
{
    x = saturate(x);

    float3 c;

    c.r = 0.13572138 + x * (4.61539260 + x * (-42.66032258 + x * (132.13108234 + x * (-152.94239396 + x * 59.28637943))));
    c.g = 0.09140261 + x * (2.19418839 + x * (4.84296658 + x * (-14.18503333 + x * (4.27729857 + x * 2.82956604))));
    c.b = 0.10667330 + x * (13.01731274 + x * (-56.64682182 + x * (95.43021951 + x * (-72.42115312 + x * 20.46897718))));

    return saturate(c);
}

float4 PSMain(PSInput input) : SV_TARGET
{
     float3 lightDir = normalize(float3(0.4,0.7,-0.6));
     float NdotL = max(dot(input.Normal, lightDir), 0.0);

    float normalized = input.scalar * 0.5 + 0.5;
    float3 color = TurboColormap(normalized);
    float ambient = 0.25;
    float3 finalColor = color * (ambient + NdotL);

    //return float4(0.8, 0.8, 0.9, 1.0); // light grey torus
    return float4(finalColor, 1);
}
