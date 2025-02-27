#define USE_CODEGEN_FIELDS
#include "Helpers/HookInstaller.hpp"
#include "Helpers/ObjectHelper.hpp"
#include "Types/PreloadedFrames.hpp"
//#include "Types/MSAAPostEffect.hpp"
#include "MRCConfig.hpp"
#include "main.hpp"

#include "System/IntPtr.hpp"
#include "System/Func_2.hpp"
#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/utils.h"

#include "UnityEngine/Time.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Vector2.hpp"
#include "UnityEngine/Vector3.hpp"
#include "UnityEngine/Texture.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/Texture2D.hpp"
#include "UnityEngine/Quaternion.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "UnityEngine/RenderTexture.hpp"
#include "UnityEngine/TextureFormat.hpp"
#include "UnityEngine/XR/XRSettings.hpp"
#include "UnityEngine/QualitySettings.hpp"
#include "UnityEngine/ImageConversion.hpp"

#include "GlobalNamespace/ExternalCamerasManager.hpp"
#include "GlobalNamespace/OculusMRCManager.hpp"
#include "GlobalNamespace/OVRCameraRig.hpp"
#include "GlobalNamespace/OVRPlugin.hpp"
#include "GlobalNamespace/OVRPlugin_Media.hpp"
#include "GlobalNamespace/OVRPlugin_Result.hpp"
#include "GlobalNamespace/OVRPlugin_OVRP_1_49_0.hpp"
#include "GlobalNamespace/OVRPlugin_OVRP_1_15_0.hpp"
#include "GlobalNamespace/OculusVRHelper.hpp"
#include "GlobalNamespace/OVRManager.hpp"
#include "GlobalNamespace/OVRManager_TrackingOrigin.hpp"
#include "GlobalNamespace/OVRExternalComposition.hpp"
#include "GlobalNamespace/OVRMixedRealityCaptureConfiguration.hpp"
#include "GlobalNamespace/OVRInput_Button.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "GlobalNamespace/BoolSO.hpp"

#include "UnityEngine/Shader.hpp"

using namespace GlobalNamespace;

bool canSwitch = true;

void HotSwitchCamera()
{
    auto& modcfg = getConfig().config;
    if (!modcfg["useCameraHotkey"].GetBool()) return;

    std::string mode = (std::string)modcfg["cameraMode"].GetString();
    if (mode == "First Person") modcfg["cameraMode"].SetString("Third Person", modcfg.GetAllocator());
    else if (mode == "Third Person") modcfg["cameraMode"].SetString("First Person", modcfg.GetAllocator());
    canSwitch = false;
}

void InitPreloadedFrames()
{
    UnityEngine::GameObject* frameObj = UnityEngine::GameObject::New_ctor(il2cpp_utils::newcsstr("PreloadedMRCFrames"));
    UnityEngine::Object::DontDestroyOnLoad(frameObj);
    preloadedFrames = frameObj->AddComponent<MRCPlus::PreloadedFrames*>();
}

void SetCullingMasks(UnityEngine::Camera* hmdCam, UnityEngine::Camera* mrcCam)
{
    if (!hmdCam || !mrcCam) return;
    auto& modcfg = getConfig().config;
    long hmdMask = hmdCam->get_cullingMask();
    long mrcMask = mrcCam->get_cullingMask();    
    mrcMask = hmdMask;

    // Only render this layer in third-person
    hmdMask = hmdMask &= ~(1 << 3);
    mrcMask = mrcMask |= 1 << 3;

    // Only render this layer in first-person
    hmdMask = hmdMask |= 1 << 6;
    mrcMask = mrcMask &= ~(1 << 6);

    // Apply transparent wall setting
    mrcMask = mrcMask |= 1 << 27;
    if (modcfg["enableTransparentWalls"].GetBool())
    {
        mrcMask = mrcMask &= ~(1 << 27);
    }
    hmdCam->set_cullingMask(hmdMask);
    mrcCam->set_cullingMask(mrcMask);
}

MAKE_HOOK_MATCH(OVRCameraRig_Start, &GlobalNamespace::OVRCameraRig::Start, void, GlobalNamespace::OVRCameraRig* self)
{
    OVRCameraRig_Start(self);
    // if (!OVRPlugin::GetUseOverriddenExternalCameraFov(0)) {
    //     OVRPlugin::Result success = OVRPlugin::OVRP_1_15_0::ovrp_GetExternalCameraIntrinsics(0, mrcInfo);
    // }

    // auto& modcfg = getConfig().config;
    // int fwidth = modcfg["width"].GetInt();
    // int fheight = modcfg["height"].GetInt();

    // OVRPlugin::Media::SetMrcFrameSize(fwidth, fheight);
    // SetAsymmetricFOV((float)fwidth, (float)fheight);
    // getLogger().info("mrcplusinit");
}

MAKE_HOOK_FIND_CLASS(OVRPlugin_InitializeMR, "", "OVRPlugin", "InitializeMixedReality", void)
{
    OVRPlugin_InitializeMR();
    InitPreloadedFrames();
    CreateReferenceObject();

    auto& modcfg = getConfig().config;
    int fwidth = modcfg["width"].GetInt();
    int fheight = modcfg["height"].GetInt();

    // int mrwidth;
    // int mrheight;
    // OVRPlugin::Media::GetMrcFrameSize(byref(mrwidth), byref(mrheight));
    // getLogger().info("MRC+ width: %d", mrwidth);
    OVRPlugin::Media::SetMrcFrameSize(fwidth, fheight);
    SetAsymmetricFOV((float)fwidth, (float)fheight);

    getLogger().info("[MRCPlus] Mixed Reality Capture initialized.");
}

MAKE_HOOK_FIND_CLASS_UNSAFE_STATIC(OVRPlugin_EncodeMrcFrame, "", "OVRPlugin/Media", "EncodeMrcFrame", bool, System::IntPtr textureHandle, System::IntPtr fgTextureHandle, ArrayW<float> audioData, int audioFrames, int audioChannels, double timestamp, double poseTime, int& outSyncId)
{
    // OVRPlugin::Media::SyncMrcFrame(outSyncId);
    OVRPlugin_EncodeMrcFrame(textureHandle, fgTextureHandle, audioData, audioFrames, audioChannels, timestamp, poseTime, outSyncId);
    // getLogger().info("[MRCPlus] MFrame encoded! %d", timestamp);
    return true;
}

MAKE_HOOK_FIND_CLASS_UNSAFE_STATIC(OVRPlugin_SyncMrcFrame, "", "OVRPlugin/OVRP_1_38_0", "ovrp_Media_SyncMrcFrame", bool, int syncId)
{
    int syncthingy = syncId;
    getLogger().info("[MRCPlus] Frame sync! %d", syncId);
    OVRPlugin_SyncMrcFrame(syncId);

    return true;
}

MAKE_HOOK_MATCH(OVRExternalComposition_Update, &GlobalNamespace::OVRExternalComposition::Update, void, GlobalNamespace::OVRExternalComposition* instance, UnityEngine::GameObject* parentObj, UnityEngine::Camera* mainCam, GlobalNamespace::OVRMixedRealityCaptureConfiguration* mrcConfig, GlobalNamespace::OVRManager_TrackingOrigin trackingOrigin)
{
    OVRExternalComposition_Update(instance, parentObj, mainCam, mrcConfig, trackingOrigin);

    auto& modcfg = getConfig().config;
    auto* mainCamera = UnityEngine::Camera::get_main();
    auto* bgCamera = instance->backgroundCamera;
    auto* fgCamera = instance->foregroundCamera;

    bool mrcPlusActive = MRCPlusEnabled();

    if (viewfinderMat && bgCamera && modcfg["showViewfinder"].GetBool())
    {
        viewfinderMat->SetTexture(il2cpp_utils::newcsstr("_NearTex"), bgCamera->get_targetTexture());
        viewfinderMat->SetTexture(il2cpp_utils::newcsstr("_FarTex"), preloadedFrames->PreviewTitle);
    }

    // Extended camera properties
    if (!mrcPlusActive) return;
    SetCullingMasks(mainCamera, bgCamera);
   
    // if (OVRPlugin::get_systemDisplayFrequency() == 90.0f && OVRPlugin::get_systemDisplayFrequenciesAvailable().Contains(120.0f))
    // {
    //     OVRPlugin::set_systemDisplayFrequency(120.0f);
    // }
    // UnityEngine::XR::XRSettings::set_eyeTextureResolutionScale(0.7f);
    // if (UnityEngine::QualitySettings::get_antiAliasing() == 4) UnityEngine::QualitySettings::set_antiAliasing(2);

    // Override camera placement
    // ArrayW<float> array_noaudio = {};
    UnityEngine::Transform* refTransform = rotationRef->get_transform();
    bgCamera->get_transform()->SetPositionAndRotation(refTransform->get_position(), refTransform->get_rotation());
    // bgCamera->Render();
    // int syncID = 0;
    // bool result = CRASH_UNLESS(il2cpp_utils::RunMethodUnsafe<bool>("", "OVRPlugin/Media", "EncodeMrcFrame", bgCamera->get_targetTexture()->GetNativeTexturePtr(), fgCamera->get_targetTexture()->GetNativeTexturePtr(), array_noaudio, 0, 0, (double)0, (double)0, byref(syncID)));
}

MAKE_HOOK_MATCH(OVRManager_LateUpdate, &GlobalNamespace::OculusVRHelper::LateUpdate, void, GlobalNamespace::OculusVRHelper* self)
{
    OVRManager_LateUpdate(self);
    UnityEngine::Camera* maincam = UnityEngine::Camera::get_main();
    auto& modcfg = getConfig().config;
    if (rotationRef && maincam && MRCPlusEnabled())
    {
        // References
        auto* refTransform = rotationRef->get_transform();
        UnityEngine::Vector3 targetPos;
        UnityEngine::Quaternion targetRot;

        // Camera modes
        bool button_x = OVRInput::Get(OVRInput::Button::One, OVRInput::Controller::LTouch);
        bool button_anyclick = OVRInput::Get(OVRInput::Button::PrimaryIndexTrigger, OVRInput::Controller::All);
        if (button_x && canSwitch) HotSwitchCamera();

        if (strcmp(modcfg["cameraMode"].GetString(), "First Person") == 0)
        {
            auto* targetTransform = maincam->get_transform();
            targetRot = targetTransform->get_rotation();
            targetPos = targetTransform->get_position();
        }
        if (strcmp(modcfg["cameraMode"].GetString(), "Third Person") == 0)
        {
            targetRot = UnityEngine::Quaternion::Euler(modcfg["angX"].GetFloat(), modcfg["angY"].GetFloat(), modcfg["angZ"].GetFloat());
            targetPos = UnityEngine::Vector3(modcfg["posX"].GetFloat(), modcfg["posY"].GetFloat(), modcfg["posZ"].GetFloat());
        }
        canSwitch = !button_x;

        // Lerp smoothing
        float smoothpos = 10.0f - modcfg["positionSmoothness"].GetFloat();
        float smoothrot = 10.0f - modcfg["rotationSmoothness"].GetFloat();
        auto lerpPos = UnityEngine::Vector3::Lerp(refTransform->get_position(), targetPos, UnityEngine::Time::get_deltaTime() * std::clamp(smoothpos, 1.0f, 10.0f));
        auto lerpRot = UnityEngine::Quaternion::Slerp(refTransform->get_rotation(), targetRot, UnityEngine::Time::get_deltaTime() * std::clamp(smoothrot, 1.0f, 10.0f));

        refTransform->SetPositionAndRotation(lerpPos, lerpRot);
    }
}

// MAKE_HOOK_MATCH(ExternalCamerasManager_Init, &GlobalNamespace::ExternalCamerasManager::OnEnable, void, GlobalNamespace::ExternalCamerasManager* instance)
// {
//     ExternalCamerasManager_Init(instance);
//     instance->oculusMRCManager->set_enableMixedReality(true);
//     OVRPlugin::Media::SyncMrcFrame(0);
// }

void MRCPlus::Hooks::Install_CameraHooks()
{
    INSTALL_HOOK(getLogger(), OVRPlugin_InitializeMR);
    INSTALL_HOOK(getLogger(), OVRCameraRig_Start);
    INSTALL_HOOK(getLogger(), OVRPlugin_SyncMrcFrame);
    INSTALL_HOOK(getLogger(), OVRExternalComposition_Update);
    INSTALL_HOOK(getLogger(), OVRManager_LateUpdate);
    getLogger().info("[MRCPlus] Installed Camera hooks");
}

