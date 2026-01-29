struct VSOut
{
    float2 textureCoords : TEXCOORD;
    float4 pos : SV_Position;
};

VSOut main(float2 pos : POSITION, float2 tex_pos : TEXCOORD)
{
    VSOut vsout;
    vsout.pos = float4(pos, 0.0f, 1.0f);
    vsout.textureCoords = tex_pos;
    return vsout;
}