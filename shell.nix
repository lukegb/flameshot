{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  nativeBuildInputs = with pkgs; ((
    with libsForQt5; [
      qttools qtsvg qtbase
      wrapQtAppsHook
    ]
  ) ++ [
    cmake

    (vscode-with-extensions.override {
      vscodeExtensions = with vscode-extensions; [
        ms-vscode.cpptools
      ] ++ vscode-utils.extensionsFromVscodeMarketplace [
        {
          name = "cmake-tools";
          publisher = "ms-vscode";
          version = "1.6.0";
          sha256 = "sha256:1j3b6wzlb5r9q2v023qq965y0avz6dphcn0f5vwm9ns9ilcgm3dw";
        }
      ];
    })
  ]);

  shellHook = ''
    for pkg in "''${qtHostPathSeen[@]}"; do
      local pluginDir="$pkg/''${qtPluginPrefix:?}"
      if [ -d "$pluginDir" ]
      then
        export QT_PLUGIN_PATH="$pluginDir"''${QT_PLUGIN_PATH:+':'}$QT_PLUGIN_PATH
      fi

      local qmlDir="$pkg/''${qtQmlPrefix:?}"
      if [ -d "$qmlDir" ]
      then
        export QML2_IMPORT_PATH="$pluginDir"''${QML2_IMPORT_PATH:+':'}$QML2_IMPORT_PATH
      fi
    done
  '';
}
