#define MATERIAL_NORMAL_MAP 0x01u
#define MATERIAL_SPECULAR 0x02u
#define MATERIAL_SPECULAR_MAP 0x04u
#define MATERIAL_ENVIRONMENT_MAP 0x08u
#define MATERIAL_ALPHA_TEST 0x10u
#define MATERIAL_PREMULTIPLIED_ALPHA 0x20u
#define MATERIAL_MODEL_SPACE_NORMAL 0x40u
#define MATERIAL_FACEGEN 0x80u
#define MATERIAL_FACEGEN_RGB_TINT 0x100u

cbuffer SceneConstants : register(b0)
{
    row_major float4x4 gWorldViewProjection;
    float4 gCameraPosition;
    float4 gAmbientTop;
    float4 gAmbientBottom;
    float4 gLightDirection[3];
    float4 gLightColor[3];
    // x: linear exposure multiplier
    float4 gPostProcessParams;
};

cbuffer MaterialConstants : register(b1)
{
    float4 gSpecularColorPower;
    // x: opacity, y: specular strength, z: environment scale, w: alpha threshold
    float4 gMaterialParams;
    float4 gTintColor;
    uint gMaterialFlags;
    float3 gMaterialPadding;
};

Texture2D<float4> gDiffuseTexture : register(t0);
Texture2D<float4> gNormalTexture : register(t1);
Texture2D<float4> gSpecularTexture : register(t2);
TextureCube<float4> gEnvironmentTexture : register(t3);
Texture2D<float4> gEnvironmentMaskTexture : register(t4);
Texture2D<float4> gFaceTintTexture : register(t5);
Texture2D<float4> gFaceDetailTexture : register(t6);
SamplerState gWrapSampler : register(s0);
SamplerState gClampSampler : register(s1);

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 textureCoordinate : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float4 tangent : TEXCOORD2;
    float2 textureCoordinate : TEXCOORD3;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), gWorldViewProjection);
    output.worldPosition = input.position;
    output.normal = input.normal;
    output.tangent = input.tangent;
    output.textureCoordinate = input.textureCoordinate;
    return output;
}

float3 ToneMapACES(float3 linearColor)
{
    const float3 numerator = linearColor * (2.51f * linearColor + 0.03f);
    const float3 denominator = linearColor * (2.43f * linearColor + 0.59f) + 0.14f;
    return saturate(numerator / denominator);
}

float3 LinearToSrgb(float3 linearColor)
{
    const float3 lower = linearColor * 12.92f;
    const float3 upper = 1.055f * pow(max(linearColor, 0.0f), 1.0f / 2.4f) - 0.055f;
    return lerp(upper, lower, step(linearColor, 0.0031308f));
}

float3 ApplyFaceGenOverlay(float3 diffuseColor, float3 tintColor, float3 detailColor)
{
    const float3 diffuseSquared = diffuseColor * diffuseColor;
    return (diffuseSquared + 2.0f * tintColor * diffuseColor -
            2.0f * tintColor * diffuseSquared) * detailColor;
}

float4 PSMain(VSOutput input) : SV_Target
{
    const float4 albedo = gDiffuseTexture.Sample(gWrapSampler, input.textureCoordinate);
    const float4 normalSample = gNormalTexture.Sample(gWrapSampler, input.textureCoordinate);

    float3 normal = normalize(input.normal);
    if ((gMaterialFlags & MATERIAL_NORMAL_MAP) != 0u)
    {
        const float3 tangent = normalize(input.tangent.xyz - normal * dot(normal, input.tangent.xyz));
        const float3 bitangent = normalize(cross(normal, tangent)) * input.tangent.w;
        const float3 tangentNormal = normalSample.xyz * 2.0f - 1.0f;
        normal = normalize(tangent * tangentNormal.x +
                           bitangent * tangentNormal.y +
                           normal * tangentNormal.z);
    }
    else if ((gMaterialFlags & MATERIAL_MODEL_SPACE_NORMAL) != 0u)
    {
        normal = normalize(normalSample.xyz * 2.0f - 1.0f);
    }

    const float3 viewDirection = normalize(gCameraPosition.xyz - input.worldPosition);
    const float hemisphereAmount = saturate(normal.z * 0.5f + 0.5f);
    float3 lighting = lerp(gAmbientBottom.rgb, gAmbientTop.rgb, hemisphereAmount);
    float3 specular = 0.0f;
    float specularMask = normalSample.a;
    if ((gMaterialFlags & MATERIAL_SPECULAR_MAP) != 0u)
    {
        specularMask = gSpecularTexture.Sample(gWrapSampler, input.textureCoordinate).r;
    }

    [unroll]
    for (int lightIndex = 0; lightIndex < 3; ++lightIndex)
    {
        const float3 lightDirection = normalize(gLightDirection[lightIndex].xyz);
        const float diffuseAmount = saturate(dot(normal, lightDirection));
        lighting += gLightColor[lightIndex].rgb * diffuseAmount;

        if ((gMaterialFlags & MATERIAL_SPECULAR) != 0u && diffuseAmount > 0.0f)
        {
            const float3 halfVector = normalize(lightDirection + viewDirection);
            const float highlight = pow(saturate(dot(normal, halfVector)),
                                        gSpecularColorPower.w);
            specular += gLightColor[lightIndex].rgb *
                        gSpecularColorPower.rgb *
                        highlight * specularMask * gMaterialParams.y;
        }
    }

    float3 environment = 0.0f;
    if ((gMaterialFlags & MATERIAL_ENVIRONMENT_MAP) != 0u)
    {
        const float3 reflection = reflect(-viewDirection, normal);
        const float environmentMask =
            gEnvironmentMaskTexture.Sample(gWrapSampler, input.textureCoordinate).r;
        environment = gEnvironmentTexture.Sample(gClampSampler, reflection).rgb *
                      gSpecularColorPower.rgb * environmentMask * gMaterialParams.z;
    }

    const float alpha = albedo.a * gMaterialParams.x;
    if ((gMaterialFlags & MATERIAL_ALPHA_TEST) != 0u && alpha < gMaterialParams.w)
    {
        discard;
    }

    float3 diffuseColor = albedo.rgb * gTintColor.rgb;
    if ((gMaterialFlags & MATERIAL_FACEGEN) != 0u)
    {
        const float3 tintColor = gFaceTintTexture.Sample(
            gWrapSampler, input.textureCoordinate).rgb;
        const float3 detailColor =
            (gFaceDetailTexture.Sample(gWrapSampler, input.textureCoordinate).rgb +
             (1.0f / 255.0f)) * (255.0f / 64.0f);
        diffuseColor = ApplyFaceGenOverlay(albedo.rgb, tintColor, detailColor);
    }
    else if ((gMaterialFlags & MATERIAL_FACEGEN_RGB_TINT) != 0u)
    {
        const float3 detailColor = float3(1.01172f, 0.996094f, 1.01172f);
        diffuseColor = ApplyFaceGenOverlay(albedo.rgb, gTintColor.rgb, detailColor);
    }

    float3 color = diffuseColor * lighting + specular + environment;
    color = LinearToSrgb(ToneMapACES(max(color, 0.0f) * gPostProcessParams.x));
    if ((gMaterialFlags & MATERIAL_PREMULTIPLIED_ALPHA) != 0u)
    {
        color *= alpha;
    }
    return float4(color, alpha);
}
