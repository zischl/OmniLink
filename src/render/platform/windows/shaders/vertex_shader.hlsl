struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};


PSInput VSMain(float3 position : POSITION, float2 texcoord : TEXCOORD0)
{
    PSInput output;
    output.position = float4(position, 1.0f);
    output.texcoord = float2(texcoord);
    return output;
}
