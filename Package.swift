// swift-tools-version: 5.10.1
import PackageDescription
    let package = Package(
        name: "CMPlayer",        
        defaultLocalization: "en",        
        products: [
            .executable(name: "cmplayer", targets: ["CMPlayer"])
            //.library(name: "CCMPAudioSupport", type: .dynamic, targets: ["CCMPAudioSupport"]),            
        ],    
        dependencies: [
            
        ],
        targets: [            
            .executableTarget(
                name: "CMPlayer",
                dependencies: [                        
                    "CCMPAudioSupport"
                ],
                cSettings: [
                    .define("CMP_PLATFORM_AMD64"),
                    //.define("CMP_PLATFORM_ARM64"),
                    .define("CMP_TARGET_WIN_V11"),
                ],
                swiftSettings: [
                    .define("CMP_PLATFORM_AMD64"),
                    //.define("CMP_PLATFORM_ARM64"),
                    .define("CMP_TARGET_WIN_V11"),
                ]
            ),
            .target(
                name: "CCMPAudioSupport",
                dependencies: [],
                cSettings: [                    
                    .headerSearchPath("include"),
                ],
                linkerSettings: [
                    .linkedLibrary("CMPAudioSupport"),
                    .unsafeFlags(["-L", "CMPAudioSupport/CMPAudioSupport/x64/Debug"])
                ]
            )          
        ]        
    )
