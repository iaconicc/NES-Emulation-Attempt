Texture2D tex;
SamplerState sample;

float4 main(float2 tex_pos : TEXCOORD) : SV_TARGET
{
    float4 colour = tex.Sample(sample, tex_pos);
    return colour;
}