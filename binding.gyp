{
  "targets": [
    {
      "target_name": "addon",
      "sources": [
        "./src/Navmesh/Detour/DetourCommon.cpp",
        "./src/Navmesh/Detour/DetourAlloc.cpp",
        "./src/Navmesh/Detour/DetourNode.cpp",
        "./src/Navmesh/Detour/DetourNavMesh.cpp",
        "./src/Navmesh/Detour/DetourNavMeshQuery.cpp",
        "./src/main.cpp"
      ],
      'include_dirs': [
        "./src/",
        "./src/Navmesh/",
        "./src/Navmesh/Recast",
        "./src/Navmesh/Detour",
        "./src/Navmesh/DetourCrowd",
        "./src/Navmesh/DetourTileCache",
        "./src/Navmesh/DebugUtils",
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": {
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "ExceptionHandling": 1
        }
      },
    }
  ]
}