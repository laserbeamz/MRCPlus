& ./build.ps1

& adb push build/libMRCPlus_0_6_0.so /sdcard/Android/data/com.beatgames.beatsaber/files/mods/libMRCPlus_0_6_0.so
& adb shell am force-stop com.beatgames.beatsaber
& adb shell am start com.beatgames.beatsaber/com.unity3d.player.UnityPlayerActivity
