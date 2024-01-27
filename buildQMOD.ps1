# Builds a .zip file for loading with BMBF
& ./build.ps1
Compress-Archive -Path  "./extern/libs/libbeatsaber-hook_3_14_0.so",`
                        "./build/libMRCPlus_0_6_0.so",`
                        "./cover.png",`
                        "./mod.json" -DestinationPath "./MRCPlus.zip" -Update
& copy-item -Force "./MRCPlus.zip" "./MRCPlus.qmod"
