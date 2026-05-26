Texture2D<float4> InputBGRA : register(t0);
RWTexture2D<float> OutputY : register(u0);
RWTexture2D<float2> OutputUV : register(u1);

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float4 bgra = InputBGRA.Load(int3(DTid.xy, 0));

    float3 rgb = bgra.rgb;
    float Y = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
    float U = -0.1146 * rgb.r - 0.3854 * rgb.g + 0.5000 * rgb.b + 0.5;
    float V = 0.5000 * rgb.r - 0.4542 * rgb.g - 0.0458 * rgb.b + 0.5;

    Y = Y * (235.0 - 16.0) / 255.0 + (16.0 / 255.0);
    U = U * (240.0 - 16.0) / 255.0 + (16.0 / 255.0);
    V = V * (240.0 - 16.0) / 255.0 + (16.0 / 255.0);

    OutputY[DTid.xy] = saturate(Y);

    if ((DTid.x % 2 == 0) && (DTid.y % 2 == 0))
    {
        float2 uvSum = float2(0, 0);
    [unroll]
        for (int dy = 0; dy < 2; ++dy)
        {
        [unroll]
            for (int dx = 0; dx < 2; ++dx)
            {
                float4 px = InputBGRA.Load(int3(DTid.xy + uint2(dx, dy), 0));
                float3 c = px.rgb;
                float u = -0.1146 * c.r - 0.3854 * c.g + 0.5000 * c.b + 0.5;
                float v = 0.5000 * c.r - 0.4542 * c.g - 0.0458 * c.b + 0.5;
                u = u * (240.0 - 16.0) / 255.0 + (16.0 / 255.0);
                v = v * (240.0 - 16.0) / 255.0 + (16.0 / 255.0);
                uvSum += float2(u, v);
            }
        }
        float2 uvAvg = saturate(uvSum / 4.0);
        OutputUV[DTid.xy / 2] = uvAvg;
    }
}