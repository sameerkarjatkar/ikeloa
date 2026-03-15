
cbuffer SceneCB : register(b0)
{
    matrix g_WorldViewProj;
};

struct VSInput {
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float scalar : TEXCOORD0;
    
};

struct PSInput {
    float4 Pos : SV_POSITION;
    float3 Normal : TEXCOORD1;
    float scalar : TEXCOORD0;
   
};

PSInput main(VSInput input)
{
    PSInput output;
    output.Pos = mul(float4(input.Pos, 1.0f), g_WorldViewProj);
    output.Normal = normalize(input.Normal);
    output.scalar = input.scalar;
    return output;
}