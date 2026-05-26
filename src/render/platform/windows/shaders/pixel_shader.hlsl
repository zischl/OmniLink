Texture2D tex : register(t0);
SamplerState samp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};


float4 PSMain(PSInput input) : SV_Target
{
    float2 flippedUV = float2(input.texcoord.x, input.texcoord.y);
    return tex.Sample(samp, flippedUV);
}
